/* check_latlon.c — verify lat/lon values against gdalinfo expected values */
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

/* Read a single double from dataset at [row][col] */
static double read_val(hid_t file, const char *dset_name, hsize_t row, hsize_t col)
{
    hid_t dset = H5Dopen2(file, dset_name, H5P_DEFAULT);
    if (dset < 0) { fprintf(stderr, "Cannot open %s\n", dset_name); return 0.0/0.0; }

    hid_t fspace = H5Dget_space(dset);
    hsize_t start[2] = {row, col};
    hsize_t count[2] = {1, 1};
    H5Sselect_hyperslab(fspace, H5S_SELECT_SET, start, NULL, count, NULL);

    hid_t mspace = H5Screate_simple(2, count, NULL);
    double val;
    H5Dread(dset, H5T_NATIVE_DOUBLE, mspace, fspace, H5P_DEFAULT, &val);
    H5Sclose(mspace);
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

        printf("\n=== %s / %s  (%d x %d)  px=%.12f  py=%.12f ===\n",
               latname, lonname, W, H, px_i, py_i);

        /* corner (0,0) */
        double exp_lat_00 = ULY;
        double exp_lon_00 = ULX;
        snprintf(label, sizeof(label), "%s[0][0]", latname);
        check(label, read_val(file, latname, 0, 0), exp_lat_00, tol);
        snprintf(label, sizeof(label), "%s[0][0]", lonname);
        check(label, read_val(file, lonname, 0, 0), exp_lon_00, tol);

        /* corner (0, W-1) — upper-right */
        double exp_lat_0W = ULY;                        /* lat doesn't vary with col */
        double exp_lon_0W = ULX + (W - 1) * px_i;
        snprintf(label, sizeof(label), "%s[0][%d]", latname, W-1);
        check(label, read_val(file, latname, 0, W-1), exp_lat_0W, tol);
        snprintf(label, sizeof(label), "%s[0][%d]", lonname, W-1);
        check(label, read_val(file, lonname, 0, W-1), exp_lon_0W, tol);

        /* corner (H-1, 0) — lower-left */
        double exp_lat_H0 = ULY + (H - 1) * py_i;
        double exp_lon_H0 = ULX;                        /* lon doesn't vary with row */
        snprintf(label, sizeof(label), "%s[%d][0]", latname, H-1);
        check(label, read_val(file, latname, H-1, 0), exp_lat_H0, tol);
        snprintf(label, sizeof(label), "%s[%d][0]", lonname, H-1);
        check(label, read_val(file, lonname, H-1, 0), exp_lon_H0, tol);

        /* corner (H-1, W-1) — lower-right */
        double exp_lat_HW = ULY + (H - 1) * py_i;
        double exp_lon_HW = ULX + (W - 1) * px_i;
        snprintf(label, sizeof(label), "%s[%d][%d]", latname, H-1, W-1);
        check(label, read_val(file, latname, H-1, W-1), exp_lat_HW, tol);
        snprintf(label, sizeof(label), "%s[%d][%d]", lonname, H-1, W-1);
        check(label, read_val(file, lonname, H-1, W-1), exp_lon_HW, tol);

        /* center */
        int cr = H / 2, cc = W / 2;
        double exp_lat_c = ULY + cr * py_i;
        double exp_lon_c = ULX + cc * px_i;
        snprintf(label, sizeof(label), "%s[%d][%d] (center)", latname, cr, cc);
        check(label, read_val(file, latname, cr, cc), exp_lat_c, tol);
        snprintf(label, sizeof(label), "%s[%d][%d] (center)", lonname, cr, cc);
        check(label, read_val(file, lonname, cr, cc), exp_lon_c, tol);

        /* verify lat is constant across a row (row 0, compare col 0 vs col W/2) */
        double lat_r0_c0   = read_val(file, latname, 0, 0);
        double lat_r0_cMid = read_val(file, latname, 0, W/2);
        snprintf(label, sizeof(label), "%s row-constant check", latname);
        check(label, lat_r0_cMid, lat_r0_c0, tol);

        /* verify lon is constant down a column (col 0, compare row 0 vs row H/2) */
        double lon_c0_r0   = read_val(file, lonname, 0, 0);
        double lon_c0_rMid = read_val(file, lonname, H/2, 0);
        snprintf(label, sizeof(label), "%s col-constant check", lonname);
        check(label, lon_c0_rMid, lon_c0_r0, tol);
    }

    H5Fclose(file);

    printf("\n%s: %d error(s)\n", nerrors == 0 ? "PASS" : "FAIL", nerrors);
    return nerrors ? 1 : 0;
}
