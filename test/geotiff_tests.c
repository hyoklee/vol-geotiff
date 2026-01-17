/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * Copyright by The HDF Group.                                               *
 * All rights reserved.                                                      *
 *                                                                           *
 * This file is part of HDF5.  The full HDF5 copyright notice, including     *
 * terms governing use, modification, and redistribution, is contained in    *
 * the COPYING file, which can be found at the root of the source code       *
 * distribution tree, or in https://support.hdfgroup.org/ftp/HDF5/releases.  *
 * If you do not have access to either file, you may request a copy from     *
 * help@hdfgroup.org.                                                        *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

/*
 * Purpose:     This file defines tests which evluate basic GeoTIFF file
 *              functionality through the GeoTIFF VOL connector.
 */

#include "geotiff_vol_connector.h"
#include "test_runner.h"
#include <H5PLpublic.h>
#include <geo_normalize.h>
#include <geotiffio.h>
#include <xtiffio.h>

#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* Helper function to set up GeoTIFF keys */
static void SetUpGeoKeys(GTIF *gtif)
{
    GTIFKeySet(gtif, GTModelTypeGeoKey, TYPE_SHORT, 1, ModelGeographic);
    GTIFKeySet(gtif, GTRasterTypeGeoKey, TYPE_SHORT, 1, RasterPixelIsArea);
    GTIFKeySet(gtif, GTCitationGeoKey, TYPE_ASCII, 0, "Test GeoTIFF");
    GTIFKeySet(gtif, GeographicTypeGeoKey, TYPE_SHORT, 1, GCS_WGS_84);
}

/* Helper function to create a grayscale GeoTIFF file */
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

    /* Set up TIFF tags */
    TIFFSetField(tif, TIFFTAG_IMAGEWIDTH, WIDTH);
    TIFFSetField(tif, TIFFTAG_IMAGELENGTH, HEIGHT);
    TIFFSetField(tif, TIFFTAG_COMPRESSION, COMPRESSION_NONE);
    TIFFSetField(tif, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_MINISBLACK);
    TIFFSetField(tif, TIFFTAG_PLANARCONFIG, PLANARCONFIG_CONTIG);
    TIFFSetField(tif, TIFFTAG_BITSPERSAMPLE, 8);
    TIFFSetField(tif, TIFFTAG_SAMPLESPERPIXEL, 1);
    TIFFSetField(tif, TIFFTAG_ROWSPERSTRIP, HEIGHT);

    /* Tie points and pixel scale */
    const double tiepoints[6] = {0, 0, 0, 100.0, 50.0, 0.0};
    const double pixscale[3] = {1.0, 1.0, 0.0};
    TIFFSetField(tif, TIFFTAG_GEOTIEPOINTS, 6, tiepoints);
    TIFFSetField(tif, TIFFTAG_GEOPIXELSCALE, 3, pixscale);

    SetUpGeoKeys(gtif);

    /* Create a gradient pattern: value = (row * 8 + col / 4) % 256 */
    for (uint32_t row = 0; row < HEIGHT; row++) {
        for (uint32_t col = 0; col < WIDTH; col++) {
            buffer[col] = (unsigned char) ((row * 8 + col / 4) % 256);
        }
        if (!TIFFWriteScanline(tif, buffer, row, 0)) {
            printf("Failed to write scanline %u\n", row);
            GTIFFree(gtif);
            TIFFClose(tif);
            return -1;
        }
    }

    GTIFWriteKeys(gtif);
    GTIFFree(gtif);
    XTIFFClose(tif);

    return 0;
}

/* Verify that GeoTIFF file open/close operations work properly */
int OpenGeoTIFFTest(const char *filename)
{
    hid_t vol_id = H5I_INVALID_HID;
    hid_t fapl_id = H5I_INVALID_HID;
    hid_t file_id = H5I_INVALID_HID;

    printf("Testing GeoTIFF VOL connector open/close with file: %s  ", filename);

    /* Add the plugin path so HDF5 can find the connector */
#ifdef GEOTIFF_VOL_PLUGIN_PATH
    if (H5PLappend(GEOTIFF_VOL_PLUGIN_PATH) < 0) {
        printf("Failed to append plugin path\n");
        goto error;
    }
#endif

    /* Register the GeoTIFF VOL connector */
    if ((vol_id = H5VLregister_connector_by_name(GEOTIFF_VOL_CONNECTOR_NAME, H5P_DEFAULT)) < 0) {
        printf("Failed to register VOL connector\n");
        goto error;
    }

    /* Create file access property list */
    if ((fapl_id = H5Pcreate(H5P_FILE_ACCESS)) < 0) {
        printf("Failed to create FAPL\n");
        goto error;
    }

    /* Set the VOL connector */
    if (H5Pset_vol(fapl_id, vol_id, NULL) < 0) {
        printf("Failed to set VOL connector\n");
        goto error;
    }

    /* Open the GeoTIFF file */
    if ((file_id = H5Fopen(filename, H5F_ACC_RDONLY, fapl_id)) < 0) {
        printf("Failed to open GeoTIFF file\n");
        goto error;
    }

    /* Close the GeoTIFF file */
    if (H5Fclose(file_id) < 0) {
        printf("Failed to close GeoTIFF file\n");
        goto error;
    }

    /* Clean up*/
    if (H5Pclose(fapl_id) < 0) {
        printf("Failed to close FAPL\n");
        goto error;
    }

    /* Unregister VOL connector */
    if (H5VLunregister_connector(vol_id) < 0) {
        printf("Failed to unregister VOL connector\n");
        goto error;
    }

    printf("PASSED\n");
    return 0;

error:
    H5E_BEGIN_TRY
    {
        H5Pclose(fapl_id);
        H5Fclose(file_id);
        if (vol_id != H5I_INVALID_HID)
            H5VLunregister_connector(vol_id);
    }
    H5E_END_TRY;
    return -1;
}

/* Verify that GeoTIFF image open/read operations work properly */
int ReadGeoTIFFTest(const char *filename)
{
    hid_t vol_id = H5I_INVALID_HID;
    hid_t fapl_id = H5I_INVALID_HID;
    hid_t file_id = H5I_INVALID_HID;
    hid_t dset_id = H5I_INVALID_HID;
    hid_t space_id = H5I_INVALID_HID;
    hid_t type_id = H5I_INVALID_HID;
    hsize_t dims[3];
    int ndims;
    unsigned char *data = NULL;
    int is_grayscale = (strcmp(filename, GRAYSCALE_FILENAME) == 0);

    printf("Testing GeoTIFF VOL connector read with file: %s  ", filename);

    /* Add the plugin path so HDF5 can find the connector */
#ifdef GEOTIFF_VOL_PLUGIN_PATH
    if (H5PLappend(GEOTIFF_VOL_PLUGIN_PATH) < 0) {
        printf("Failed to append plugin path\n");
        goto error;
    }
#endif

    /* Register the GeoTIFF VOL connector */
    if ((vol_id = H5VLregister_connector_by_name(GEOTIFF_VOL_CONNECTOR_NAME, H5P_DEFAULT)) < 0) {
        printf("Failed to register VOL connector\n");
        goto error;
    }

    /* Create file access property list */
    if ((fapl_id = H5Pcreate(H5P_FILE_ACCESS)) < 0) {
        printf("Failed to create FAPL\n");
        goto error;
    }

    /* Set the VOL connector */
    if (H5Pset_vol(fapl_id, vol_id, NULL) < 0) {
        printf("Failed to set VOL connector\n");
        goto error;
    }

    /* Open the GeoTIFF file */
    if ((file_id = H5Fopen(filename, H5F_ACC_RDONLY, fapl_id)) < 0) {
        printf("Failed to open GeoTIFF file\n");
        goto error;
    }

    /* Open the image dataset */
    if ((dset_id = H5Dopen2(file_id, "image0", H5P_DEFAULT)) < 0) {
        printf("Failed to open image dataset\n");
        goto error;
    }

    /* Get dataspace */
    if ((space_id = H5Dget_space(dset_id)) < 0) {
        printf("Failed to get dataspace\n");
        goto error;
    }

    if ((ndims = H5Sget_simple_extent_ndims(space_id)) < 0) {
        printf("Failed to get number of dimensions\n");
        goto error;
    }

    if (ndims > 0 && ndims <= 3) {
        if (H5Sget_simple_extent_dims(space_id, dims, NULL) < 0) {
            printf("Failed to get dimensions\n");
            goto error;
        }
    }

    /* Get datatype */
    if ((type_id = H5Dget_type(dset_id)) < 0) {
        printf("Failed to get datatype\n");
        goto error;
    }

    /* Verify dimensions match expected */
    if (is_grayscale) {
        if (ndims != 2 || dims[0] != HEIGHT || dims[1] != WIDTH) {
            printf("VERIFICATION FAILED: Expected dimensions 32x32, got ");
            for (int i = 0; i < ndims; i++) {
                printf("%llux", (unsigned long long) dims[i]);
            }
            printf("\n");
            goto error;
        }
    } else {
        if (ndims != 3 || dims[0] != HEIGHT || dims[1] != WIDTH || dims[2] != 3) {
            printf("VERIFICATION FAILED: Expected dimensions 32x32x3, got ");
            for (int i = 0; i < ndims; i++) {
                printf("%llux", (unsigned long long) dims[i]);
            }
            printf("\n");
            goto error;
        }
    }

    /* Allocate buffer and read data */
    size_t data_size;
    if (is_grayscale) {
        data_size = WIDTH * HEIGHT;
    } else {
        data_size = WIDTH * HEIGHT * 3;
    }

    data = (unsigned char *) malloc(data_size);
    if (!data) {
        printf("Failed to allocate read buffer\n");
        goto error;
    }

    if (H5Dread(dset_id, H5T_NATIVE_UCHAR, H5S_ALL, H5S_ALL, H5P_DEFAULT, data) < 0) {
        printf("Failed to read dataset\n");
        goto error;
    }

    /* Verify pixel data matches expected pattern */
    if (is_grayscale) {
        /* Grayscale pattern: value = (row * 8 + col / 4) % 256 */
        for (int row = 0; row < HEIGHT; row++) {
            for (int col = 0; col < WIDTH; col++) {
                unsigned char expected = (unsigned char) ((row * 8 + col / 4) % 256);
                unsigned char actual = data[row * WIDTH + col];
                if (actual != expected) {
                    printf("VERIFICATION FAILED: Pixel[%d,%d] expected %u, got %u\n", row, col,
                           expected, actual);
                    goto error;
                }
            }
        }
    } else {
        /* RGB pattern: R=row*8, G=col*8, B=(row+col)*4 */
        for (int row = 0; row < HEIGHT; row++) {
            for (int col = 0; col < WIDTH; col++) {
                int idx = (row * WIDTH + col) * 3;
                unsigned char expected_r = (unsigned char) ((row * 8) % 256);
                unsigned char expected_g = (unsigned char) ((col * 8) % 256);
                unsigned char expected_b = (unsigned char) (((row + col) * 4) % 256);
                unsigned char actual_r = data[idx + 0];
                unsigned char actual_g = data[idx + 1];
                unsigned char actual_b = data[idx + 2];

                if (actual_r != expected_r || actual_g != expected_g || actual_b != expected_b) {
                    printf("VERIFICATION FAILED: Pixel[%d,%d] expected RGB(%u,%u,%u), got "
                           "RGB(%u,%u,%u)\n",
                           row, col, expected_r, expected_g, expected_b, actual_r, actual_g,
                           actual_b);
                    goto error;
                }
            }
        }
    }

    /* Clean up */
    free(data);
    if (H5Tclose(type_id) < 0) {
        printf("Failed to close datatype\n");
        goto error;
    }
    type_id = H5I_INVALID_HID;
    if (H5Sclose(space_id) < 0) {
        printf("Failed to close dataspace\n");
        goto error;
    }
    space_id = H5I_INVALID_HID;
    if (H5Dclose(dset_id) < 0) {
        printf("Failed to close dataset\n");
        goto error;
    }
    dset_id = H5I_INVALID_HID;
    if (H5Fclose(file_id) < 0) {
        printf("Failed to close file\n");
        goto error;
    }
    file_id = H5I_INVALID_HID;
    if (H5Pclose(fapl_id) < 0) {
        printf("Failed to close FAPL\n");
        goto error;
    }
    fapl_id = H5I_INVALID_HID;

    /* Unregister VOL connector */
    if (H5VLunregister_connector(vol_id) < 0) {
        printf("Failed to unregister VOL connector\n");
        goto error;
    }

    printf("PASSED\n");
    return 0;

error:
    /* Clean up in reverse order of creation - no error checks */
    if (data)
        free(data);
    H5E_BEGIN_TRY
    {
        H5Tclose(type_id);
        H5Sclose(space_id);
        H5Dclose(dset_id);
        H5Fclose(file_id);
        H5Pclose(fapl_id);
        if (vol_id != H5I_INVALID_HID)
            H5VLunregister_connector(vol_id);
    }
    H5E_END_TRY;

    printf("FAILED\n");
    return 1;
}

/* Verify that GeoTIFF band/hyperslab reading works properly */
int BandReadGeoTIFFTest(const char *filename)
{
    hid_t vol_id = H5I_INVALID_HID;
    hid_t fapl_id = H5I_INVALID_HID;
    hid_t file_id = H5I_INVALID_HID;
    hid_t dset_id = H5I_INVALID_HID;
    hid_t space_id = H5I_INVALID_HID;
    hid_t mem_space_id = H5I_INVALID_HID;
    hid_t file_space_id = H5I_INVALID_HID;
    hsize_t dims[3], start[3], count[3];
    int ndims;
    unsigned char *band_data = NULL;
    size_t band_size;

    printf("Testing GeoTIFF VOL connector band reading with file: %s  ", filename);

    /* Add the plugin path so HDF5 can find the connector */
#ifdef GEOTIFF_VOL_PLUGIN_PATH
    if (H5PLappend(GEOTIFF_VOL_PLUGIN_PATH) < 0) {
        printf("Failed to append plugin path\n");
        goto error;
    }
#endif

    /* Register the GeoTIFF VOL connector */
    if ((vol_id = H5VLregister_connector_by_name(GEOTIFF_VOL_CONNECTOR_NAME, H5P_DEFAULT)) < 0) {
        printf("Failed to register VOL connector\n");
        goto error;
    }

    /* Create file access property list */
    if ((fapl_id = H5Pcreate(H5P_FILE_ACCESS)) < 0) {
        printf("Failed to create FAPL\n");
        goto error;
    }

    /* Set the VOL connector */
    if (H5Pset_vol(fapl_id, vol_id, NULL) < 0) {
        printf("Failed to set VOL connector\n");
        goto error;
    }

    /* Open the GeoTIFF file */
    if ((file_id = H5Fopen(filename, H5F_ACC_RDONLY, fapl_id)) < 0) {
        printf("Failed to open GeoTIFF file\n");
        goto error;
    }

    /* Open the image dataset */
    if ((dset_id = H5Dopen2(file_id, "image0", H5P_DEFAULT)) < 0) {
        printf("Failed to open image dataset\n");
        goto error;
    }

    /* Get dataspace */
    if ((space_id = H5Dget_space(dset_id)) < 0) {
        printf("Failed to get dataspace\n");
        goto error;
    }

    if ((ndims = H5Sget_simple_extent_ndims(space_id)) < 0) {
        printf("Failed to get number of dimensions\n");
        goto error;
    }

    if (ndims <= 0 || ndims > 3) {
        printf("Invalid number of dimensions: %d\n", ndims);
        goto error;
    }

    if (H5Sget_simple_extent_dims(space_id, dims, NULL) < 0) {
        printf("Failed to get dimensions\n");
        goto error;
    }

    /* Only test band reading for multi-band (RGB) images */
    if (ndims != 3 || dims[2] <= 1) {
        printf("not an RGB image\n");
        goto error;
    }

    /* Select first band (red channel): all rows, all columns, band 0 */
    start[0] = 0;
    start[1] = 0;
    start[2] = 0;
    count[0] = dims[0];
    count[1] = dims[1];
    count[2] = 1;

    if ((file_space_id = H5Scopy(space_id)) < 0) {
        printf("Failed to copy dataspace\n");
        goto error;
    }

    if (H5Sselect_hyperslab(file_space_id, H5S_SELECT_SET, start, NULL, count, NULL) < 0) {
        printf("Failed to select hyperslab\n");
        goto error;
    }

    band_size = (size_t) (dims[0] * dims[1]);
    band_data = (unsigned char *) malloc(band_size);
    if (!band_data) {
        printf("Failed to allocate memory for band data\n");
        goto error;
    }

    /* Initialize buffer with a sentinel value (0xFF) to verify only selected region is written */
    memset(band_data, 0xFF, band_size);

    hsize_t mem_dims[2] = {dims[0], dims[1]};
    if ((mem_space_id = H5Screate_simple(2, mem_dims, NULL)) < 0) {
        printf("Failed to create memory dataspace\n");
        goto error;
    }

    if (H5Dread(dset_id, H5T_NATIVE_UCHAR, mem_space_id, file_space_id, H5P_DEFAULT, band_data) <
        0) {
        printf("Failed to read band 0\n");
        goto error;
    }

    /* Verify the red channel data matches expected pattern: R = (row * 8) % 256 */
    for (hsize_t row = 0; row < dims[0]; row++) {
        for (hsize_t col = 0; col < dims[1]; col++) {
            unsigned char expected = (unsigned char) ((row * 8) % 256);
            unsigned char actual = band_data[row * dims[1] + col];
            if (actual != expected) {
                printf("VERIFICATION FAILED: Band 0 pixel[%llu,%llu] expected %u, got %u\n",
                       (unsigned long long) row, (unsigned long long) col, expected, actual);
                goto error;
            }
        }
    }

    /* Clean up */
    if (band_data)
        free(band_data);
    if (mem_space_id != H5I_INVALID_HID && H5Sclose(mem_space_id) < 0) {
        printf("Failed to close memory dataspace\n");
        goto error;
    }
    if (file_space_id != H5I_INVALID_HID && H5Sclose(file_space_id) < 0) {
        printf("Failed to close file dataspace\n");
        goto error;
    }
    if (H5Sclose(space_id) < 0) {
        printf("Failed to close dataspace\n");
        goto error;
    }
    if (H5Dclose(dset_id) < 0) {
        printf("Failed to close dataset\n");
        goto error;
    }
    if (H5Fclose(file_id) < 0) {
        printf("Failed to close file\n");
        goto error;
    }
    if (H5Pclose(fapl_id) < 0) {
        printf("Failed to close FAPL\n");
        goto error;
    }
    if (H5VLunregister_connector(vol_id) < 0) {
        printf("Failed to unregister VOL connector\n");
        goto error;
    }

    printf("PASSED\n");
    return 0;

error:
    if (band_data)
        free(band_data);
    H5E_BEGIN_TRY
    {
        H5Sclose(mem_space_id);
        H5Sclose(file_space_id);
        H5Sclose(space_id);
        H5Dclose(dset_id);
        H5Fclose(file_id);
        H5Pclose(fapl_id);
        if (vol_id != H5I_INVALID_HID)
            H5VLunregister_connector(vol_id);
    }
    H5E_END_TRY;

    printf("FAILED\n");
    return 1;
}

/* Helper function to create a GeoTIFF file with a specific datatype */
static int CreateTypedGeoTIFF(const char *filename, uint16_t sample_format,
                              uint16_t bits_per_sample)
{
    TIFF *tif = NULL;
    GTIF *gtif = NULL;
    void *buffer = NULL;
    size_t element_size = bits_per_sample / 8;
    size_t row_size = WIDTH * element_size;

    if ((tif = XTIFFOpen(filename, "w")) == NULL) {
        printf("Failed to create %s\n", filename);
        return -1;
    }

    if ((gtif = GTIFNew(tif)) == NULL) {
        printf("Failed to create GeoTIFF handle for %s\n", filename);
        TIFFClose(tif);
        return -1;
    }

    /* Set up TIFF tags */
    TIFFSetField(tif, TIFFTAG_IMAGEWIDTH, WIDTH);
    TIFFSetField(tif, TIFFTAG_IMAGELENGTH, HEIGHT);
    TIFFSetField(tif, TIFFTAG_COMPRESSION, COMPRESSION_NONE);
    TIFFSetField(tif, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_MINISBLACK);
    TIFFSetField(tif, TIFFTAG_PLANARCONFIG, PLANARCONFIG_CONTIG);
    TIFFSetField(tif, TIFFTAG_BITSPERSAMPLE, bits_per_sample);
    TIFFSetField(tif, TIFFTAG_SAMPLESPERPIXEL, 1);
    TIFFSetField(tif, TIFFTAG_SAMPLEFORMAT, sample_format);
    TIFFSetField(tif, TIFFTAG_ROWSPERSTRIP, HEIGHT);

    /* Tie points and pixel scale */
    const double tiepoints[6] = {0, 0, 0, 100.0, 50.0, 0.0};
    const double pixscale[3] = {1.0, 1.0, 0.0};
    TIFFSetField(tif, TIFFTAG_GEOTIEPOINTS, 6, tiepoints);
    TIFFSetField(tif, TIFFTAG_GEOPIXELSCALE, 3, pixscale);

    /* Set up geo keys */
    GTIFKeySet(gtif, GTModelTypeGeoKey, TYPE_SHORT, 1, ModelGeographic);
    GTIFKeySet(gtif, GTRasterTypeGeoKey, TYPE_SHORT, 1, RasterPixelIsArea);
    GTIFKeySet(gtif, GTCitationGeoKey, TYPE_ASCII, 0, "Datatype Test GeoTIFF");
    GTIFKeySet(gtif, GeographicTypeGeoKey, TYPE_SHORT, 1, GCS_WGS_84);

    /* Allocate buffer for one row */
    buffer = malloc(row_size);
    if (!buffer) {
        printf("Failed to allocate buffer\n");
        GTIFFree(gtif);
        TIFFClose(tif);
        return -1;
    }

    /* Write test pattern data */
    for (int row = 0; row < HEIGHT; row++) {
        /* Fill buffer with simple pattern based on datatype */
        switch (sample_format) {
            case SAMPLEFORMAT_UINT:
                if (bits_per_sample == 8) {
                    for (int col = 0; col < WIDTH; col++)
                        ((uint8_t *) buffer)[col] = (uint8_t) ((row + col) % 256);
                } else if (bits_per_sample == 16) {
                    for (int col = 0; col < WIDTH; col++)
                        ((uint16_t *) buffer)[col] = (uint16_t) ((row * 256 + col) % 65536);
                } else if (bits_per_sample == 32) {
                    for (int col = 0; col < WIDTH; col++)
                        ((uint32_t *) buffer)[col] = (uint32_t) (row * 1000 + col);
                } else if (bits_per_sample == 64) {
                    for (int col = 0; col < WIDTH; col++)
                        ((uint64_t *) buffer)[col] = (uint64_t) (row * 1000 + col);
                }
                break;

            case SAMPLEFORMAT_INT:
                if (bits_per_sample == 8) {
                    for (int col = 0; col < WIDTH; col++)
                        ((int8_t *) buffer)[col] = (int8_t) ((row + col - 64) % 128);
                } else if (bits_per_sample == 16) {
                    for (int col = 0; col < WIDTH; col++)
                        ((int16_t *) buffer)[col] = (int16_t) ((row * 100 + col - 1000));
                } else if (bits_per_sample == 32) {
                    for (int col = 0; col < WIDTH; col++)
                        ((int32_t *) buffer)[col] = (int32_t) (row * 1000 + col - 16000);
                } else if (bits_per_sample == 64) {
                    for (int col = 0; col < WIDTH; col++)
                        ((int64_t *) buffer)[col] = (int64_t) (row * 1000 + col - 16000);
                }
                break;

            case SAMPLEFORMAT_IEEEFP:
                if (bits_per_sample == 32) {
                    for (int col = 0; col < WIDTH; col++)
                        ((float *) buffer)[col] = (float) (row + col * 0.5);
                } else if (bits_per_sample == 64) {
                    for (int col = 0; col < WIDTH; col++)
                        ((double *) buffer)[col] = (double) (row + col * 0.5);
                }
                break;

            default:
                /* For unsupported formats, write zeros */
                memset(buffer, 0, row_size);
                break;
        }

        if (!TIFFWriteScanline(tif, buffer, (uint32_t) row, 0)) {
            printf("Failed to write scanline %d\n", row);
            free(buffer);
            GTIFFree(gtif);
            TIFFClose(tif);
            return -1;
        }
    }

    free(buffer);
    GTIFWriteKeys(gtif);
    GTIFFree(gtif);
    XTIFFClose(tif);

    return 0;
}

/* Test datatype conversion by reading a file with one type into a buffer of another type */
int DatatypeConversionTest(hid_t mem_type_id, hid_t file_type_id, const char *mem_type_name,
                           const char *file_type_name)
{
    hid_t vol_id = H5I_INVALID_HID;
    hid_t fapl_id = H5I_INVALID_HID;
    hid_t file_id = H5I_INVALID_HID;
    hid_t dset_id = H5I_INVALID_HID;
    hid_t dset_type_id = H5I_INVALID_HID;
    hid_t space_id = H5I_INVALID_HID;
    void *data = NULL;
    char filename[256];
    uint16_t sample_format = 0;
    uint16_t bits_per_sample = 0;
    size_t mem_type_size = 0;
    hsize_t dims[3];
    int ndims = 0;
    size_t num_elements = 0;

    memset(filename, 0, sizeof(filename));

    printf("Testing conversion of %s to %s on dataset read  ", file_type_name, mem_type_name);

    /* Map file_type_id to TIFF sample_format and bits_per_sample */
    if (H5Tequal(file_type_id, H5T_NATIVE_UCHAR)) {
        sample_format = SAMPLEFORMAT_UINT;
        bits_per_sample = 8;
    } else if (H5Tequal(file_type_id, H5T_NATIVE_USHORT)) {
        sample_format = SAMPLEFORMAT_UINT;
        bits_per_sample = 16;
    } else if (H5Tequal(file_type_id, H5T_NATIVE_UINT)) {
        sample_format = SAMPLEFORMAT_UINT;
        bits_per_sample = 32;
    } else if (H5Tequal(file_type_id, H5T_NATIVE_UINT64)) {
        sample_format = SAMPLEFORMAT_UINT;
        bits_per_sample = 64;
    } else if (H5Tequal(file_type_id, H5T_NATIVE_CHAR)) {
        sample_format = SAMPLEFORMAT_INT;
        bits_per_sample = 8;
    } else if (H5Tequal(file_type_id, H5T_NATIVE_SHORT)) {
        sample_format = SAMPLEFORMAT_INT;
        bits_per_sample = 16;
    } else if (H5Tequal(file_type_id, H5T_NATIVE_INT)) {
        sample_format = SAMPLEFORMAT_INT;
        bits_per_sample = 32;
    } else if (H5Tequal(file_type_id, H5T_NATIVE_INT64)) {
        sample_format = SAMPLEFORMAT_INT;
        bits_per_sample = 64;
    } else if (H5Tequal(file_type_id, H5T_NATIVE_FLOAT)) {
        sample_format = SAMPLEFORMAT_IEEEFP;
        bits_per_sample = 32;
    } else if (H5Tequal(file_type_id, H5T_NATIVE_DOUBLE)) {
        sample_format = SAMPLEFORMAT_IEEEFP;
        bits_per_sample = 64;
    } else {
        printf("TEST ERROR: Unsupported file datatype\n");
        goto error;
    }

    /* Generate unique filename for this test */
    snprintf(filename, sizeof(filename), "_tmp_dtype_test_sf%d_bps%d.tif", sample_format,
             bits_per_sample);

    /* Create GeoTIFF file with the specified datatype */
    if (CreateTypedGeoTIFF(filename, sample_format, bits_per_sample) != 0) {
        printf("Failed to create test GeoTIFF file with sample_format=%d, bits_per_sample=%d\n",
               sample_format, bits_per_sample);
        goto error;
    }

    /* Add the plugin path so HDF5 can find the connector */
#ifdef GEOTIFF_VOL_PLUGIN_PATH
    if (H5PLappend(GEOTIFF_VOL_PLUGIN_PATH) < 0) {
        printf("Failed to append plugin path\n");
        goto error;
    }
#endif

    /* Register the GeoTIFF VOL connector */
    if ((vol_id = H5VLregister_connector_by_name(GEOTIFF_VOL_CONNECTOR_NAME, H5P_DEFAULT)) < 0) {
        printf("Failed to register VOL connector\n");
        goto error;
    }

    /* Create file access property list */
    if ((fapl_id = H5Pcreate(H5P_FILE_ACCESS)) < 0) {
        printf("Failed to create FAPL\n");
        goto error;
    }

    /* Set the VOL connector */
    if (H5Pset_vol(fapl_id, vol_id, NULL) < 0) {
        printf("Failed to set VOL connector\n");
        goto error;
    }

    /* Open the GeoTIFF file */
    if ((file_id = H5Fopen(filename, H5F_ACC_RDONLY, fapl_id)) < 0) {
        printf("Failed to open GeoTIFF file\n");
        goto error;
    }

    /* Open the image dataset */
    if ((dset_id = H5Dopen2(file_id, "image0", H5P_DEFAULT)) < 0) {
        printf("Failed to open image dataset\n");
        goto error;
    }

    /* Get dataset type to verify it matches file_type_id */
    if ((dset_type_id = H5Dget_type(dset_id)) < 0) {
        printf("Failed to get dataset type\n");
        goto error;
    }

    if (!H5Tequal(dset_type_id, file_type_id)) {
        printf("Dataset type does not match the type it was created with\n");
        goto error;
    }

    /* Get dataspace and dimensions */
    if ((space_id = H5Dget_space(dset_id)) < 0) {
        printf("Failed to get dataspace\n");
        goto error;
    }

    if ((ndims = H5Sget_simple_extent_ndims(space_id)) < 0) {
        printf("Failed to get number of dimensions\n");
        goto error;
    }

    if (H5Sget_simple_extent_dims(space_id, dims, NULL) < 0) {
        printf("Failed to get dimensions\n");
        goto error;
    }

    /* Allocate buffer for reading into memory type */
    if ((mem_type_size = H5Tget_size(mem_type_id)) == 0) {
        printf("Failed to get memory type size\n");
        goto error;
    }

    num_elements = (size_t) (dims[0] * dims[1]);

    if ((data = malloc(num_elements * mem_type_size)) == NULL) {
        printf("Failed to allocate read buffer\n");
        goto error;
    }

    /* Attempt to read with datatype conversion */
    if (H5Dread(dset_id, mem_type_id, H5S_ALL, H5S_ALL, H5P_DEFAULT, data) < 0) {
        printf("failed to perform datatype conversion\n");
        goto error;
    }

    /* Verify data correctness - check a sample of converted values */
    int verification_failed = 0;
    for (int row = 0; row < HEIGHT && row < 4; row++) {
        for (int col = 0; col < WIDTH && col < 8; col++) {
            int idx = row * WIDTH + col;
            double expected_source_val = 0.0;
            double actual_converted_val = 0.0;

            /* Compute expected source value based on file type pattern */
            if (H5Tequal(file_type_id, H5T_NATIVE_UCHAR)) {
                expected_source_val = (double) ((row + col) % 256);
            } else if (H5Tequal(file_type_id, H5T_NATIVE_USHORT)) {
                expected_source_val = (double) ((row * 256 + col) % 65536);
            } else if (H5Tequal(file_type_id, H5T_NATIVE_UINT)) {
                expected_source_val = (double) (row * 1000 + col);
            } else if (H5Tequal(file_type_id, H5T_NATIVE_UINT64)) {
                expected_source_val = (double) (row * 1000 + col);
            } else if (H5Tequal(file_type_id, H5T_NATIVE_CHAR)) {
                expected_source_val = (double) ((row + col - 64) % 128);
            } else if (H5Tequal(file_type_id, H5T_NATIVE_SHORT)) {
                expected_source_val = (double) (row * 100 + col - 1000);
            } else if (H5Tequal(file_type_id, H5T_NATIVE_INT)) {
                expected_source_val = (double) (row * 1000 + col - 16000);
            } else if (H5Tequal(file_type_id, H5T_NATIVE_INT64)) {
                expected_source_val = (double) (row * 1000 + col - 16000);
            } else if (H5Tequal(file_type_id, H5T_NATIVE_FLOAT)) {
                expected_source_val = (double) (row + col * 0.5);
            } else if (H5Tequal(file_type_id, H5T_NATIVE_DOUBLE)) {
                expected_source_val = (double) (row + col * 0.5);
            }

            /* Read actual converted value based on memory type */
            if (H5Tequal(mem_type_id, H5T_NATIVE_UCHAR)) {
                actual_converted_val = (double) ((uint8_t *) data)[idx];
            } else if (H5Tequal(mem_type_id, H5T_NATIVE_USHORT)) {
                actual_converted_val = (double) ((uint16_t *) data)[idx];
            } else if (H5Tequal(mem_type_id, H5T_NATIVE_UINT)) {
                actual_converted_val = (double) ((uint32_t *) data)[idx];
            } else if (H5Tequal(mem_type_id, H5T_NATIVE_UINT64)) {
                actual_converted_val = (double) ((uint64_t *) data)[idx];
            } else if (H5Tequal(mem_type_id, H5T_NATIVE_CHAR)) {
                actual_converted_val = (double) ((int8_t *) data)[idx];
            } else if (H5Tequal(mem_type_id, H5T_NATIVE_SHORT)) {
                actual_converted_val = (double) ((int16_t *) data)[idx];
            } else if (H5Tequal(mem_type_id, H5T_NATIVE_INT)) {
                actual_converted_val = (double) ((int32_t *) data)[idx];
            } else if (H5Tequal(mem_type_id, H5T_NATIVE_INT64)) {
                actual_converted_val = (double) ((int64_t *) data)[idx];
            } else if (H5Tequal(mem_type_id, H5T_NATIVE_FLOAT)) {
                actual_converted_val = (double) ((float *) data)[idx];
            } else if (H5Tequal(mem_type_id, H5T_NATIVE_DOUBLE)) {
                actual_converted_val = (double) ((double *) data)[idx];
            }

            /* Verify conversion correctness with tolerance for floating point */
            double expected_converted = expected_source_val;
            double tolerance = 0.01;

            /* Handle special conversion cases */
            /* For conversions to unsigned types, negative values should clamp to 0 */
            if (expected_source_val < 0 && (H5Tequal(mem_type_id, H5T_NATIVE_UCHAR) ||
                                            H5Tequal(mem_type_id, H5T_NATIVE_USHORT) ||
                                            H5Tequal(mem_type_id, H5T_NATIVE_UINT) ||
                                            H5Tequal(mem_type_id, H5T_NATIVE_UINT64))) {
                expected_converted = 0.0;
            }

            /* For float-to-integer conversions, expect truncation */
            if ((H5Tequal(file_type_id, H5T_NATIVE_FLOAT) ||
                 H5Tequal(file_type_id, H5T_NATIVE_DOUBLE)) &&
                !(H5Tequal(mem_type_id, H5T_NATIVE_FLOAT) ||
                  H5Tequal(mem_type_id, H5T_NATIVE_DOUBLE))) {
                expected_converted = (double) ((int64_t) expected_source_val);
            }

            /* Handle overflow/underflow by clamping to destination type range */
            if (H5Tequal(mem_type_id, H5T_NATIVE_UCHAR)) {
                if (expected_converted > 255.0)
                    expected_converted = 255.0;
                if (expected_converted < 0.0)
                    expected_converted = 0.0;
            } else if (H5Tequal(mem_type_id, H5T_NATIVE_CHAR)) {
                if (expected_converted > 127.0)
                    expected_converted = 127.0;
                if (expected_converted < -128.0)
                    expected_converted = -128.0;
            } else if (H5Tequal(mem_type_id, H5T_NATIVE_USHORT)) {
                if (expected_converted > 65535.0)
                    expected_converted = 65535.0;
                if (expected_converted < 0.0)
                    expected_converted = 0.0;
            } else if (H5Tequal(mem_type_id, H5T_NATIVE_SHORT)) {
                if (expected_converted > 32767.0)
                    expected_converted = 32767.0;
                if (expected_converted < -32768.0)
                    expected_converted = -32768.0;
            } else if (H5Tequal(mem_type_id, H5T_NATIVE_UINT)) {
                if (expected_converted > 4294967295.0)
                    expected_converted = 4294967295.0;
                if (expected_converted < 0.0)
                    expected_converted = 0.0;
            } else if (H5Tequal(mem_type_id, H5T_NATIVE_INT)) {
                if (expected_converted > 2147483647.0)
                    expected_converted = 2147483647.0;
                if (expected_converted < -2147483648.0)
                    expected_converted = -2147483648.0;
            }

            /* Check if values match within tolerance */
            if (fabs(actual_converted_val - expected_converted) > tolerance) {
                printf(
                    "VERIFICATION FAILED at [%d,%d]: expected %.2f, got %.2f (source was %.2f)\n",
                    row, col, expected_converted, actual_converted_val, expected_source_val);
                verification_failed = 1;
                break;
            }
        }
        if (verification_failed)
            break;
    }

    if (verification_failed) {
        printf("Data verification failed\n");
        goto error;
    }

    /* Clean up resources */
    if (data)
        free(data);
    data = NULL;
    if (space_id != H5I_INVALID_HID && H5Sclose(space_id) < 0) {
        printf("Failed to close dataspace\n");
        goto error;
    }
    space_id = H5I_INVALID_HID;
    if (dset_type_id != H5I_INVALID_HID && H5Tclose(dset_type_id) < 0) {
        printf("Failed to close dataset datatype\n");
        goto error;
    }
    dset_type_id = H5I_INVALID_HID;
    if (dset_id != H5I_INVALID_HID && H5Dclose(dset_id) < 0) {
        printf("Failed to close dataset\n");
        goto error;
    }
    dset_id = H5I_INVALID_HID;
    if (file_id != H5I_INVALID_HID && H5Fclose(file_id) < 0) {
        printf("Failed to close file\n");
        goto error;
    }
    file_id = H5I_INVALID_HID;
    if (fapl_id != H5I_INVALID_HID && H5Pclose(fapl_id) < 0) {
        printf("Failed to close FAPL\n");
        goto error;
    }
    fapl_id = H5I_INVALID_HID;
    if (vol_id != H5I_INVALID_HID && H5VLunregister_connector(vol_id) < 0) {
        printf("Failed to unregister VOL connector\n");
        goto error;
    }
    vol_id = H5I_INVALID_HID;

    /* Attempt to delete the temporary GeoTIFF file */
    if (remove(filename) != 0) {
        printf("WARNING: Failed to delete temporary file %s\n", filename);
    }
    memset(filename, 0, sizeof(filename));

    /* If read succeeded, this means conversion is working! */
    printf("PASSED\n");
    return 0;

error:
    printf("FAILED\n");

    /* Error cleanup */
    if (data)
        free(data);
    H5E_BEGIN_TRY
    {
        H5Sclose(space_id);
        H5Tclose(dset_type_id);
        H5Dclose(dset_id);
        H5Fclose(file_id);
        H5Pclose(fapl_id);
        if (vol_id != H5I_INVALID_HID)
            H5VLunregister_connector(vol_id);
    }
    H5E_END_TRY;

    /* Attempt to delete the temporary file even on error */
    if (filename[0] != '\0')
        remove(filename);

    return 1;
}

/* Helper function to create a multi-image GeoTIFF file with distinct RGB data */
static int CreateMultiImageGeoTIFF(const char *filename, uint32_t num_images)
{
    TIFF *tif = NULL;

    if ((tif = XTIFFOpen(filename, "w")) == NULL) {
        printf("Failed to create multi-image GeoTIFF %s\n", filename);
        return -1;
    }

    for (uint32_t img_idx = 0; img_idx < num_images; img_idx++) {
        GTIF *gtif = NULL;

        if ((gtif = GTIFNew(tif)) == NULL) {
            printf("Failed to create GeoTIFF handle for image %u\n", img_idx);
            TIFFClose(tif);
            return -1;
        }

        /* Set up TIFF tags for RGB image */
        TIFFSetField(tif, TIFFTAG_IMAGEWIDTH, WIDTH);
        TIFFSetField(tif, TIFFTAG_IMAGELENGTH, HEIGHT);
        TIFFSetField(tif, TIFFTAG_COMPRESSION, COMPRESSION_NONE);
        TIFFSetField(tif, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_RGB);
        TIFFSetField(tif, TIFFTAG_PLANARCONFIG, PLANARCONFIG_CONTIG);
        TIFFSetField(tif, TIFFTAG_BITSPERSAMPLE, 8);
        TIFFSetField(tif, TIFFTAG_SAMPLESPERPIXEL, 3);
        TIFFSetField(tif, TIFFTAG_ROWSPERSTRIP, HEIGHT);

        /* Tie points and pixel scale */
        const double tiepoints[6] = {0, 0, 0, 100.0 + img_idx * 10, 50.0 + img_idx * 10, 0.0};
        const double pixscale[3] = {1.0, 1.0, 0.0};
        TIFFSetField(tif, TIFFTAG_GEOTIEPOINTS, 6, tiepoints);
        TIFFSetField(tif, TIFFTAG_GEOPIXELSCALE, 3, pixscale);

        /* Set up geo keys */
        GTIFKeySet(gtif, GTModelTypeGeoKey, TYPE_SHORT, 1, ModelGeographic);
        GTIFKeySet(gtif, GTRasterTypeGeoKey, TYPE_SHORT, 1, RasterPixelIsArea);
        GTIFKeySet(gtif, GTCitationGeoKey, TYPE_ASCII, 0, "Multi-image Test GeoTIFF");
        GTIFKeySet(gtif, GeographicTypeGeoKey, TYPE_SHORT, 1, GCS_WGS_84);

        /* Allocate buffer for one scanline (row) */
        size_t scanline_size = WIDTH * 3;
        unsigned char *scanline = (unsigned char *) malloc(scanline_size);
        if (!scanline) {
            printf("Failed to allocate scanline buffer for image %u\n", img_idx);
            GTIFFree(gtif);
            TIFFClose(tif);
            return -1;
        }

        /* Write RGB data with pattern unique to this image
         * Image 0: R=(row*8)%256,        G=(col*8)%256,        B=((row+col)*4)%256
         * Image 1: R=(row*8+50)%256,     G=(col*8+50)%256,     B=((row+col)*4+50)%256
         * Image 2: R=(row*8+100)%256,    G=(col*8+100)%256,    B=((row+col)*4+100)%256
         * etc.
         */
        uint32_t offset = img_idx * 50;

        for (uint32_t row = 0; row < HEIGHT; row++) {
            for (uint32_t col = 0; col < WIDTH; col++) {
                uint32_t idx = col * 3;
                scanline[idx + 0] = (unsigned char) ((row * 8 + offset) % 256);
                scanline[idx + 1] = (unsigned char) ((col * 8 + offset) % 256);
                scanline[idx + 2] = (unsigned char) (((row + col) * 4 + offset) % 256);
            }

            if (!TIFFWriteScanline(tif, scanline, row, 0)) {
                printf("Failed to write scanline %u for image %u\n", row, img_idx);
                free(scanline);
                GTIFFree(gtif);
                TIFFClose(tif);
                return -1;
            }
        }

        free(scanline);
        GTIFWriteKeys(gtif);
        GTIFFree(gtif);

        /* Write directory for this image (except for the last one) */
        if (img_idx < num_images - 1) {
            if (!TIFFWriteDirectory(tif)) {
                printf("Failed to write directory for image %u\n", img_idx);
                TIFFClose(tif);
                return -1;
            }
        }
    }

    XTIFFClose(tif);
    return 0;
}

/* Test reading multiple images from a single GeoTIFF file */
int MultiImageReadGeoTIFFTest(void)
{
    const char *filename = "_tmp_multi_image.tif";
    const uint32_t NUM_IMAGES = 3;
    hid_t vol_id = H5I_INVALID_HID;
    hid_t fapl_id = H5I_INVALID_HID;
    hid_t file_id = H5I_INVALID_HID;
    hid_t dset_id = H5I_INVALID_HID;
    hid_t space_id = H5I_INVALID_HID;
    hsize_t dims[3];
    int ndims;
    unsigned char *data = NULL;
    size_t data_size;

    printf("Testing multi-image GeoTIFF reading  ");

    /* Create test file with multiple RGB images */
    if (CreateMultiImageGeoTIFF(filename, NUM_IMAGES) != 0) {
        printf("Failed to create multi-image test file\n");
        goto error;
    }

    /* Add the plugin path so HDF5 can find the connector */
#ifdef GEOTIFF_VOL_PLUGIN_PATH
    if (H5PLappend(GEOTIFF_VOL_PLUGIN_PATH) < 0) {
        printf("Failed to append plugin path\n");
        goto error;
    }
#endif

    /* Register the GeoTIFF VOL connector */
    if ((vol_id = H5VLregister_connector_by_name(GEOTIFF_VOL_CONNECTOR_NAME, H5P_DEFAULT)) < 0) {
        printf("Failed to register VOL connector\n");
        goto error;
    }

    /* Create file access property list */
    if ((fapl_id = H5Pcreate(H5P_FILE_ACCESS)) < 0) {
        printf("Failed to create FAPL\n");
        goto error;
    }

    /* Set the VOL connector */
    if (H5Pset_vol(fapl_id, vol_id, NULL) < 0) {
        printf("Failed to set VOL connector\n");
        goto error;
    }

    /* Open the GeoTIFF file */
    if ((file_id = H5Fopen(filename, H5F_ACC_RDONLY, fapl_id)) < 0) {
        printf("Failed to open GeoTIFF file\n");
        goto error;
    }

    /* Allocate buffer for reading image data */
    data_size = WIDTH * HEIGHT * 3;
    data = (unsigned char *) malloc(data_size);
    if (!data) {
        printf("Failed to allocate read buffer\n");
        goto error;
    }

    /* Test reading each image as a separate dataset */
    for (uint32_t img_idx = 0; img_idx < NUM_IMAGES; img_idx++) {
        char dset_name[32];
        snprintf(dset_name, sizeof(dset_name), "image%u", img_idx);

        /* Open the image dataset */
        if ((dset_id = H5Dopen2(file_id, dset_name, H5P_DEFAULT)) < 0) {
            printf("Failed to open dataset %s\n", dset_name);
            goto error;
        }

        /* Get dataspace */
        if ((space_id = H5Dget_space(dset_id)) < 0) {
            printf("Failed to get dataspace for %s\n", dset_name);
            goto error;
        }

        if ((ndims = H5Sget_simple_extent_ndims(space_id)) < 0) {
            printf("Failed to get number of dimensions for %s\n", dset_name);
            goto error;
        }

        if (ndims != 3) {
            printf("VERIFICATION FAILED: Expected 3 dimensions for %s, got %d\n", dset_name, ndims);
            goto error;
        }

        if (H5Sget_simple_extent_dims(space_id, dims, NULL) < 0) {
            printf("Failed to get dimensions for %s\n", dset_name);
            goto error;
        }

        /* Verify dimensions */
        if (dims[0] != HEIGHT || dims[1] != WIDTH || dims[2] != 3) {
            printf("VERIFICATION FAILED: Expected dimensions %dx%dx3 for %s, got %llux%llux%llu\n",
                   HEIGHT, WIDTH, dset_name, (unsigned long long) dims[0],
                   (unsigned long long) dims[1], (unsigned long long) dims[2]);
            goto error;
        }

        /* Read the dataset */
        if (H5Dread(dset_id, H5T_NATIVE_UCHAR, H5S_ALL, H5S_ALL, H5P_DEFAULT, data) < 0) {
            printf("Failed to read dataset %s\n", dset_name);
            goto error;
        }

        /* Verify the data matches the expected pattern for this image */
        uint32_t offset = img_idx * 50;
        for (uint32_t row = 0; row < HEIGHT; row++) {
            for (uint32_t col = 0; col < WIDTH; col++) {
                uint32_t idx = (row * WIDTH + col) * 3;
                unsigned char expected_r = (unsigned char) ((row * 8 + offset) % 256);
                unsigned char expected_g = (unsigned char) ((col * 8 + offset) % 256);
                unsigned char expected_b = (unsigned char) (((row + col) * 4 + offset) % 256);
                unsigned char actual_r = data[idx + 0];
                unsigned char actual_g = data[idx + 1];
                unsigned char actual_b = data[idx + 2];

                if (actual_r != expected_r || actual_g != expected_g || actual_b != expected_b) {
                    printf("VERIFICATION FAILED: %s pixel[%u,%u] expected RGB(%u,%u,%u), got "
                           "RGB(%u,%u,%u)\n",
                           dset_name, row, col, expected_r, expected_g, expected_b, actual_r,
                           actual_g, actual_b);
                    goto error;
                }
            }
        }

        /* Close dataset and dataspace for this image */
        if (H5Sclose(space_id) < 0) {
            printf("Failed to close dataspace for %s\n", dset_name);
            goto error;
        }
        space_id = H5I_INVALID_HID;

        if (H5Dclose(dset_id) < 0) {
            printf("Failed to close dataset %s\n", dset_name);
            goto error;
        }
        dset_id = H5I_INVALID_HID;
    }

    /* Clean up */
    free(data);
    data = NULL;

    if (H5Fclose(file_id) < 0) {
        printf("Failed to close file\n");
        goto error;
    }
    file_id = H5I_INVALID_HID;

    if (H5Pclose(fapl_id) < 0) {
        printf("Failed to close FAPL\n");
        goto error;
    }
    fapl_id = H5I_INVALID_HID;

    /* Unregister VOL connector */
    if (H5VLunregister_connector(vol_id) < 0) {
        printf("Failed to unregister VOL connector\n");
        goto error;
    }
    vol_id = H5I_INVALID_HID;

    /* Delete temporary test file */
    if (remove(filename) != 0) {
        printf("WARNING: Failed to delete temporary file %s\n", filename);
    }

    printf("PASSED\n");
    return 0;

error:
    /* Clean up on error */
    if (data)
        free(data);
    H5E_BEGIN_TRY
    {
        H5Sclose(space_id);
        H5Dclose(dset_id);
        H5Fclose(file_id);
        H5Pclose(fapl_id);
        if (vol_id != H5I_INVALID_HID)
            H5VLunregister_connector(vol_id);
    }
    H5E_END_TRY;

    /* Attempt to delete temporary file */
    remove(filename);

    printf("FAILED\n");
    return 1;
}

/* Test dataset error handling - verify proper errors for invalid dataset access */
int DatasetErrorHandlingTest(const char *filename)
{
    hid_t vol_id = H5I_INVALID_HID;
    hid_t fapl_id = H5I_INVALID_HID;
    hid_t file_id = H5I_INVALID_HID;
    hid_t dset_id = H5I_INVALID_HID;

    printf("Testing dataset error handling with file: %s  ", filename);

    /* Add the plugin path so HDF5 can find the connector */
#ifdef GEOTIFF_VOL_PLUGIN_PATH
    if (H5PLappend(GEOTIFF_VOL_PLUGIN_PATH) < 0) {
        printf("Failed to append plugin path\n");
        goto error;
    }
#endif

    /* Register the GeoTIFF VOL connector */
    if ((vol_id = H5VLregister_connector_by_name(GEOTIFF_VOL_CONNECTOR_NAME, H5P_DEFAULT)) < 0) {
        printf("Failed to register VOL connector\n");
        goto error;
    }

    /* Create file access property list */
    if ((fapl_id = H5Pcreate(H5P_FILE_ACCESS)) < 0) {
        printf("Failed to create FAPL\n");
        goto error;
    }

    /* Set the VOL connector */
    if (H5Pset_vol(fapl_id, vol_id, NULL) < 0) {
        printf("Failed to set VOL connector\n");
        goto error;
    }

    /* Open the GeoTIFF file */
    if ((file_id = H5Fopen(filename, H5F_ACC_RDONLY, fapl_id)) < 0) {
        printf("Failed to open GeoTIFF file\n");
        goto error;
    }

    /* Test 1: Try to access image1 in a single-image file (should fail) */
    H5E_BEGIN_TRY
    {
        dset_id = H5Dopen2(file_id, "image1", H5P_DEFAULT);
    }
    H5E_END_TRY;

    if (dset_id >= 0) {
        printf("VERIFICATION FAILED: Opening image1 in single-image file should have failed\n");
        H5Dclose(dset_id);
        goto error;
    }
    /* Expected failure - this is correct */

    /* Test 2: Try to open a dataset with invalid name (not "imageN" format) */
    const char *invalid_names[] = {"data", "/dataset", "image", "img0", "0image", "image_0"};
    int num_invalid_names = sizeof(invalid_names) / sizeof(invalid_names[0]);

    for (int i = 0; i < num_invalid_names; i++) {
        H5E_BEGIN_TRY
        {
            dset_id = H5Dopen2(file_id, invalid_names[i], H5P_DEFAULT);
        }
        H5E_END_TRY;

        if (dset_id >= 0) {
            printf("VERIFICATION FAILED: Opening dataset '%s' should have failed\n",
                   invalid_names[i]);
            H5Dclose(dset_id);
            goto error;
        }
        /* Expected failure - this is correct */
    }

    if (H5Fclose(file_id) < 0) {
        printf("Failed to close file\n");
        goto error;
    }
    file_id = H5I_INVALID_HID;

    if (H5Pclose(fapl_id) < 0) {
        printf("Failed to close FAPL\n");
        goto error;
    }
    fapl_id = H5I_INVALID_HID;

    /* Unregister VOL connector */
    if (H5VLunregister_connector(vol_id) < 0) {
        printf("Failed to unregister VOL connector\n");
        goto error;
    }
    vol_id = H5I_INVALID_HID;

    printf("PASSED\n");
    return 0;

error:
    H5E_BEGIN_TRY
    {
        H5Dclose(dset_id);
        H5Fclose(file_id);
        H5Pclose(fapl_id);
        if (vol_id != H5I_INVALID_HID)
            H5VLunregister_connector(vol_id);
    }
    H5E_END_TRY;

    printf("FAILED\n");
    return 1;
}

/* Test point selection reading - verify only selected points are read */
int PointReadGeoTIFFTest(const char *filename)
{
    hid_t vol_id = H5I_INVALID_HID;
    hid_t fapl_id = H5I_INVALID_HID;
    hid_t file_id = H5I_INVALID_HID;
    hid_t dset_id = H5I_INVALID_HID;
    hid_t space_id = H5I_INVALID_HID;
    hid_t mem_space_id = H5I_INVALID_HID;
    hid_t file_space_id = H5I_INVALID_HID;
    hsize_t dims[3];
    int ndims;
    unsigned char *buffer = NULL;
    size_t buffer_size;
    const hsize_t NUM_POINTS = 5;
    hsize_t coords[5 * 3]; /* Flat array: max 5 points × 3 dimensions */

    printf("Testing GeoTIFF VOL connector point selection reading with file: %s  ", filename);

    /* Add the plugin path so HDF5 can find the connector */
#ifdef GEOTIFF_VOL_PLUGIN_PATH
    if (H5PLappend(GEOTIFF_VOL_PLUGIN_PATH) < 0) {
        printf("Failed to append plugin path\n");
        goto error;
    }
#endif

    /* Register the GeoTIFF VOL connector */
    if ((vol_id = H5VLregister_connector_by_name(GEOTIFF_VOL_CONNECTOR_NAME, H5P_DEFAULT)) < 0) {
        printf("Failed to register VOL connector\n");
        goto error;
    }

    /* Create file access property list */
    if ((fapl_id = H5Pcreate(H5P_FILE_ACCESS)) < 0) {
        printf("Failed to create FAPL\n");
        goto error;
    }

    /* Set the VOL connector */
    if (H5Pset_vol(fapl_id, vol_id, NULL) < 0) {
        printf("Failed to set VOL connector\n");
        goto error;
    }

    /* Open the GeoTIFF file */
    if ((file_id = H5Fopen(filename, H5F_ACC_RDONLY, fapl_id)) < 0) {
        printf("Failed to open GeoTIFF file\n");
        goto error;
    }

    /* Open the image dataset */
    if ((dset_id = H5Dopen2(file_id, "image0", H5P_DEFAULT)) < 0) {
        printf("Failed to open image dataset\n");
        goto error;
    }

    /* Get dataspace */
    if ((space_id = H5Dget_space(dset_id)) < 0) {
        printf("Failed to get dataspace\n");
        goto error;
    }

    if ((ndims = H5Sget_simple_extent_ndims(space_id)) < 0) {
        printf("Failed to get number of dimensions\n");
        goto error;
    }

    if (ndims <= 0 || ndims > 3) {
        printf("Invalid number of dimensions: %d\n", ndims);
        goto error;
    }

    if (H5Sget_simple_extent_dims(space_id, dims, NULL) < 0) {
        printf("Failed to get dimensions\n");
        goto error;
    }

    /* Select 5 specific points based on dimensionality */
    /* Pack coordinates as flat array: coord[0], coord[1], ..., coord[ndims-1] for each point */
    if (ndims == 2) {
        /* Grayscale image: select points (0,0), (5,5), (10,15), (20,20), (31,31) */
        coords[0] = 0;
        coords[1] = 0;
        coords[2] = 5;
        coords[3] = 5;
        coords[4] = 10;
        coords[5] = 15;
        coords[6] = 20;
        coords[7] = 20;
        coords[8] = 31;
        coords[9] = 31;
    } else if (ndims == 3) {
        /* RGB image: select points from different bands */
        coords[0] = 0;
        coords[1] = 0;
        coords[2] = 0; /* Red channel */
        coords[3] = 5;
        coords[4] = 5;
        coords[5] = 1; /* Green channel */
        coords[6] = 10;
        coords[7] = 15;
        coords[8] = 2; /* Blue channel */
        coords[9] = 20;
        coords[10] = 20;
        coords[11] = 0; /* Red channel */
        coords[12] = 31;
        coords[13] = 31;
        coords[14] = 1; /* Green channel */
    } else {
        printf("Unsupported number of dimensions: %d\n", ndims);
        goto error;
    }

    /* Create file space selection for the points */
    if ((file_space_id = H5Scopy(space_id)) < 0) {
        printf("Failed to copy dataspace\n");
        goto error;
    }

    if (H5Sselect_elements(file_space_id, H5S_SELECT_SET, NUM_POINTS, coords) < 0) {
        printf("Failed to select points\n");
        goto error;
    }

    /* Allocate buffer large enough to hold more than just the selected points */
    buffer_size = NUM_POINTS * 2; /* Double the size to test sentinel values */
    buffer = (unsigned char *) malloc(buffer_size);
    if (!buffer) {
        printf("Failed to allocate memory for buffer\n");
        goto error;
    }

    /* Initialize entire buffer with sentinel value (0xAA) */
    memset(buffer, 0xAA, buffer_size);

    /* Create memory dataspace for the selected points */
    hsize_t mem_dims[1] = {NUM_POINTS};
    if ((mem_space_id = H5Screate_simple(1, mem_dims, NULL)) < 0) {
        printf("Failed to create memory dataspace\n");
        goto error;
    }

    /* Read the selected points */
    if (H5Dread(dset_id, H5T_NATIVE_UCHAR, mem_space_id, file_space_id, H5P_DEFAULT, buffer) < 0) {
        printf("Failed to read selected points\n");
        goto error;
    }

    /* Verify the selected points contain expected data */
    for (hsize_t i = 0; i < NUM_POINTS; i++) {
        unsigned char expected;

        if (ndims == 2) {
            /* Grayscale pattern: value = (row * 8 + col / 4) % 256 */
            hsize_t row = coords[i * 2 + 0];
            hsize_t col = coords[i * 2 + 1];
            expected = (unsigned char) ((row * 8 + col / 4) % 256);
        } else {
            /* RGB pattern depends on channel:
             * R = (row * 8) % 256
             * G = (col * 8) % 256
             * B = ((row + col) * 4) % 256
             */
            hsize_t row = coords[i * 3 + 0];
            hsize_t col = coords[i * 3 + 1];
            hsize_t band = coords[i * 3 + 2];

            if (band == 0) {
                expected = (unsigned char) ((row * 8) % 256);
            } else if (band == 1) {
                expected = (unsigned char) ((col * 8) % 256);
            } else {
                expected = (unsigned char) (((row + col) * 4) % 256);
            }
        }

        if (buffer[i] != expected) {
            printf("VERIFICATION FAILED: Point %llu expected %u, got %u\n", (unsigned long long) i,
                   expected, buffer[i]);
            goto error;
        }
    }

    /* Verify that buffer locations beyond the selected points retain sentinel value */
    for (hsize_t i = NUM_POINTS; i < buffer_size; i++) {
        if (buffer[i] != 0xAA) {
            printf("VERIFICATION FAILED: Buffer position %llu beyond selected points was modified "
                   "(expected 0xAA, got 0x%02X)\n",
                   (unsigned long long) i, buffer[i]);
            goto error;
        }
    }

    /* Clean up */
    free(buffer);
    H5Sclose(mem_space_id);
    H5Sclose(file_space_id);
    H5Sclose(space_id);
    H5Dclose(dset_id);
    H5Fclose(file_id);
    H5Pclose(fapl_id);
    H5VLunregister_connector(vol_id);

    printf("PASSED\n");
    return 0;

error:
    H5E_BEGIN_TRY
    {
        if (buffer)
            free(buffer);
        H5Sclose(mem_space_id);
        H5Sclose(file_space_id);
        H5Sclose(space_id);
        H5Dclose(dset_id);
        H5Fclose(file_id);
        H5Pclose(fapl_id);
        if (vol_id != H5I_INVALID_HID)
            H5VLunregister_connector(vol_id);
    }
    H5E_END_TRY;

    printf("FAILED\n");
    return 1;
}

/* Verify that link existence check works properly */
int LinkExistsTest(const char *filename)
{
    hid_t vol_id = H5I_INVALID_HID;
    hid_t fapl_id = H5I_INVALID_HID;
    hid_t file_id = H5I_INVALID_HID;
    htri_t exists;

    printf("Testing GeoTIFF VOL connector link exists with file: %s  ", filename);

    /* Add the plugin path so HDF5 can find the connector */
#ifdef GEOTIFF_VOL_PLUGIN_PATH
    if (H5PLappend(GEOTIFF_VOL_PLUGIN_PATH) < 0) {
        printf("Failed to append plugin path\n");
        goto error;
    }
#endif

    /* Register the GeoTIFF VOL connector */
    if ((vol_id = H5VLregister_connector_by_name(GEOTIFF_VOL_CONNECTOR_NAME, H5P_DEFAULT)) < 0) {
        printf("Failed to register VOL connector\n");
        goto error;
    }

    /* Create file access property list */
    if ((fapl_id = H5Pcreate(H5P_FILE_ACCESS)) < 0) {
        printf("Failed to create FAPL\n");
        goto error;
    }

    /* Set the VOL connector */
    if (H5Pset_vol(fapl_id, vol_id, NULL) < 0) {
        printf("Failed to set VOL connector\n");
        goto error;
    }

    /* Open the GeoTIFF file */
    if ((file_id = H5Fopen(filename, H5F_ACC_RDONLY, fapl_id)) < 0) {
        printf("Failed to open GeoTIFF file\n");
        goto error;
    }

    /* Check that "image0" exists */
    if ((exists = H5Lexists(file_id, "image0", H5P_DEFAULT)) < 0) {
        printf("Failed to check link existence for 'image0'\n");
        goto error;
    }

    if (!exists) {
        printf("VERIFICATION FAILED: Link 'image0' should exist but doesn't\n");
        goto error;
    }

    /* Check that a non-existent link doesn't exist */
    if ((exists = H5Lexists(file_id, "nonexistent", H5P_DEFAULT)) < 0) {
        printf("Failed to check link existence for 'nonexistent'\n");
        goto error;
    }

    if (exists) {
        printf("VERIFICATION FAILED: Link 'nonexistent' should not exist but does\n");
        goto error;
    }

    /* Clean up */
    if (H5Fclose(file_id) < 0) {
        printf("Failed to close file\n");
        goto error;
    }

    if (H5Pclose(fapl_id) < 0) {
        printf("Failed to close FAPL\n");
        goto error;
    }

    /* Unregister VOL connector */
    if (H5VLunregister_connector(vol_id) < 0) {
        printf("Failed to unregister VOL connector\n");
        goto error;
    }

    printf("PASSED\n");
    return 0;

error:
    H5E_BEGIN_TRY
    {
        H5Fclose(file_id);
        H5Pclose(fapl_id);
        if (vol_id != H5I_INVALID_HID)
            H5VLunregister_connector(vol_id);
    }
    H5E_END_TRY;

    printf("FAILED\n");
    return 1;
}

/* Callback for link iteration */
static herr_t link_iterate_callback(hid_t group, const char *name, const H5L_info2_t *info,
                                    void *op_data)
{
    int *count = (int *) op_data;

    (void) group; /* Unused */
    (void) info;  /* Unused */

    /* Verify we got the expected link name */
    if (strcmp(name, "image0") != 0) {
        printf("VERIFICATION FAILED: Expected link name 'image0', got '%s'\n", name);
        return -1;
    }

    /* Verify link info */
    if (info->type != H5L_TYPE_HARD) {
        printf("VERIFICATION FAILED: Expected hard link, got type %d\n", info->type);
        return -1;
    }

    (*count)++;
    return 0;
}

/* Verify that link iteration works properly */
int LinkIterateTest(const char *filename)
{
    hid_t vol_id = H5I_INVALID_HID;
    hid_t fapl_id = H5I_INVALID_HID;
    hid_t file_id = H5I_INVALID_HID;
    int link_count = 0;
    hsize_t idx = 0;

    printf("Testing GeoTIFF VOL connector link iteration with file: %s  ", filename);

    /* Add the plugin path so HDF5 can find the connector */
#ifdef GEOTIFF_VOL_PLUGIN_PATH
    if (H5PLappend(GEOTIFF_VOL_PLUGIN_PATH) < 0) {
        printf("Failed to append plugin path\n");
        goto error;
    }
#endif

    /* Register the GeoTIFF VOL connector */
    if ((vol_id = H5VLregister_connector_by_name(GEOTIFF_VOL_CONNECTOR_NAME, H5P_DEFAULT)) < 0) {
        printf("Failed to register VOL connector\n");
        goto error;
    }

    /* Create file access property list */
    if ((fapl_id = H5Pcreate(H5P_FILE_ACCESS)) < 0) {
        printf("Failed to create FAPL\n");
        goto error;
    }

    /* Set the VOL connector */
    if (H5Pset_vol(fapl_id, vol_id, NULL) < 0) {
        printf("Failed to set VOL connector\n");
        goto error;
    }

    /* Open the GeoTIFF file */
    if ((file_id = H5Fopen(filename, H5F_ACC_RDONLY, fapl_id)) < 0) {
        printf("Failed to open GeoTIFF file\n");
        goto error;
    }

    /* Iterate over links in root group */
    if (H5Literate2(file_id, H5_INDEX_NAME, H5_ITER_INC, &idx, link_iterate_callback, &link_count) <
        0) {
        printf("Failed to iterate over links\n");
        goto error;
    }

    /* Verify we found exactly one link */
    if (link_count != 1) {
        printf("VERIFICATION FAILED: Expected 1 link, found %d\n", link_count);
        goto error;
    }

    /* Verify index was updated */
    if (idx != 1) {
        printf("VERIFICATION FAILED: Expected index 1 after iteration, got %llu\n",
               (unsigned long long) idx);
        goto error;
    }

    /* Clean up */
    if (H5Fclose(file_id) < 0) {
        printf("Failed to close file\n");
        goto error;
    }

    if (H5Pclose(fapl_id) < 0) {
        printf("Failed to close FAPL\n");
        goto error;
    }

    /* Unregister VOL connector */
    if (H5VLunregister_connector(vol_id) < 0) {
        printf("Failed to unregister VOL connector\n");
        goto error;
    }

    printf("PASSED\n");
    return 0;

error:
    H5E_BEGIN_TRY
    {
        H5Fclose(file_id);
        H5Pclose(fapl_id);
        if (vol_id != H5I_INVALID_HID)
            H5VLunregister_connector(vol_id);
    }
    H5E_END_TRY;

    printf("FAILED\n");
    return 1;
}

/* Test that unsupported TIFF features are properly rejected */
int UnsupportedFeaturesTest(void)
{
    hid_t vol_id = H5I_INVALID_HID;
    hid_t fapl_id = H5I_INVALID_HID;
    hid_t file_id = H5I_INVALID_HID;
    hid_t dset_id = H5I_INVALID_HID;
    const char *planar_separate_file = "_tmp_planar_separate.tif";
    const char *palette_file = "_tmp_palette.tif";
    const char *nonbyte_aligned_file = "_tmp_4bit.tif";
    TIFF *tif = NULL;
    GTIF *gtif = NULL;

    printf("Testing rejection of unsupported TIFF features  ");

    /* Test 1: Create a TIFF with PLANARCONFIG_SEPARATE (RGB planes stored separately) */
    if ((tif = XTIFFOpen(planar_separate_file, "w")) != NULL) {
        if ((gtif = GTIFNew(tif)) != NULL) {
            TIFFSetField(tif, TIFFTAG_IMAGEWIDTH, WIDTH);
            TIFFSetField(tif, TIFFTAG_IMAGELENGTH, HEIGHT);
            TIFFSetField(tif, TIFFTAG_COMPRESSION, COMPRESSION_NONE);
            TIFFSetField(tif, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_RGB);
            TIFFSetField(tif, TIFFTAG_PLANARCONFIG, PLANARCONFIG_SEPARATE);
            TIFFSetField(tif, TIFFTAG_BITSPERSAMPLE, 8);
            TIFFSetField(tif, TIFFTAG_SAMPLESPERPIXEL, 3);
            TIFFSetField(tif, TIFFTAG_ROWSPERSTRIP, HEIGHT);

            const double tiepoints[6] = {0, 0, 0, 100.0, 50.0, 0.0};
            const double pixscale[3] = {1.0, 1.0, 0.0};
            TIFFSetField(tif, TIFFTAG_GEOTIEPOINTS, 6, tiepoints);
            TIFFSetField(tif, TIFFTAG_GEOPIXELSCALE, 3, pixscale);
            SetUpGeoKeys(gtif);

            /* Write dummy data for each plane */
            unsigned char buffer[WIDTH];
            memset(buffer, 128, WIDTH);
            for (int plane = 0; plane < 3; plane++) {
                for (uint32_t row = 0; row < HEIGHT; row++) {
                    TIFFWriteScanline(tif, buffer, row, (uint16_t) plane);
                }
            }

            GTIFWriteKeys(gtif);
            GTIFFree(gtif);
        }
        XTIFFClose(tif);
    }

    /* Test 2: Create a TIFF with palette color (unsupported photometric) */
    if ((tif = XTIFFOpen(palette_file, "w")) != NULL) {
        if ((gtif = GTIFNew(tif)) != NULL) {
            TIFFSetField(tif, TIFFTAG_IMAGEWIDTH, WIDTH);
            TIFFSetField(tif, TIFFTAG_IMAGELENGTH, HEIGHT);
            TIFFSetField(tif, TIFFTAG_COMPRESSION, COMPRESSION_NONE);
            TIFFSetField(tif, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_PALETTE);
            TIFFSetField(tif, TIFFTAG_PLANARCONFIG, PLANARCONFIG_CONTIG);
            TIFFSetField(tif, TIFFTAG_BITSPERSAMPLE, 8);
            TIFFSetField(tif, TIFFTAG_SAMPLESPERPIXEL, 1);
            TIFFSetField(tif, TIFFTAG_ROWSPERSTRIP, HEIGHT);

            /* Set up a dummy color map */
            uint16_t colormap[3][256];
            for (int i = 0; i < 256; i++) {
                colormap[0][i] = (uint16_t) (i << 8);
                colormap[1][i] = (uint16_t) (i << 8);
                colormap[2][i] = (uint16_t) (i << 8);
            }
            TIFFSetField(tif, TIFFTAG_COLORMAP, colormap[0], colormap[1], colormap[2]);

            const double tiepoints[6] = {0, 0, 0, 100.0, 50.0, 0.0};
            const double pixscale[3] = {1.0, 1.0, 0.0};
            TIFFSetField(tif, TIFFTAG_GEOTIEPOINTS, 6, tiepoints);
            TIFFSetField(tif, TIFFTAG_GEOPIXELSCALE, 3, pixscale);
            SetUpGeoKeys(gtif);

            unsigned char buffer[WIDTH];
            memset(buffer, 0, WIDTH);
            for (uint32_t row = 0; row < HEIGHT; row++) {
                TIFFWriteScanline(tif, buffer, row, 0);
            }

            GTIFWriteKeys(gtif);
            GTIFFree(gtif);
        }
        XTIFFClose(tif);
    }

    /* Test 3: Create a TIFF with 4-bit samples (non-byte-aligned) */
    if ((tif = XTIFFOpen(nonbyte_aligned_file, "w")) != NULL) {
        if ((gtif = GTIFNew(tif)) != NULL) {
            TIFFSetField(tif, TIFFTAG_IMAGEWIDTH, WIDTH);
            TIFFSetField(tif, TIFFTAG_IMAGELENGTH, HEIGHT);
            TIFFSetField(tif, TIFFTAG_COMPRESSION, COMPRESSION_NONE);
            TIFFSetField(tif, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_MINISBLACK);
            TIFFSetField(tif, TIFFTAG_PLANARCONFIG, PLANARCONFIG_CONTIG);
            TIFFSetField(tif, TIFFTAG_BITSPERSAMPLE, 4);
            TIFFSetField(tif, TIFFTAG_SAMPLESPERPIXEL, 1);
            TIFFSetField(tif, TIFFTAG_ROWSPERSTRIP, HEIGHT);

            const double tiepoints[6] = {0, 0, 0, 100.0, 50.0, 0.0};
            const double pixscale[3] = {1.0, 1.0, 0.0};
            TIFFSetField(tif, TIFFTAG_GEOTIEPOINTS, 6, tiepoints);
            TIFFSetField(tif, TIFFTAG_GEOPIXELSCALE, 3, pixscale);
            SetUpGeoKeys(gtif);

            /* 4-bit data: 2 pixels per byte */
            unsigned char buffer[WIDTH / 2];
            memset(buffer, 0x55, WIDTH / 2);
            for (uint32_t row = 0; row < HEIGHT; row++) {
                TIFFWriteScanline(tif, buffer, row, 0);
            }

            GTIFWriteKeys(gtif);
            GTIFFree(gtif);
        }
        XTIFFClose(tif);
    }

    /* Now test that opening these files fails with appropriate errors */
#ifdef GEOTIFF_VOL_PLUGIN_PATH
    if (H5PLappend(GEOTIFF_VOL_PLUGIN_PATH) < 0) {
        printf("Failed to append plugin path\n");
        goto error;
    }
#endif

    if ((vol_id = H5VLregister_connector_by_name(GEOTIFF_VOL_CONNECTOR_NAME, H5P_DEFAULT)) < 0) {
        printf("Failed to register VOL connector\n");
        goto error;
    }

    if ((fapl_id = H5Pcreate(H5P_FILE_ACCESS)) < 0) {
        printf("Failed to create FAPL\n");
        goto error;
    }

    if (H5Pset_vol(fapl_id, vol_id, NULL) < 0) {
        printf("Failed to set VOL connector\n");
        goto error;
    }

    /* Test that PLANARCONFIG_SEPARATE is rejected */
    if ((file_id = H5Fopen(planar_separate_file, H5F_ACC_RDONLY, fapl_id)) >= 0) {
        H5E_BEGIN_TRY
        {
            dset_id = H5Dopen2(file_id, "image0", H5P_DEFAULT);
        }
        H5E_END_TRY;

        if (dset_id >= 0) {
            printf("VERIFICATION FAILED: PLANARCONFIG_SEPARATE should have been rejected\n");
            goto error;
        }
        H5Fclose(file_id);
    }

    /* Test that PHOTOMETRIC_PALETTE is rejected */
    if ((file_id = H5Fopen(palette_file, H5F_ACC_RDONLY, fapl_id)) >= 0) {
        H5E_BEGIN_TRY
        {
            dset_id = H5Dopen2(file_id, "image0", H5P_DEFAULT);
        }
        H5E_END_TRY;

        if (dset_id >= 0) {
            printf("VERIFICATION FAILED: PHOTOMETRIC_PALETTE should have been rejected\n");
            goto error;
        }
        H5Fclose(file_id);
    }

    /* Test that non-byte-aligned bit depths are rejected */
    if ((file_id = H5Fopen(nonbyte_aligned_file, H5F_ACC_RDONLY, fapl_id)) >= 0) {
        H5E_BEGIN_TRY
        {
            dset_id = H5Dopen2(file_id, "image0", H5P_DEFAULT);
        }
        H5E_END_TRY;

        if (dset_id >= 0) {
            printf("VERIFICATION FAILED: 4-bit samples should have been rejected\n");
            goto error;
        }
        H5Fclose(file_id);
        file_id = H5I_INVALID_HID;
    }

    /* Clean up */
    if (H5Pclose(fapl_id) < 0) {
        printf("Failed to close FAPL\n");
        goto error;
    }
    fapl_id = H5I_INVALID_HID;

    if (H5VLunregister_connector(vol_id) < 0) {
        printf("Failed to unregister VOL connector\n");
        goto error;
    }
    vol_id = H5I_INVALID_HID;

    /* Delete temporary test files */
    remove(planar_separate_file);
    remove(palette_file);
    remove(nonbyte_aligned_file);

    printf("PASSED\n");
    return 0;

error:
    H5E_BEGIN_TRY
    {
        H5Dclose(dset_id);
        H5Fclose(file_id);
        H5Pclose(fapl_id);
        if (vol_id != H5I_INVALID_HID)
            H5VLunregister_connector(vol_id);
    }
    H5E_END_TRY;

    remove(planar_separate_file);
    remove(palette_file);
    remove(nonbyte_aligned_file);

    printf("FAILED\n");
    return 1;
}

/* Test H5Gget_info on single and multi-image GeoTIFF files */
int GroupGetInfoTest(void)
{
    hid_t vol_id = H5I_INVALID_HID;
    hid_t fapl_id = H5I_INVALID_HID;
    hid_t file_id = H5I_INVALID_HID;
    hid_t group_id = H5I_INVALID_HID;
    H5G_info_t group_info;
    const char *single_image_file = "_tmp_single_image_groupinfo.tif";
    const char *multi_image_file = "_tmp_multi_image_groupinfo.tif";
    const uint32_t NUM_IMAGES = 3;

    printf("Testing H5Gget_info on GeoTIFF files  ");

    /* Create single-image test file */
    if (CreateGrayscaleGeoTIFF(single_image_file) != 0) {
        printf("Failed to create single-image test file\n");
        goto error;
    }

    /* Create multi-image test file */
    if (CreateMultiImageGeoTIFF(multi_image_file, NUM_IMAGES) != 0) {
        printf("Failed to create multi-image test file\n");
        goto error;
    }

    /* Add the plugin path so HDF5 can find the connector */
#ifdef GEOTIFF_VOL_PLUGIN_PATH
    if (H5PLappend(GEOTIFF_VOL_PLUGIN_PATH) < 0) {
        printf("Failed to append plugin path\n");
        goto error;
    }
#endif

    /* Register the GeoTIFF VOL connector */
    if ((vol_id = H5VLregister_connector_by_name(GEOTIFF_VOL_CONNECTOR_NAME, H5P_DEFAULT)) < 0) {
        printf("Failed to register VOL connector\n");
        goto error;
    }

    /* Create file access property list */
    if ((fapl_id = H5Pcreate(H5P_FILE_ACCESS)) < 0) {
        printf("Failed to create FAPL\n");
        goto error;
    }

    /* Set the VOL connector */
    if (H5Pset_vol(fapl_id, vol_id, NULL) < 0) {
        printf("Failed to set VOL connector\n");
        goto error;
    }

    /* Test 1: Single-image file - should report 1 link (image0) */
    if ((file_id = H5Fopen(single_image_file, H5F_ACC_RDONLY, fapl_id)) < 0) {
        printf("Failed to open single-image GeoTIFF file\n");
        goto error;
    }

    /* Open root group */
    if ((group_id = H5Gopen2(file_id, "/", H5P_DEFAULT)) < 0) {
        printf("Failed to open root group in single-image file\n");
        goto error;
    }

    /* Get group info */
    if (H5Gget_info(group_id, &group_info) < 0) {
        printf("Failed to get group info for single-image file\n");
        goto error;
    }

    /* Verify single-image file has 1 link */
    if (group_info.nlinks != 1) {
        printf("VERIFICATION FAILED: Single-image file expected 1 link, got %llu\n",
               (unsigned long long) group_info.nlinks);
        goto error;
    }

    /* Close group and file */
    if (H5Gclose(group_id) < 0) {
        printf("Failed to close group in single-image file\n");
        goto error;
    }
    group_id = H5I_INVALID_HID;

    if (H5Fclose(file_id) < 0) {
        printf("Failed to close single-image file\n");
        goto error;
    }

    /* Test 2: Multi-image file - should report NUM_IMAGES links (image0, image1, image2, ...) */
    if ((file_id = H5Fopen(multi_image_file, H5F_ACC_RDONLY, fapl_id)) < 0) {
        printf("Failed to open multi-image GeoTIFF file\n");
        goto error;
    }

    /* Open root group */
    if ((group_id = H5Gopen2(file_id, "/", H5P_DEFAULT)) < 0) {
        printf("Failed to open root group in multi-image file\n");
        goto error;
    }

    /* Get group info */
    if (H5Gget_info(group_id, &group_info) < 0) {
        printf("Failed to get group info for multi-image file\n");
        goto error;
    }

    /* Verify multi-image file has NUM_IMAGES links */
    if (group_info.nlinks != NUM_IMAGES) {
        printf("VERIFICATION FAILED: Multi-image file expected %u links, got %llu\n", NUM_IMAGES,
               (unsigned long long) group_info.nlinks);
        goto error;
    }

    /* Clean up */
    if (H5Gclose(group_id) < 0) {
        printf("Failed to close group in multi-image file\n");
        goto error;
    }
    group_id = H5I_INVALID_HID;

    if (H5Fclose(file_id) < 0) {
        printf("Failed to close multi-image file\n");
        goto error;
    }
    file_id = H5I_INVALID_HID;

    if (H5Pclose(fapl_id) < 0) {
        printf("Failed to close FAPL\n");
        goto error;
    }
    fapl_id = H5I_INVALID_HID;

    /* Unregister VOL connector */
    if (H5VLunregister_connector(vol_id) < 0) {
        printf("Failed to unregister VOL connector\n");
        goto error;
    }
    vol_id = H5I_INVALID_HID;

    /* Delete temporary test files */
    if (remove(single_image_file) != 0) {
        printf("WARNING: Failed to delete temporary file %s\n", single_image_file);
    }
    if (remove(multi_image_file) != 0) {
        printf("WARNING: Failed to delete temporary file %s\n", multi_image_file);
    }

    printf("PASSED\n");
    return 0;

error:
    /* Clean up on error */
    H5E_BEGIN_TRY
    {
        H5Gclose(group_id);
        H5Fclose(file_id);
        H5Pclose(fapl_id);
        if (vol_id != H5I_INVALID_HID)
            H5VLunregister_connector(vol_id);
    }
    H5E_END_TRY;

    /* Attempt to delete temporary files */
    remove(single_image_file);
    remove(multi_image_file);

    printf("FAILED\n");
    return 1;
}

/* Test reading tiled TIFF files through the VOL connector */
int TiledTIFFReadTest(const char *filename, int is_rgb)
{
    hid_t vol_id = H5I_INVALID_HID;
    hid_t fapl_id = H5I_INVALID_HID;
    hid_t file_id = H5I_INVALID_HID;
    hid_t dset_id = H5I_INVALID_HID;
    hid_t space_id = H5I_INVALID_HID;
    unsigned char *data = NULL;
    hsize_t dims[3];
    int ndims;
    uint32_t width = 512;
    uint32_t height = 512;
    uint32_t tile_width = 128;
    uint32_t tile_height = 128;

    printf("Testing tiled TIFF read (%s) with file: %s  ", is_rgb ? "RGB" : "grayscale", filename);

    /* Check if file exists, if not generate it */
    if (access(filename, F_OK) != 0) {
        printf("\nGenerating tiled TIFF file...\n");
        if (generate_tiled_tiff(filename, is_rgb, width, height, tile_width, tile_height) != 0) {
            printf("Failed to generate tiled TIFF file\n");
            goto error;
        }
    }

    /* Add the plugin path so HDF5 can find the connector */
#ifdef GEOTIFF_VOL_PLUGIN_PATH
    if (H5PLappend(GEOTIFF_VOL_PLUGIN_PATH) < 0) {
        printf("Failed to append plugin path\n");
        goto error;
    }
#endif

    /* Register the GeoTIFF VOL connector */
    if ((vol_id = H5VLregister_connector_by_name(GEOTIFF_VOL_CONNECTOR_NAME, H5P_DEFAULT)) < 0) {
        printf("Failed to register VOL connector\n");
        goto error;
    }

    /* Create file access property list */
    if ((fapl_id = H5Pcreate(H5P_FILE_ACCESS)) < 0) {
        printf("Failed to create FAPL\n");
        goto error;
    }

    /* Set the VOL connector */
    if (H5Pset_vol(fapl_id, vol_id, NULL) < 0) {
        printf("Failed to set VOL connector\n");
        goto error;
    }

    /* Open the GeoTIFF file */
    if ((file_id = H5Fopen(filename, H5F_ACC_RDONLY, fapl_id)) < 0) {
        printf("Failed to open GeoTIFF file\n");
        goto error;
    }

    /* Open the image dataset */
    if ((dset_id = H5Dopen2(file_id, "image0", H5P_DEFAULT)) < 0) {
        printf("Failed to open image dataset\n");
        goto error;
    }

    /* Get dataspace */
    if ((space_id = H5Dget_space(dset_id)) < 0) {
        printf("Failed to get dataspace\n");
        goto error;
    }

    /* Get dimensions */
    if ((ndims = H5Sget_simple_extent_ndims(space_id)) < 0) {
        printf("Failed to get number of dimensions\n");
        goto error;
    }

    if (H5Sget_simple_extent_dims(space_id, dims, NULL) < 0) {
        printf("Failed to get dimensions\n");
        goto error;
    }

    /* Verify dimensions */
    if (is_rgb) {
        if (ndims != 3 || dims[0] != height || dims[1] != width || dims[2] != 3) {
            printf("VERIFICATION FAILED: Expected dimensions %ux%ux3, got ", height, width);
            for (int i = 0; i < ndims; i++) {
                printf("%llu%s", (unsigned long long) dims[i], (i < ndims - 1) ? "x" : "");
            }
            printf("\n");
            goto error;
        }
    } else {
        if (ndims != 2 || dims[0] != height || dims[1] != width) {
            printf("VERIFICATION FAILED: Expected dimensions %ux%u, got ", height, width);
            for (int i = 0; i < ndims; i++) {
                printf("%llu%s", (unsigned long long) dims[i], (i < ndims - 1) ? "x" : "");
            }
            printf("\n");
            goto error;
        }
    }

    /* Allocate buffer and read data */
    size_t data_size = is_rgb ? (width * height * 3) : (width * height);
    data = (unsigned char *) malloc(data_size);
    if (!data) {
        printf("Failed to allocate read buffer\n");
        goto error;
    }

    if (H5Dread(dset_id, H5T_NATIVE_UCHAR, H5S_ALL, H5S_ALL, H5P_DEFAULT, data) < 0) {
        printf("Failed to read dataset\n");
        goto error;
    }

    /* Verify pixel data matches expected pattern (sample a few pixels) */
    int verification_failures = 0;
    for (uint32_t row = 0; row < height && verification_failures < 5; row += 64) {
        for (uint32_t col = 0; col < width && verification_failures < 5; col += 64) {
            if (is_rgb) {
                /* RGB pattern: R=row*8, G=col*8, B=(row+col)*4 */
                size_t idx = (row * width + col) * 3;
                unsigned char expected_r = (unsigned char) ((row * 8) % 256);
                unsigned char expected_g = (unsigned char) ((col * 8) % 256);
                unsigned char expected_b = (unsigned char) (((row + col) * 4) % 256);
                unsigned char actual_r = data[idx + 0];
                unsigned char actual_g = data[idx + 1];
                unsigned char actual_b = data[idx + 2];

                if (actual_r != expected_r || actual_g != expected_g || actual_b != expected_b) {
                    printf("VERIFICATION FAILED: Pixel[%u,%u] expected RGB(%u,%u,%u), got "
                           "RGB(%u,%u,%u)\n",
                           row, col, expected_r, expected_g, expected_b, actual_r, actual_g,
                           actual_b);
                    verification_failures++;
                }
            } else {
                /* Grayscale pattern: value = (row * 8 + col / 4) % 256 */
                size_t idx = row * width + col;
                unsigned char expected = (unsigned char) ((row * 8 + col / 4) % 256);
                unsigned char actual = data[idx];
                if (actual != expected) {
                    printf("VERIFICATION FAILED: Pixel[%u,%u] expected %u, got %u\n", row, col,
                           expected, actual);
                    verification_failures++;
                }
            }
        }
    }

    if (verification_failures > 0) {
        goto error;
    }

    /* Clean up - check return values */
    free(data);
    data = NULL;

    if (H5Sclose(space_id) < 0) {
        printf("Failed to close dataspace\n");
        goto error;
    }
    space_id = H5I_INVALID_HID;

    if (H5Dclose(dset_id) < 0) {
        printf("Failed to close dataset\n");
        goto error;
    }
    dset_id = H5I_INVALID_HID;

    if (H5Fclose(file_id) < 0) {
        printf("Failed to close file\n");
        goto error;
    }
    file_id = H5I_INVALID_HID;

    if (H5Pclose(fapl_id) < 0) {
        printf("Failed to close FAPL\n");
        goto error;
    }
    fapl_id = H5I_INVALID_HID;

    /* Unregister VOL connector */
    if (H5VLunregister_connector(vol_id) < 0) {
        printf("Failed to unregister VOL connector\n");
        goto error;
    }

    printf("PASSED\n");
    return 0;

error:
    /* Clean up in reverse order - no error checks */
    if (data)
        free(data);
    H5E_BEGIN_TRY
    {
        H5Sclose(space_id);
        H5Dclose(dset_id);
        H5Fclose(file_id);
        H5Pclose(fapl_id);
        if (vol_id != H5I_INVALID_HID)
            H5VLunregister_connector(vol_id);
    }
    H5E_END_TRY;

    printf("FAILED\n");
    return 1;
}

/* Helper to create a small GeoTIFF with geographic (lat/lon) coordinates */
static int CreateGeographicGeoTIFF(const char *filename)
{
    TIFF *tif = NULL;
    GTIF *gtif = NULL;
    unsigned char buffer[10]; /* 10x10 image */
    const uint32_t width = 10;
    const uint32_t height = 10;

    if ((tif = XTIFFOpen(filename, "w")) == NULL) {
        printf("Failed to create %s\\n", filename);
        return -1;
    }

    if ((gtif = GTIFNew(tif)) == NULL) {
        printf("Failed to create GeoTIFF handle for %s\\n", filename);
        TIFFClose(tif);
        return -1;
    }

    /* Set up TIFF tags */
    TIFFSetField(tif, TIFFTAG_IMAGEWIDTH, width);
    TIFFSetField(tif, TIFFTAG_IMAGELENGTH, height);
    TIFFSetField(tif, TIFFTAG_COMPRESSION, COMPRESSION_NONE);
    TIFFSetField(tif, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_MINISBLACK);
    TIFFSetField(tif, TIFFTAG_PLANARCONFIG, PLANARCONFIG_CONTIG);
    TIFFSetField(tif, TIFFTAG_BITSPERSAMPLE, 8);
    TIFFSetField(tif, TIFFTAG_SAMPLESPERPIXEL, 1);
    TIFFSetField(tif, TIFFTAG_ROWSPERSTRIP, height);

    /* Geographic coordinates: top-left at (-120°, 40°), 0.1° per pixel */
    const double tiepoints[6] = {0, 0, 0, -120.0, 40.0, 0.0};
    const double pixscale[3] = {0.1, 0.1, 0.0};
    TIFFSetField(tif, TIFFTAG_GEOTIEPOINTS, 6, tiepoints);
    TIFFSetField(tif, TIFFTAG_GEOPIXELSCALE, 3, pixscale);

    /* Set GeoKeys for geographic coordinate system (WGS84) */
    GTIFKeySet(gtif, GTModelTypeGeoKey, TYPE_SHORT, 1, ModelGeographic);
    GTIFKeySet(gtif, GTRasterTypeGeoKey, TYPE_SHORT, 1, RasterPixelIsArea);
    GTIFKeySet(gtif, GeographicTypeGeoKey, TYPE_SHORT, 1, GCS_WGS_84);

    /* Write simple gradient data */
    for (uint32_t row = 0; row < height; row++) {
        for (uint32_t col = 0; col < width; col++) {
            buffer[col] = (unsigned char) ((row + col) * 10);
        }
        if (!TIFFWriteScanline(tif, buffer, row, 0)) {
            printf("Failed to write scanline %u\\n", row);
            GTIFFree(gtif);
            TIFFClose(tif);
            return -1;
        }
    }

    GTIFWriteKeys(gtif);
    GTIFFree(gtif);
    XTIFFClose(tif);

    return 0;
}

/* Helper to create a small GeoTIFF with projected (UTM) coordinates */
static int CreateProjectedGeoTIFF(const char *filename)
{
    TIFF *tif = NULL;
    GTIF *gtif = NULL;
    unsigned char buffer[10]; /* 10x10 image */
    const uint32_t width = 10;
    const uint32_t height = 10;

    if ((tif = XTIFFOpen(filename, "w")) == NULL) {
        printf("Failed to create %s\n", filename);
        return -1;
    }

    if ((gtif = GTIFNew(tif)) == NULL) {
        printf("Failed to create GeoTIFF handle for %s\n", filename);
        TIFFClose(tif);
        return -1;
    }

    /* Set up TIFF tags */
    TIFFSetField(tif, TIFFTAG_IMAGEWIDTH, width);
    TIFFSetField(tif, TIFFTAG_IMAGELENGTH, height);
    TIFFSetField(tif, TIFFTAG_COMPRESSION, COMPRESSION_NONE);
    TIFFSetField(tif, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_MINISBLACK);
    TIFFSetField(tif, TIFFTAG_PLANARCONFIG, PLANARCONFIG_CONTIG);
    TIFFSetField(tif, TIFFTAG_BITSPERSAMPLE, 8);
    TIFFSetField(tif, TIFFTAG_SAMPLESPERPIXEL, 1);
    TIFFSetField(tif, TIFFTAG_ROWSPERSTRIP, height);

    /* UTM Zone 11N coordinates: top-left at (500000E, 4500000N), 100m per pixel */
    const double tiepoints[6] = {0, 0, 0, 500000.0, 4500000.0, 0.0};
    const double pixscale[3] = {100.0, 100.0, 0.0};
    TIFFSetField(tif, TIFFTAG_GEOTIEPOINTS, 6, tiepoints);
    TIFFSetField(tif, TIFFTAG_GEOPIXELSCALE, 3, pixscale);

    /* Set GeoKeys for UTM Zone 11N, WGS84 */
    GTIFKeySet(gtif, GTModelTypeGeoKey, TYPE_SHORT, 1, ModelProjected);
    GTIFKeySet(gtif, GTRasterTypeGeoKey, TYPE_SHORT, 1, RasterPixelIsArea);
    GTIFKeySet(gtif, ProjectedCSTypeGeoKey, TYPE_SHORT, 1, 32611); /* WGS84 UTM Zone 11N */

    /* Write simple gradient data */
    for (uint32_t row = 0; row < height; row++) {
        for (uint32_t col = 0; col < width; col++) {
            buffer[col] = (unsigned char) ((row + col) * 10);
        }
        if (!TIFFWriteScanline(tif, buffer, row, 0)) {
            printf("Failed to write scanline %u\n", row);
            GTIFFree(gtif);
            TIFFClose(tif);
            return -1;
        }
    }

    GTIFWriteKeys(gtif);
    GTIFFree(gtif);
    XTIFFClose(tif);

    return 0;
}

/* Test reading coordinates attribute from geographic GeoTIFF */
int CoordinatesAttributeGeographicTest(const char *unused)
{
    (void) unused;
    const char *filename = "_tmp_geographic_coords.tif";
    hid_t vol_id = H5I_INVALID_HID;
    hid_t fapl_id = H5I_INVALID_HID;
    hid_t file_id = H5I_INVALID_HID;
    hid_t dset_id = H5I_INVALID_HID;
    hid_t attr_id = H5I_INVALID_HID;
    hid_t space_id = H5I_INVALID_HID;
    hid_t type_id = H5I_INVALID_HID;

    typedef struct {
        double lon;
        double lat;
    } coord_t;

    coord_t *coords = NULL;
    const uint32_t width = 10;
    const uint32_t height = 10;

    printf("Testing coordinates attribute with geographic GeoTIFF...");

    /* Create test file */
    if (CreateGeographicGeoTIFF(filename) < 0) {
        printf("Failed to create test GeoTIFF\n");
        goto error;
    }

    /* Register VOL connector */
    if ((vol_id = H5VLregister_connector_by_name(GEOTIFF_VOL_CONNECTOR_NAME, H5P_DEFAULT)) < 0) {
        printf("Failed to register VOL connector\n");
        goto error;
    }

    /* Set up FAPL with GeoTIFF VOL */
    if ((fapl_id = H5Pcreate(H5P_FILE_ACCESS)) < 0) {
        printf("Failed to create FAPL\n");
        goto error;
    }

    if (H5Pset_vol(fapl_id, vol_id, NULL) < 0) {
        printf("Failed to set VOL connector\n");
        goto error;
    }

    /* Open file and dataset */
    if ((file_id = H5Fopen(filename, H5F_ACC_RDONLY, fapl_id)) < 0) {
        printf("Failed to open file\n");
        goto error;
    }

    if ((dset_id = H5Dopen(file_id, "image0", H5P_DEFAULT)) < 0) {
        printf("Failed to open dataset\n");
        goto error;
    }

    /* Open coordinates attribute */
    if ((attr_id = H5Aopen(dset_id, "coordinates", H5P_DEFAULT)) < 0) {
        printf("Failed to open coordinates attribute\n");
        goto error;
    }

    /* Verify attribute dataspace matches dataset */
    if ((space_id = H5Aget_space(attr_id)) < 0) {
        printf("Failed to get attribute dataspace\n");
        goto error;
    }

    hsize_t dims[2];
    if (H5Sget_simple_extent_dims(space_id, dims, NULL) < 0) {
        printf("Failed to get dataspace dimensions\n");
        goto error;
    }

    if (dims[0] != height || dims[1] != width) {
        printf("Attribute dimensions (%llu, %llu) don't match expected (%u, %u)\n",
               (unsigned long long) dims[0], (unsigned long long) dims[1], height, width);
        goto error;
    }

    /* Verify attribute type is compound with lon/lat */
    if ((type_id = H5Aget_type(attr_id)) < 0) {
        printf("Failed to get attribute type\n");
        goto error;
    }

    if (H5Tget_class(type_id) != H5T_COMPOUND) {
        printf("Attribute type is not compound\n");
        goto error;
    }

    if (H5Tget_nmembers(type_id) != 2) {
        printf("Compound type has %d members, expected 2\n", H5Tget_nmembers(type_id));
        goto error;
    }

    /* Allocate and read coordinates */
    coords = (coord_t *) malloc(width * height * sizeof(coord_t));
    if (!coords) {
        printf("Failed to allocate memory for coordinates\n");
        goto error;
    }

    if (H5Aread(attr_id, type_id, coords) < 0) {
        printf("Failed to read coordinates attribute\n");
        goto error;
    }

    /* Verify coordinates at specific pixels
     * Tiepoint: pixel (0,0) = (-120.0°, 40.0°)
     * Pixel scale: 0.1° per pixel
     * Note: Y increases downward in image, but latitude should decrease
     */
    const double epsilon = 1e-6;

    /* Check pixel (0, 0) - top-left corner */
    double expected_lon = -120.0;
    double expected_lat = 40.0;
    if (fabs(coords[0].lon - expected_lon) > epsilon ||
        fabs(coords[0].lat - expected_lat) > epsilon) {
        printf("Pixel (0,0): expected (%.6f, %.6f), got (%.6f, %.6f)\n", expected_lon, expected_lat,
               coords[0].lon, coords[0].lat);
        goto error;
    }

    /* Check pixel (0, 5) - middle of top row */
    expected_lon = -120.0 + 5 * 0.1; /* -119.5 */
    expected_lat = 40.0;
    size_t idx = 0 * width + 5;
    if (fabs(coords[idx].lon - expected_lon) > epsilon ||
        fabs(coords[idx].lat - expected_lat) > epsilon) {
        printf("Pixel (0,5): expected (%.6f, %.6f), got (%.6f, %.6f)\n", expected_lon, expected_lat,
               coords[idx].lon, coords[idx].lat);
        goto error;
    }

    /* Check pixel (5, 0) - middle of left column */
    expected_lon = -120.0;
    expected_lat = 40.0 - 5 * 0.1; /* 39.5 (Y increases down, lat decreases) */
    idx = 5 * width + 0;
    if (fabs(coords[idx].lon - expected_lon) > epsilon ||
        fabs(coords[idx].lat - expected_lat) > epsilon) {
        printf("Pixel (5,0): expected (%.6f, %.6f), got (%.6f, %.6f)\n", expected_lon, expected_lat,
               coords[idx].lon, coords[idx].lat);
        goto error;
    }

    /* Check pixel (9, 9) - bottom-right corner */
    expected_lon = -120.0 + 9 * 0.1; /* -119.1 */
    expected_lat = 40.0 - 9 * 0.1;   /* 39.1 */
    idx = 9 * width + 9;
    if (fabs(coords[idx].lon - expected_lon) > epsilon ||
        fabs(coords[idx].lat - expected_lat) > epsilon) {
        printf("Pixel (9,9): expected (%.6f, %.6f), got (%.6f, %.6f)\n", expected_lon, expected_lat,
               coords[idx].lon, coords[idx].lat);
        goto error;
    }

    /* Clean up */
    free(coords);
    H5Tclose(type_id);
    H5Sclose(space_id);
    H5Aclose(attr_id);
    H5Dclose(dset_id);
    H5Fclose(file_id);
    H5Pclose(fapl_id);
    H5VLunregister_connector(vol_id);
    unlink(filename);

    printf("PASSED\n");
    return 0;

error:
    if (coords)
        free(coords);
    H5E_BEGIN_TRY
    {
        H5Tclose(type_id);
        H5Sclose(space_id);
        H5Aclose(attr_id);
        H5Dclose(dset_id);
        H5Fclose(file_id);
        H5Pclose(fapl_id);
        if (vol_id != H5I_INVALID_HID)
            H5VLunregister_connector(vol_id);
    }
    H5E_END_TRY;
    unlink(filename);

    printf("FAILED\n");
    return 1;
}

/* Test reading coordinates attribute from projected GeoTIFF (requires conversion) */
int CoordinatesAttributeProjectedTest(const char *unused)
{
    (void) unused;
    const char *filename = "_tmp_projected_coords.tif";
    hid_t vol_id = H5I_INVALID_HID;
    hid_t fapl_id = H5I_INVALID_HID;
    hid_t file_id = H5I_INVALID_HID;
    hid_t dset_id = H5I_INVALID_HID;
    hid_t attr_id = H5I_INVALID_HID;
    hid_t type_id = H5I_INVALID_HID;

    typedef struct {
        double lon;
        double lat;
    } coord_t;

    coord_t *coords = NULL;
    const uint32_t width = 10;
    const uint32_t height = 10;

    printf("Testing coordinates attribute with projected (UTM) GeoTIFF...");

    /* Create test file */
    if (CreateProjectedGeoTIFF(filename) < 0) {
        printf("Failed to create test GeoTIFF\n");
        goto error;
    }

    /* Register VOL connector */
    if ((vol_id = H5VLregister_connector_by_name(GEOTIFF_VOL_CONNECTOR_NAME, H5P_DEFAULT)) < 0) {
        printf("Failed to register VOL connector\n");
        goto error;
    }

    /* Set up FAPL with GeoTIFF VOL */
    if ((fapl_id = H5Pcreate(H5P_FILE_ACCESS)) < 0) {
        printf("Failed to create FAPL\n");
        goto error;
    }

    if (H5Pset_vol(fapl_id, vol_id, NULL) < 0) {
        printf("Failed to set VOL connector\n");
        goto error;
    }

    /* Open file and dataset */
    if ((file_id = H5Fopen(filename, H5F_ACC_RDONLY, fapl_id)) < 0) {
        printf("Failed to open file\n");
        goto error;
    }

    if ((dset_id = H5Dopen(file_id, "image0", H5P_DEFAULT)) < 0) {
        printf("Failed to open dataset\n");
        goto error;
    }

    /* Open coordinates attribute */
    if ((attr_id = H5Aopen(dset_id, "coordinates", H5P_DEFAULT)) < 0) {
        printf("Failed to open coordinates attribute\n");
        goto error;
    }

    /* Get attribute type */
    if ((type_id = H5Aget_type(attr_id)) < 0) {
        printf("Failed to get attribute type\n");
        goto error;
    }

    /* Allocate and read coordinates */
    coords = (coord_t *) malloc(width * height * sizeof(coord_t));
    if (!coords) {
        printf("Failed to allocate memory for coordinates\n");
        goto error;
    }

    if (H5Aread(attr_id, type_id, coords) < 0) {
        printf("Failed to read coordinates attribute\n");
        goto error;
    }

    /* Verify coordinates are reasonable for UTM Zone 11N
     * UTM Zone 11N is roughly -120° to -114° longitude, 0° to 84° latitude
     * Our test data is at approximately:
     * - Easting 500000m = center of zone = approximately -117° longitude
     * - Northing 4500000m = approximately 40.6° latitude
     */

    /* Check that coordinates are in reasonable ranges and not NaN */
    for (uint32_t row = 0; row < height; row++) {
        for (uint32_t col = 0; col < width; col++) {
            size_t idx = row * width + col;

            if (isnan(coords[idx].lon) || isnan(coords[idx].lat)) {
                printf("Pixel (%u,%u): got NaN coordinates\n", row, col);
                goto error;
            }

            /* Rough bounds check for UTM Zone 11N */
            if (coords[idx].lon < -121.0 || coords[idx].lon > -113.0) {
                printf("Pixel (%u,%u): longitude %.6f out of expected range [-121, -113]\n", row,
                       col, coords[idx].lon);
                goto error;
            }

            if (coords[idx].lat < 40.0 || coords[idx].lat > 41.0) {
                printf("Pixel (%u,%u): latitude %.6f out of expected range [40, 41]\n", row, col,
                       coords[idx].lat);
                goto error;
            }
        }
    }

    /* Verify that longitude increases with column */
    if (coords[0 * width + 9].lon <= coords[0 * width + 0].lon) {
        printf("Expected longitude to increase from west to east\n");
        goto error;
    }

    /* Verify that latitude decreases with row (Y increases down) */
    if (coords[9 * width + 0].lat >= coords[0 * width + 0].lat) {
        printf("Expected latitude to decrease from north to south\n");
        goto error;
    }

    /* Clean up */
    free(coords);
    H5Tclose(type_id);
    H5Aclose(attr_id);
    H5Dclose(dset_id);
    H5Fclose(file_id);
    H5Pclose(fapl_id);
    H5VLunregister_connector(vol_id);
    unlink(filename);

    printf("PASSED\n");
    return 0;

error:
    if (coords)
        free(coords);
    H5E_BEGIN_TRY
    {
        H5Tclose(type_id);
        H5Aclose(attr_id);
        H5Dclose(dset_id);
        H5Fclose(file_id);
        H5Pclose(fapl_id);
        if (vol_id != H5I_INVALID_HID)
            H5VLunregister_connector(vol_id);
    }
    H5E_END_TRY;
    unlink(filename);

    printf("FAILED\n");
    return 1;
}

/* Reference counting tests - verify out-of-order closing works correctly */

/* Test: Close file before dataset */
int RefCountCloseFileBeforeDatasetTest(const char *filename)
{
    hid_t vol_id = H5I_INVALID_HID;
    hid_t fapl_id = H5I_INVALID_HID;
    hid_t file_id = H5I_INVALID_HID;
    hid_t dset_id = H5I_INVALID_HID;
    hid_t space_id = H5I_INVALID_HID;
    unsigned char *data = NULL;
    hsize_t dims[3];
    int ndims;

    printf("Testing close file before dataset...  ");

#ifdef GEOTIFF_VOL_PLUGIN_PATH
    if (H5PLappend(GEOTIFF_VOL_PLUGIN_PATH) < 0) {
        printf("FAILED: Could not append plugin path\n");
        goto error;
    }
#endif

    if ((vol_id = H5VLregister_connector_by_name(GEOTIFF_VOL_CONNECTOR_NAME, H5P_DEFAULT)) < 0) {
        printf("FAILED: Could not register VOL connector\n");
        goto error;
    }

    if ((fapl_id = H5Pcreate(H5P_FILE_ACCESS)) < 0) {
        printf("FAILED: Could not create FAPL\n");
        goto error;
    }

    if (H5Pset_vol(fapl_id, vol_id, NULL) < 0) {
        printf("FAILED: Could not set VOL connector\n");
        goto error;
    }

    if ((file_id = H5Fopen(filename, H5F_ACC_RDONLY, fapl_id)) < 0) {
        printf("FAILED: Could not open file\n");
        goto error;
    }

    if ((dset_id = H5Dopen2(file_id, "/image0", H5P_DEFAULT)) < 0) {
        printf("FAILED: Could not open dataset\n");
        goto error;
    }

    /* Close file BEFORE dataset (out of order) */
    if (H5Fclose(file_id) < 0) {
        printf("FAILED: Could not close file\n");
        goto error;
    }
    file_id = H5I_INVALID_HID;

    /* Dataset should still be accessible - get dataspace */
    if ((space_id = H5Dget_space(dset_id)) < 0) {
        printf("FAILED: Could not get dataspace after file close\n");
        goto error;
    }

    if ((ndims = H5Sget_simple_extent_dims(space_id, dims, NULL)) < 0) {
        printf("FAILED: Could not get dimensions after file close\n");
        goto error;
    }

    if (ndims != 2 || dims[0] != HEIGHT || dims[1] != WIDTH) {
        printf("FAILED: Incorrect dimensions after file close\n");
        goto error;
    }

    /* Read data */
    data = (unsigned char *) malloc(HEIGHT * WIDTH * sizeof(unsigned char));
    if (H5Dread(dset_id, H5T_NATIVE_UCHAR, H5S_ALL, H5S_ALL, H5P_DEFAULT, data) < 0) {
        printf("FAILED: Could not read dataset after file close\n");
        goto error;
    }

    /* Verify some data */
    if (data[0] != 0 || data[WIDTH - 1] != 7) {
        printf("FAILED: Data mismatch after file close\n");
        goto error;
    }

    /* Clean up */
    free(data);
    H5Sclose(space_id);
    H5Dclose(dset_id);
    H5Pclose(fapl_id);
    H5VLunregister_connector(vol_id);

    printf("PASSED\n");
    return 0;

error:
    H5E_BEGIN_TRY
    {
        if (data)
            free(data);
        H5Sclose(space_id);
        H5Dclose(dset_id);
        H5Fclose(file_id);
        H5Pclose(fapl_id);
        if (vol_id != H5I_INVALID_HID)
            H5VLunregister_connector(vol_id);
    }
    H5E_END_TRY;
    return -1;
}

#define REFCOUNT_ATTR_TEST_FILENAME "refcount_test.tif"

/* Test: Close dataset before attribute */
int RefCountCloseDatasetBeforeAttributeTest(void)
{
    hid_t vol_id = H5I_INVALID_HID;
    hid_t fapl_id = H5I_INVALID_HID;
    hid_t file_id = H5I_INVALID_HID;
    hid_t dset_id = H5I_INVALID_HID;
    hid_t attr_id = H5I_INVALID_HID;
    hid_t space_id = H5I_INVALID_HID;
    hsize_t dims[3];
    int ndims;

    printf("Testing close dataset before attribute...  ");

#ifdef GEOTIFF_VOL_PLUGIN_PATH
    if (H5PLappend(GEOTIFF_VOL_PLUGIN_PATH) < 0) {
        printf("FAILED: Could not append plugin path\n");
        goto error;
    }
#endif

    if ((vol_id = H5VLregister_connector_by_name(GEOTIFF_VOL_CONNECTOR_NAME, H5P_DEFAULT)) < 0) {
        printf("FAILED: Could not register VOL connector\n");
        goto error;
    }

    if ((fapl_id = H5Pcreate(H5P_FILE_ACCESS)) < 0) {
        printf("FAILED: Could not create FAPL\n");
        goto error;
    }

    if (H5Pset_vol(fapl_id, vol_id, NULL) < 0) {
        printf("FAILED: Could not set VOL connector\n");
        goto error;
    }

    /* Create a GeoTIFF file with the coordinates attribute */
    if (CreateGeographicGeoTIFF(REFCOUNT_ATTR_TEST_FILENAME) < 0) {
        printf("FAILED: Could not create test GeoTIFF file\n");
        goto error;
    }

    if ((file_id = H5Fopen(REFCOUNT_ATTR_TEST_FILENAME, H5F_ACC_RDONLY, fapl_id)) < 0) {
        printf("FAILED: Could not open file\n");
        goto error;
    }

    if ((dset_id = H5Dopen2(file_id, "/image0", H5P_DEFAULT)) < 0) {
        printf("FAILED: Could not open dataset\n");
        goto error;
    }

    if ((attr_id = H5Aopen(dset_id, "coordinates", H5P_DEFAULT)) < 0) {
        printf("FAILED: Could not open attribute\n");
        goto error;
    }

    /* Close dataset BEFORE attribute (out of order) */
    if (H5Dclose(dset_id) < 0) {
        printf("FAILED: Could not close dataset\n");
        goto error;
    }
    dset_id = H5I_INVALID_HID;

    /* Attribute should still be accessible - get dataspace */
    if ((space_id = H5Aget_space(attr_id)) < 0) {
        printf("FAILED: Could not get attribute dataspace after dataset close\n");
        goto error;
    }

    if ((ndims = H5Sget_simple_extent_dims(space_id, dims, NULL)) < 0) {
        printf("FAILED: Could not get attribute dimensions after dataset close\n");
        goto error;
    }

    /* CreateGeographicGeoTIFF creates a 10x10 image */
    if (ndims != 2 || dims[0] != 10 || dims[1] != 10) {
        printf("FAILED: Incorrect attribute dimensions after dataset close (expected 10x10, got "
               "%llux%llu)\n",
               (unsigned long long) dims[0], (unsigned long long) dims[1]);
        goto error;
    }

    /* Clean up */
    H5Sclose(space_id);
    H5Aclose(attr_id);
    H5Fclose(file_id);
    H5Pclose(fapl_id);
    H5VLunregister_connector(vol_id);

    printf("PASSED\n");
    return 0;

error:
    H5E_BEGIN_TRY
    {
        H5Sclose(space_id);
        H5Aclose(attr_id);
        H5Dclose(dset_id);
        H5Fclose(file_id);
        H5Pclose(fapl_id);
        if (vol_id != H5I_INVALID_HID)
            H5VLunregister_connector(vol_id);
    }
    H5E_END_TRY;
    return -1;
}

/* Test: Close file with multiple open children */
int RefCountCloseFileWithMultipleChildrenTest(const char *filename)
{
    hid_t vol_id = H5I_INVALID_HID;
    hid_t fapl_id = H5I_INVALID_HID;
    hid_t file_id = H5I_INVALID_HID;
    hid_t dset_id = H5I_INVALID_HID;
    hid_t attr_id = H5I_INVALID_HID;
    unsigned char *data = NULL;

    printf("Testing close file with multiple open children...  ");

#ifdef GEOTIFF_VOL_PLUGIN_PATH
    if (H5PLappend(GEOTIFF_VOL_PLUGIN_PATH) < 0) {
        printf("FAILED: Could not append plugin path\n");
        goto error;
    }
#endif

    if ((vol_id = H5VLregister_connector_by_name(GEOTIFF_VOL_CONNECTOR_NAME, H5P_DEFAULT)) < 0) {
        printf("FAILED: Could not register VOL connector\n");
        goto error;
    }

    if ((fapl_id = H5Pcreate(H5P_FILE_ACCESS)) < 0) {
        printf("FAILED: Could not create FAPL\n");
        goto error;
    }

    if (H5Pset_vol(fapl_id, vol_id, NULL) < 0) {
        printf("FAILED: Could not set VOL connector\n");
        goto error;
    }

    if ((file_id = H5Fopen(filename, H5F_ACC_RDONLY, fapl_id)) < 0) {
        printf("FAILED: Could not open file\n");
        goto error;
    }

    if ((dset_id = H5Dopen2(file_id, "/image0", H5P_DEFAULT)) < 0) {
        printf("FAILED: Could not open dataset\n");
        goto error;
    }

    if ((attr_id = H5Aopen(dset_id, "coordinates", H5P_DEFAULT)) < 0) {
        printf("FAILED: Could not open attribute\n");
        goto error;
    }

    /* Close file FIRST (out of order - dataset and attribute still open) */
    if (H5Fclose(file_id) < 0) {
        printf("FAILED: Could not close file\n");
        goto error;
    }
    file_id = H5I_INVALID_HID;

    /* Dataset should still be readable */
    data = (unsigned char *) malloc(HEIGHT * WIDTH * sizeof(unsigned char));
    if (H5Dread(dset_id, H5T_NATIVE_UCHAR, H5S_ALL, H5S_ALL, H5P_DEFAULT, data) < 0) {
        printf("FAILED: Could not read dataset after file close\n");
        goto error;
    }

    if (data[0] != 0 || data[WIDTH - 1] != 7) {
        printf("FAILED: Data mismatch after file close\n");
        goto error;
    }

    /* Clean up in arbitrary order */
    free(data);
    H5Aclose(attr_id);
    H5Dclose(dset_id);
    H5Pclose(fapl_id);
    H5VLunregister_connector(vol_id);

    printf("PASSED\n");
    return 0;

error:
    H5E_BEGIN_TRY
    {
        if (data)
            free(data);
        H5Aclose(attr_id);
        H5Dclose(dset_id);
        H5Fclose(file_id);
        H5Pclose(fapl_id);
        if (vol_id != H5I_INVALID_HID)
            H5VLunregister_connector(vol_id);
    }
    H5E_END_TRY;
    return -1;
}

/*
 * Comprehensive test for real GeoTIFF files
 * Tests all basic VOL operations generically on any GeoTIFF file
 */
int RealFileComprehensiveTest(const char *filename)
{
    hid_t vol_id = H5I_INVALID_HID;
    hid_t fapl_id = H5I_INVALID_HID;
    hid_t file_id = H5I_INVALID_HID;
    hid_t dset_id = H5I_INVALID_HID;
    hid_t space_id = H5I_INVALID_HID;
    hid_t type_id = H5I_INVALID_HID;
    hid_t attr_id = H5I_INVALID_HID;
    hid_t group_id = H5I_INVALID_HID;
    hsize_t dims[3];
    int ndims;
    unsigned char *data = NULL;
    int num_images = 0;

    printf("Testing comprehensive VOL operations on real file: %s\n", filename);

    /* Add the plugin path so HDF5 can find the connector */
#ifdef GEOTIFF_VOL_PLUGIN_PATH
    if (H5PLappend(GEOTIFF_VOL_PLUGIN_PATH) < 0) {
        printf("  FAILED: Could not append plugin path\n");
        goto error;
    }
#endif

    /* Register the GeoTIFF VOL connector */
    if ((vol_id = H5VLregister_connector_by_name(GEOTIFF_VOL_CONNECTOR_NAME, H5P_DEFAULT)) < 0) {
        printf("  FAILED: Could not register VOL connector\n");
        goto error;
    }

    /* Create file access property list */
    if ((fapl_id = H5Pcreate(H5P_FILE_ACCESS)) < 0) {
        printf("  FAILED: Could not create FAPL\n");
        goto error;
    }

    /* Set the VOL connector */
    if (H5Pset_vol(fapl_id, vol_id, NULL) < 0) {
        printf("  FAILED: Could not set VOL connector\n");
        goto error;
    }

    /* Open the GeoTIFF file */
    printf("  Opening file...\n");
    if ((file_id = H5Fopen(filename, H5F_ACC_RDONLY, fapl_id)) < 0) {
        printf("  FAILED: Could not open GeoTIFF file\n");
        goto error;
    }

    /* Open root group and get info to count images */
    printf("  Counting images in file...\n");
    if ((group_id = H5Gopen2(file_id, "/", H5P_DEFAULT)) < 0) {
        printf("  FAILED: Could not open root group\n");
        goto error;
    }

    /* Use H5Gget_info to get number of links (images) */
    H5G_info_t group_info;
    if (H5Gget_info(group_id, &group_info) < 0) {
        printf("  FAILED: Could not get group info\n");
        goto error;
    }

    num_images = (int) group_info.nlinks;

    if (num_images == 0) {
        printf("  FAILED: No images found in file\n");
        goto error;
    }

    /* Test the first image */
    printf("  Testing image0...\n");
    if ((dset_id = H5Dopen2(file_id, "image0", H5P_DEFAULT)) < 0) {
        printf("  FAILED: Could not open image0 dataset\n");
        goto error;
    }

    /* Get dataspace to determine dimensions */
    printf("  Getting image dimensions...\n");
    if ((space_id = H5Dget_space(dset_id)) < 0) {
        printf("  FAILED: Could not get dataspace\n");
        goto error;
    }

    if ((ndims = H5Sget_simple_extent_ndims(space_id)) < 0) {
        printf("  FAILED: Could not get number of dimensions\n");
        goto error;
    }

    if (ndims < 2 || ndims > 3) {
        printf("  FAILED: Unexpected number of dimensions: %d (expected 2 or 3)\n", ndims);
        goto error;
    }

    if (H5Sget_simple_extent_dims(space_id, dims, NULL) < 0) {
        printf("  FAILED: Could not get dimensions\n");
        goto error;
    }

    /* Determine format: grayscale, grayscale+alpha, RGB, or RGBA */
    int is_grayscale = (ndims == 2);
    int is_grayscale_alpha = (ndims == 3 && dims[2] == 2);
    int is_rgb = (ndims == 3 && dims[2] == 3);
    int is_rgba = (ndims == 3 && dims[2] == 4);

    if (!is_grayscale && !is_grayscale_alpha && !is_rgb && !is_rgba) {
        printf("  FAILED: Unsupported image format (dims: ");
        for (int i = 0; i < ndims; i++) {
            printf("%llu%s", (unsigned long long) dims[i], (i < ndims - 1) ? "x" : "");
        }
        printf(")\n");
        goto error;
    }

    /* Image format detected (no output unless failure) */
    (void) is_grayscale;
    (void) is_grayscale_alpha;
    (void) is_rgb;
    (void) is_rgba;

    /* Get datatype */
    if ((type_id = H5Dget_type(dset_id)) < 0) {
        printf("  FAILED: Could not get datatype\n");
        goto error;
    }

    /* Verify it's a supported type (should be unsigned char) */
    H5T_class_t type_class = H5Tget_class(type_id);

    if (type_class != H5T_INTEGER) {
        printf("  FAILED: Unexpected datatype class (expected integer)\n");
        goto error;
    }

    /* Allocate buffer based on dimensions and read data */
    size_t data_size;
    if (is_grayscale) {
        data_size = dims[0] * dims[1];
    } else {
        data_size = dims[0] * dims[1] * dims[2];
    }

    data = (unsigned char *) malloc(data_size * sizeof(unsigned char));
    if (!data) {
        printf("  FAILED: Could not allocate read buffer of size %zu\n", data_size);
        goto error;
    }

    if (H5Dread(dset_id, H5T_NATIVE_UCHAR, H5S_ALL, H5S_ALL, H5P_DEFAULT, data) < 0) {
        printf("  FAILED: Could not read dataset\n");
        goto error;
    }

    /* Check if file has spatial metadata using libgeotiff directly */
    printf("  Checking for spatial metadata...\n");
    TIFF *tif_check = XTIFFOpen(filename, "r");
    GTIF *gtif_check = NULL;
    int has_spatial_data = 0;

    if (tif_check) {
        gtif_check = GTIFNew(tif_check);
        if (gtif_check) {
            /* Use GTIFGetDefn to properly check if we have complete geotransform data */
            GTIFDefn defn;
            if (GTIFGetDefn(gtif_check, &defn)) {
                /* Also verify we can actually convert coordinates */
                double x = 0.0, y = 0.0;
                if (GTIFImageToPCS(gtif_check, &x, &y)) {
                    has_spatial_data = 1;
                    printf(
                        "  INFO: File has complete geotransform data (can compute coordinates)\n");
                } else {
                    printf("  INFO: File has GeoTIFF definition but ImageToPCS transformation "
                           "failed\n");
                }
            } else {
                printf("  INFO: File has no complete GeoTIFF definition (cannot compute "
                       "coordinates)\n");
            }
            GTIFFree(gtif_check);
        }
        XTIFFClose(tif_check);
    }

    /* Try to open coordinates attribute - it's always present for image datasets */
    printf("  Checking coordinates attribute...\n");
    H5E_BEGIN_TRY
    {
        attr_id = H5Aopen(dset_id, "coordinates", H5P_DEFAULT);
    }
    H5E_END_TRY;

    if (attr_id < 0) {
        printf("  FAILED: Coordinates attribute should always exist for image datasets\n");
        goto error;
    }

    /* Get attribute dataspace to determine size */
    hid_t attr_space = H5Aget_space(attr_id);
    if (attr_space < 0) {
        printf("  FAILED: Could not get coordinates attribute dataspace\n");
        goto error;
    }

    hssize_t num_elements = H5Sget_simple_extent_npoints(attr_space);
    /* Coordinates are always per-pixel: height * width, regardless of samples per pixel */
    hssize_t expected_coord_count = (hssize_t) (dims[0] * dims[1]);

    if (num_elements != expected_coord_count) {
        printf("  FAILED: Coordinates attribute has wrong number of elements (expected %lld, "
               "got %lld)\n",
               (long long) expected_coord_count, (long long) num_elements);
        H5Sclose(attr_space);
        goto error;
    }

    /* Allocate and read coordinates (lon/lat pairs as compound type) */
    typedef struct {
        double lon;
        double lat;
    } coord_pair_t;

    coord_pair_t *coords = (coord_pair_t *) malloc((size_t) num_elements * sizeof(coord_pair_t));
    if (!coords) {
        printf("  FAILED: Could not allocate memory for coordinates\n");
        H5Sclose(attr_space);
        goto error;
    }

    /* Get the attribute's type directly */
    hid_t attr_type = H5Aget_type(attr_id);
    if (attr_type < 0) {
        printf("  FAILED: Could not get coordinates attribute type\n");
        free(coords);
        H5Sclose(attr_space);
        goto error;
    }

    /* Check if it's a compound type and print its structure */
    H5T_class_t attr_type_class = H5Tget_class(attr_type);
    if (attr_type_class == H5T_COMPOUND) {
        int nmembers = H5Tget_nmembers(attr_type);
        for (int i = 0; i < nmembers; i++) {
            char *member_name = H5Tget_member_name(attr_type, (unsigned) i);
            H5free_memory(member_name);
        }
    }

    /* Try to read the coordinates using the attribute's own type */
    herr_t read_status;
    read_status = H5Aread(attr_id, attr_type, coords);

    H5Tclose(attr_type);

    if (read_status < 0) {
        if (has_spatial_data) {
            printf("  FAILED: File has geotransform but reading coordinates failed\n");
            printf("  Printing HDF5 error stack:\n");
            H5Eprint2(H5E_DEFAULT, stderr);
            free(coords);
            H5Sclose(attr_space);
            goto error;
        } else {
        }
    } else {
        if (!has_spatial_data) {
            printf(
                "  WARNING: Reading coordinates succeeded even though file has no geotransform\n");
        } else {

            /* Print first few coordinate pairs for verification */
            int print_count = (num_elements < 3) ? (int) num_elements : 3;
            printf("  First coordinates: ");
            for (int i = 0; i < print_count; i++) {
                printf("(%.6f, %.6f)%s", coords[i].lon, coords[i].lat,
                       (i < print_count - 1) ? ", " : "");
            }
            if (num_elements > 3) {
                printf("...");
            }
            printf("\n");
        }
    }

    free(coords);
    H5Sclose(attr_space);
    H5Aclose(attr_id);
    attr_id = H5I_INVALID_HID;

    /* Clean up */
    free(data);
    data = NULL;

    if (H5Tclose(type_id) < 0) {
        printf("  FAILED: Could not close datatype\n");
        goto error;
    }
    type_id = H5I_INVALID_HID;

    if (H5Sclose(space_id) < 0) {
        printf("  FAILED: Could not close dataspace\n");
        goto error;
    }
    space_id = H5I_INVALID_HID;

    if (H5Dclose(dset_id) < 0) {
        printf("  FAILED: Could not close dataset\n");
        goto error;
    }
    dset_id = H5I_INVALID_HID;

    if (H5Gclose(group_id) < 0) {
        printf("  FAILED: Could not close group\n");
        goto error;
    }
    group_id = H5I_INVALID_HID;

    if (H5Fclose(file_id) < 0) {
        printf("  FAILED: Could not close file\n");
        goto error;
    }
    file_id = H5I_INVALID_HID;

    if (H5Pclose(fapl_id) < 0) {
        printf("  FAILED: Could not close FAPL\n");
        goto error;
    }
    fapl_id = H5I_INVALID_HID;

    if (H5VLunregister_connector(vol_id) < 0) {
        printf("  FAILED: Could not unregister VOL connector\n");
        goto error;
    }
    vol_id = H5I_INVALID_HID;

    return 0;

error:
    H5E_BEGIN_TRY
    {
        if (data)
            free(data);
        H5Aclose(attr_id);
        H5Tclose(type_id);
        H5Sclose(space_id);
        H5Dclose(dset_id);
        H5Gclose(group_id);
        H5Fclose(file_id);
        H5Pclose(fapl_id);
        if (vol_id != H5I_INVALID_HID)
            H5VLunregister_connector(vol_id);
    }
    H5E_END_TRY;
    return -1;
}

/* Test: num_images attribute on file object */
int NumImagesAttributeTest(void)
{
    hid_t vol_id = H5I_INVALID_HID;
    hid_t fapl_id = H5I_INVALID_HID;
    hid_t file_id = H5I_INVALID_HID;
    hid_t attr_id = H5I_INVALID_HID;
    uint64_t num_images;
    const char *single_file = "test_num_images_single.tif";
    const char *multi_file = "test_num_images_multi.tif";

    printf("Testing num_images attribute on file object...  ");

    /* Create test files */
    if (CreateGrayscaleGeoTIFF(single_file) != 0) {
        printf("FAILED: Could not create single-image test file\n");
        goto error;
    }

    if (CreateMultiImageGeoTIFF(multi_file, 3) != 0) {
        printf("FAILED: Could not create multi-image test file\n");
        goto error;
    }

#ifdef GEOTIFF_VOL_PLUGIN_PATH
    if (H5PLappend(GEOTIFF_VOL_PLUGIN_PATH) < 0) {
        printf("FAILED: Could not append plugin path\n");
        goto error;
    }
#endif

    if ((vol_id = H5VLregister_connector_by_name(GEOTIFF_VOL_CONNECTOR_NAME, H5P_DEFAULT)) < 0) {
        printf("FAILED: Could not register VOL connector\n");
        goto error;
    }

    if ((fapl_id = H5Pcreate(H5P_FILE_ACCESS)) < 0) {
        printf("FAILED: Could not create FAPL\n");
        goto error;
    }

    if (H5Pset_vol(fapl_id, vol_id, NULL) < 0) {
        printf("FAILED: Could not set VOL connector\n");
        goto error;
    }

    /* Test single-image file */
    if ((file_id = H5Fopen(single_file, H5F_ACC_RDONLY, fapl_id)) < 0) {
        printf("FAILED: Could not open single-image file\n");
        goto error;
    }

    if ((attr_id = H5Aopen(file_id, "num_images", H5P_DEFAULT)) < 0) {
        printf("FAILED: Could not open num_images attribute on single-image file\n");
        goto error;
    }

    if (H5Aread(attr_id, H5T_NATIVE_UINT64, &num_images) < 0) {
        printf("FAILED: Could not read num_images attribute\n");
        goto error;
    }

    if (num_images != 1) {
        printf("FAILED: Expected 1 image in single-image file, got %llu\n",
               (unsigned long long) num_images);
        goto error;
    }

    if (H5Aclose(attr_id) < 0) {
        printf("FAILED: Could not close num_images attribute\n");
        goto error;
    }

    if (H5Fclose(file_id) < 0) {
        printf("FAILED: Could not close single-image file\n");
        goto error;
    }

    /* Test multi-image file */
    if ((file_id = H5Fopen(multi_file, H5F_ACC_RDONLY, fapl_id)) < 0) {
        printf("FAILED: Could not open multi-image file\n");
        goto error;
    }

    if ((attr_id = H5Aopen(file_id, "num_images", H5P_DEFAULT)) < 0) {
        printf("FAILED: Could not open num_images attribute on multi-image file\n");
        goto error;
    }

    if (H5Aread(attr_id, H5T_NATIVE_UINT64, &num_images) < 0) {
        printf("FAILED: Could not read num_images attribute\n");
        goto error;
    }

    if (num_images != 3) {
        printf("FAILED: Expected 3 images in multi-image file, got %llu\n",
               (unsigned long long) num_images);
        goto error;
    }

    /* Clean up */
    if (H5Aclose(attr_id) < 0) {
        printf("FAILED: Could not close num_images attribute\n");
        goto error;
    }
    if (H5Fclose(file_id) < 0) {
        printf("FAILED: Could not close multi-image file\n");
        goto error;
    }
    if (H5Pclose(fapl_id) < 0) {
        printf("FAILED: Could not close FAPL\n");
        goto error;
    }
    if (H5VLunregister_connector(vol_id) < 0) {
        printf("FAILED: Could not unregister VOL connector\n");
        goto error;
    }

    printf("PASSED\n");
    return 0;

error:
    H5E_BEGIN_TRY
    {
        H5Aclose(attr_id);
        H5Fclose(file_id);
        H5Pclose(fapl_id);
        if (vol_id != H5I_INVALID_HID)
            H5VLunregister_connector(vol_id);
    }
    H5E_END_TRY;
    return -1;
}
