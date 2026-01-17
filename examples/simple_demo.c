/*
 * Simple demonstration of using the GeoTIFF VOL connector
 * Reads and displays basic information from a GeoTIFF file
 * Must be run from the build directory
 */

#include <hdf5.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include <geotiffio.h>
#include <xtiffio.h>

#define GEOTIFF_VOL_CONNECTOR_NAME "geotiff_vol_connector"

#define IMAGE_SIZE 256
#define OUTPUT_FILENAME "demo_rings.tif"

/* HSV to RGB conversion for colorful rings */
void hsv_to_rgb(float h, float s, float v, unsigned char *r, unsigned char *g, unsigned char *b)
{
    float c = v * s;
    float x = c * (1.0f - fabsf(fmodf(h / 60.0f, 2.0f) - 1.0f));
    float m = v - c;
    float r_prime, g_prime, b_prime;

    if (h >= 0 && h < 60) {
        r_prime = c;
        g_prime = x;
        b_prime = 0;
    } else if (h >= 60 && h < 120) {
        r_prime = x;
        g_prime = c;
        b_prime = 0;
    } else if (h >= 120 && h < 180) {
        r_prime = 0;
        g_prime = c;
        b_prime = x;
    } else if (h >= 180 && h < 240) {
        r_prime = 0;
        g_prime = x;
        b_prime = c;
    } else if (h >= 240 && h < 300) {
        r_prime = x;
        g_prime = 0;
        b_prime = c;
    } else {
        r_prime = c;
        g_prime = 0;
        b_prime = x;
    }

    *r = (unsigned char) ((r_prime + m) * 255.0f);
    *g = (unsigned char) ((g_prime + m) * 255.0f);
    *b = (unsigned char) ((b_prime + m) * 255.0f);
}

int create_demo_image(void)
{
    TIFF *tif = NULL;
    GTIF *gtif = NULL;
    unsigned char *scanline = NULL;
    float center_x = IMAGE_SIZE / 2.0f;
    float center_y = IMAGE_SIZE / 2.0f;
    float max_radius = sqrtf(center_x * center_x + center_y * center_y);

    printf("Creating demo GeoTIFF: %s (%dx%d RGB with concentric rings)...\n", OUTPUT_FILENAME,
           IMAGE_SIZE, IMAGE_SIZE);

    /* Open TIFF file for writing */
    tif = XTIFFOpen(OUTPUT_FILENAME, "w");
    if (!tif) {
        fprintf(stderr, "Failed to create %s\n", OUTPUT_FILENAME);
        return 1;
    }

    /* Create GeoTIFF handle */
    gtif = GTIFNew(tif);
    if (!gtif) {
        fprintf(stderr, "Failed to create GeoTIFF handle\n");
        XTIFFClose(tif);
        return 1;
    }

    /* Set TIFF tags for RGB image */
    TIFFSetField(tif, TIFFTAG_IMAGEWIDTH, IMAGE_SIZE);
    TIFFSetField(tif, TIFFTAG_IMAGELENGTH, IMAGE_SIZE);
    TIFFSetField(tif, TIFFTAG_COMPRESSION, COMPRESSION_NONE);
    TIFFSetField(tif, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_RGB);
    TIFFSetField(tif, TIFFTAG_PLANARCONFIG, PLANARCONFIG_CONTIG);
    TIFFSetField(tif, TIFFTAG_BITSPERSAMPLE, 8);
    TIFFSetField(tif, TIFFTAG_SAMPLESPERPIXEL, 3);
    TIFFSetField(tif, TIFFTAG_ROWSPERSTRIP, IMAGE_SIZE);

    /* Set geographic metadata */
    /* Tie point: pixel (0,0,0) -> geographic (-120.0, 40.0, 0.0) [California area] */
    const double tiepoints[6] = {0, 0, 0, -120.0, 40.0, 0.0};
    /* Pixel scale: 0.01 degrees per pixel */
    const double pixscale[3] = {0.01, 0.01, 0.0};

    TIFFSetField(tif, TIFFTAG_GEOTIEPOINTS, 6, tiepoints);
    TIFFSetField(tif, TIFFTAG_GEOPIXELSCALE, 3, pixscale);

    /* Set GeoTIFF keys */
    GTIFKeySet(gtif, GTModelTypeGeoKey, TYPE_SHORT, 1, ModelGeographic);
    GTIFKeySet(gtif, GTRasterTypeGeoKey, TYPE_SHORT, 1, RasterPixelIsArea);
    GTIFKeySet(gtif, GTCitationGeoKey, TYPE_ASCII, 0, "Demo GeoTIFF with Concentric Rings");
    GTIFKeySet(gtif, GeographicTypeGeoKey, TYPE_SHORT, 1, GCS_WGS_84);

    /* Allocate scanline buffer */
    scanline = (unsigned char *) malloc(IMAGE_SIZE * 3);
    if (!scanline) {
        fprintf(stderr, "Failed to allocate memory\n");
        GTIFFree(gtif);
        XTIFFClose(tif);
        return 1;
    }

    /* Generate concentric rings pattern */
    for (int row = 0; row < IMAGE_SIZE; row++) {
        for (int col = 0; col < IMAGE_SIZE; col++) {
            /* Calculate distance from center */
            float dx = (float)col - center_x;
            float dy = (float)row - center_y;
            float distance = sqrtf(dx * dx + dy * dy);

            /* Normalize distance to 0-1 range */
            float normalized_dist = distance / max_radius;

            /* Create color based on distance (rainbow rings) */
            /* Multiple the distance to create multiple rings */
            float hue = fmodf(normalized_dist * 720.0f, 360.0f); /* 2 full rainbow cycles */
            float saturation = 0.9f;
            float value =
                0.8f + 0.2f * sinf(normalized_dist * 12.0f * (float)M_PI); /* Brightness variation */

            unsigned char r, g, b;
            hsv_to_rgb(hue, saturation, value, &r, &g, &b);

            int idx = col * 3;
            scanline[idx + 0] = r;
            scanline[idx + 1] = g;
            scanline[idx + 2] = b;
        }

        if (!TIFFWriteScanline(tif, scanline, (uint32_t)row, 0)) {
            fprintf(stderr, "Failed to write scanline %d\n", row);
            free(scanline);
            GTIFFree(gtif);
            XTIFFClose(tif);
            return 1;
        }
    }

    /* Clean up */
    free(scanline);
    GTIFWriteKeys(gtif);
    GTIFFree(gtif);
    XTIFFClose(tif);

    printf("Successfully created %s\n", OUTPUT_FILENAME);
    printf("Image features:\n");
    printf("  - Size: %dx%d pixels\n", IMAGE_SIZE, IMAGE_SIZE);
    printf("  - Format: RGB (3 channels)\n");
    printf("  - Geographic location: California area (WGS84)\n");
    printf("  - Pixel scale: 0.01 degrees/pixel\n");

    return 0;
}

int main(void)
{
    hid_t vol_id, fapl_id, file_id;
    hid_t dset_id, space_id, type_id;
    hsize_t dims[3];
    int ndims;
    struct stat st;
    char plugin_path[PATH_MAX];
    char cwd[PATH_MAX];

    /* Check if being run from build directory */
    if (stat("./src", &st) != 0 || !S_ISDIR(st.st_mode)) {
        printf("ERROR: This program must be run from the build directory\n");
        printf("Usage: cd build && ./examples/simple_demo <geotiff_file>\n");
        return 1;
    }

    /* Get absolute path to ./src for HDF5_PLUGIN_PATH */
    if (getcwd(cwd, sizeof(cwd)) == NULL) {
        printf("ERROR: Failed to get current working directory\n");
        return 1;
    }

    int ret = snprintf(plugin_path, sizeof(plugin_path), "%s/src", cwd);
    if (ret < 0 || (size_t) ret >= sizeof(plugin_path)) {
        printf("ERROR: Current working directory path is too long\n");
        return 1;
    }

    /* Set HDF5_PLUGIN_PATH environment variable */
    if (setenv("HDF5_PLUGIN_PATH", plugin_path, 1) != 0) {
        printf("ERROR: Failed to set HDF5_PLUGIN_PATH\n");
        return 1;
    }

    printf("===========================================\n");
    printf("GeoTIFF VOL Connector Simple Demo\n");
    printf("===========================================\n");

    printf("Creating demo GeoTIFF file with concentric rings...\n");

    if (create_demo_image() != 0) {
        printf("ERROR: Failed to create demo GeoTIFF file\n");
        return 1;
    }

    /* Register the GeoTIFF VOL connector */
    vol_id = H5VLregister_connector_by_name(GEOTIFF_VOL_CONNECTOR_NAME, H5P_DEFAULT);
    if (vol_id < 0) {
        printf("ERROR: Failed to register GeoTIFF VOL connector\n");
        printf("Make sure the VOL connector library is built in %s\n", plugin_path);
        return 1;
    }
    printf("Registered GeoTIFF VOL connector (ID: %ld)\n", (long) vol_id);

    /* Create file access property list */
    fapl_id = H5Pcreate(H5P_FILE_ACCESS);
    if (fapl_id < 0) {
        printf("ERROR: Failed to create FAPL\n");
        H5VLunregister_connector(vol_id);
        return 1;
    }

    /* Set the VOL connector */
    if (H5Pset_vol(fapl_id, vol_id, NULL) < 0) {
        printf("ERROR: Failed to set VOL connector\n");
        H5Pclose(fapl_id);
        H5VLunregister_connector(vol_id);
        return 1;
    }

    /* Open the GeoTIFF file */
    file_id = H5Fopen(OUTPUT_FILENAME, H5F_ACC_RDONLY, fapl_id);
    if (file_id < 0) {
        printf("ERROR: Failed to open GeoTIFF file\n");
        H5Pclose(fapl_id);
        H5VLunregister_connector(vol_id);
        return 1;
    }
    printf("Opened GeoTIFF file\n\n");

    /* Open the image dataset */
    printf("Dataset Information:\n");
    printf("-------------------\n");
    dset_id = H5Dopen2(file_id, "/image0", H5P_DEFAULT);
    if (dset_id < 0) {
        printf("ERROR: Failed to open /image dataset\n");
        H5Fclose(file_id);
        H5Pclose(fapl_id);
        H5VLunregister_connector(vol_id);
        return 1;
    }

    /* Get dataspace */
    space_id = H5Dget_space(dset_id);
    if (space_id < 0) {
        printf("ERROR: Failed to get dataspace\n");
        H5Dclose(dset_id);
        H5Fclose(file_id);
        H5Pclose(fapl_id);
        H5VLunregister_connector(vol_id);
        return 1;
    }

    /* Get and display dimensions */
    ndims = H5Sget_simple_extent_ndims(space_id);
    if (ndims > 0 && ndims <= 3) {
        H5Sget_simple_extent_dims(space_id, dims, NULL);
        printf("Dataset: /image\n");
        printf("  Rank: %d\n", ndims);
        printf("  Dimensions: [");
        for (int i = 0; i < ndims; i++) {
            printf("%lu%s", (unsigned long) dims[i], (i < ndims - 1) ? ", " : "");
        }
        printf("]\n");
    }

    /* Get and display datatype */
    type_id = H5Dget_type(dset_id);
    if (type_id >= 0) {
        H5T_class_t type_class = H5Tget_class(type_id);
        size_t type_size = H5Tget_size(type_id);
        const char *type_name = "UNKNOWN";

        switch (type_class) {
            case H5T_INTEGER:
                type_name = "INTEGER";
                break;
            case H5T_FLOAT:
                type_name = "FLOAT";
                break;
            case H5T_STRING:
                type_name = "STRING";
                break;
            case H5T_BITFIELD:
                type_name = "BITFIELD";
                break;
            case H5T_OPAQUE:
                type_name = "OPAQUE";
                break;
            case H5T_COMPOUND:
                type_name = "COMPOUND";
                break;
            case H5T_REFERENCE:
                type_name = "REFERENCE";
                break;
            case H5T_ENUM:
                type_name = "ENUM";
                break;
            case H5T_VLEN:
                type_name = "VLEN";
                break;
            case H5T_ARRAY:
                type_name = "ARRAY";
                break;
            default:
                break;
        }

        printf("  Datatype: %s (%zu bytes)\n", type_name, type_size);
        H5Tclose(type_id);
    }

    /* Read and display sample data */
    if (ndims > 0) {
        size_t total_elements = 1;
        for (int i = 0; i < ndims; i++) {
            total_elements *= dims[i];
        }

        size_t sample_size = (total_elements < 10) ? total_elements : 10;
        unsigned char *data = (unsigned char *) malloc(sample_size);

        if (data) {
            /* Create memory space for sample */
            hsize_t sample_dims[1] = {sample_size};
            hid_t mem_space = H5Screate_simple(1, sample_dims, NULL);

            /* Select hyperslab for first elements */
            hsize_t start[3] = {0, 0, 0};
            hsize_t count[3] = {1, 1, 1};
            if (ndims == 2) {
                count[0] = 1;
                count[1] = sample_size < dims[1] ? sample_size : dims[1];
            } else if (ndims == 3) {
                count[0] = 1;
                count[1] = sample_size < dims[1] ? sample_size : dims[1];
                count[2] = 1;
            }

            hid_t file_space_sel = H5Scopy(space_id);
            if (H5Sselect_hyperslab(file_space_sel, H5S_SELECT_SET, start, NULL, count, NULL) >=
                0) {
                if (H5Dread(dset_id, H5T_NATIVE_UCHAR, mem_space, file_space_sel, H5P_DEFAULT,
                            data) >= 0) {
                    printf("  Sample data (first %zu values): [", sample_size);
                    for (size_t i = 0; i < sample_size; i++) {
                        printf("%u%s", data[i], (i < sample_size - 1) ? ", " : "");
                    }
                    printf("]\n");
                }
            }

            H5Sclose(mem_space);
            H5Sclose(file_space_sel);
            free(data);
        }
    }

    /* Clean up */
    H5Sclose(space_id);
    H5Dclose(dset_id);
    H5Fclose(file_id);
    H5Pclose(fapl_id);
    H5VLunregister_connector(vol_id);

    return 0;
}
