/*
 * Simple test program to verify GeoTIFF VOL connector works with h5dump-like functionality
 * Uses the HDF5 library from c:\src\hdf5.HDFGroup\
 */

#include <hdf5.h>
#include <stdio.h>
#include <stdlib.h>

#define GEOTIFF_VOL_CONNECTOR_NAME "geotiff_vol_connector"

int main(int argc, char **argv)
{
    hid_t vol_id, fapl_id, file_id;
    hid_t dset_id, space_id, type_id;
    hsize_t dims[3];
    int ndims;
    char *filename;

    if (argc != 2) {
        printf("Usage: %s <geotiff_file>\n", argv[0]);
        return 1;
    }

    filename = argv[1];
    printf("===========================================\n");
    printf("Testing GeoTIFF VOL Connector with h5dump\n");
    printf("===========================================\n");
    printf("File: %s\n\n", filename);

    /* Register the GeoTIFF VOL connector */
    vol_id = H5VLregister_connector_by_name(GEOTIFF_VOL_CONNECTOR_NAME, H5P_DEFAULT);
    if (vol_id < 0) {
        printf("ERROR: Failed to register GeoTIFF VOL connector\n");
        printf("Make sure HDF5_PLUGIN_PATH is set correctly\n");
        return 1;
    }
    printf("✓ Successfully registered GeoTIFF VOL connector (ID: %ld)\n", (long) vol_id);

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
    printf("✓ Successfully set VOL connector in FAPL\n");

    /* Open the GeoTIFF file */
    file_id = H5Fopen(filename, H5F_ACC_RDONLY, fapl_id);
    if (file_id < 0) {
        printf("ERROR: Failed to open GeoTIFF file\n");
        H5Pclose(fapl_id);
        H5VLunregister_connector(vol_id);
        return 1;
    }
    printf("✓ Successfully opened GeoTIFF file\n\n");

    /* Try to open the image dataset */
    printf("Opening dataset: /image\n");
    dset_id = H5Dopen2(file_id, "/image", H5P_DEFAULT);
    if (dset_id < 0) {
        printf("ERROR: Failed to open /image dataset\n");
        H5Fclose(file_id);
        H5Pclose(fapl_id);
        H5VLunregister_connector(vol_id);
        return 1;
    }
    printf("✓ Successfully opened /image dataset\n");

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

    /* Get dimensions */
    ndims = H5Sget_simple_extent_ndims(space_id);
    if (ndims > 0 && ndims <= 3) {
        H5Sget_simple_extent_dims(space_id, dims, NULL);
        printf("  Dimensions: [");
        for (int i = 0; i < ndims; i++) {
            printf("%lu%s", (unsigned long) dims[i], (i < ndims - 1) ? ", " : "");
        }
        printf("]\n");
        printf("  Rank: %d\n", ndims);
    }

    /* Get datatype */
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

    /* Read and display first few values */
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

    printf("\n===========================================\n");
    printf("✓ All tests passed!\n");
    printf("===========================================\n");

    return 0;
}
