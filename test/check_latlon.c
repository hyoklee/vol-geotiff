/* check_latlon.c — verify lat/lon values against gdalinfo expected values.
 * Handles both 1D (geographic/EPSG:4326) and 2D (projected CRS) lat/lon arrays.
 */
#include <stdio.h>
#include <math.h>
#include <hdf5.h>

/* From gdalinfo:
 *   Origin    = (-66.387972587588806, -21.716193957864899)  (ulx, uly)
 *   PixelSize =  (0.000542232520256, -0.000542232520256)    (px, py)
 *   Size      = 2088 x 1910
 *   Overview 0: 1044 x 955
 *   Overview 1:  522 x 477
 *   Overview 2:  261 x 238
 */
static const double ULX = -66.387972587588806;
static const double ULY = -21.716193957864899;
static const double PX  =  0.000542232520256;
static const double PY  = -0.000542232520256;

/* Main image and overview dimensions */
static const struct { int w, h; } dims[4] = {
    {2088, 1910},   /* image0 */
    {1044,  955},   /* image1 (overview 0) */
    { 522,  477},   /* image2 (overview 1) */
    { 261,  238},   /* image3 (overview 2) */
};

static int nerrors = 0;

/*
 * Read a single double from a dataset.
 * - For 1D datasets: lat uses `row` as index, lon uses `col` as index.
 * - For 2D datasets: uses [row, col].
 * is_lat: true for lat datasets, false for lon datasets.
 */
static double read_latlon(hid_t file, const char *dset_name, int is_lat,
                          hsize_t row, hsize_t col)
{
    hid_t dset = H5Dopen2(file, dset_name, H5P_DEFAULT);
    if (dset < 0) {
        fprintf(stderr, "Cannot open %s\n", dset_name);
        return 0.0 / 0.0;
    }

    hid_t fspace = H5Dget_space(dset);
    int ndims = H5Sget_simple_extent_ndims(fspace);
    double val = 0.0;

    if (ndims == 1) {
        /* 1D: lat indexed by row, lon indexed by col */
        hsize_t idx = is_lat ? row : col;
        hsize_t start[1] = {idx};
        hsize_t count[1] = {1};
        H5Sselect_hyperslab(fspace, H5S_SELECT_SET, start, NULL, count, NULL);
        hid_t mspace = H5Screate_simple(1, count, NULL);
        H5Dread(dset, H5T_NATIVE_DOUBLE, mspace, fspace, H5P_DEFAULT, &val);
        H5Sclose(mspace);
    } else {
        /* 2D: [row, col] */
        hsize_t start[2] = {row, col};
        hsize_t count[2] = {1, 1};
        H5Sselect_hyperslab(fspace, H5S_SELECT_SET, start, NULL, count, NULL);
        hid_t mspace = H5Screate_simple(2, count, NULL);
        H5Dread(dset, H5T_NATIVE_DOUBLE, mspace, fspace, H5P_DEFAULT, &val);
        H5Sclose(mspace);
    }

    H5Sclose(fspace);
    H5Dclose(dset);
    return val;
}

static void check(const char *label, double got, double expected, double tol)
{
    double diff = fabs(got - expected);
    const char *status = (diff <= tol) ? "OK" : "FAIL";
    printf("  %-40s got=%.10f  expected=%.10f  diff=%.2e  %s\n",
           label, got, expected, diff, status);
    if (diff > tol) nerrors++;
}

int main(int argc, char *argv[])
{
    const char *fname = (argc > 1) ? argv[1]
        : "EMIT_L2B_FRCOVPV_001_20260311T195709_2607013_014.tif";

    hid_t fapl = H5Pcreate(H5P_FILE_ACCESS);
    hid_t file = H5Fopen(fname, H5F_ACC_RDONLY, fapl);
    H5Pclose(fapl);
    if (file < 0) { fprintf(stderr, "Cannot open %s\n", fname); return 1; }

    double tol = 1e-9;   /* tolerance: <1 nm in degrees */
    char latname[16], lonname[16], label[64];

    for (int i = 0; i < 4; i++) {
        int W = dims[i].w, H = dims[i].h;
        snprintf(latname, sizeof(latname), "lat%d", i);
        snprintf(lonname, sizeof(lonname), "lon%d", i);

        /* pixel size scaled proportionally to this overview's resolution */
        double px_i = PX * ((double) dims[0].w / W);
        double py_i = PY * ((double) dims[0].h / H);

        /* Detect 1D vs 2D by checking dataset rank */
        hid_t dset = H5Dopen2(file, latname, H5P_DEFAULT);
        int ndims = 0;
        if (dset >= 0) {
            hid_t sp = H5Dget_space(dset);
            ndims = H5Sget_simple_extent_ndims(sp);
            H5Sclose(sp);
            H5Dclose(dset);
        }
        const char *layout = (ndims == 1) ? "1D" : "2D";

        printf("\n=== %s / %s  (%d x %d)  px=%.12f  py=%.12f  [%s] ===\n",
               latname, lonname, W, H, px_i, py_i, layout);

        /* corner (0,0) */
        double exp_lat_00 = ULY;
        double exp_lon_00 = ULX;
        snprintf(label, sizeof(label), "%s[0]", latname);
        check(label, read_latlon(file, latname, 1, 0, 0), exp_lat_00, tol);
        snprintf(label, sizeof(label), "%s[0]", lonname);
        check(label, read_latlon(file, lonname, 0, 0, 0), exp_lon_00, tol);

        /* corner (0, W-1) — upper-right: lat unchanged (row=0), lon at col W-1 */
        double exp_lat_0W = ULY;
        double exp_lon_0W = ULX + (W - 1) * px_i;
        snprintf(label, sizeof(label), "%s[0] (upper-right)", latname);
        check(label, read_latlon(file, latname, 1, 0, W-1), exp_lat_0W, tol);
        snprintf(label, sizeof(label), "%s[%d]", lonname, W-1);
        check(label, read_latlon(file, lonname, 0, 0, W-1), exp_lon_0W, tol);

        /* corner (H-1, 0) — lower-left: lat at row H-1, lon unchanged (col=0) */
        double exp_lat_H0 = ULY + (H - 1) * py_i;
        double exp_lon_H0 = ULX;
        snprintf(label, sizeof(label), "%s[%d]", latname, H-1);
        check(label, read_latlon(file, latname, 1, H-1, 0), exp_lat_H0, tol);
        snprintf(label, sizeof(label), "%s[0] (lower-left)", lonname);
        check(label, read_latlon(file, lonname, 0, H-1, 0), exp_lon_H0, tol);

        /* corner (H-1, W-1) — lower-right */
        double exp_lat_HW = ULY + (H - 1) * py_i;
        double exp_lon_HW = ULX + (W - 1) * px_i;
        snprintf(label, sizeof(label), "%s[%d] (lower-right)", latname, H-1);
        check(label, read_latlon(file, latname, 1, H-1, W-1), exp_lat_HW, tol);
        snprintf(label, sizeof(label), "%s[%d] (lower-right)", lonname, W-1);
        check(label, read_latlon(file, lonname, 0, H-1, W-1), exp_lon_HW, tol);

        /* center */
        int cr = H / 2, cc = W / 2;
        double exp_lat_c = ULY + cr * py_i;
        double exp_lon_c = ULX + cc * px_i;
        snprintf(label, sizeof(label), "%s[%d] (center)", latname, cr);
        check(label, read_latlon(file, latname, 1, cr, cc), exp_lat_c, tol);
        snprintf(label, sizeof(label), "%s[%d] (center)", lonname, cc);
        check(label, read_latlon(file, lonname, 0, cr, cc), exp_lon_c, tol);

        /* For 1D: verify lat array size == H, lon array size == W */
        if (ndims == 1) {
            hid_t ld = H5Dopen2(file, latname, H5P_DEFAULT);
            hid_t sp = H5Dget_space(ld);
            hsize_t lat_n = 0;
            H5Sget_simple_extent_dims(sp, &lat_n, NULL);
            H5Sclose(sp); H5Dclose(ld);

            hid_t lond = H5Dopen2(file, lonname, H5P_DEFAULT);
            sp = H5Dget_space(lond);
            hsize_t lon_n = 0;
            H5Sget_simple_extent_dims(sp, &lon_n, NULL);
            H5Sclose(sp); H5Dclose(lond);

            snprintf(label, sizeof(label), "%s size == %d (height)", latname, H);
            check(label, (double) lat_n, (double) H, 0.5);
            snprintf(label, sizeof(label), "%s size == %d (width)", lonname, W);
            check(label, (double) lon_n, (double) W, 0.5);
        }
    }

    H5Fclose(file);

    printf("\n%s: %d error(s)\n", nerrors == 0 ? "PASS" : "FAIL", nerrors);
    return nerrors ? 1 : 0;
}
