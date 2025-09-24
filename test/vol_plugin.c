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
 * Purpose:     Tests basic VOL plugin operations (registration, etc.).
 *              Uses the template VOL connector which is loaded as a
 *              dynamically-loaded plugin.
 */

// cppcheck-suppress missingInclude
#include "template_vol_connector.h"

#include <hdf5.h>
#include <stdlib.h>
#include <string.h>

/* Portable getenv helper that returns a heap-allocated copy */
static char *get_env_dup(const char *name)
{
#ifdef _WIN32
    char *value = NULL;
    size_t len = 0;
    if (_dupenv_s(&value, &len, name) != 0)
        return NULL;
    return value; /* caller must free */
#else
    const char *v = getenv(name);
    return v ? strdup(v) : NULL; /* caller must free */
#endif
}

/* herr_t values from H5private.h */
#define SUCCEED 0
#define FAIL (-1)

/* Testing macros from h5test.h
 *
 * The name of the test is printed by saying TESTING("something") which will
 * result in the string `Testing something' being flushed to standard output.
 * If a test passes, fails, or is skipped then the PASSED(), H5_FAILED(), or
 * SKIPPED() macro should be called.  After H5_FAILED() or SKIPPED() the caller
 * should print additional information to stdout indented by at least four
 * spaces.  If the h5_errors() is used for automatic error handling then
 * the H5_FAILED() macro is invoked automatically when an API function fails.
 */

#define AT() printf("   at %s:%d...\n", __FILE__, __LINE__);
#define TESTING(WHAT)                                                                              \
    {                                                                                              \
        printf("Testing %-62s", WHAT);                                                             \
        fflush(stdout);                                                                            \
    }
#define PASSED()                                                                                   \
    {                                                                                              \
        puts(" PASSED");                                                                           \
        fflush(stdout);                                                                            \
    }
#define H5_FAILED()                                                                                \
    {                                                                                              \
        puts("*FAILED*");                                                                          \
        fflush(stdout);                                                                            \
    }
#define H5_WARNING()                                                                               \
    {                                                                                              \
        puts("*WARNING*");                                                                         \
        fflush(stdout);                                                                            \
    }
#define SKIPPED()                                                                                  \
    {                                                                                              \
        puts(" -SKIP-");                                                                           \
        fflush(stdout);                                                                            \
    }
#define PUTS_ERROR(s)                                                                              \
    {                                                                                              \
        puts(s);                                                                                   \
        AT();                                                                                      \
        goto error;                                                                                \
    }
#define TEST_ERROR                                                                                 \
    {                                                                                              \
        H5_FAILED();                                                                               \
        AT();                                                                                      \
        goto error;                                                                                \
    }
#define STACK_ERROR                                                                                \
    {                                                                                              \
        H5Eprint2(H5E_DEFAULT, stdout);                                                            \
        goto error;                                                                                \
    }
#define FAIL_STACK_ERROR                                                                           \
    {                                                                                              \
        H5_FAILED();                                                                               \
        AT();                                                                                      \
        H5Eprint2(H5E_DEFAULT, stdout);                                                            \
        goto error;                                                                                \
    }
#define FAIL_PUTS_ERROR(s)                                                                         \
    {                                                                                              \
        H5_FAILED();                                                                               \
        AT();                                                                                      \
        puts(s);                                                                                   \
        goto error;                                                                                \
    }

/*-------------------------------------------------------------------------
 * Function:    test_registration_by_value()
 *
 * Purpose:     Tests if we can load, register, and close a VOL
 *              connector by value.
 *
 * Return:      SUCCEED/FAIL
 *
 *-------------------------------------------------------------------------
 */
static herr_t test_registration_by_value(void)
{
    htri_t is_registered = FAIL;
    hid_t vol_id = H5I_INVALID_HID;
    hid_t pre_id = H5I_INVALID_HID;

    TESTING("VOL registration by value");

    /* Ensure connector is not pre-registered (CI/env may set defaults) */
    if ((is_registered = H5VLis_connector_registered_by_name(GEOTIFF_VOL_CONNECTOR_NAME)) < 0)
        TEST_ERROR;
    if (true == is_registered) {
        if ((pre_id = H5VLget_connector_id_by_name(GEOTIFF_VOL_CONNECTOR_NAME)) < 0)
            TEST_ERROR;
        if (H5VLunregister_connector(pre_id) < 0)
            TEST_ERROR;
    }

    /* Register the connector by value */
    if ((vol_id = H5VLregister_connector_by_value(GEOTIFF_VOL_CONNECTOR_VALUE, H5P_DEFAULT)) < 0)
        TEST_ERROR;

    /* The connector should be registered now */
    if ((is_registered = H5VLis_connector_registered_by_name(GEOTIFF_VOL_CONNECTOR_NAME)) < 0)
        TEST_ERROR;
    if (false == is_registered)
        FAIL_PUTS_ERROR("VOL connector was not registered");

    /* Unregister the connector */
    if (H5VLunregister_connector(vol_id) < 0)
        TEST_ERROR;

    /* The connector should not be registered now */
    if ((is_registered = H5VLis_connector_registered_by_name(GEOTIFF_VOL_CONNECTOR_NAME)) < 0)
        TEST_ERROR;
    if (true == is_registered)
        FAIL_PUTS_ERROR("VOL connector is inappropriately registered");

    PASSED();
    return SUCCEED;

error:
    H5E_BEGIN_TRY
    {
        H5VLunregister_connector(vol_id);
    }
    H5E_END_TRY;
    return FAIL;

} /* end test_registration_by_value() */

/*-------------------------------------------------------------------------
 * Function:    test_registration_by_name()
 *
 * Purpose:     Tests if we can load, register, and close a VOL
 *              connector by name.
 *
 * Return:      SUCCEED/FAIL
 *
 *-------------------------------------------------------------------------
 */
static herr_t test_registration_by_name(void)
{
    htri_t is_registered = FAIL;
    hid_t vol_id = H5I_INVALID_HID;
    hid_t pre_id = H5I_INVALID_HID;

    TESTING("VOL registration by name");

    /* Ensure connector is not pre-registered */
    if ((is_registered = H5VLis_connector_registered_by_name(GEOTIFF_VOL_CONNECTOR_NAME)) < 0)
        TEST_ERROR;
    if (true == is_registered) {
        if ((pre_id = H5VLget_connector_id_by_name(GEOTIFF_VOL_CONNECTOR_NAME)) < 0)
            TEST_ERROR;
        if (H5VLunregister_connector(pre_id) < 0)
            TEST_ERROR;
    }

    /* Register the connector by name */
    if ((vol_id = H5VLregister_connector_by_name(GEOTIFF_VOL_CONNECTOR_NAME, H5P_DEFAULT)) < 0)
        TEST_ERROR;

    /* The connector should be registered now */
    if ((is_registered = H5VLis_connector_registered_by_name(GEOTIFF_VOL_CONNECTOR_NAME)) < 0)
        TEST_ERROR;
    if (false == is_registered)
        FAIL_PUTS_ERROR("VOL connector was not registered");

    /* Unregister the connector */
    if (H5VLunregister_connector(vol_id) < 0)
        TEST_ERROR;

    /* The connector should not be registered now */
    if ((is_registered = H5VLis_connector_registered_by_name(GEOTIFF_VOL_CONNECTOR_NAME)) < 0)
        TEST_ERROR;
    if (true == is_registered)
        FAIL_PUTS_ERROR("VOL connector is inappropriately registered");

    PASSED();
    return SUCCEED;

error:
    H5E_BEGIN_TRY
    {
        H5VLunregister_connector(vol_id);
    }
    H5E_END_TRY;
    return FAIL;

} /* end test_registration_by_name() */

/*-------------------------------------------------------------------------
 * Function:    test_multiple_registration()
 *
 * Purpose:     Tests if we can register a VOL connector multiple times.
 *
 * Return:      SUCCEED/FAIL
 *
 *-------------------------------------------------------------------------
 */
#define N_REGISTRATIONS 10
static herr_t test_multiple_registration(void)
{
    htri_t is_registered = FAIL;
    hid_t vol_ids[N_REGISTRATIONS];
    int i;
    hid_t pre_id = H5I_INVALID_HID;

    TESTING("registering a VOL connector multiple times");

    /* Ensure connector is not pre-registered */
    if ((is_registered = H5VLis_connector_registered_by_name(GEOTIFF_VOL_CONNECTOR_NAME)) < 0)
        TEST_ERROR;
    if (true == is_registered) {
        if ((pre_id = H5VLget_connector_id_by_name(GEOTIFF_VOL_CONNECTOR_NAME)) < 0)
            TEST_ERROR;
        if (H5VLunregister_connector(pre_id) < 0)
            TEST_ERROR;
    }

    /* Register the connector multiple times */
    for (i = 0; i < N_REGISTRATIONS; i++) {
        if ((vol_ids[i] = H5VLregister_connector_by_name(GEOTIFF_VOL_CONNECTOR_NAME, H5P_DEFAULT)) <
            0)
            TEST_ERROR;
    }

    /* The connector should be registered now */
    if ((is_registered = H5VLis_connector_registered_by_name(GEOTIFF_VOL_CONNECTOR_NAME)) < 0)
        TEST_ERROR;
    if (false == is_registered)
        FAIL_PUTS_ERROR("VOL connector was not registered");

    /* Unregister the connector */
    for (i = 0; i < N_REGISTRATIONS; i++) {
        if (H5VLunregister_connector(vol_ids[i]) < 0)
            TEST_ERROR;
        /* Also test close on some of the IDs. This call currently works
         * identically to unregister.
         */
        i++;
        if (i < N_REGISTRATIONS) {
            if (H5VLclose(vol_ids[i]) < 0)
                TEST_ERROR;
        }
    }

    /* The connector should not be registered now */
    if ((is_registered = H5VLis_connector_registered_by_name(GEOTIFF_VOL_CONNECTOR_NAME)) < 0)
        TEST_ERROR;
    if (true == is_registered)
        FAIL_PUTS_ERROR("VOL connector is inappropriately registered");

    PASSED();
    return SUCCEED;

error:
    H5E_BEGIN_TRY
    {
        for (i = 0; i < N_REGISTRATIONS; i++)
            H5VLunregister_connector(vol_ids[i]);
    }
    H5E_END_TRY;
    return FAIL;

} /* end test_multiple_registration() */

/*-------------------------------------------------------------------------
 * Function:    test_getters()
 *
 * Purpose:     Tests H5VL getters
 *
 * Return:      SUCCEED/FAIL
 *
 *-------------------------------------------------------------------------
 */
static herr_t test_getters(void)
{
    htri_t is_registered = FAIL;
    hid_t vol_id = H5I_INVALID_HID;
    hid_t vol_id_out = H5I_INVALID_HID;
    hid_t pre_id = H5I_INVALID_HID;

    TESTING("VOL getters");

    /* Ensure connector is not pre-registered */
    if ((is_registered = H5VLis_connector_registered_by_name(GEOTIFF_VOL_CONNECTOR_NAME)) < 0)
        TEST_ERROR;
    if (true == is_registered) {
        if ((pre_id = H5VLget_connector_id_by_name(GEOTIFF_VOL_CONNECTOR_NAME)) < 0)
            TEST_ERROR;
        if (H5VLunregister_connector(pre_id) < 0)
            TEST_ERROR;
    }

    /* Register the connector by name */
    if ((vol_id = H5VLregister_connector_by_name(GEOTIFF_VOL_CONNECTOR_NAME, H5P_DEFAULT)) < 0)
        TEST_ERROR;

    /* Get the connector's ID by name */
    if ((vol_id_out = H5VLget_connector_id_by_name(GEOTIFF_VOL_CONNECTOR_NAME)) < 0)
        TEST_ERROR;

    /* Validate the returned ID is valid; avoid strict equality checks across versions */
    if (vol_id_out <= 0)
        FAIL_PUTS_ERROR("VOL connector ID (get-by-name) is invalid");

    /* Unregister the connector */
    if (H5VLunregister_connector(vol_id) < 0)
        TEST_ERROR;

    PASSED();
    return SUCCEED;

error:
    H5E_BEGIN_TRY
    {
        H5VLunregister_connector(vol_id);
    }
    H5E_END_TRY;
    return FAIL;

} /* end test_getters() */

/*-------------------------------------------------------------------------
 * Function:    main
 *
 * Purpose:     Tests VOL connector plugin operations
 *
 * Return:      EXIT_SUCCESS/EXIT_FAILURE
 *
 *-------------------------------------------------------------------------
 */
int main(void)
{
    int nerrors = 0;
    char *path = NULL;

    puts("Testing VOL connector plugin functionality.");

    path = get_env_dup("HDF5_PLUGIN_PATH");
    printf("HDF5_PLUGIN_PATH = ");
    if (path)
        printf("%s\n", path);
    else
        printf("NULL\n");
    if (path)
        free(path);

    nerrors += test_registration_by_name() < 0 ? 1 : 0;
    nerrors += test_registration_by_value() < 0 ? 1 : 0;
    nerrors += test_multiple_registration() < 0 ? 1 : 0;
    nerrors += test_getters() < 0 ? 1 : 0;

    if (nerrors) {
        printf("***** %d VOL connector plugin TEST%s FAILED! *****\n", nerrors,
               nerrors > 1 ? "S" : "");
        exit(EXIT_FAILURE);
    }

    puts("All VOL connector plugin tests passed.");

    exit(EXIT_SUCCESS);

} /* end main() */
