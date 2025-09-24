/*
 * Test program for GeoTIFF VOL connector
 * Tests reading a GeoTIFF file through HDF5 interface
 */

// cppcheck-suppress missingInclude
#include "template_vol_connector.h"
#include <hdf5.h>
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv)
{
    hid_t fapl_id, file_id, vol_id;
    hid_t dset_id, space_id, type_id;
    hsize_t dims[3];
    herr_t ret;

    if (argc != 2) {
        printf("Usage: %s <geotiff_file>\n", argv[0]);
        return 1;
    }

    printf("Testing GeoTIFF VOL connector with file: %s\n", argv[1]);

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

    /* Try to open the image dataset */
    dset_id = H5Dopen2(file_id, "/image", H5P_DEFAULT);
    if (dset_id >= 0) {
        printf("Successfully opened image dataset\n");

        /* Get dataspace */
        space_id = H5Dget_space(dset_id);
        if (space_id >= 0) {
            int ndims = H5Sget_simple_extent_ndims(space_id);
            if (ndims > 0 && ndims <= 3) {
                H5Sget_simple_extent_dims(space_id, dims, NULL);
                printf("Image dimensions: ");
                for (int i = 0; i < ndims; i++) {
                    printf("%llu%s", dims[i], (i < ndims - 1) ? " x " : "");
                }
                printf("\n");
            }
            H5Sclose(space_id);
        }

        /* Get datatype */
        type_id = H5Dget_type(dset_id);
        if (type_id >= 0) {
            H5T_class_t type_class = H5Tget_class(type_id);
            size_t type_size = H5Tget_size(type_id);
            printf("Image datatype: class=%d, size=%zu bytes\n", type_class, type_size);
            H5Tclose(type_id);
        }

        H5Dclose(dset_id);
    } else {
        printf("Failed to open image dataset\n");
    }

    /* Clean up */
    H5Fclose(file_id);
    H5Pclose(fapl_id);
    H5VLunregister_connector(vol_id);

    printf("Test completed successfully\n");
    return 0;
}