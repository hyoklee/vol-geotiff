/*
 * geotiff_to_nc4.c
 *
 * Read a GeoTIFF file via the vol-geotiff VOL connector (nc_open)
 * and write all variables and global attributes to a new NC4 file.
 *
 * Compile with:
 *   gcc -o geotiff_to_nc4 geotiff_to_nc4.c \
 *       -I/home/hyoklee/netcdf-c/include \
 *       -L/home/hyoklee/netcdf-c/build-x86 -lnetcdf \
 *       -Wl,-rpath,/home/hyoklee/netcdf-c/build-x86
 *
 * Usage:
 *   HDF5_PLUGIN_PATH=... HDF5_VOL_CONNECTOR=geotiff_vol_connector \
 *   LD_LIBRARY_PATH=... ./geotiff_to_nc4 input.tif output.nc4
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netcdf.h>

#define ERR(e) do { \
    fprintf(stderr, "Error at %s:%d: %s\n", __FILE__, __LINE__, nc_strerror(e)); \
    exit(1); \
} while(0)

#define CHECK(e) do { int _r = (e); if (_r != NC_NOERR) ERR(_r); } while(0)

/* Copy a single attribute from src file/var to dst file/var */
static void copy_att(int src_ncid, int src_varid,
                     int dst_ncid, int dst_varid,
                     const char *name)
{
    nc_type xtype;
    size_t len;
    int r;

    r = nc_inq_att(src_ncid, src_varid, name, &xtype, &len);
    if (r != NC_NOERR) return;

    if (xtype == NC_STRING) {
        char **strvals = (char **)malloc(len * sizeof(char *));
        CHECK(nc_get_att_string(src_ncid, src_varid, name, strvals));
        CHECK(nc_put_att_string(dst_ncid, dst_varid, name, len, (const char **)strvals));
        nc_free_string(len, strvals);
        free(strvals);
    } else {
        size_t typesz;
        CHECK(nc_inq_type(src_ncid, xtype, NULL, &typesz));
        void *buf = malloc(len * typesz);
        CHECK(nc_get_att(src_ncid, src_varid, name, buf));
        CHECK(nc_put_att(dst_ncid, dst_varid, name, xtype, len, buf));
        free(buf);
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

    int src, dst;
    int ndims, nvars, ngatts, unlimdim;
    int r;

    /* Open source via VOL connector */
    r = nc_open(infile, NC_NOWRITE, &src);
    if (r != NC_NOERR) {
        fprintf(stderr, "Cannot open %s: %s\n", infile, nc_strerror(r));
        return 1;
    }

    CHECK(nc_inq(src, &ndims, &nvars, &ngatts, &unlimdim));
    printf("Source: %d dims, %d vars, %d global attrs\n", ndims, nvars, ngatts);

    /*
     * Unset HDF5_VOL_CONNECTOR so nc_create uses native HDF5.
     * The geotiff VOL connector's file_create returns NULL (read-only),
     * which would cause nc_create to fail with permission denied.
     */
    unsetenv("HDF5_VOL_CONNECTOR");

    /* Create destination NC4 file */
    r = nc_create(outfile, NC_NETCDF4 | NC_CLOBBER, &dst);
    if (r != NC_NOERR) {
        fprintf(stderr, "Cannot create %s: %s\n", outfile, nc_strerror(r));
        nc_close(src);
        return 1;
    }

    /* --- Copy dimensions --- */
    int dimids_map[NC_MAX_DIMS];  /* src dimid -> dst dimid */
    for (int i = 0; i < ndims; i++) {
        char dname[NC_MAX_NAME + 1];
        size_t dlen;
        CHECK(nc_inq_dim(src, i, dname, &dlen));
        int is_unlim = (i == unlimdim);
        CHECK(nc_def_dim(dst, dname, is_unlim ? NC_UNLIMITED : dlen, &dimids_map[i]));
        printf("  dim[%d] %s = %zu%s\n", i, dname, dlen, is_unlim ? " (unlimited)" : "");
    }

    /* --- Copy variables (define phase) --- */
    int var_map[NC_MAX_VARS];  /* src varid -> dst varid */
    for (int v = 0; v < nvars; v++) {
        char vname[NC_MAX_NAME + 1];
        nc_type vtype;
        int vndims, vnatts;
        int vdimids[NC_MAX_VAR_DIMS];

        CHECK(nc_inq_var(src, v, vname, &vtype, &vndims, vdimids, &vnatts));

        /* Map src dimids to dst dimids */
        int dst_dimids[NC_MAX_VAR_DIMS];
        for (int d = 0; d < vndims; d++)
            dst_dimids[d] = dimids_map[vdimids[d]];

        CHECK(nc_def_var(dst, vname, vtype, vndims, dst_dimids, &var_map[v]));
        printf("  var[%d] %s (%d dims, %d attrs)\n", v, vname, vndims, vnatts);

        /* Copy variable attributes */
        for (int a = 0; a < vnatts; a++) {
            char aname[NC_MAX_NAME + 1];
            CHECK(nc_inq_attname(src, v, a, aname));
            copy_att(src, v, dst, var_map[v], aname);
        }
    }

    /* --- Copy global attributes --- */
    for (int a = 0; a < ngatts; a++) {
        char aname[NC_MAX_NAME + 1];
        CHECK(nc_inq_attname(src, NC_GLOBAL, a, aname));
        copy_att(src, NC_GLOBAL, dst, NC_GLOBAL, aname);
        printf("  gatt: %s\n", aname);
    }

    /* End define mode */
    CHECK(nc_enddef(dst));

    /* --- Copy variable data --- */
    for (int v = 0; v < nvars; v++) {
        char vname[NC_MAX_NAME + 1];
        nc_type vtype;
        int vndims, vnatts;
        int vdimids[NC_MAX_VAR_DIMS];

        CHECK(nc_inq_var(src, v, vname, &vtype, &vndims, vdimids, &vnatts));

        /* Compute total number of elements */
        size_t total = 1;
        size_t shape[NC_MAX_VAR_DIMS];
        for (int d = 0; d < vndims; d++) {
            size_t dlen;
            CHECK(nc_inq_dimlen(src, vdimids[d], &dlen));
            shape[d] = dlen;
            total *= dlen;
        }

        if (total == 0) continue;

        size_t typesz;
        CHECK(nc_inq_type(src, vtype, NULL, &typesz));

        printf("  copying data for %s (%zu elements, %zu bytes each)...\n",
               vname, total, typesz);
        fflush(stdout);

        void *buf = malloc(total * typesz);
        if (!buf) {
            fprintf(stderr, "Out of memory for variable %s\n", vname);
            continue;
        }

        r = nc_get_var(src, v, buf);
        if (r != NC_NOERR) {
            fprintf(stderr, "  Warning: failed to read %s: %s\n", vname, nc_strerror(r));
            free(buf);
            continue;
        }

        r = nc_put_var(dst, var_map[v], buf);
        if (r != NC_NOERR) {
            fprintf(stderr, "  Warning: failed to write %s: %s\n", vname, nc_strerror(r));
        }

        free(buf);
    }

    nc_close(src);
    nc_close(dst);

    printf("\nDone. Written to %s\n", outfile);
    return 0;
}
