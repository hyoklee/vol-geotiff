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
#include "geotiff_vol_connector.h"
#include "test_runner.h"
#include <H5PLpublic.h>

#include <stdlib.h>
#include <string.h>

/* herr_t values from H5private.h */
#define SUCCEED 0

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
herr_t test_registration_by_value(void)
{
    htri_t is_registered = FAIL;
    hid_t vol_id = H5I_INVALID_HID;
    hid_t pre_id = H5I_INVALID_HID;

    TESTING("VOL registration by value");

    /* Add the plugin path so HDF5 can find the connector */
#ifdef GEOTIFF_VOL_PLUGIN_PATH
    if (H5PLappend(GEOTIFF_VOL_PLUGIN_PATH) < 0)
        TEST_ERROR;
#endif

    /* Ensure connector is not pre-registered (CI/env may set defaults) */
    if ((is_registered = H5VLis_connector_registered_by_name(GEOTIFF_VOL_CONNECTOR_NAME)) < 0)
        TEST_ERROR;
    if (true == is_registered) {
        if ((pre_id = H5VLget_connector_id_by_name(GEOTIFF_VOL_CONNECTOR_NAME)) < 0)
            TEST_ERROR;
        if (H5VLclose(pre_id) < 0)
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
herr_t test_registration_by_name(void)
{
    htri_t is_registered = FAIL;
    hid_t vol_id = H5I_INVALID_HID;
    hid_t pre_id = H5I_INVALID_HID;

    TESTING("VOL registration by name");

    /* Add the plugin path so HDF5 can find the connector */
#ifdef GEOTIFF_VOL_PLUGIN_PATH
    if (H5PLappend(GEOTIFF_VOL_PLUGIN_PATH) < 0)
        TEST_ERROR;
#endif

    /* Ensure connector is not pre-registered */
    if ((is_registered = H5VLis_connector_registered_by_name(GEOTIFF_VOL_CONNECTOR_NAME)) < 0)
        TEST_ERROR;
    if (true == is_registered) {
        if ((pre_id = H5VLget_connector_id_by_name(GEOTIFF_VOL_CONNECTOR_NAME)) < 0)
            TEST_ERROR;
        if (H5VLclose(pre_id) < 0)
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
herr_t test_multiple_registration(void)
{
    htri_t is_registered = FAIL;
    hid_t vol_ids[N_REGISTRATIONS];
    int i;
    hid_t pre_id = H5I_INVALID_HID;

    TESTING("registering a VOL connector multiple times");

    /* Add the plugin path so HDF5 can find the connector */
#ifdef GEOTIFF_VOL_PLUGIN_PATH
    if (H5PLappend(GEOTIFF_VOL_PLUGIN_PATH) < 0)
        TEST_ERROR;
#endif

    /* Ensure connector is not pre-registered */
    if ((is_registered = H5VLis_connector_registered_by_name(GEOTIFF_VOL_CONNECTOR_NAME)) < 0)
        TEST_ERROR;
    if (true == is_registered) {
        if ((pre_id = H5VLget_connector_id_by_name(GEOTIFF_VOL_CONNECTOR_NAME)) < 0)
            TEST_ERROR;
        if (H5VLclose(pre_id) < 0)
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
herr_t test_getters(void)
{
    htri_t is_registered = FAIL;
    hid_t vol_id = H5I_INVALID_HID;
    hid_t vol_id_out = H5I_INVALID_HID;
    hid_t pre_id = H5I_INVALID_HID;

    TESTING("VOL getters");

    /* Add the plugin path so HDF5 can find the connector */
#ifdef GEOTIFF_VOL_PLUGIN_PATH
    if (H5PLappend(GEOTIFF_VOL_PLUGIN_PATH) < 0)
        TEST_ERROR;
#endif

    /* Ensure connector is not pre-registered */
    if ((is_registered = H5VLis_connector_registered_by_name(GEOTIFF_VOL_CONNECTOR_NAME)) < 0)
        TEST_ERROR;
    if (true == is_registered) {
        if ((pre_id = H5VLget_connector_id_by_name(GEOTIFF_VOL_CONNECTOR_NAME)) < 0)
            TEST_ERROR;
        if (H5VLclose(pre_id) < 0)
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

    /* Close the ID we got from the getter (this decrements the reference count) */
    if (H5VLclose(vol_id_out) < 0)
        TEST_ERROR;

    /* Unregister the connector */
    if (H5VLunregister_connector(vol_id) < 0)
        TEST_ERROR;

    /* Verify that connector is truly unregistered */
    if ((is_registered = H5VLis_connector_registered_by_name(GEOTIFF_VOL_CONNECTOR_NAME)) < 0)
        TEST_ERROR;

    if (true == is_registered)
        FAIL_PUTS_ERROR("VOL connector could not be unregistered");

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
