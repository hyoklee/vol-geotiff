/*
 * Purpose: Contains macros to facilitate error reporting for the GeoTIFF VOL connector.
 */

#ifndef GEOTIFF_VOL_ERR_H_
#define GEOTIFF_VOL_ERR_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "H5Epublic.h"
#include "H5Ipublic.h"

#include "H5pubconf.h"

/* Identifiers for HDF5's error API */
extern hid_t H5_geotiff_err_stack_g;
extern hid_t H5_geotiff_err_class_g;

#define HDF5_VOL_GEOTIFF_ERR_CLS_NAME "HDF5 GEOTIFF VOL"
#define HDF5_VOL_GEOTIFF_LIB_NAME "HDF5 GEOTIFF VOL"
#define HDF5_VOL_GEOTIFF_LIB_VER "1.0.0"

#define SUCCEED 0
#define FAIL (-1)

#ifndef FALSE
#define FALSE false
#endif
#ifndef TRUE
#define TRUE true
#endif

/* Error Macros */
#ifdef H5_NO_DEPRECATED_SYMBOLS

/*
 * Macro to push the current function to the current error stack
 * and then goto the "done" label, which should appear inside the
 * function. (v2 errors only)
 */
#define FUNC_GOTO_ERROR(err_major, err_minor, ret_val, ...)                                        \
    do {                                                                                           \
        H5E_auto2_t err_func;                                                                      \
                                                                                                   \
        /* Check whether automatic error reporting has been disabled */                            \
        (void) H5Eget_auto2(H5E_DEFAULT, &err_func, NULL);                                         \
        if (err_func) {                                                                            \
            if (H5_geotiff_err_stack_g >= 0 && H5_geotiff_err_class_g >= 0) {                      \
                H5Epush2(H5_geotiff_err_stack_g, __FILE__, __func__, __LINE__,                     \
                         H5_geotiff_err_class_g, err_major, err_minor, __VA_ARGS__);               \
            } else {                                                                               \
                fprintf(stderr, __VA_ARGS__);                                                      \
                fprintf(stderr, "\n");                                                             \
            }                                                                                      \
        }                                                                                          \
                                                                                                   \
        ret_value = ret_val;                                                                       \
        goto done;                                                                                 \
    } while (0)

/*
 * Macro to push the current function to the current error stack
 * without calling goto. This is used for handling the case where
 * an error occurs during cleanup past the "done" label inside a
 * function so that an infinite loop does not occur where goto
 * continually branches back to the label. (v2 errors only)
 */
#define FUNC_DONE_ERROR(err_major, err_minor, ret_val, ...)                                        \
    do {                                                                                           \
        H5E_auto2_t err_func;                                                                      \
                                                                                                   \
        /* Check whether automatic error reporting has been disabled */                            \
        (void) H5Eget_auto2(H5E_DEFAULT, &err_func, NULL);                                         \
        if (err_func) {                                                                            \
            if (H5_geotiff_err_stack_g >= 0 && H5_geotiff_err_class_g >= 0)                        \
                H5Epush2(H5_geotiff_err_stack_g, __FILE__, __func__, __LINE__,                     \
                         H5_geotiff_err_class_g, err_major, err_minor, __VA_ARGS__);               \
            else {                                                                                 \
                fprintf(stderr, __VA_ARGS__);                                                      \
                fprintf(stderr, "\n");                                                             \
            }                                                                                      \
        }                                                                                          \
                                                                                                   \
        ret_value = ret_val;                                                                       \
    } while (0)

/*
 * Macro to print out the GEOTIFF VOL connector's current error stack
 * and then clear it for future use. (v2 errors only)
 */
#define PRINT_ERROR_STACK                                                                          \
    do {                                                                                           \
        H5E_auto2_t err_func;                                                                      \
                                                                                                   \
        /* Check whether automatic error reporting has been disabled */                            \
        (void) H5Eget_auto2(H5E_DEFAULT, &err_func, NULL);                                         \
        if (err_func) {                                                                            \
            if ((H5_geotiff_err_stack_g >= 0) && (H5Eget_num(H5_geotiff_err_stack_g) > 0)) {       \
                H5Eprint2(H5_geotiff_err_stack_g, NULL);                                           \
                H5Eclear2(H5_geotiff_err_stack_g);                                                 \
            }                                                                                      \
        }                                                                                          \
    } while (0)

#else

/*
 * Macro to push the current function to the current error stack
 * and then goto the "done" label, which should appear inside the
 * function. (compatible with v1 and v2 errors)
 */
#define FUNC_GOTO_ERROR(err_major, err_minor, ret_val, ...)                                        \
    do {                                                                                           \
        unsigned is_v2_err;                                                                        \
        union {                                                                                    \
            H5E_auto1_t err_func_v1;                                                               \
            H5E_auto2_t err_func_v2;                                                               \
        } err_func;                                                                                \
                                                                                                   \
        /* Determine version of error */                                                           \
        (void) H5Eauto_is_v2(H5E_DEFAULT, &is_v2_err);                                             \
                                                                                                   \
        if (is_v2_err)                                                                             \
            (void) H5Eget_auto2(H5E_DEFAULT, &err_func.err_func_v2, NULL);                         \
        else                                                                                       \
            (void) H5Eget_auto1(&err_func.err_func_v1, NULL);                                      \
                                                                                                   \
        /* Check whether automatic error reporting has been disabled */                            \
        if ((is_v2_err && err_func.err_func_v2) || (!is_v2_err && err_func.err_func_v1)) {         \
            if (H5_geotiff_err_stack_g >= 0 && H5_geotiff_err_class_g >= 0) {                      \
                H5Epush2(H5_geotiff_err_stack_g, __FILE__, __func__, __LINE__,                     \
                         H5_geotiff_err_class_g, err_major, err_minor, __VA_ARGS__);               \
            } else {                                                                               \
                fprintf(stderr, __VA_ARGS__);                                                      \
                fprintf(stderr, "\n");                                                             \
            }                                                                                      \
        }                                                                                          \
                                                                                                   \
        ret_value = ret_val;                                                                       \
        goto done;                                                                                 \
    } while (0)

/*
 * Macro to push the current function to the current error stack
 * without calling goto. This is used for handling the case where
 * an error occurs during cleanup past the "done" label inside a
 * function so that an infinite loop does not occur where goto
 * continually branches back to the label. (compatible with v1
 * and v2 errors)
 */
#define FUNC_DONE_ERROR(err_major, err_minor, ret_val, ...)                                        \
    do {                                                                                           \
        unsigned is_v2_err;                                                                        \
        union {                                                                                    \
            H5E_auto1_t err_func_v1;                                                               \
            H5E_auto2_t err_func_v2;                                                               \
        } err_func;                                                                                \
                                                                                                   \
        /* Determine version of error */                                                           \
        (void) H5Eauto_is_v2(H5E_DEFAULT, &is_v2_err);                                             \
                                                                                                   \
        if (is_v2_err)                                                                             \
            (void) H5Eget_auto2(H5E_DEFAULT, &err_func.err_func_v2, NULL);                         \
        else                                                                                       \
            (void) H5Eget_auto1(&err_func.err_func_v1, NULL);                                      \
                                                                                                   \
        /* Check whether automatic error reporting has been disabled */                            \
        if ((is_v2_err && err_func.err_func_v2) || (!is_v2_err && err_func.err_func_v1)) {         \
            if (H5_geotiff_err_stack_g >= 0 && H5_geotiff_err_class_g >= 0) {                      \
                H5Epush2(H5_geotiff_err_stack_g, __FILE__, __func__, __LINE__,                     \
                         H5_geotiff_err_class_g, err_major, err_minor, __VA_ARGS__);               \
            } else {                                                                               \
                fprintf(stderr, __VA_ARGS__);                                                      \
                fprintf(stderr, "\n");                                                             \
            }                                                                                      \
        }                                                                                          \
                                                                                                   \
        ret_value = ret_val;                                                                       \
    } while (0)

/*
 * Macro to print out the GEOTIFF VOL connector's current error stack
 * and then clear it for future use. (compatible with v1 and v2 errors)
 */
#define PRINT_ERROR_STACK                                                                          \
    do {                                                                                           \
        unsigned is_v2_err;                                                                        \
        union {                                                                                    \
            H5E_auto1_t err_func_v1;                                                               \
            H5E_auto2_t err_func_v2;                                                               \
        } err_func;                                                                                \
                                                                                                   \
        /* Determine version of error */                                                           \
        (void) H5Eauto_is_v2(H5E_DEFAULT, &is_v2_err);                                             \
                                                                                                   \
        if (is_v2_err)                                                                             \
            (void) H5Eget_auto2(H5E_DEFAULT, &err_func.err_func_v2, NULL);                         \
        else                                                                                       \
            (void) H5Eget_auto1(&err_func.err_func_v1, NULL);                                      \
                                                                                                   \
        /* Check whether automatic error reporting has been disabled */                            \
        if ((is_v2_err && err_func.err_func_v2) || (!is_v2_err && err_func.err_func_v1)) {         \
            if ((H5_geotiff_err_stack_g >= 0) && (H5Eget_num(H5_geotiff_err_stack_g) > 0)) {       \
                H5Eprint2(H5_geotiff_err_stack_g, NULL);                                           \
                H5Eclear2(H5_geotiff_err_stack_g);                                                 \
            }                                                                                      \
        }                                                                                          \
    } while (0)

#endif /* H5_NO_DEPRECATED_SYMBOLS */

/*
 * Macro to simply jump to the "done" label inside the function,
 * setting ret_value to the given value. This is often used for
 * short circuiting in functions when certain conditions arise.
 */
#define FUNC_GOTO_DONE(ret_val)                                                                    \
    do {                                                                                           \
        ret_value = ret_val;                                                                       \
        goto done;                                                                                 \
    } while (0)

/* Error handling macros for the GEOTIFF VOL test suite */

/*
 * Print the current location on the standard output stream.
 */
#define AT() printf("   at %s:%d in %s()...\n", __FILE__, __LINE__, __func__);

/*
 * The name of the test is printed by saying TESTING("something") which will
 * result in the string `Testing something' being flushed to standard output.
 * If a test passes, fails, or is skipped then the PASSED(), H5_FAILED(), or
 * SKIPPED() macro should be called.  After H5_FAILED() or SKIPPED() the caller
 * should print additional information to stdout indented by at least four
 * spaces.
 */
#define TESTING(S)                                                                                 \
    {                                                                                              \
        printf("Testing %-66s", S);                                                                \
        fflush(stdout);                                                                            \
    }
#define PASSED()                                                                                   \
    {                                                                                              \
        puts("PASSED");                                                                            \
        fflush(stdout);                                                                            \
    }
#define H5_FAILED()                                                                                \
    {                                                                                              \
        puts("*FAILED*");                                                                          \
        fflush(stdout);                                                                            \
    }
#define SKIPPED()                                                                                  \
    {                                                                                              \
        puts("- SKIPPED -");                                                                       \
        fflush(stdout);                                                                            \
    }

#define TEST_ERROR                                                                                 \
    {                                                                                              \
        H5_FAILED();                                                                               \
        AT();                                                                                      \
        goto error;                                                                                \
    }

#ifdef __cplusplus
}
#endif

#endif /* GEOTIFF_VOL_ERR_H_ */