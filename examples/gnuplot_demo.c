/*
 * GeoTIFF VOL Connector gnuplot Visualization Demo
 *
 * This program demonstrates:
 * 1. Creating a GeoTIFF file with RGB concentric rings
 * 2. Reading the file via the HDF5 VOL connector
 * 3. Visualizing the data using gnuplot
 *
 * Must be run from the build directory
 */

/* Windows compatibility */
#ifdef _WIN32
#define _USE_MATH_DEFINES
#include <windows.h>
#include <direct.h>
#include <io.h>
#ifndef PATH_MAX
#define PATH_MAX MAX_PATH
#endif
#define getcwd _getcwd
#define chmod(path, mode) _chmod(path, _S_IREAD | _S_IWRITE)
#define setenv(name, value, overwrite) _putenv_s(name, value)
#define S_ISDIR(m) (((m) & _S_IFMT) == _S_IFDIR)
#endif

#include <hdf5.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#ifndef _WIN32
#include <unistd.h>
#endif

#include <geotiffio.h>
#include <xtiffio.h>

#define GEOTIFF_VOL_CONNECTOR_NAME "geotiff_vol_connector"

#define IMAGE_SIZE 256
#define OUTPUT_FILENAME "demo_rings_gnuplot.tif"
#define GNUPLOT_RGB_DATA "gnuplot_rgb.dat"
#define GNUPLOT_GRAY_DATA "gnuplot_gray.dat"
#define GNUPLOT_SCRIPT "gnuplot_commands.gp"

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
    const double tiepoints[6] = {0, 0, 0, -120.0, 40.0, 0.0};
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
            float dx = (float)col - center_x;
            float dy = (float)row - center_y;
            float distance = sqrtf(dx * dx + dy * dy);
            float normalized_dist = distance / max_radius;

            /* Create rainbow rings */
            float hue = fmodf(normalized_dist * 720.0f, 360.0f);
            float saturation = 0.9f;
            float value = 0.8f + 0.2f * sinf(normalized_dist * 12.0f * (float)M_PI);

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

    free(scanline);
    GTIFWriteKeys(gtif);
    GTIFFree(gtif);
    XTIFFClose(tif);

    printf("Successfully created %s\n", OUTPUT_FILENAME);
    return 0;
}

int write_gnuplot_rgb_data(unsigned char *data, hsize_t height, hsize_t width)
{
    FILE *fp = fopen(GNUPLOT_RGB_DATA, "w");
    if (!fp) {
        fprintf(stderr, "Failed to create %s\n", GNUPLOT_RGB_DATA);
        return 1;
    }

    printf("Writing RGB data to %s...\n", GNUPLOT_RGB_DATA);

    /* Write data in gnuplot binary matrix format */
    /* Format: row col red green blue */
    for (hsize_t row = 0; row < height; row++) {
        for (hsize_t col = 0; col < width; col++) {
            hsize_t idx = (row * width + col) * 3;
            unsigned char r = data[idx + 0];
            unsigned char g = data[idx + 1];
            unsigned char b = data[idx + 2];

            fprintf(fp, "%zu %zu %u %u %u\n", col, row, r, g, b);
        }
        fprintf(fp, "\n"); /* Blank line between rows for gnuplot */
    }

    fclose(fp);
    printf("Successfully wrote RGB data\n");
    return 0;
}

int write_gnuplot_grayscale_data(unsigned char *data, hsize_t height, hsize_t width)
{
    FILE *fp = fopen(GNUPLOT_GRAY_DATA, "w");
    if (!fp) {
        fprintf(stderr, "Failed to create %s\n", GNUPLOT_GRAY_DATA);
        return 1;
    }

    printf("Writing grayscale data (red channel) to %s...\n", GNUPLOT_GRAY_DATA);

    /* Write just the red channel as grayscale */
    for (hsize_t row = 0; row < height; row++) {
        for (hsize_t col = 0; col < width; col++) {
            hsize_t idx = (row * width + col) * 3;
            unsigned char intensity = data[idx + 0]; /* Red channel */

            fprintf(fp, "%zu %zu %u\n", col, row, intensity);
        }
        fprintf(fp, "\n");
    }

    fclose(fp);
    printf("Successfully wrote grayscale data\n");
    return 0;
}

int create_gnuplot_script(hsize_t height, hsize_t width)
{
    FILE *fp = fopen(GNUPLOT_SCRIPT, "w");
    if (!fp) {
        fprintf(stderr, "Failed to create %s\n", GNUPLOT_SCRIPT);
        return 1;
    }

    fprintf(fp, "#!/usr/bin/gnuplot\n");
    fprintf(fp, "# GeoTIFF VOL Connector Visualization\n\n");

    fprintf(fp, "set terminal png truecolor size %zu,%zu\n", width * 3, height * 3);
    fprintf(fp, "set output 'gnuplot_visualization.png'\n\n");

    /* Plot RGB color image */
    fprintf(fp, "# RGB Color Image\n");
    fprintf(fp, "set title 'GeoTIFF accessed via HDF5 VOL Connector' font ',14'\n");
    fprintf(fp, "set xlabel 'X (pixels)'\n");
    fprintf(fp, "set ylabel 'Y (pixels)'\n");
    fprintf(fp, "set size ratio -1\n");
    fprintf(fp, "set xrange [0:%zu]\n", width - 1);
    fprintf(fp, "set yrange [%zu:0]\n", height - 1);
    fprintf(fp, "unset colorbox\n");
    fprintf(fp, "unset border\n");
    fprintf(fp, "unset xtics\n");
    fprintf(fp, "unset ytics\n");
    fprintf(fp, "unset grid\n");
    fprintf(fp, "set lmargin at screen 0.05\n");
    fprintf(fp, "set rmargin at screen 0.95\n");
    fprintf(fp, "set bmargin at screen 0.05\n");
    fprintf(fp, "set tmargin at screen 0.90\n");
    fprintf(fp, "plot '%s' using 1:2:3:4:5 with rgbimage pixels notitle\n", GNUPLOT_RGB_DATA);

    fclose(fp);

    /* Make script executable */
    chmod(GNUPLOT_SCRIPT, 0755);

    printf("Successfully created gnuplot script: %s\n", GNUPLOT_SCRIPT);
    return 0;
}

int visualize_with_gnuplot(unsigned char *data, hsize_t height, hsize_t width)
{
    printf("\n===========================================\n");
    printf("Visualizing with gnuplot\n");
    printf("===========================================\n");

    /* Write data files for gnuplot */
    if (write_gnuplot_rgb_data(data, height, width) != 0) {
        return 1;
    }

    if (write_gnuplot_grayscale_data(data, height, width) != 0) {
        return 1;
    }

    /* Create gnuplot script */
    if (create_gnuplot_script(height, width) != 0) {
        return 1;
    }

    /* Execute gnuplot */
    printf("\nRunning gnuplot...\n");
    int ret = system("gnuplot " GNUPLOT_SCRIPT);
    if (ret != 0) {
        fprintf(stderr, "WARNING: gnuplot command failed (return code: %d)\n", ret);
        fprintf(stderr, "Make sure gnuplot is installed: sudo apt-get install gnuplot\n");
        return 1;
    }

    printf("Successfully created visualization: gnuplot_visualization.png\n");

    /* Try to open the image viewer */
    printf("\nAttempting to display image...\n");
    ret = system("xdg-open gnuplot_visualization.png 2>/dev/null || "
                 "eog gnuplot_visualization.png 2>/dev/null || "
                 "display gnuplot_visualization.png 2>/dev/null || "
                 "echo 'Please open gnuplot_visualization.png manually'");

    return 0;
}

int main(int argc, char *argv[])
{
    hid_t vol_id, fapl_id, file_id;
    hid_t dset_id, space_id;
    hsize_t dims[3];
    int ndims;
    struct stat st;
    char plugin_path[PATH_MAX];
    char cwd[PATH_MAX];
    unsigned char *data = NULL;
    const char *input_filename = NULL;
    int create_demo = 1;

    /* Parse command line arguments */
    if (argc > 2) {
        printf("Usage: %s [tiff_file]\n", argv[0]);
        printf("  If no file specified, creates and visualizes demo concentric rings\n");
        printf("  If file specified, visualizes that GeoTIFF file\n");
        return 1;
    }

    if (argc == 2) {
        input_filename = argv[1];
        create_demo = 0;

        /* Check if file exists */
        if (stat(input_filename, &st) != 0) {
            printf("ERROR: File not found: %s\n", input_filename);
            return 1;
        }
    } else {
        input_filename = OUTPUT_FILENAME;
        create_demo = 1;
    }

    /* Check if being run from build directory */
    if (stat("./src", &st) != 0 || !S_ISDIR(st.st_mode)) {
        printf("ERROR: This program must be run from the build directory\n");
        printf("Usage: cd build && ./examples/gnuplot_demo [tiff_file]\n");
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
    printf("GeoTIFF VOL Connector gnuplot Demo\n");
    printf("===========================================\n\n");

    /* Step 1: Create demo GeoTIFF if needed */
    if (create_demo) {
        printf("[1/3] Creating demo GeoTIFF file\n");
        if (create_demo_image() != 0) {
            printf("ERROR: Failed to create demo GeoTIFF file\n");
            return 1;
        }
        printf("\n");
    } else {
        printf("Input file: %s\n\n", input_filename);
    }

    /* Step 2: Read via VOL connector */
    printf("[%s] Reading GeoTIFF via HDF5 VOL connector\n", create_demo ? "2/3" : "1/2");

    /* Register the GeoTIFF VOL connector */
    vol_id = H5VLregister_connector_by_name(GEOTIFF_VOL_CONNECTOR_NAME, H5P_DEFAULT);
    if (vol_id < 0) {
        printf("ERROR: Failed to register GeoTIFF VOL connector\n");
        printf("Make sure the VOL connector library is built in %s\n", plugin_path);
        return 1;
    }
    printf("Registered GeoTIFF VOL connector\n");

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
    file_id = H5Fopen(input_filename, H5F_ACC_RDONLY, fapl_id);
    if (file_id < 0) {
        printf("ERROR: Failed to open GeoTIFF file: %s\n", input_filename);
        H5Pclose(fapl_id);
        H5VLunregister_connector(vol_id);
        return 1;
    }
    printf("Opened GeoTIFF file via VOL connector: %s\n", input_filename);

    /* Open the image dataset */
    dset_id = H5Dopen2(file_id, "/image0", H5P_DEFAULT);
    if (dset_id < 0) {
        printf("ERROR: Failed to open /image0 dataset\n");
        H5Fclose(file_id);
        H5Pclose(fapl_id);
        H5VLunregister_connector(vol_id);
        return 1;
    }

    /* Get dataspace and dimensions */
    space_id = H5Dget_space(dset_id);
    if (space_id < 0) {
        printf("ERROR: Failed to get dataspace\n");
        H5Dclose(dset_id);
        H5Fclose(file_id);
        H5Pclose(fapl_id);
        H5VLunregister_connector(vol_id);
        return 1;
    }

    ndims = H5Sget_simple_extent_ndims(space_id);
    if (ndims != 3) {
        printf("ERROR: Expected 3D dataset (height, width, channels), got %dD\n", ndims);
        H5Sclose(space_id);
        H5Dclose(dset_id);
        H5Fclose(file_id);
        H5Pclose(fapl_id);
        H5VLunregister_connector(vol_id);
        return 1;
    }

    H5Sget_simple_extent_dims(space_id, dims, NULL);
    printf("Dataset dimensions: [%lu, %lu, %lu] (height, width, channels)\n",
           (unsigned long) dims[0], (unsigned long) dims[1], (unsigned long) dims[2]);

    /* Allocate memory for the entire dataset */
    size_t total_bytes = dims[0] * dims[1] * dims[2];
    data = (unsigned char *) malloc(total_bytes);
    if (!data) {
        printf("ERROR: Failed to allocate %zu bytes for image data\n", total_bytes);
        H5Sclose(space_id);
        H5Dclose(dset_id);
        H5Fclose(file_id);
        H5Pclose(fapl_id);
        H5VLunregister_connector(vol_id);
        return 1;
    }

    /* Read the entire dataset */
    if (H5Dread(dset_id, H5T_NATIVE_UCHAR, H5S_ALL, H5S_ALL, H5P_DEFAULT, data) < 0) {
        printf("ERROR: Failed to read dataset\n");
        free(data);
        H5Sclose(space_id);
        H5Dclose(dset_id);
        H5Fclose(file_id);
        H5Pclose(fapl_id);
        H5VLunregister_connector(vol_id);
        return 1;
    }

    printf("Successfully read %zu bytes from dataset\n", total_bytes);
    printf("\n");

    /* Step 3: Visualize with gnuplot */
    printf("[%s] Visualizing data\n", create_demo ? "3/3" : "2/2");
    int viz_ret = visualize_with_gnuplot(data, dims[0], dims[1]);

    /* Clean up */
    free(data);
    H5Sclose(space_id);
    H5Dclose(dset_id);
    H5Fclose(file_id);
    H5Pclose(fapl_id);
    H5VLunregister_connector(vol_id);

    if (viz_ret == 0) {
        printf("\n===========================================\n");
        printf("Demo completed successfully!\n");
        printf("===========================================\n");
        printf("\nGenerated files:\n");
        if (create_demo) {
            printf("  - %s (GeoTIFF source file)\n", OUTPUT_FILENAME);
        }
        printf("  - %s (RGB data for gnuplot)\n", GNUPLOT_RGB_DATA);
        printf("  - %s (Grayscale data for gnuplot)\n", GNUPLOT_GRAY_DATA);
        printf("  - %s (gnuplot script)\n", GNUPLOT_SCRIPT);
        printf("  - gnuplot_visualization.png (final visualization)\n");
    }

    return viz_ret;
}
