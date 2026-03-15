/*
 * check_image_data.c
 *
 * Verify that imageN variable data read via the netCDF API (through the
 * vol-geotiff VOL connector) matches the pixel values read directly from
 * the TIFF file using libtiff.
 *
 * For each image directory (image0, image1, ...) the test:
 *   1. Reads the full image row by row via nc_get_vara_double().
 *   2. Reads the same rows directly from TIFF (strip or tiled).
 *   3. Compares N_SAMPLE evenly-spaced rows, all columns.
 *
 * Compile (run from build directory or set paths accordingly):
 *   gcc -o check_image_data check_image_data.c \
 *       -I/home/hyoklee/netcdf-c/include \
 *       -I/home/hyoklee/hdf5/install-x86/include \
 *       -L/home/hyoklee/netcdf-c/build-x86 -lnetcdf \
 *       -L/home/hyoklee/hdf5/install-x86/lib -lhdf5 \
 *       -ltiff \
 *       -Wl,-rpath,/home/hyoklee/netcdf-c/build-x86 \
 *       -Wl,-rpath,/home/hyoklee/hdf5/install-x86/lib \
 *       -lm
 *
 * Run with VOL connector active:
 *   HDF5_PLUGIN_PATH=.../build-x86/src \
 *   HDF5_VOL_CONNECTOR=geotiff_vol_connector \
 *   LD_LIBRARY_PATH=... \
 *   ./check_image_data [path/to/file.tif]
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <netcdf.h>
#include <tiffio.h>

#define N_SAMPLE 8   /* number of evenly-spaced rows to sample per image */

static int nerrors = 0;

static void check_val(const char *label, double got, double expected)
{
    int pass;
    if (isnan(expected) && isnan(got)) {
        pass = 1;
    } else if (expected == got) {
        pass = 1;
    } else {
        double diff = fabs(got - expected);
        double scale = fabs(expected) > 1e-300 ? fabs(expected) : 1.0;
        pass = (diff / scale < 1e-12);
    }
    if (!pass) {
        printf("  FAIL  %-60s got=%.15g  expected=%.15g\n", label, got, expected);
        nerrors++;
    }
}

/*
 * Read one row from a TIFF image (handles both strip and tiled layouts).
 * Converts the native pixel type to double.
 * Returns 0 on success, -1 on failure.
 */
static int read_tiff_row_as_double(TIFF *tiff, uint32_t row,
                                   uint32_t width, uint16_t bits, uint16_t sfmt,
                                   uint16_t spp,
                                   double *out)
{
    size_t elem_size = bits / 8;

    if (!TIFFIsTiled(tiff)) {
        /* Strip layout: use TIFFReadScanline */
        tsize_t scanline_bytes = TIFFScanlineSize(tiff);
        void *buf = _TIFFmalloc(scanline_bytes);
        if (!buf) return -1;
        if (TIFFReadScanline(tiff, buf, row, 0) < 0) {
            _TIFFfree(buf);
            return -1;
        }
        for (uint32_t col = 0; col < width; col++) {
            const unsigned char *p = (const unsigned char *)buf + col * spp * elem_size;
            if (bits == 64 && sfmt == SAMPLEFORMAT_IEEEFP)
                out[col] = ((const double *)p)[0];
            else if (bits == 32 && sfmt == SAMPLEFORMAT_IEEEFP)
                out[col] = (double)((const float *)p)[0];
            else if (bits == 16 && sfmt == SAMPLEFORMAT_INT)
                out[col] = (double)((const int16_t *)p)[0];
            else if (bits == 16 && sfmt == SAMPLEFORMAT_UINT)
                out[col] = (double)((const uint16_t *)p)[0];
            else if (bits == 8)
                out[col] = (double)((const uint8_t *)p)[0];
            else
                out[col] = 0.0; /* unsupported type */
        }
        _TIFFfree(buf);
        return 0;
    }

    /* Tiled layout: read each tile that contains this row */
    uint32_t tile_w = 0, tile_h = 0;
    TIFFGetField(tiff, TIFFTAG_TILEWIDTH,  &tile_w);
    TIFFGetField(tiff, TIFFTAG_TILELENGTH, &tile_h);

    tsize_t tile_size = TIFFTileSize(tiff);
    void *tile_buf = _TIFFmalloc(tile_size);
    if (!tile_buf) return -1;

    uint32_t tile_row = (row / tile_h) * tile_h; /* top row of the tile containing `row` */
    uint32_t row_in_tile = row - tile_row;

    for (uint32_t tc = 0; tc < width; tc += tile_w) {
        if (TIFFReadTile(tiff, tile_buf, tc, tile_row, 0, 0) < 0) {
            _TIFFfree(tile_buf);
            return -1;
        }
        uint32_t actual_tw = (tc + tile_w > width) ? width - tc : tile_w;
        for (uint32_t col_in_tile = 0; col_in_tile < actual_tw; col_in_tile++) {
            uint32_t col = tc + col_in_tile;
            size_t off = (row_in_tile * tile_w + col_in_tile) * spp * elem_size;
            const unsigned char *p = (const unsigned char *)tile_buf + off;
            if (bits == 64 && sfmt == SAMPLEFORMAT_IEEEFP)
                out[col] = ((const double *)p)[0];
            else if (bits == 32 && sfmt == SAMPLEFORMAT_IEEEFP)
                out[col] = (double)((const float *)p)[0];
            else if (bits == 16 && sfmt == SAMPLEFORMAT_INT)
                out[col] = (double)((const int16_t *)p)[0];
            else if (bits == 16 && sfmt == SAMPLEFORMAT_UINT)
                out[col] = (double)((const uint16_t *)p)[0];
            else if (bits == 8)
                out[col] = (double)((const uint8_t *)p)[0];
            else
                out[col] = 0.0;
        }
    }
    _TIFFfree(tile_buf);
    return 0;
}

int main(int argc, char *argv[])
{
    const char *fname = (argc > 1) ? argv[1]
        : "EMIT_L2B_FRCOVPV_001_20260311T195709_2607013_014.tif";

    /* --- Open via netCDF API (VOL connector must be active in env) --- */
    int ncid, r;
    r = nc_open(fname, NC_NOWRITE, &ncid);
    if (r != NC_NOERR) {
        fprintf(stderr, "nc_open failed for %s: %s\n", fname, nc_strerror(r));
        return 1;
    }

    /* --- Open directly via libtiff for ground-truth pixel values --- */
    TIFF *tiff = TIFFOpen(fname, "r");
    if (!tiff) {
        fprintf(stderr, "TIFFOpen failed for %s\n", fname);
        nc_close(ncid);
        return 1;
    }

    int ndirs = (int) TIFFNumberOfDirectories(tiff);
    printf("File: %s  (%d TIFF director%s)\n", fname, ndirs,
           ndirs == 1 ? "y" : "ies");

    for (int d = 0; d < ndirs; d++) {
        char vname[32];
        snprintf(vname, sizeof(vname), "image%d", d);

        /* --- netCDF side --- */
        int varid;
        r = nc_inq_varid(ncid, vname, &varid);
        if (r != NC_NOERR) {
            fprintf(stderr, "  %s: not found in netCDF view (%s)\n", vname, nc_strerror(r));
            nerrors++;
            continue;
        }

        int vndims;
        int vdimids[NC_MAX_VAR_DIMS];
        r = nc_inq_var(ncid, varid, NULL, NULL, &vndims, vdimids, NULL);
        if (r != NC_NOERR || vndims < 2) {
            fprintf(stderr, "  %s: unexpected ndims=%d\n", vname, vndims);
            nerrors++;
            continue;
        }

        size_t nrows = 0, ncols = 0;
        nc_inq_dimlen(ncid, vdimids[0], &nrows);
        nc_inq_dimlen(ncid, vdimids[1], &ncols);

        /* --- libtiff side --- */
        TIFFSetDirectory(tiff, (uint16_t) d);
        uint32_t tiff_w = 0, tiff_h = 0;
        TIFFGetField(tiff, TIFFTAG_IMAGEWIDTH,  &tiff_w);
        TIFFGetField(tiff, TIFFTAG_IMAGELENGTH, &tiff_h);
        uint16_t bits = 0, sfmt = 0, spp = 0;
        TIFFGetFieldDefaulted(tiff, TIFFTAG_BITSPERSAMPLE,  &bits);
        TIFFGetFieldDefaulted(tiff, TIFFTAG_SAMPLEFORMAT,   &sfmt);
        TIFFGetFieldDefaulted(tiff, TIFFTAG_SAMPLESPERPIXEL, &spp);
        if (spp == 0) spp = 1;

        const char *layout = TIFFIsTiled(tiff) ? "tiled" : "strip";
        printf("\n=== %s  nc=%zux%zu  tiff=%ux%u  bits=%u  %s ===\n",
               vname, nrows, ncols, tiff_h, tiff_w, bits, layout);

        /* Verify dimensions match */
        if ((size_t)tiff_w != ncols || (size_t)tiff_h != nrows) {
            printf("  FAIL  dimension mismatch\n");
            nerrors++;
            continue;
        }
        printf("  dimensions OK\n");

        double *nc_row   = (double *) malloc(ncols * sizeof(double));
        double *tiff_row = (double *) malloc(ncols * sizeof(double));
        if (!nc_row || !tiff_row) {
            free(nc_row);
            free(tiff_row);
            fprintf(stderr, "  malloc failed\n");
            nerrors++;
            continue;
        }

        int step = (int)(nrows / N_SAMPLE);
        if (step < 1) step = 1;
        int row_errors = 0;

        for (int s = 0; s < N_SAMPLE; s++) {
            size_t row = (size_t)(s * step);
            if (row >= nrows) break;

            /* Read row via netCDF */
            size_t start[2] = {row, 0};
            size_t count[2] = {1, ncols};
            r = nc_get_vara_double(ncid, varid, start, count, nc_row);
            if (r != NC_NOERR) {
                fprintf(stderr, "  nc_get_vara_double row %zu: %s\n", row, nc_strerror(r));
                row_errors++;
                nerrors++;
                continue;
            }

            /* Read row from TIFF */
            if (read_tiff_row_as_double(tiff, (uint32_t)row,
                                        tiff_w, bits, sfmt, spp, tiff_row) < 0) {
                fprintf(stderr, "  TIFF read row %zu failed\n", row);
                row_errors++;
                nerrors++;
                continue;
            }

            /* Compare every pixel */
            char label[128];
            for (size_t col = 0; col < ncols; col++) {
                snprintf(label, sizeof(label), "%s[%zu][%zu]", vname, row, col);
                check_val(label, nc_row[col], tiff_row[col]);
                if (nerrors > 20) {
                    printf("  (too many errors, stopping at row %zu col %zu)\n", row, col);
                    goto next_image;
                }
            }
            printf("  row %4zu  OK (%zu cols)\n", row, ncols);
        }

        if (row_errors == 0)
            printf("  %d sampled rows all match\n", N_SAMPLE);

next_image:
        free(nc_row);
        free(tiff_row);
    }

    TIFFClose(tiff);
    nc_close(ncid);

    printf("\n%s: %d error(s)\n", nerrors == 0 ? "PASS" : "FAIL", nerrors);
    return nerrors ? 1 : 0;
}
