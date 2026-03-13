/*
 * geotiff_fill_data.c
 *
 * Read variable data from a GeoTIFF (via vol-geotiff VOL connector using nc_open)
 * and write the data into an existing NetCDF-4/HDF5 file (opened with native HDF5,
 * bypassing the VOL connector for the output file).
 *
 * This is step 2 of a two-step process:
 *   Step 1: ncgen creates empty nc4 skeleton from ncdump CDL header
 *   Step 2: this program fills in the variable data
 *
 * Compile:
 *   gcc -o geotiff_fill_data geotiff_fill_data.c \
 *       -I/home/hyoklee/netcdf-c/include \
 *       -I/home/hyoklee/hdf5/install-x86/include \
 *       -L/home/hyoklee/netcdf-c/build-x86 -lnetcdf \
 *       -L/home/hyoklee/hdf5/install-x86/lib -lhdf5 \
 *       -Wl,-rpath,/home/hyoklee/netcdf-c/build-x86 \
 *       -Wl,-rpath,/home/hyoklee/hdf5/install-x86/lib
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netcdf.h>
#include <hdf5.h>

#define NC_ERR(e) do { \
    fprintf(stderr, "NetCDF error at %s:%d: %s\n", __FILE__, __LINE__, nc_strerror(e)); \
    exit(1); \
} while(0)

#define H5_ERR(msg) do { \
    fprintf(stderr, "HDF5 error at %s:%d: %s\n", __FILE__, __LINE__, msg); \
    exit(1); \
} while(0)

#define NC_CHECK(e) do { int _r = (e); if (_r != NC_NOERR) NC_ERR(_r); } while(0)

static hid_t nc_type_to_hdf5(nc_type t)
{
    switch (t) {
        case NC_DOUBLE: return H5T_NATIVE_DOUBLE;
        case NC_FLOAT:  return H5T_NATIVE_FLOAT;
        case NC_INT:    return H5T_NATIVE_INT;
        case NC_SHORT:  return H5T_NATIVE_SHORT;
        case NC_BYTE:   return H5T_NATIVE_SCHAR;
        case NC_UBYTE:  return H5T_NATIVE_UCHAR;
        case NC_UINT:   return H5T_NATIVE_UINT;
        case NC_INT64:  return H5T_NATIVE_INT64;
        case NC_UINT64: return H5T_NATIVE_UINT64;
        default:        return -1;
    }
}

int main(int argc, char *argv[])
{
    if (argc < 3) {
        fprintf(stderr, "Usage: %s <input.tif> <output.nc4>\n", argv[0]);
        return 1;
    }

    const char *infile  = argv[1];
    const char *outfile = argv[2];

    int src;
    int ndims, nvars, ngatts, unlimdim;
    int r;

    /* Open source GeoTIFF via VOL connector */
    r = nc_open(infile, NC_NOWRITE, &src);
    if (r != NC_NOERR) {
        fprintf(stderr, "Cannot open %s: %s\n", infile, nc_strerror(r));
        return 1;
    }

    NC_CHECK(nc_inq(src, &ndims, &nvars, &ngatts, &unlimdim));
    printf("Source: %d dims, %d vars\n", ndims, nvars);

    /* Open output nc4 file using native HDF5 (bypasses the VOL connector) */
    hid_t native_fapl = H5Pcreate(H5P_FILE_ACCESS);
    if (native_fapl < 0) H5_ERR("H5Pcreate failed");

    if (H5Pset_vol(native_fapl, H5VL_NATIVE, NULL) < 0)
        H5_ERR("H5Pset_vol(native) failed");

    hid_t out_file = H5Fopen(outfile, H5F_ACC_RDWR, native_fapl);
    H5Pclose(native_fapl);
    if (out_file < 0) H5_ERR("H5Fopen failed on output nc4");

    /* Copy variable data: read via nc_get_var, write via H5Dwrite */
    for (int v = 0; v < nvars; v++) {
        char vname[NC_MAX_NAME + 1];
        nc_type vtype;
        int vndims, vnatts;
        int vdimids[NC_MAX_VAR_DIMS];

        NC_CHECK(nc_inq_var(src, v, vname, &vtype, &vndims, vdimids, &vnatts));

        /* Compute total number of elements */
        size_t total = 1;
        for (int d = 0; d < vndims; d++) {
            size_t dlen;
            NC_CHECK(nc_inq_dimlen(src, vdimids[d], &dlen));
            total *= dlen;
        }

        if (total == 0) {
            printf("  skip %s (empty)\n", vname);
            continue;
        }

        hid_t h5type = nc_type_to_hdf5(vtype);
        if (h5type < 0) {
            printf("  skip %s (unsupported type %d)\n", vname, vtype);
            continue;
        }

        size_t typesz;
        NC_CHECK(nc_inq_type(src, vtype, NULL, &typesz));

        printf("  reading %s (%zu elements × %zu bytes)...", vname, total, typesz);
        fflush(stdout);

        void *buf = malloc(total * typesz);
        if (!buf) {
            fprintf(stderr, "Out of memory for %s\n", vname);
            continue;
        }

        r = nc_get_var(src, v, buf);
        if (r != NC_NOERR) {
            fprintf(stderr, "\n  Warning: failed to read %s: %s\n", vname, nc_strerror(r));
            free(buf);
            continue;
        }
        printf(" done\n  writing to nc4...");
        fflush(stdout);

        /* Open HDF5 dataset by name in the output file */
        hid_t dset = H5Dopen2(out_file, vname, H5P_DEFAULT);
        if (dset < 0) {
            fprintf(stderr, "\n  Warning: cannot open dataset '%s' in output file\n", vname);
            free(buf);
            continue;
        }

        herr_t herr = H5Dwrite(dset, h5type, H5S_ALL, H5S_ALL, H5P_DEFAULT, buf);
        H5Dclose(dset);
        free(buf);

        if (herr < 0)
            fprintf(stderr, "\n  Warning: H5Dwrite failed for %s\n", vname);
        else
            printf(" done\n");
    }

    H5Fclose(out_file);
    nc_close(src);

    printf("\nData copy complete: %s\n", outfile);
    return 0;
}
