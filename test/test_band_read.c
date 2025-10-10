/*
 * Test program for GeoTIFF VOL connector band reading
 * Tests reading individual bands from a GeoTIFF file
 */

#include "template_vol_connector.h"
#include <hdf5.h>
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv)
{
    hid_t fapl_id, file_id, vol_id;
    hid_t dset_id, space_id, mem_space_id, file_space_id;
    hsize_t dims[3], start[3], count[3];
    int ndims;
    herr_t ret;
    unsigned char *full_data = NULL;
    unsigned char *band_data = NULL;
    size_t data_size;

    if (argc != 2) {
        printf("Usage: %s <geotiff_file>\n", argv[0]);
        return 1;
    }

    printf("Testing GeoTIFF VOL connector band reading with file: %s\n", argv[1]);

    /* Register the GeoTIFF VOL connector */
    vol_id = H5VLregister_connector_by_name(GEOTIFF_VOL_CONNECTOR_NAME, H5P_DEFAULT);
    if (vol_id < 0) {
        printf("Failed to register GeoTIFF VOL connector\n");
        return 1;
    }

    /* Create file access property list */
    fapl_id = H5Pcreate(H5P_FILE_ACCESS);
    if (fapl_id < 0) {
        printf("Failed to create FAPL\n");
        H5VLunregister_connector(vol_id);
        return 1;
    }

    /* Set the VOL connector */
    ret = H5Pset_vol(fapl_id, vol_id, NULL);
    if (ret < 0) {
        printf("Failed to set VOL connector\n");
        H5Pclose(fapl_id);
        H5VLunregister_connector(vol_id);
        return 1;
    }

    /* Open the GeoTIFF file */
    file_id = H5Fopen(argv[1], H5F_ACC_RDONLY, fapl_id);
    if (file_id < 0) {
        printf("Failed to open GeoTIFF file\n");
        H5Pclose(fapl_id);
        H5VLunregister_connector(vol_id);
        return 1;
    }

    printf("Successfully opened GeoTIFF file\n");

    /* Open the image dataset */
    dset_id = H5Dopen2(file_id, "/image", H5P_DEFAULT);
    if (dset_id < 0) {
        printf("Failed to open image dataset\n");
        H5Fclose(file_id);
        H5Pclose(fapl_id);
        H5VLunregister_connector(vol_id);
        return 1;
    }

    printf("Successfully opened image dataset\n");

    /* Get dataspace */
    space_id = H5Dget_space(dset_id);
    if (space_id < 0) {
        printf("Failed to get dataspace\n");
        H5Dclose(dset_id);
        H5Fclose(file_id);
        H5Pclose(fapl_id);
        H5VLunregister_connector(vol_id);
        return 1;
    }

    ndims = H5Sget_simple_extent_ndims(space_id);
    if (ndims < 0 || ndims > 3) {
        printf("Invalid number of dimensions: %d\n", ndims);
        H5Sclose(space_id);
        H5Dclose(dset_id);
        H5Fclose(file_id);
        H5Pclose(fapl_id);
        H5VLunregister_connector(vol_id);
        return 1;
    }

    H5Sget_simple_extent_dims(space_id, dims, NULL);
    printf("Image dimensions: ");
    for (int i = 0; i < ndims; i++) {
        printf("%lu%s", (unsigned long) dims[i], (i < ndims - 1) ? " x " : "");
    }
    printf("\n");

    /* Test 1: Read full dataset */
    printf("\nTest 1: Reading full dataset...\n");
    data_size = 1;
    for (int i = 0; i < ndims; i++) {
        data_size *= dims[i];
    }
    full_data = (unsigned char *) malloc(data_size);
    if (!full_data) {
        printf("Failed to allocate memory for full data\n");
        H5Sclose(space_id);
        H5Dclose(dset_id);
        H5Fclose(file_id);
        H5Pclose(fapl_id);
        H5VLunregister_connector(vol_id);
        return 1;
    }

    ret = H5Dread(dset_id, H5T_NATIVE_UCHAR, H5S_ALL, H5S_ALL, H5P_DEFAULT, full_data);
    if (ret < 0) {
        printf("Failed to read full dataset\n");
    } else {
        printf("Successfully read full dataset (%zu bytes)\n", data_size);
        printf("First few values: %u %u %u %u\n", full_data[0], full_data[1], full_data[2], full_data[3]);
    }

    /* Test 2: Read a single band (if multi-band) */
    if (ndims == 3 && dims[2] > 1) {
        printf("\nTest 2: Reading first band only...\n");

        /* Select first band: all rows, all columns, band 0 */
        start[0] = 0;
        start[1] = 0;
        start[2] = 0;
        count[0] = dims[0];
        count[1] = dims[1];
        count[2] = 1;

        file_space_id = H5Scopy(space_id);
        ret = H5Sselect_hyperslab(file_space_id, H5S_SELECT_SET, start, NULL, count, NULL);
        if (ret < 0) {
            printf("Failed to select hyperslab\n");
        } else {
            size_t band_size = dims[0] * dims[1];
            band_data = (unsigned char *) malloc(band_size);
            if (!band_data) {
                printf("Failed to allocate memory for band data\n");
            } else {
                hsize_t mem_dims[2] = {dims[0], dims[1]};
                mem_space_id = H5Screate_simple(2, mem_dims, NULL);

                ret = H5Dread(dset_id, H5T_NATIVE_UCHAR, mem_space_id, file_space_id, H5P_DEFAULT, band_data);
                if (ret < 0) {
                    printf("Failed to read band 0\n");
                } else {
                    printf("Successfully read band 0 (%zu bytes)\n", band_size);
                    printf("First few values: %u %u %u %u\n", band_data[0], band_data[1], band_data[2], band_data[3]);
                }

                H5Sclose(mem_space_id);
                free(band_data);
            }
        }
        H5Sclose(file_space_id);
    }

    /* Clean up */
    if (full_data)
        free(full_data);
    H5Sclose(space_id);
    H5Dclose(dset_id);
    H5Fclose(file_id);
    H5Pclose(fapl_id);
    H5VLunregister_connector(vol_id);

    printf("\nTest completed successfully\n");
    return 0;
}
