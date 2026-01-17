/*
 * Test program for GeoTIFF VOL connector
 * Tests reading a GeoTIFF file through HDF5 interface
 */

#include "test_runner.h"
#include "geotiff_vol_connector.h"
#include <geotiffio.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <xtiffio.h>

/* Helper functions for generating test files */
static void SetUpGrayscaleTIFF(TIFF *tif);
static void SetUpGeoKeys(GTIF *gtif);
static int CreateGrayscaleGeoTIFF(const char *filename);
static int CreateRGBGeoTIFF(const char *filename);

/* Test functions */
int main(int argc, char **argv)
{
    int num_failures = 0;
    const char *test_name = (argc > 1) ? argv[1] : "all";
    int run_all = (strcmp(test_name, "all") == 0);

    /* Handle real_file_test with filename parameter */
    if (strcmp(test_name, "real_file_test") == 0) {
        if (argc < 3) {
            printf("Error: real_file_test requires a filename argument\n");
            printf("Usage: %s real_file_test <filename>\n", argv[0]);
            return 1;
        }
        const char *filename = argv[2];
        printf("Running real file test on: %s\n\n", filename);
        return RealFileComprehensiveTest(filename) != 0 ? 1 : 0;
    }

    /* Generate test files first (always needed) */
    printf("Generating test GeoTIFF files...\n");
    if (CreateGrayscaleGeoTIFF(GRAYSCALE_FILENAME) != 0) {
        printf("Failed to generate grayscale test file\n");
        goto error;
    }
    if (CreateRGBGeoTIFF(RGB_FILENAME) != 0) {
        printf("Failed to generate RGB test file\n");
        goto error;
    }
    printf("Test files generated successfully\n\n");

    /* Run plugin-handling tests */
    if (run_all || strcmp(test_name, "test_getters") == 0)
        num_failures += (test_getters() != 0 ? 1 : 0);

    if (run_all || strcmp(test_name, "test_multiple_registration") == 0)
        num_failures += (test_multiple_registration() != 0 ? 1 : 0);

    if (run_all || strcmp(test_name, "test_registration_by_name") == 0)
        num_failures += (test_registration_by_name() != 0 ? 1 : 0);

    if (run_all || strcmp(test_name, "test_registration_by_value") == 0)
        num_failures += (test_registration_by_value() != 0 ? 1 : 0);

    /* Run GeoTIFF functionality tests (each test manages its own connector registration) */
    if (run_all || strcmp(test_name, "open_grayscale") == 0)
        num_failures += (OpenGeoTIFFTest(GRAYSCALE_FILENAME) != 0 ? 1 : 0);

    if (run_all || strcmp(test_name, "open_rgb") == 0)
        num_failures += (OpenGeoTIFFTest(RGB_FILENAME) != 0 ? 1 : 0);

    if (run_all || strcmp(test_name, "read_grayscale") == 0)
        num_failures += (ReadGeoTIFFTest(GRAYSCALE_FILENAME) != 0 ? 1 : 0);

    if (run_all || strcmp(test_name, "read_rgb") == 0)
        num_failures += (ReadGeoTIFFTest(RGB_FILENAME) != 0 ? 1 : 0);

    if (run_all || strcmp(test_name, "band_read_rgb") == 0)
        num_failures += (BandReadGeoTIFFTest(RGB_FILENAME) != 0 ? 1 : 0);

    if (run_all || strcmp(test_name, "point_read_grayscale") == 0)
        num_failures += (PointReadGeoTIFFTest(GRAYSCALE_FILENAME) != 0 ? 1 : 0);

    if (run_all || strcmp(test_name, "point_read_rgb") == 0)
        num_failures += (PointReadGeoTIFFTest(RGB_FILENAME) != 0 ? 1 : 0);

    if (run_all || strcmp(test_name, "multi_image_read") == 0)
        num_failures += (MultiImageReadGeoTIFFTest() != 0 ? 1 : 0);

    if (run_all || strcmp(test_name, "dataset_error_handling") == 0)
        num_failures += (DatasetErrorHandlingTest(GRAYSCALE_FILENAME) != 0 ? 1 : 0);

    /* Datatype conversion tests */
    hid_t test_types[] = {H5T_NATIVE_UCHAR, H5T_NATIVE_USHORT, H5T_NATIVE_UINT, H5T_NATIVE_UINT64,
                          H5T_NATIVE_CHAR,  H5T_NATIVE_SHORT,  H5T_NATIVE_INT,  H5T_NATIVE_INT64,
                          H5T_NATIVE_FLOAT, H5T_NATIVE_DOUBLE};

    const char *type_names[] = {"UCHAR", "USHORT", "UINT",  "UINT64", "CHAR",
                                "SHORT", "INT",    "INT64", "FLOAT",  "DOUBLE"};

    int num_types = sizeof(test_types) / sizeof(test_types[0]);

    for (int i = 0; i < num_types; i++) {
        for (int j = 0; j < num_types; j++) {
            /* Skip same-type "conversions" */
            if (i == j)
                continue;

            /* Build test name: dtype_conv_MEMTYPE_FILETYPE */
            char dtype_test_name[128];
            snprintf(dtype_test_name, sizeof(dtype_test_name), "dtype_conv_%s_%s", type_names[i],
                     type_names[j]);

            if (run_all || strcmp(test_name, dtype_test_name) == 0) {
                num_failures += (DatatypeConversionTest(test_types[i], test_types[j], type_names[i],
                                                        type_names[j]) != 0
                                     ? 1
                                     : 0);
            }
        }
    }
    if (run_all || strcmp(test_name, "link_exists_rgb") == 0)
        num_failures += (LinkExistsTest(RGB_FILENAME) != 0 ? 1 : 0);

    if (run_all || strcmp(test_name, "link_iterate_rgb") == 0)
        num_failures += (LinkIterateTest(RGB_FILENAME) != 0 ? 1 : 0);

    if (run_all || strcmp(test_name, "unsupported_features") == 0)
        num_failures += (UnsupportedFeaturesTest() != 0 ? 1 : 0);

    if (run_all || strcmp(test_name, "group_get_info") == 0)
        num_failures += (GroupGetInfoTest() != 0 ? 1 : 0);

    /* Tiled TIFF tests */
    if (run_all || strcmp(test_name, "tiled_read_grayscale") == 0)
        num_failures += (TiledTIFFReadTest("test_tiled_grayscale.tif", 0) != 0 ? 1 : 0);

    if (run_all || strcmp(test_name, "tiled_read_rgb") == 0)
        num_failures += (TiledTIFFReadTest("test_tiled_rgb.tif", 1) != 0 ? 1 : 0);

    /* Coordinates attribute tests */
    if (run_all || strcmp(test_name, "coordinates_attr_geographic") == 0)
        num_failures += (CoordinatesAttributeGeographicTest(NULL) != 0 ? 1 : 0);

    if (run_all || strcmp(test_name, "coordinates_attr_projected") == 0)
        num_failures += (CoordinatesAttributeProjectedTest(NULL) != 0 ? 1 : 0);

    /* Reference counting tests */
    if (run_all || strcmp(test_name, "refcount_close_file_before_dataset") == 0)
        num_failures += (RefCountCloseFileBeforeDatasetTest(GRAYSCALE_FILENAME) != 0 ? 1 : 0);

    if (run_all || strcmp(test_name, "refcount_close_dataset_before_attribute") == 0)
        num_failures += (RefCountCloseDatasetBeforeAttributeTest() != 0 ? 1 : 0);

    if (run_all || strcmp(test_name, "refcount_close_file_with_multiple_children") == 0)
        num_failures +=
            (RefCountCloseFileWithMultipleChildrenTest(GRAYSCALE_FILENAME) != 0 ? 1 : 0);

    if (run_all || strcmp(test_name, "num_images_attribute") == 0)
        num_failures += (NumImagesAttributeTest() != 0 ? 1 : 0);

    if (num_failures == 0) {
        printf("\n%s: All tests completed successfully\n", test_name);
    } else {
        printf("\n%s: %d test(s) failed\n", test_name, num_failures);
    }

    return (num_failures == 0 ? 0 : 1);
error:
    printf("Error occurred during the testing process, aborting\n");
    return 1;
}

/* Helper function implementations */

static int CreateGrayscaleGeoTIFF(const char *filename)
{
    TIFF *tif = NULL;
    GTIF *gtif = NULL;
    unsigned char buffer[WIDTH];

    if ((tif = XTIFFOpen(filename, "w")) == NULL) {
        printf("Failed to create %s\n", filename);
        return -1;
    }

    if ((gtif = GTIFNew(tif)) == NULL) {
        printf("Failed to create GeoTIFF handle for %s\n", filename);
        TIFFClose(tif);
        return -1;
    }

    SetUpGrayscaleTIFF(tif);
    SetUpGeoKeys(gtif);

    /* Create a gradient pattern: value = (row * 8 + col / 4) % 256 */
    for (uint32_t row = 0; row < HEIGHT; row++) {
        for (uint32_t col = 0; col < WIDTH; col++) {
            buffer[col] = (unsigned char) ((row * 8 + col / 4) % 256);
        }
        if (!TIFFWriteScanline(tif, buffer, row, 0)) {
            TIFFError("WriteGrayscaleImage", "Failed to write scanline %d\n", row);
        }
    }

    GTIFWriteKeys(gtif);
    GTIFFree(gtif);
    XTIFFClose(tif);

    return 0;
}

static int CreateRGBGeoTIFF(const char *filename)
{
    unsigned char buffer[WIDTH * 3]; /* RGB: 3 bytes per pixel */
    TIFF *tif = NULL;
    GTIF *gtif = NULL;

    if ((tif = XTIFFOpen(filename, "w")) == NULL) {
        printf("Failed to create %s\n", filename);
        return -1;
    }

    if ((gtif = GTIFNew(tif)) == NULL) {
        printf("Failed to create GeoTIFF handle for %s\n", filename);
        TIFFClose(tif);
        return -1;
    }

    TIFFSetField(tif, TIFFTAG_IMAGEWIDTH, WIDTH);
    TIFFSetField(tif, TIFFTAG_IMAGELENGTH, HEIGHT);
    TIFFSetField(tif, TIFFTAG_COMPRESSION, COMPRESSION_NONE);
    TIFFSetField(tif, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_RGB);
    TIFFSetField(tif, TIFFTAG_PLANARCONFIG, PLANARCONFIG_CONTIG);
    TIFFSetField(tif, TIFFTAG_BITSPERSAMPLE, 8);
    TIFFSetField(tif, TIFFTAG_SAMPLESPERPIXEL, 3);
    TIFFSetField(tif, TIFFTAG_ROWSPERSTRIP, HEIGHT);

    /* Tie points: pixel (0,0,0) -> geographic (200.0, 60.0, 0.0) */
    const double tiepoints[6] = {0, 0, 0, 200.0, 60.0, 0.0};
    /* Pixel scale: 2.0 units per pixel in X and Y */
    const double pixscale[3] = {2.0, 2.0, 0.0};

    TIFFSetField(tif, TIFFTAG_GEOTIEPOINTS, 6, tiepoints);
    TIFFSetField(tif, TIFFTAG_GEOPIXELSCALE, 3, pixscale);

    SetUpGeoKeys(gtif);

    /* Write to RGB image.
     * Create a color pattern:
     * R = row * 8
     * G = col * 8
     * B = (row + col) * 4
     */
    for (uint32_t row = 0; row < HEIGHT; row++) {
        for (uint32_t col = 0; col < WIDTH; col++) {
            uint32_t idx = col * 3;
            buffer[idx + 0] = (unsigned char) ((row * 8) % 256);         /* Red */
            buffer[idx + 1] = (unsigned char) ((col * 8) % 256);         /* Green */
            buffer[idx + 2] = (unsigned char) (((row + col) * 4) % 256); /* Blue */
        }
        if (!TIFFWriteScanline(tif, buffer, row, 0)) {
            TIFFError("WriteRGBImage", "Failed to write scanline %d\n", row);
        }
    }

    GTIFWriteKeys(gtif);
    GTIFFree(gtif);
    XTIFFClose(tif);

    return 0;
}

static void SetUpGrayscaleTIFF(TIFF *tif)
{
    TIFFSetField(tif, TIFFTAG_IMAGEWIDTH, WIDTH);
    TIFFSetField(tif, TIFFTAG_IMAGELENGTH, HEIGHT);
    TIFFSetField(tif, TIFFTAG_COMPRESSION, COMPRESSION_NONE);
    TIFFSetField(tif, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_MINISBLACK);
    TIFFSetField(tif, TIFFTAG_PLANARCONFIG, PLANARCONFIG_CONTIG);
    TIFFSetField(tif, TIFFTAG_BITSPERSAMPLE, 8);
    TIFFSetField(tif, TIFFTAG_SAMPLESPERPIXEL, 1);
    TIFFSetField(tif, TIFFTAG_ROWSPERSTRIP, HEIGHT);

    /* Tie points: pixel (0,0,0) -> geographic (100.0, 50.0, 0.0) */
    const double tiepoints[6] = {0, 0, 0, 100.0, 50.0, 0.0};
    /* Pixel scale: 1.0 units per pixel in X and Y */
    const double pixscale[3] = {1.0, 1.0, 0.0};

    TIFFSetField(tif, TIFFTAG_GEOTIEPOINTS, 6, tiepoints);
    TIFFSetField(tif, TIFFTAG_GEOPIXELSCALE, 3, pixscale);
}

static void SetUpGeoKeys(GTIF *gtif)
{
    GTIFKeySet(gtif, GTModelTypeGeoKey, TYPE_SHORT, 1, ModelGeographic);
    GTIFKeySet(gtif, GTRasterTypeGeoKey, TYPE_SHORT, 1, RasterPixelIsArea);
    GTIFKeySet(gtif, GTCitationGeoKey, TYPE_ASCII, 0, "Test GeoTIFF");
    GTIFKeySet(gtif, GeographicTypeGeoKey, TYPE_SHORT, 1, GCS_WGS_84);
}

/* Generate a tiled TIFF with a predictable pattern for testing */
int generate_tiled_tiff(const char *filename, int is_rgb, uint32_t width, uint32_t height,
                        uint32_t tile_width, uint32_t tile_height)
{
    TIFF *tif = NULL;
    GTIF *gtif = NULL;
    unsigned char *tile_buf = NULL;
    uint32_t samples_per_pixel = is_rgb ? 3 : 1;
    uint32_t tile_size = tile_width * tile_height * samples_per_pixel;
    int ret = 0;

    /* Open TIFF for writing */
    if ((tif = XTIFFOpen(filename, "w")) == NULL) {
        printf("Failed to create tiled TIFF %s\n", filename);
        return -1;
    }

    /* Create GeoTIFF handle */
    if ((gtif = GTIFNew(tif)) == NULL) {
        printf("Failed to create GeoTIFF handle for %s\n", filename);
        XTIFFClose(tif);
        return -1;
    }

    /* Set TIFF tags */
    TIFFSetField(tif, TIFFTAG_IMAGEWIDTH, width);
    TIFFSetField(tif, TIFFTAG_IMAGELENGTH, height);
    TIFFSetField(tif, TIFFTAG_COMPRESSION, COMPRESSION_NONE);
    TIFFSetField(tif, TIFFTAG_PHOTOMETRIC, is_rgb ? PHOTOMETRIC_RGB : PHOTOMETRIC_MINISBLACK);
    TIFFSetField(tif, TIFFTAG_PLANARCONFIG, PLANARCONFIG_CONTIG);
    TIFFSetField(tif, TIFFTAG_BITSPERSAMPLE, 8);
    TIFFSetField(tif, TIFFTAG_SAMPLESPERPIXEL, samples_per_pixel);
    TIFFSetField(tif, TIFFTAG_TILEWIDTH, tile_width);
    TIFFSetField(tif, TIFFTAG_TILELENGTH, tile_height);

    /* Set GeoTIFF metadata */
    const double tiepoints[6] = {0, 0, 0, 100.0, 50.0, 0.0};
    const double pixscale[3] = {1.0, 1.0, 0.0};
    TIFFSetField(tif, TIFFTAG_GEOTIEPOINTS, 6, tiepoints);
    TIFFSetField(tif, TIFFTAG_GEOPIXELSCALE, 3, pixscale);
    SetUpGeoKeys(gtif);

    /* Allocate tile buffer */
    tile_buf = (unsigned char *) malloc(tile_size);
    if (!tile_buf) {
        printf("Failed to allocate tile buffer\n");
        ret = -1;
        goto cleanup;
    }

    /* Write tiles with predictable pattern */
    for (uint32_t row = 0; row < height; row += tile_height) {
        for (uint32_t col = 0; col < width; col += tile_width) {
            /* Fill tile with pattern data */
            uint32_t tile_h = (row + tile_height > height) ? height - row : tile_height;
            uint32_t tile_w = (col + tile_width > width) ? width - col : tile_width;

            for (uint32_t ty = 0; ty < tile_h; ty++) {
                for (uint32_t tx = 0; tx < tile_w; tx++) {
                    uint32_t global_row = row + ty;
                    uint32_t global_col = col + tx;
                    uint32_t idx = (ty * tile_width + tx) * samples_per_pixel;

                    if (is_rgb) {
                        /* RGB pattern: R=row*8, G=col*8, B=(row+col)*4 */
                        tile_buf[idx + 0] = (unsigned char) ((global_row * 8) % 256);
                        tile_buf[idx + 1] = (unsigned char) ((global_col * 8) % 256);
                        tile_buf[idx + 2] = (unsigned char) (((global_row + global_col) * 4) % 256);
                    } else {
                        /* Grayscale pattern: value = (row * 8 + col / 4) % 256 */
                        tile_buf[idx] = (unsigned char) ((global_row * 8 + global_col / 4) % 256);
                    }
                }
            }

            /* Write the tile */
            if (TIFFWriteTile(tif, tile_buf, col, row, 0, 0) < 0) {
                printf("Failed to write tile at row=%u, col=%u\n", row, col);
                ret = -1;
                goto cleanup;
            }
        }
    }

cleanup:
    if (tile_buf)
        free(tile_buf);
    if (gtif) {
        GTIFWriteKeys(gtif);
        GTIFFree(gtif);
    }
    if (tif)
        XTIFFClose(tif);

    return ret;
}
