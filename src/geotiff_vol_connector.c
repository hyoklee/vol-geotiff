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

/* Purpose:     HDF5 Virtual Object Layer (VOL) connector for GeoTIFF files
 *              Enables reading GeoTIFF files through HDF5 tools and netCDF-C
 */

/* This connector's header */
#include "geotiff_vol_connector.h"

#include <H5PLextern.h>
#include <assert.h>
#include <geotiff/xtiffio.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _MSC_VER
#ifndef strdup
#define strdup _strdup
#endif
/* MSVC doesn't support __attribute__ */
#ifndef __attribute__
#define __attribute__(x)
#endif
#endif

#define SUCCEED 0

static hbool_t H5_geotiff_initialized_g = FALSE;

/* Identifiers for HDF5's error API */
hid_t H5_geotiff_err_stack_g = H5I_INVALID_HID;
hid_t H5_geotiff_err_class_g = H5I_INVALID_HID;
hid_t H5_geotiff_obj_err_maj_g = H5I_INVALID_HID;
hid_t H5_geotiff_parse_err_min_g = H5I_INVALID_HID;
hid_t H5_geotiff_link_table_err_min_g = H5I_INVALID_HID;
hid_t H5_geotiff_link_table_iter_err_min_g = H5I_INVALID_HID;
hid_t H5_geotiff_attr_table_err_min_g = H5I_INVALID_HID;
hid_t H5_geotiff_attr_table_iter_err_min_g = H5I_INVALID_HID;
hid_t H5_geotiff_object_table_err_min_g = H5I_INVALID_HID;
hid_t H5_geotiff_object_table_iter_err_min_g = H5I_INVALID_HID;

/* Helper functions */
static herr_t geotiff_read_image_data(geotiff_dataset_t *dset);
static herr_t geotiff_get_hdf5_type_from_tiff(uint16_t sample_format, uint16_t bits_per_sample,
                                              hid_t *type_id);

/* The VOL class struct */
static const H5VL_class_t geotiff_class_g = {
    3,                           /* VOL class struct version */
    GEOTIFF_VOL_CONNECTOR_VALUE, /* value                    */
    GEOTIFF_VOL_CONNECTOR_NAME,  /* name                     */
    1,                           /* version                  */
    0,                           /* capability flags         */
    geotiff_init_connector,      /* initialize               */
    geotiff_term_connector,      /* terminate                */
    {
        /* info_cls */
        (size_t) 0, /* size    */
        NULL,       /* copy    */
        NULL,       /* compare */
        NULL,       /* free    */
        NULL,       /* to_str  */
        NULL,       /* from_str */
    },
    {
        /* wrap_cls */
        NULL, /* get_object   */
        NULL, /* get_wrap_ctx */
        NULL, /* wrap_object  */
        NULL, /* unwrap_object */
        NULL, /* free_wrap_ctx */
    },
    {
        /* attribute_cls */
        NULL,              /* create       */
        geotiff_attr_open, /* open         */
        geotiff_attr_read, /* read         */
        NULL,              /* write        */
        geotiff_attr_get,  /* get          */
        NULL,              /* specific     */
        NULL,              /* optional     */
        geotiff_attr_close /* close        */
    },
    {
        /* dataset_cls */
        NULL,                 /* create       */
        geotiff_dataset_open, /* open         */
        geotiff_dataset_read, /* read         */
        NULL,                 /* write        */
        geotiff_dataset_get,  /* get          */
        NULL,                 /* specific     */
        NULL,                 /* optional     */
        geotiff_dataset_close /* close        */
    },
    {
        /* datatype_cls */
        NULL, /* commit       */
        NULL, /* open         */
        NULL, /* get_size     */
        NULL, /* specific     */
        NULL, /* optional     */
        NULL  /* close        */
    },
    {
        /* file_cls */
        geotiff_file_create, /* create       */
        geotiff_file_open,   /* open         */
        geotiff_file_get,    /* get          */
        NULL,                /* specific     */
        NULL,                /* optional     */
        geotiff_file_close   /* close        */
    },
    {
        /* group_cls */
        NULL,               /* create       */
        geotiff_group_open, /* open         */
        geotiff_group_get,  /* get          */
        NULL,               /* specific     */
        NULL,               /* optional     */
        geotiff_group_close /* close        */
    },
    {
        /* link_cls */
        NULL,                  /* create       */
        NULL,                  /* copy         */
        NULL,                  /* move         */
        NULL,                  /* get          */
        geotiff_link_specific, /* specific     */
        NULL                   /* optional     */
    },
    {
        /* object_cls */
        NULL, /* open         */
        NULL, /* copy         */
        NULL, /* get          */
        NULL, /* specific     */
        NULL  /* optional     */
    },
    {
        /* introscpect_cls */
        geotiff_introspect_get_conn_cls, /* get_conn_cls  */
        NULL,                            /* get_cap_flags */
        geotiff_introspect_opt_query     /* opt_query     */
    },
    {
        /* request_cls */
        NULL, /* wait         */
        NULL, /* notify       */
        NULL, /* cancel       */
        NULL, /* specific     */
        NULL, /* optional     */
        NULL  /* free         */
    },
    {
        /* blob_cls */
        NULL, /* put          */
        NULL, /* get          */
        NULL, /* specific     */
        NULL  /* optional     */
    },
    {
        /* token_cls */
        NULL, /* cmp          */
        NULL, /* to_str       */
        NULL  /* from_str     */
    },
    NULL /* optional     */
};

/* Helper function to get HDF5 type from TIFF sample format and bits per sample */
herr_t geotiff_get_hdf5_type_from_tiff(uint16_t sample_format, uint16_t bits_per_sample,
                                       hid_t *type_id)
{
    herr_t ret_value = SUCCEED;
    hid_t new_type = H5I_INVALID_HID;
    hid_t predef_type = H5I_INVALID_HID;
    assert(type_id);

    switch (sample_format) {
        case SAMPLEFORMAT_UINT:
            switch (bits_per_sample) {
                case 8:
                    predef_type = H5T_NATIVE_UCHAR;
                    break;
                case 16:
                    predef_type = H5T_NATIVE_USHORT;
                    break;
                case 32:
                    predef_type = H5T_NATIVE_UINT;
                    break;
                case 64:
                    predef_type = H5T_NATIVE_UINT64;
                    break;
                default:
                    predef_type = H5T_NATIVE_UCHAR;
                    break;
            }
            break;
        case SAMPLEFORMAT_INT:
            switch (bits_per_sample) {
                case 8:
                    predef_type = H5T_NATIVE_CHAR;
                    break;
                case 16:
                    predef_type = H5T_NATIVE_SHORT;
                    break;
                case 32:
                    predef_type = H5T_NATIVE_INT;
                    break;
                case 64:
                    predef_type = H5T_NATIVE_INT64;
                    break;
                default:
                    predef_type = H5T_NATIVE_CHAR;
                    break;
            }
            break;
        case SAMPLEFORMAT_IEEEFP:
            switch (bits_per_sample) {
                case 32:
                    predef_type = H5T_NATIVE_FLOAT;
                    break;
                case 64:
                    predef_type = H5T_NATIVE_DOUBLE;
                    break;
                default:
                    predef_type = H5T_NATIVE_FLOAT;
                    break;
            }
            break;

        default:
            predef_type = H5T_NATIVE_UCHAR;
            break;
    }

    if ((new_type = H5Tcopy(predef_type)) < 0)
        FUNC_GOTO_ERROR(H5E_DATATYPE, H5E_CANTCOPY, FAIL, "Failed to copy predef_type datatype");

    *type_id = new_type;

done:
    if (ret_value < 0 && new_type != H5I_INVALID_HID) {
        H5E_BEGIN_TRY
        {
            H5Tclose(new_type);
        }
        H5E_END_TRY;
    }
    return ret_value;
}

/* File operations */
void *geotiff_file_create(const char __attribute__((unused)) * name,
                          unsigned __attribute__((unused)) flags,
                          hid_t __attribute__((unused)) fcpl_id,
                          hid_t __attribute__((unused)) fapl_id,
                          hid_t __attribute__((unused)) dxpl_id,
                          void __attribute__((unused)) * *req)
{
    return NULL;
}

void *geotiff_file_open(const char *name, unsigned flags, hid_t fapl_id,
                        hid_t __attribute__((unused)) dxpl_id, void __attribute__((unused)) * *req)
{
    geotiff_file_t *file = NULL;
    geotiff_file_t *ret_value = NULL;

    /* We only support read-only access for GeoTIFF files */
    if (flags != H5F_ACC_RDONLY)
        FUNC_GOTO_ERROR(H5E_FILE, H5E_UNSUPPORTED, NULL,
                        "GeoTIFF VOL connector only supports read-only access");

    if ((file = (geotiff_file_t *) calloc(1, sizeof(geotiff_file_t))) == NULL)
        FUNC_GOTO_ERROR(H5E_FILE, H5E_CANTALLOC, NULL,
                        "Failed to allocate memory for GeoTIFF file struct");

    if ((file->tiff = XTIFFOpen(name, "r")) == NULL)
        FUNC_GOTO_ERROR(H5E_FILE, H5E_CANTOPENFILE, NULL, "Failed to open GeoTIFF file: %s", name);

    if ((file->filename = strdup(name)) == NULL)
        FUNC_GOTO_ERROR(H5E_FILE, H5E_CANTALLOC, NULL, "Failed to duplicate filename string");

    file->flags = flags;
    file->plist_id = fapl_id;

    ret_value = file;

done:
    if (!ret_value) {
        if (file) {
            H5E_BEGIN_TRY
            {
                geotiff_file_close(file, dxpl_id, req);
            }
            H5E_END_TRY;
        }
    }

    return ret_value;
}

// cppcheck-suppress constParameterCallback
herr_t geotiff_file_get(void *file, H5VL_file_get_args_t *args,
                        hid_t __attribute__((unused)) dxpl_id, void __attribute__((unused)) * *req)
{
    const geotiff_file_t *f = (const geotiff_file_t *) file;
    herr_t ret_value = SUCCEED;

    switch (args->op_type) {
        case H5VL_FILE_GET_NAME:
            /* HDF5 1.14+/develop uses buf, buf_size, buf_len */
            if (args->args.get_name.buf && args->args.get_name.buf_size > 0) {
                size_t ncopy = strlen(f->filename);
                if (ncopy >= args->args.get_name.buf_size)
                    ncopy = args->args.get_name.buf_size - 1;
                memcpy(args->args.get_name.buf, f->filename, ncopy);
                args->args.get_name.buf[ncopy] = '\0';
            }
            /* Some HDF5 versions may not provide buf_len. If available, setting it is optional. */
            break;
        default:
            FUNC_GOTO_ERROR(H5E_FILE, H5E_UNSUPPORTED, FAIL, "Unsupported file get operation");
    }
done:
    return ret_value;
}

herr_t geotiff_file_close(void *file, hid_t __attribute__((unused)) dxpl_id,
                          void __attribute__((unused)) * *req)
{
    geotiff_file_t *f = (geotiff_file_t *) file;

    if (f) {
        if (f->tiff)
            TIFFClose(f->tiff);
        if (f->filename)
            free(f->filename);
        free(f);
    }

    return SUCCEED;
}

/* Dataset operations */
void *geotiff_dataset_open(void *obj, const H5VL_loc_params_t __attribute__((unused)) * loc_params,
                           const char *name, hid_t __attribute__((unused)) dapl_id,
                           hid_t __attribute__((unused)) dxpl_id,
                           void __attribute__((unused)) * *req)
{
    geotiff_file_t *file = (geotiff_file_t *) obj;
    geotiff_dataset_t *dset = NULL;
    geotiff_dataset_t *ret_value = NULL;

    uint32_t width = 0;
    uint32_t height = 0;
    uint16_t samples_per_pixel = 0;
    uint16_t bits_per_sample = 0;
    uint16_t sample_format = 0;

    uint16_t num_dirs = 0;
    hsize_t dims[3] = {0, 0, 0};

    H5T_class_t dtype_class = H5T_NO_CLASS;

    if (!file || !name) {
        FUNC_GOTO_ERROR(H5E_ARGS, H5E_BADVALUE, NULL, "Invalid file or dataset name");
    }

    if ((dset = (geotiff_dataset_t *) malloc(sizeof(geotiff_dataset_t))) == NULL) {
        FUNC_GOTO_ERROR(H5E_DATASET, H5E_CANTALLOC, NULL,
                        "Failed to allocate memory for GeoTIFF dataset struct");
    }

    if ((dset->name = strdup(name)) == NULL) {
        FUNC_GOTO_ERROR(H5E_DATASET, H5E_CANTALLOC, NULL,
                        "Failed to duplicate dataset name string");
    }

    dset->file = file;
    dset->data = NULL;
    dset->data_size = 0;
    dset->is_image = false;
    dset->space_id = H5I_INVALID_HID;
    dset->type_id = H5I_INVALID_HID;
    dset->gtif = NULL;
    dset->directory_index = -1;

    /* Parse dataset name to extract image index (e.g., "image0", "/image1", etc.) */
    int image_index = 0;
    if (sscanf(name, "/image%d", &image_index) == 1 || sscanf(name, "image%d", &image_index) == 1) {
        if (image_index < 0)
            FUNC_GOTO_ERROR(H5E_DATASET, H5E_BADVALUE, NULL,
                            "Invalid image index %d (must be non-negative)", image_index);
    } else {
        FUNC_GOTO_ERROR(H5E_DATASET, H5E_BADVALUE, NULL,
                        "Invalid dataset name '%s', expected 'imageN' format", name);
    }

    /* Check if this image directory exists in the TIFF file */
    num_dirs = TIFFNumberOfDirectories(file->tiff);
    if (image_index >= num_dirs) {
        FUNC_GOTO_ERROR(H5E_DATASET, H5E_NOTFOUND, NULL,
                        "Image %d does not exist (file has %d image%s)", image_index, num_dirs,
                        num_dirs == 1 ? "" : "s");
    }

    /* Navigate to the correct TIFF directory for this image */
    if (!TIFFSetDirectory(file->tiff, (uint16_t) image_index)) {
        FUNC_GOTO_ERROR(H5E_DATASET, H5E_CANTGET, NULL, "Failed to set TIFF directory to %d",
                        image_index);
    }

    /* Create GeoTIFF handle for this directory's geo keys
     * GTIF handles are per-dataset to support multi-image
     * files where each image may have different geo keys */
    if ((dset->gtif = GTIFNew(file->tiff)) == NULL) {
        FUNC_GOTO_ERROR(H5E_DATASET, H5E_CANTGET, NULL,
                        "Failed to create GeoTIFF handle for image %d", image_index);
    }

    dset->is_image = true;
    dset->directory_index = image_index;

    /* Read image metadata from TIFF tags (for this directory) */
    if (!TIFFGetField(file->tiff, TIFFTAG_IMAGEWIDTH, &width))
        FUNC_GOTO_ERROR(H5E_DATASET, H5E_CANTGET, NULL, "Failed to get image width from TIFF");
    if (!TIFFGetField(file->tiff, TIFFTAG_IMAGELENGTH, &height))
        FUNC_GOTO_ERROR(H5E_DATASET, H5E_CANTGET, NULL, "Failed to get image height from TIFF");

    if (width == 0 || height == 0 || width > 65535 || height > 65535)
        FUNC_GOTO_ERROR(H5E_DATASET, H5E_CANTGET, NULL, "Invalid image dimensions");

    TIFFGetFieldDefaulted(file->tiff, TIFFTAG_SAMPLESPERPIXEL, &samples_per_pixel);
    TIFFGetFieldDefaulted(file->tiff, TIFFTAG_BITSPERSAMPLE, &bits_per_sample);
    TIFFGetFieldDefaulted(file->tiff, TIFFTAG_SAMPLEFORMAT, &sample_format);

    /* Validate TIFF configuration - check for unsupported features */
    {
        uint16_t planar_config = 0;
        uint16_t photometric = 0;
        uint16_t *extra_samples = NULL;
        uint16_t extra_sample_count = 0;

        /* Check planar configuration - we only support contiguous (interleaved) data */
        TIFFGetFieldDefaulted(file->tiff, TIFFTAG_PLANARCONFIG, &planar_config);
        if (planar_config != PLANARCONFIG_CONTIG)
            FUNC_GOTO_ERROR(H5E_DATASET, H5E_UNSUPPORTED, NULL,
                            "Unsupported planar configuration: %d (only PLANARCONFIG_CONTIG is "
                            "supported)",
                            planar_config);

        /* Check photometric interpretation - validate we support this color space */
        if (TIFFGetField(file->tiff, TIFFTAG_PHOTOMETRIC, &photometric)) {
            if (photometric != PHOTOMETRIC_MINISBLACK && photometric != PHOTOMETRIC_RGB &&
                photometric != PHOTOMETRIC_MINISWHITE)
                FUNC_GOTO_ERROR(H5E_DATASET, H5E_UNSUPPORTED, NULL,
                                "Unsupported photometric interpretation: %d (only MINISBLACK, "
                                "MINISWHITE, and RGB "
                                "are supported)",
                                photometric);
        }

        /* Check for extra samples that would complicate interpretation */
        if (TIFFGetField(file->tiff, TIFFTAG_EXTRASAMPLES, &extra_sample_count, &extra_samples)) {
            /* Allow alpha channel (extra_sample_count == 1) for RGBA, but warn about others */
            if (samples_per_pixel == 4 && extra_sample_count == 1) {
                /* This is likely RGBA with alpha channel - acceptable */
                if (extra_samples[0] != EXTRASAMPLE_ASSOCALPHA &&
                    extra_samples[0] != EXTRASAMPLE_UNASSALPHA)
                    FUNC_GOTO_ERROR(H5E_DATASET, H5E_UNSUPPORTED, NULL,
                                    "Unsupported extra sample type: %d (expected alpha channel)",
                                    extra_samples[0]);
            } else if (extra_sample_count > 0) {
                FUNC_GOTO_ERROR(H5E_DATASET, H5E_UNSUPPORTED, NULL,
                                "Unsupported configuration: %d extra samples found (only alpha "
                                "channel for RGBA is supported)",
                                extra_sample_count);
            }
        }

        /* Validate bits per sample is byte-aligned */
        if (bits_per_sample % 8 != 0)
            FUNC_GOTO_ERROR(H5E_DATASET, H5E_UNSUPPORTED, NULL,
                            "Unsupported bits per sample: %d (must be a multiple of 8)",
                            bits_per_sample);
    }

    if (geotiff_get_hdf5_type_from_tiff(sample_format, bits_per_sample, &dset->type_id) < 0)
        FUNC_GOTO_ERROR(H5E_DATASET, H5E_CANTGET, NULL,
                        "Failed to get HDF5 datatype from TIFF sample format");

    if ((dtype_class = H5Tget_class(dset->type_id)) < 0)
        FUNC_GOTO_ERROR(H5E_DATASET, H5E_CANTGET, NULL, "Failed to get datatype class");

    if (dtype_class != H5T_INTEGER && dtype_class != H5T_FLOAT)
        FUNC_GOTO_ERROR(H5E_DATASET, H5E_UNSUPPORTED, NULL,
                        "Unsupported datatype class for GeoTIFF image data");

    /* Create dataspace based on samples per pixel */
    if (samples_per_pixel == 1) {
        /* Grayscale: 2D dataspace [height, width] */
        dims[0] = height;
        dims[1] = width;
        if ((dset->space_id = H5Screate_simple(2, dims, NULL)) < 0) {
            FUNC_GOTO_ERROR(H5E_DATASET, H5E_CANTCREATE, NULL,
                            "Failed to create dataspace for grayscale dataset");
        }
    } else if (samples_per_pixel == 3 || samples_per_pixel == 4) {
        /* RGB or RGBA: 3D dataspace [height, width, samples] */
        dims[0] = height;
        dims[1] = width;
        dims[2] = samples_per_pixel;
        if ((dset->space_id = H5Screate_simple(3, dims, NULL)) < 0) {
            FUNC_GOTO_ERROR(H5E_DATASET, H5E_CANTCREATE, NULL,
                            "Failed to create dataspace for RGB/RGBA dataset");
        }
    } else {
        FUNC_GOTO_ERROR(H5E_DATASET, H5E_UNSUPPORTED, NULL,
                        "Unsupported samples per pixel: %d (only 1, 3, or 4 supported)",
                        samples_per_pixel);
    }

    if (geotiff_read_image_data(dset) < 0)
        FUNC_GOTO_ERROR(H5E_DATASET, H5E_CANTGET, NULL, "failed to read image data");

    ret_value = dset;

done:
    if (!ret_value) {
        if (dset)
            H5E_BEGIN_TRY
            {
                geotiff_dataset_close(dset, dxpl_id, req);
            }
        H5E_END_TRY;
    }

    return ret_value;
}

/* Struct for H5Dscatter's callback that allows it to scatter from a non-global response buffer */
typedef struct response_read_info {
    void *buffer;
    void *read_size;
} response_read_info;

static herr_t dataset_read_scatter_op(const void **src_buf, size_t *src_buf_bytes_used,
                                      void *op_data)
{
    response_read_info *resp_info = (response_read_info *) op_data;
    *src_buf = resp_info->buffer;
    *src_buf_bytes_used = *((size_t *) resp_info->read_size);

    return 0;
} /* end dataset_read_scatter_op() */

/* Helper function: Prepare a buffer with data in the requested memory type.
 * If conversion is needed, allocates a new buffer and performs conversion.
 * If no conversion needed, returns pointer to original data.
 */
static herr_t prepare_converted_buffer(const geotiff_dataset_t *dset, hid_t mem_type_id,
                                       size_t num_elements, void **out_buffer,
                                       size_t *out_buffer_size, hbool_t *out_tconv_buf_allocated)
{
    herr_t ret_value = SUCCEED;
    htri_t types_equal = 0;
    size_t dataset_type_size = 0;
    size_t mem_type_size = 0;
    void *conversion_buf = NULL;

    if (!dset || !out_buffer || !out_buffer_size || !out_tconv_buf_allocated)
        FUNC_GOTO_ERROR(H5E_ARGS, H5E_BADVALUE, FAIL, "invalid arguments");

    /* Check if types are equal */
    if ((types_equal = H5Tequal(mem_type_id, dset->type_id)) < 0)
        FUNC_GOTO_ERROR(H5E_DATATYPE, H5E_CANTCOMPARE, FAIL, "failed to compare datatypes");

    if (types_equal) {
        /* No conversion needed - return borrowed pointer to cached data */
        *out_buffer = dset->data;
        *out_buffer_size = dset->data_size;
        *out_tconv_buf_allocated = FALSE;
        FUNC_GOTO_DONE(SUCCEED);
    }

    /* Conversion needed - allocate buffer and convert */
    if ((dataset_type_size = H5Tget_size(dset->type_id)) == 0)
        FUNC_GOTO_ERROR(H5E_DATATYPE, H5E_CANTGET, FAIL, "failed to get dataset type size");

    if ((mem_type_size = H5Tget_size(mem_type_id)) == 0)
        FUNC_GOTO_ERROR(H5E_DATATYPE, H5E_CANTGET, FAIL, "failed to get memory type size");

    /* Allocate buffer large enough for in-place conversion (max of src/dst) */
    size_t src_data_size = num_elements * dataset_type_size;
    size_t dst_data_size = num_elements * mem_type_size;
    size_t conversion_buf_size = (src_data_size > dst_data_size) ? src_data_size : dst_data_size;

    if ((conversion_buf = malloc(conversion_buf_size)) == NULL)
        FUNC_GOTO_ERROR(H5E_RESOURCE, H5E_CANTALLOC, FAIL,
                        "failed to allocate memory for datatype conversion");

    /* Copy source data */
    memcpy(conversion_buf, dset->data, src_data_size);

    /* Perform in-place conversion */
    if (H5Tconvert(dset->type_id, mem_type_id, num_elements, conversion_buf, NULL, H5P_DEFAULT) < 0)
        FUNC_GOTO_ERROR(H5E_DATATYPE, H5E_CANTCONVERT, FAIL,
                        "failed to convert data from dataset type to memory type");

    /* Return owned buffer */
    *out_buffer = conversion_buf;
    *out_buffer_size = dst_data_size;
    *out_tconv_buf_allocated = TRUE;
    conversion_buf = NULL; /* Transfer ownership */

done:
    if (conversion_buf)
        free(conversion_buf);

    return ret_value;
} /* end prepare_converted_buffer() */

/* Helper function: Transfer data from source buffer to user buffer.
 * Handles both simple memcpy (when both selections are ALL) and scatter operations.
 */
static herr_t transfer_data_to_user(const void *source_buf, size_t source_size, hid_t mem_type_id,
                                    hid_t mem_space_id, void *user_buf)
{
    herr_t ret_value = SUCCEED;
    H5S_sel_type mem_sel_type = H5S_SEL_ERROR;

    if (!source_buf || !user_buf)
        FUNC_GOTO_ERROR(H5E_ARGS, H5E_BADVALUE, FAIL, "invalid arguments");

    /* Determine if we can use simple memcpy or need scatter */
    if (mem_space_id == 0 || mem_space_id == H5S_ALL) {
        /* Simple case: copy entire buffer directly */
        memcpy(user_buf, source_buf, source_size);
        FUNC_GOTO_DONE(SUCCEED);
    }

    /* Check selection type */
    if ((mem_sel_type = H5Sget_select_type(mem_space_id)) < 0)
        FUNC_GOTO_ERROR(H5E_DATASPACE, H5E_CANTGET, FAIL,
                        "failed to get memory space selection type");

    if (mem_sel_type == H5S_SEL_ALL) {
        /* Simple case: copy entire buffer directly */
        memcpy(user_buf, source_buf, source_size);
    } else {
        /* Use scatter for non-trivial selections */
        response_read_info resp_info;
        resp_info.read_size = &source_size;
        resp_info.buffer = (void *) source_buf;

        if (H5Dscatter(dataset_read_scatter_op, &resp_info, mem_type_id, mem_space_id, user_buf) <
            0)
            FUNC_GOTO_ERROR(H5E_DATASET, H5E_READERROR, FAIL, "can't scatter data to user buffer");
    }

done:
    return ret_value;
} /* end transfer_data_to_user() */

herr_t geotiff_dataset_read(size_t __attribute__((unused)) count, void *dset[], hid_t mem_type_id[],
                            hid_t __attribute__((unused)) mem_space_id[], hid_t file_space_id[],
                            hid_t __attribute__((unused)) dxpl_id, void *buf[],
                            void __attribute__((unused)) * *req)
{
    const geotiff_dataset_t *d = (const geotiff_dataset_t *) dset[0];
    herr_t ret_value = SUCCEED;
    H5S_sel_type file_sel_type = H5S_SEL_ERROR;
    H5S_sel_type mem_sel_type = H5S_SEL_ERROR;
    hssize_t num_elements = 0;
    void *source_buf = NULL;
    size_t source_size = 0;
    hbool_t tconv_buf_allocated = FALSE;
    /* To follow H5S_ALL semantics, we set up local vars for effective values of mem/filespace */
    hid_t effective_file_space_id = file_space_id[0];
    hid_t effective_mem_space_id = mem_space_id[0];

    void *gathered_buf = NULL;

    if (!d || !buf[0])
        FUNC_GOTO_ERROR(H5E_DATASET, H5E_CANTGET, FAIL, "invalid dataset buffer");

    /* Set up dataspaces and element count */
    if (file_space_id[0] == 0) {
        file_sel_type = H5S_SEL_ALL;
    } else if ((file_sel_type = H5Sget_select_type(file_space_id[0])) < 0) {
        FUNC_GOTO_ERROR(H5E_DATASET, H5E_CANTGET, FAIL,
                        "failed to get file dataspace selection type");
    }

    if (mem_space_id[0] == 0) {
        mem_sel_type = H5S_SEL_ALL;
    } else if ((mem_sel_type = H5Sget_select_type(mem_space_id[0])) < 0) {
        FUNC_GOTO_ERROR(H5E_DATASET, H5E_CANTGET, FAIL,
                        "failed to get memory dataspace selection type");
    }

    if (file_sel_type == H5S_SEL_ALL && mem_sel_type == H5S_SEL_ALL) {
        num_elements = H5Sget_simple_extent_npoints(d->space_id);
        effective_file_space_id = d->space_id;
        effective_mem_space_id = d->space_id;
    } else if (file_sel_type == H5S_SEL_ALL && mem_sel_type != H5S_SEL_ALL) {
        num_elements = H5Sget_select_npoints(mem_space_id[0]);
        effective_file_space_id = d->space_id;
    } else if (file_sel_type != H5S_SEL_ALL && mem_sel_type == H5S_SEL_ALL) {
        num_elements = H5Sget_select_npoints(file_space_id[0]);
        effective_mem_space_id = d->space_id;
    } else {
        /* Both selections are provided - verify equivalence */
        num_elements = H5Sget_select_npoints(file_space_id[0]);
        if (num_elements != H5Sget_select_npoints(mem_space_id[0]))
            FUNC_GOTO_ERROR(H5E_DATASET, H5E_CANTGET, FAIL,
                            "file and memory selections have different number of points");
    }

    /* Prepare source buffer with type conversion if needed.
     * If we have a non-trivial file selection, we need the full dataset in the source buffer
     * for H5Dgather to extract the selection from. Otherwise, we only need num_elements.
     */
    size_t prepare_num_elements;
    hssize_t temp_npoints;

    if (file_sel_type != H5S_SEL_ALL && effective_file_space_id != d->space_id) {
        /* Will need to gather - prepare full dataset */
        if ((temp_npoints = H5Sget_simple_extent_npoints(d->space_id)) < 0)
            FUNC_GOTO_ERROR(H5E_DATASET, H5E_CANTGET, FAIL, "failed to get dataset extent");
        prepare_num_elements = (size_t) temp_npoints;
    } else {
        /* No gather needed - prepare only selected elements */
        if (num_elements < 0)
            FUNC_GOTO_ERROR(H5E_DATASET, H5E_CANTGET, FAIL, "invalid number of elements");
        prepare_num_elements = (size_t) num_elements;
    }

    if (prepare_converted_buffer(d, mem_type_id[0], prepare_num_elements, &source_buf, &source_size,
                                 &tconv_buf_allocated) < 0)
        FUNC_GOTO_ERROR(H5E_DATASET, H5E_CANTGET, FAIL, "failed to prepare converted buffer");

    /* If file selection is non-trivial (hyperslab, points), gather selected data first */
    if (file_sel_type != H5S_SEL_ALL && effective_file_space_id != d->space_id) {
        /* Allocate buffer for gathered data */
        if (num_elements < 0)
            FUNC_GOTO_ERROR(H5E_DATASET, H5E_CANTGET, FAIL,
                            "invalid number of elements for gather");
        size_t gathered_size = (size_t) num_elements * H5Tget_size(mem_type_id[0]);

        if ((gathered_buf = malloc(gathered_size)) == NULL)
            FUNC_GOTO_ERROR(H5E_RESOURCE, H5E_CANTALLOC, FAIL,
                            "failed to allocate buffer for gathered data");

        /* Gather selected data from source buffer according to file space selection.
         * Note: We pass file_space_id[0] which has the selection, and source_buf which
         * must be sized according to the full extent described by the selection's dataspace.
         */
        if (H5Dgather(file_space_id[0], source_buf, mem_type_id[0], gathered_size, gathered_buf,
                      NULL, NULL) < 0) {
            FUNC_GOTO_ERROR(H5E_DATASET, H5E_READERROR, FAIL, "failed to gather selected data");
        }

        /* Free original buffer if we allocated it for type conversion */
        if (tconv_buf_allocated && source_buf) {
            free(source_buf);
            source_buf = NULL;
        }

        /* Use gathered buffer as new source */
        source_buf = gathered_buf;
        source_size = gathered_size;
        tconv_buf_allocated = TRUE;
    }

    /* Transfer data to user buffer (handles selections via scatter if needed) */
    if (transfer_data_to_user(source_buf, source_size, mem_type_id[0], effective_mem_space_id,
                              buf[0]) < 0)
        FUNC_GOTO_ERROR(H5E_DATASET, H5E_READERROR, FAIL, "failed to transfer data to user buffer");

done:
    /* Clean up allocated buffer if needed */
    if (tconv_buf_allocated && source_buf)
        free(source_buf);

    if (ret_value < 0 && gathered_buf)
        free(gathered_buf);

    return ret_value;
}

// cppcheck-suppress constParameterCallback
herr_t geotiff_dataset_get(void *dset, H5VL_dataset_get_args_t *args,
                           hid_t __attribute__((unused)) dxpl_id,
                           void __attribute__((unused)) * *req)
{
    const geotiff_dataset_t *d = (const geotiff_dataset_t *) dset;
    herr_t ret_value = SUCCEED;

    switch (args->op_type) {
        case H5VL_DATASET_GET_SPACE:
            /* Return a copy of the dataspace */
            assert(d->space_id != H5I_INVALID_HID);

            args->args.get_space.space_id = H5Scopy(d->space_id);
            if (args->args.get_space.space_id < 0)
                FUNC_GOTO_ERROR(H5E_DATASET, H5E_CANTGET, FAIL, "Failed to copy dataspace");
            break;

        case H5VL_DATASET_GET_TYPE:
            /* Return a copy of the datatype */
            args->args.get_type.type_id = H5Tcopy(d->type_id);
            if (args->args.get_type.type_id < 0)
                FUNC_GOTO_ERROR(H5E_DATASET, H5E_CANTGET, FAIL, "Failed to copy datatype");
            break;

        default:
            FUNC_GOTO_ERROR(H5E_DATASET, H5E_UNSUPPORTED, FAIL,
                            "Unsupported dataset get operation");
            break;
    }
done:
    return ret_value;
}

herr_t geotiff_dataset_close(void *dset, hid_t __attribute__((unused)) dxpl_id,
                             void __attribute__((unused)) * *req)
{
    geotiff_dataset_t *d = (geotiff_dataset_t *) dset;
    herr_t ret_value = SUCCEED;

    if (d) {
        if (d->gtif)
            GTIFFree(d->gtif);
        if (d->name)
            free(d->name);
        if (d->data)
            free(d->data);
        /* Use FUNC_DONE_ERROR to try to complete resource release after failure */
        if (d->space_id != H5I_INVALID_HID)
            if (H5Sclose(d->space_id) < 0)
                FUNC_DONE_ERROR(H5E_DATASET, H5E_CLOSEERROR, FAIL, "Failed to close dataspace");
        if (d->type_id != H5I_INVALID_HID)
            if (H5Tclose(d->type_id) < 0)
                FUNC_DONE_ERROR(H5E_DATASET, H5E_CLOSEERROR, FAIL, "Failed to close datatype");

        free(d);
    }

    return ret_value;
}

/* Group operations */
void *geotiff_group_open(void *obj, const H5VL_loc_params_t __attribute__((unused)) * loc_params,
                         const char *name, hid_t __attribute__((unused)) gapl_id,
                         hid_t __attribute__((unused)) dxpl_id, void __attribute__((unused)) * *req)
{
    geotiff_file_t *file = (geotiff_file_t *) obj;
    geotiff_group_t *grp = NULL;
    geotiff_group_t *ret_value = NULL;

    if (!file || !name)
        FUNC_GOTO_ERROR(H5E_ARGS, H5E_BADVALUE, NULL, "Invalid file or group name");

    if (strcmp(name, "/") != 0)
        FUNC_GOTO_ERROR(H5E_ARGS, H5E_UNSUPPORTED, NULL,
                        "GeoTIFF VOL connector currently only supports root group '/'");

    if ((grp = (geotiff_group_t *) calloc(1, sizeof(geotiff_group_t))) == NULL)
        FUNC_GOTO_ERROR(H5E_SYM, H5E_CANTALLOC, NULL,
                        "Failed to allocate memory for GeoTIFF group struct");

    if ((grp->name = strdup(name)) == NULL)
        FUNC_GOTO_ERROR(H5E_SYM, H5E_CANTALLOC, NULL, "Failed to duplicate group name string");

    grp->file = file;

    ret_value = grp;
done:
    if (!ret_value && grp) {
        H5E_BEGIN_TRY
        {
            geotiff_group_close(grp, dxpl_id, req);
        }
        H5E_END_TRY;
    }

    return ret_value;
}

herr_t geotiff_group_get(void *obj, H5VL_group_get_args_t *args,
                         hid_t __attribute__((unused)) dxpl_id, void __attribute__((unused)) * *req)
{
    herr_t ret_value = SUCCEED;

    if (!args)
        FUNC_GOTO_ERROR(H5E_ARGS, H5E_BADVALUE, FAIL, "invalid arguments");

    switch (args->op_type) {
        case H5VL_GROUP_GET_INFO: {
            geotiff_group_t *grp = (geotiff_group_t *) obj;
            H5G_info_t *ginfo = args->args.get_info.ginfo;
            uint16_t num_dirs = 0;

            if (!grp || !ginfo)
                FUNC_GOTO_ERROR(H5E_ARGS, H5E_BADVALUE, FAIL, "invalid group or info pointer");

            if (!grp->file || !grp->file->tiff)
                FUNC_GOTO_ERROR(H5E_ARGS, H5E_BADVALUE, FAIL, "invalid file object");

            /* Get number of TIFF directories (images) in the file */
            num_dirs = TIFFNumberOfDirectories(grp->file->tiff);

            /* Fill in group info structure */
            ginfo->storage_type = H5G_STORAGE_TYPE_COMPACT;
            ginfo->nlinks = num_dirs; /* Number of image links (image0, image1, ...) */
            ginfo->max_corder = -1;   /* No creation order tracking */
            ginfo->mounted = false;   /* No files mounted on this group */

            break;
        }

        case H5VL_GROUP_GET_GCPL: {
            FUNC_GOTO_ERROR(H5E_VOL, H5E_UNSUPPORTED, FAIL, "GCPL get operation not supported");
        }

        default: {
            FUNC_GOTO_ERROR(H5E_VOL, H5E_UNSUPPORTED, FAIL, "unknown group get operation");
        }
    }

done:
    return ret_value;
}

herr_t geotiff_group_close(void *grp, hid_t __attribute__((unused)) dxpl_id,
                           void __attribute__((unused)) * *req)
{
    geotiff_group_t *g = (geotiff_group_t *) grp;

    if (g) {
        if (g->name)
            free(g->name);
        free(g);
    }

    return SUCCEED;
}

/* Attribute operations */
void *geotiff_attr_open(void *obj, const H5VL_loc_params_t __attribute__((unused)) * loc_params,
                        const char *name, hid_t __attribute__((unused)) aapl_id,
                        hid_t __attribute__((unused)) dxpl_id, void __attribute__((unused)) * *req)
{
    geotiff_file_t *file = (geotiff_file_t *) obj;
    geotiff_attr_t *attr = NULL;
    geotiff_attr_t *ret_value = NULL;

    if (!file || !name)
        FUNC_GOTO_ERROR(H5E_ATTR, H5E_BADVALUE, NULL, "Invalid file or attribute name");

    if ((attr = (geotiff_attr_t *) calloc(1, sizeof(geotiff_attr_t))) == NULL)
        FUNC_GOTO_ERROR(H5E_ATTR, H5E_CANTALLOC, NULL,
                        "Failed to allocate memory for GeoTIFF attribute struct");

    if ((attr->name = strdup(name)) == NULL)
        FUNC_GOTO_ERROR(H5E_ATTR, H5E_CANTALLOC, NULL, "Failed to duplicate attribute name string");

    if ((attr->space_id = H5Screate(H5S_SCALAR)) < 0)
        FUNC_GOTO_ERROR(H5E_ATTR, H5E_CANTCREATE, NULL,
                        "Failed to create scalar dataspace for attribute");

    attr->file = file;
    attr->data = NULL;
    attr->data_size = 0;
    attr->type_id = H5T_NATIVE_CHAR;

    ret_value = attr;
done:
    if (!ret_value && attr) {
        H5E_BEGIN_TRY
        {
            geotiff_attr_close(attr, dxpl_id, req);
        }
        H5E_END_TRY;
    }
    return ret_value;
}

// cppcheck-suppress constParameterCallback
herr_t geotiff_attr_read(void *attr, hid_t __attribute__((unused)) mem_type_id, void *buf,
                         hid_t __attribute__((unused)) dxpl_id, void __attribute__((unused)) * *req)
{
    const geotiff_attr_t *a = (const geotiff_attr_t *) attr;
    herr_t ret_value = SUCCEED;
    if (!a || !buf)
        FUNC_GOTO_ERROR(H5E_ATTR, H5E_BADVALUE, FAIL, "Invalid attribute or buffer");

    if (a->data && a->data_size > 0) {
        memcpy(buf, a->data, a->data_size);
    }

done:
    return ret_value;
}

// cppcheck-suppress constParameterCallback
herr_t geotiff_attr_get(void *obj, H5VL_attr_get_args_t *args,
                        hid_t __attribute__((unused)) dxpl_id, void __attribute__((unused)) * *req)
{
    const geotiff_attr_t *a = (const geotiff_attr_t *) obj;
    herr_t ret_value = SUCCEED;

    switch (args->op_type) {
        case H5VL_ATTR_GET_SPACE:
            if ((args->args.get_space.space_id = H5Scopy(a->space_id)) < 0)
                FUNC_GOTO_ERROR(H5E_ATTR, H5E_CANTGET, FAIL, "Failed to copy attribute dataspace");
            break;
        case H5VL_ATTR_GET_TYPE:
            if ((args->args.get_type.type_id = H5Tcopy(a->type_id)) < 0)
                FUNC_GOTO_ERROR(H5E_ATTR, H5E_CANTGET, FAIL, "Failed to copy attribute datatype");
            break;
        default:
            FUNC_GOTO_ERROR(H5E_ATTR, H5E_UNSUPPORTED, FAIL, "Unsupported attribute get operation");
            break;
    }

done:
    return ret_value;
}

herr_t geotiff_attr_close(void *attr, hid_t __attribute__((unused)) dxpl_id,
                          void __attribute__((unused)) * *req)
{
    geotiff_attr_t *a = (geotiff_attr_t *) attr;
    herr_t ret_value = SUCCEED;

    if (a) {
        if (a->name)
            free(a->name);
        if (a->data)
            free(a->data);
        /* Use FUNC_DONE_ERROR to try to complete resource release after failure */
        if (a->space_id != H5I_INVALID_HID)
            if (H5Sclose(a->space_id) < 0)
                FUNC_DONE_ERROR(H5E_ATTR, H5E_CLOSEERROR, FAIL,
                                "Failed to close attribute dataspace");
        if (a->type_id != H5I_INVALID_HID)
            if (H5Tclose(a->type_id) < 0)
                FUNC_DONE_ERROR(H5E_ATTR, H5E_CLOSEERROR, FAIL,
                                "Failed to close attribute datatype");
        free(a);
    }

    return ret_value;
}

/* Helper function to read hyperslab selection (bands/regions) from GeoTIFF */
herr_t geotiff_read_hyperslab(const geotiff_dataset_t *dset, const hsize_t *start,
                              const hsize_t __attribute__((unused)) * stride, const hsize_t *count,
                              const hsize_t *block, int ndims,
                              hid_t __attribute__((unused)) mem_type_id, void *buf)
{
    geotiff_file_t *file = dset->file;
    uint32_t width, height;
    uint16_t samples_per_pixel, bits_per_sample, sample_format;
    size_t elem_size;
    tsize_t scanline_size;
    herr_t ret_value = SUCCEED;
    unsigned char *scanline_buf = NULL;
    unsigned char *output = (unsigned char *) buf;
    hsize_t row_start, row_count, col_start, col_count, band_start, band_count;
    hsize_t band_idx;

    if (!file || !file->tiff || !buf)
        FUNC_GOTO_ERROR(H5E_DATASET, H5E_BADVALUE, FAIL, "Invalid file or buffer");

    /* Get TIFF dimensions */
    if (!TIFFGetField(file->tiff, TIFFTAG_IMAGEWIDTH, &width) ||
        !TIFFGetField(file->tiff, TIFFTAG_IMAGELENGTH, &height))
        FUNC_GOTO_ERROR(H5E_DATASET, H5E_CANTGET, FAIL, "Failed to get image dimensions from TIFF");

    TIFFGetFieldDefaulted(file->tiff, TIFFTAG_SAMPLESPERPIXEL, &samples_per_pixel);
    TIFFGetFieldDefaulted(file->tiff, TIFFTAG_BITSPERSAMPLE, &bits_per_sample);
    TIFFGetFieldDefaulted(file->tiff, TIFFTAG_SAMPLEFORMAT, &sample_format);

    elem_size = bits_per_sample / 8;
    scanline_size = TIFFScanlineSize(file->tiff);

    if (scanline_size <= 0 || elem_size == 0)
        FUNC_GOTO_ERROR(H5E_DATASET, H5E_CANTGET, FAIL, "invalid scanline or element size");

    /* Parse hyperslab parameters based on dimensionality */
    if (ndims == 2) {
        /* 2D: [rows, cols] */
        row_start = start[0];
        row_count = count[0] * block[0];
        col_start = start[1];
        col_count = count[1] * block[1];
        band_start = 0;
        band_count = samples_per_pixel;
    } else if (ndims == 3) {
        /* 3D: [rows, cols, bands] */
        row_start = start[0];
        row_count = count[0] * block[0];
        col_start = start[1];
        col_count = count[1] * block[1];
        band_start = start[2];
        band_count = count[2] * block[2];
    } else {
        FUNC_GOTO_ERROR(H5E_DATASET, H5E_UNSUPPORTED, FAIL, "Unsupported number of dimensions: %d",
                        ndims);
    }

    /* Validate selection bounds */
    if (row_start + row_count > height || col_start + col_count > width ||
        band_start + band_count > samples_per_pixel)
        FUNC_GOTO_ERROR(H5E_DATASET, H5E_BADVALUE, FAIL, "Hyperslab selection out of bounds");

    /* Validate scanline size before allocation */
    if (scanline_size < 0)
        FUNC_GOTO_ERROR(H5E_DATASET, H5E_BADVALUE, FAIL, "Invalid scanline size");

    /* Allocate scanline buffer */
    if ((scanline_buf = (unsigned char *) malloc((size_t) scanline_size)) == NULL)
        FUNC_GOTO_ERROR(H5E_DATASET, H5E_CANTALLOC, FAIL,
                        "Failed to allocate memory for scanline buffer");

    /* Read selected region band by band */
    {
        size_t output_offset = 0;

        for (uint32_t row = (uint32_t) row_start; row < row_start + row_count; row++) {
            /* Read the scanline */
            if (TIFFReadScanline(file->tiff, scanline_buf, row, 0) < 0)
                FUNC_GOTO_ERROR(H5E_DATASET, H5E_READERROR, FAIL,
                                "Failed to read scanline %u from TIFF", row);

            /* Extract selected columns and bands */
            for (uint32_t col = (uint32_t) col_start; col < col_start + col_count; col++) {
                for (band_idx = band_start; band_idx < band_start + band_count; band_idx++) {
                    /* Calculate position in scanline buffer */
                    size_t pixel_offset =
                        col * samples_per_pixel * elem_size + band_idx * elem_size;

                    /* Copy the band data */
                    memcpy(output + output_offset, scanline_buf + pixel_offset, elem_size);
                    output_offset += elem_size;
                }
            }
        }
    }
done:
    if (scanline_buf)
        free(scanline_buf);

    return ret_value;
}

/* Helper function to read image data from TIFF
 *
 * TODO: Currently reads entire image into memory. For large images, this may
 *       exceed available memory.
 */
static herr_t geotiff_read_image_data(geotiff_dataset_t *dset)
{
    geotiff_file_t *file = dset->file;
    uint32_t width = 0;
    uint32_t height = 0;
    uint16_t samples_per_pixel = 0;
    uint16_t bits_per_sample = 0;
    tsize_t scanline_size = 0;
    uint32_t row = 0;
    unsigned char *image_data = NULL;
    herr_t ret_value = SUCCEED;

    assert(dset);
    assert(file);
    assert(file->tiff);

    if (!TIFFGetField(file->tiff, TIFFTAG_IMAGEWIDTH, &width))
        FUNC_GOTO_ERROR(H5E_DATASET, H5E_CANTGET, FAIL, "Failed to get image width from TIFF");
    if (!TIFFGetField(file->tiff, TIFFTAG_IMAGELENGTH, &height))
        FUNC_GOTO_ERROR(H5E_DATASET, H5E_CANTGET, FAIL, "Failed to get image height from TIFF");

    TIFFGetFieldDefaulted(file->tiff, TIFFTAG_SAMPLESPERPIXEL, &samples_per_pixel);
    TIFFGetFieldDefaulted(file->tiff, TIFFTAG_BITSPERSAMPLE, &bits_per_sample);

    scanline_size = TIFFScanlineSize(file->tiff);

    if (scanline_size <= 0)
        FUNC_GOTO_ERROR(H5E_DATASET, H5E_CANTGET, FAIL, "invalid scanline size");

    /* Validate reasonable data size to prevent memory issues */
    size_t total_size = (size_t) height * (size_t) scanline_size;
    if (total_size > 100 * 1024 * 1024)
        FUNC_GOTO_ERROR(H5E_DATASET, H5E_CANTGET, FAIL, "total read size exceeds 100MB");

    dset->data_size = total_size;
    if ((dset->data = malloc(dset->data_size)) == NULL)
        FUNC_GOTO_ERROR(H5E_DATASET, H5E_CANTALLOC, FAIL,
                        "Failed to allocate memory for image data");

    image_data = (unsigned char *) dset->data;

    /* Check if this is a tiled or striped TIFF and read accordingly */
    if (TIFFIsTiled(file->tiff)) {
        /* Tiled TIFF - read tile by tile */
        uint32_t tile_width = 0, tile_height = 0;
        unsigned char *tile_buf = NULL;
        tsize_t tile_size = 0;

        /* Get tile dimensions */
        if (!TIFFGetField(file->tiff, TIFFTAG_TILEWIDTH, &tile_width))
            FUNC_GOTO_ERROR(H5E_DATASET, H5E_CANTGET, FAIL, "Failed to get tile width from TIFF");
        if (!TIFFGetField(file->tiff, TIFFTAG_TILELENGTH, &tile_height))
            FUNC_GOTO_ERROR(H5E_DATASET, H5E_CANTGET, FAIL, "Failed to get tile height from TIFF");

        tile_size = TIFFTileSize(file->tiff);
        if (tile_size <= 0)
            FUNC_GOTO_ERROR(H5E_DATASET, H5E_CANTGET, FAIL, "Invalid tile size");

        /* Allocate buffer for reading tiles */
        if ((tile_buf = (unsigned char *) malloc((size_t) tile_size)) == NULL)
            FUNC_GOTO_ERROR(H5E_DATASET, H5E_CANTALLOC, FAIL, "Failed to allocate tile buffer");

        /* Read tiles and copy into image buffer */
        for (uint32_t tile_row = 0; tile_row < height; tile_row += tile_height) {
            for (uint32_t tile_col = 0; tile_col < width; tile_col += tile_width) {
                /* Read the tile */
                if (TIFFReadTile(file->tiff, tile_buf, tile_col, tile_row, 0, 0) < 0) {
                    free(tile_buf);
                    FUNC_GOTO_ERROR(H5E_DATASET, H5E_READERROR, FAIL,
                                    "Failed to read tile at row=%u, col=%u", tile_row, tile_col);
                }

                /* Calculate actual tile dimensions (may be partial at edges) */
                uint32_t actual_tile_height =
                    (tile_row + tile_height > height) ? height - tile_row : tile_height;
                uint32_t actual_tile_width =
                    (tile_col + tile_width > width) ? width - tile_col : tile_width;

                /* Copy tile data into image buffer, row by row */
                for (uint32_t ty = 0; ty < actual_tile_height; ty++) {
                    uint32_t image_row = tile_row + ty;
                    size_t tile_row_offset = ty * tile_width * samples_per_pixel;
                    size_t image_row_offset =
                        (size_t) image_row * (size_t) scanline_size + tile_col * samples_per_pixel;
                    size_t copy_bytes =
                        actual_tile_width * samples_per_pixel * (bits_per_sample / 8);

                    memcpy(image_data + image_row_offset, tile_buf + tile_row_offset, copy_bytes);
                }
            }
        }

        free(tile_buf);
    } else {
        /* Striped TIFF - read scanline by scanline */
        for (row = 0; row < height; row++) {
            if (TIFFReadScanline(file->tiff, image_data + row * scanline_size, row, 0) < 0)
                FUNC_GOTO_ERROR(H5E_DATASET, H5E_READERROR, FAIL,
                                "Failed to read scanline %u from TIFF", row);
        }
    }

done:
    if (ret_value < 0) {
        free(dset->data);
        dset->data = NULL;
        dset->data_size = 0;
    }

    return ret_value;
}

/* These two functions are necessary to load this plugin using
 * the HDF5 library. */
H5PL_type_t H5PLget_plugin_type(void)
{
    return H5PL_TYPE_VOL;
}
const void *H5PLget_plugin_info(void)
{
    return &geotiff_class_g;
}

/*---------------------------------------------------------------------------
 * Function:    geotiff_introspect_opt_query
 *
 * Purpose:     Query if an optional operation is supported by this connector
 *
 * Returns:     SUCCEED (Can't fail)
 *
 *---------------------------------------------------------------------------
 */
herr_t geotiff_introspect_opt_query(void __attribute__((unused)) * obj, H5VL_subclass_t subcls,
                                    int opt_type, uint64_t __attribute__((unused)) * flags)
{
    /* We don't support any optional operations */
    (void) subcls;
    (void) opt_type;
    *flags = 0;
    return SUCCEED;
}

herr_t geotiff_init_connector(hid_t __attribute__((unused)) vipl_id)
{
    herr_t ret_value = SUCCEED;

    /* Register the connector with HDF5's error reporting API */
    if ((H5_geotiff_err_class_g =
             H5Eregister_class(HDF5_VOL_GEOTIFF_ERR_CLS_NAME, HDF5_VOL_GEOTIFF_LIB_NAME,
                               HDF5_VOL_GEOTIFF_LIB_VER)) < 0)
        FUNC_GOTO_ERROR(H5E_VOL, H5E_CANTINIT, FAIL, "can't register with HDF5 error API");

    /* Create a separate error stack for the GEOTIFF VOL to report errors with */
    if ((H5_geotiff_err_stack_g = H5Ecreate_stack()) < 0)
        FUNC_GOTO_ERROR(H5E_VOL, H5E_CANTINIT, FAIL, "can't create error stack");

    /* Set up a few GEOTIFF VOL-specific error API message classes */
    if ((H5_geotiff_obj_err_maj_g =
             H5Ecreate_msg(H5_geotiff_err_class_g, H5E_MAJOR, "Object interface")) < 0)
        FUNC_GOTO_ERROR(H5E_VOL, H5E_CANTINIT, FAIL,
                        "can't create error message for object interface");
    if ((H5_geotiff_parse_err_min_g = H5Ecreate_msg(H5_geotiff_err_class_g, H5E_MINOR,
                                                    "Error occurred while parsing JSON")) < 0)
        FUNC_GOTO_ERROR(H5E_VOL, H5E_CANTINIT, FAIL,
                        "can't create error message for JSON parsing failures");
    if ((H5_geotiff_link_table_err_min_g = H5Ecreate_msg(
             H5_geotiff_err_class_g, H5E_MINOR, "Can't build table of links for iteration")) < 0)
        FUNC_GOTO_ERROR(H5E_VOL, H5E_CANTINIT, FAIL,
                        "can't create error message for link table build error");
    if ((H5_geotiff_link_table_iter_err_min_g = H5Ecreate_msg(
             H5_geotiff_err_class_g, H5E_MINOR, "Can't iterate through link table")) < 0)
        FUNC_GOTO_ERROR(H5E_VOL, H5E_CANTINIT, FAIL,
                        "can't create error message for link table iteration error");
    if ((H5_geotiff_attr_table_err_min_g =
             H5Ecreate_msg(H5_geotiff_err_class_g, H5E_MINOR,
                           "Can't build table of attributes for iteration")) < 0)
        FUNC_GOTO_ERROR(H5E_VOL, H5E_CANTINIT, FAIL,
                        "can't create error message for attribute table build error");
    if ((H5_geotiff_attr_table_iter_err_min_g = H5Ecreate_msg(
             H5_geotiff_err_class_g, H5E_MINOR, "Can't iterate through attribute table")) < 0)
        FUNC_GOTO_ERROR(H5E_VOL, H5E_CANTINIT, FAIL,
                        "can't create message for attribute iteration error");
    if ((H5_geotiff_object_table_err_min_g = H5Ecreate_msg(
             H5_geotiff_err_class_g, H5E_MINOR, "Can't build table of objects for iteration")) < 0)
        FUNC_GOTO_ERROR(H5E_VOL, H5E_CANTINIT, FAIL,
                        "can't create error message for object table build error");
    if ((H5_geotiff_object_table_iter_err_min_g = H5Ecreate_msg(
             H5_geotiff_err_class_g, H5E_MINOR, "Can't iterate through object table")) < 0)
        FUNC_GOTO_ERROR(H5E_VOL, H5E_CANTINIT, FAIL,
                        "can't create message for object iteration error");

    /* Initialized */
    H5_geotiff_initialized_g = TRUE;

done:
    if (ret_value < 0)
        geotiff_term_connector();

    return ret_value;
}

herr_t geotiff_term_connector(void)
{
    herr_t ret_value = SUCCEED;

    /* Unregister from the HDF5 error API */
    if (H5_geotiff_err_class_g >= 0) {
        if (H5_geotiff_obj_err_maj_g >= 0 && H5Eclose_msg(H5_geotiff_obj_err_maj_g) < 0)
            FUNC_DONE_ERROR(H5E_VOL, H5E_CLOSEERROR, FAIL,
                            "can't unregister error message for object interface");
        if (H5_geotiff_parse_err_min_g >= 0 && H5Eclose_msg(H5_geotiff_parse_err_min_g) < 0)
            FUNC_DONE_ERROR(H5E_VOL, H5E_CLOSEERROR, FAIL,
                            "can't unregister error message for JSON parsing error");
        if (H5_geotiff_link_table_err_min_g >= 0 &&
            H5Eclose_msg(H5_geotiff_link_table_err_min_g) < 0)
            FUNC_DONE_ERROR(H5E_VOL, H5E_CLOSEERROR, FAIL,
                            "can't unregister error message for building link table");
        if (H5_geotiff_link_table_iter_err_min_g >= 0 &&
            H5Eclose_msg(H5_geotiff_link_table_iter_err_min_g) < 0)
            FUNC_DONE_ERROR(H5E_VOL, H5E_CLOSEERROR, FAIL,
                            "can't unregister error message for iterating over link table");
        if (H5_geotiff_attr_table_err_min_g >= 0 &&
            H5Eclose_msg(H5_geotiff_attr_table_err_min_g) < 0)
            FUNC_DONE_ERROR(H5E_VOL, H5E_CLOSEERROR, FAIL,
                            "can't unregister error message for building attribute table");
        if (H5_geotiff_attr_table_iter_err_min_g >= 0 &&
            H5Eclose_msg(H5_geotiff_attr_table_iter_err_min_g) < 0)
            FUNC_DONE_ERROR(H5E_VOL, H5E_CLOSEERROR, FAIL,
                            "can't unregister error message for iterating over attribute table");
        if (H5_geotiff_object_table_iter_err_min_g >= 0 &&
            H5Eclose_msg(H5_geotiff_object_table_err_min_g) < 0)
            FUNC_DONE_ERROR(H5E_VOL, H5E_CLOSEERROR, FAIL,
                            "can't unregister error message for build object table");
        if (H5_geotiff_object_table_iter_err_min_g >= 0 &&
            H5Eclose_msg(H5_geotiff_object_table_iter_err_min_g) < 0)
            FUNC_DONE_ERROR(H5E_VOL, H5E_CLOSEERROR, FAIL,
                            "can't unregister error message for iterating over object table");

        if (H5Eunregister_class(H5_geotiff_err_class_g) < 0)
            FUNC_DONE_ERROR(H5E_VOL, H5E_CLOSEERROR, FAIL, "can't unregister from HDF5 error API");

        /* Print the current error stack before destroying it */
        PRINT_ERROR_STACK;

        /* Destroy the error stack */
        if (H5Eclose_stack(H5_geotiff_err_stack_g) < 0) {
            FUNC_DONE_ERROR(H5E_VOL, H5E_CLOSEERROR, FAIL, "can't close error stack");
            PRINT_ERROR_STACK;
        }

        H5_geotiff_err_stack_g = H5I_INVALID_HID;
        H5_geotiff_err_class_g = H5I_INVALID_HID;
        H5_geotiff_obj_err_maj_g = H5I_INVALID_HID;
        H5_geotiff_parse_err_min_g = H5I_INVALID_HID;
        H5_geotiff_link_table_err_min_g = H5I_INVALID_HID;
        H5_geotiff_link_table_iter_err_min_g = H5I_INVALID_HID;
        H5_geotiff_attr_table_err_min_g = H5I_INVALID_HID;
        H5_geotiff_attr_table_iter_err_min_g = H5I_INVALID_HID;
    }

    return ret_value;
}

herr_t geotiff_introspect_get_conn_cls(void __attribute__((unused)) * obj,
                                       H5VL_get_conn_lvl_t __attribute__((unused)) lvl,
                                       const H5VL_class_t __attribute__((unused)) * *conn_cls)
{
    herr_t ret_value = SUCCEED;

    assert(conn_cls);

    /* Retrieve the VOL connector class */
    *conn_cls = &geotiff_class_g;

    return ret_value;
}

/*---------------------------------------------------------------------------
 * Function:    geotiff_link_specific
 *
 * Purpose:     Handles link-specific operations for the GeoTIFF VOL connector
 *
 * Return:      SUCCEED/FAIL
 *
 * Note:        The GeoTIFF VOL has a flat structure with only image datasets
 *              at the root level (e.g., "image0"). We pretend each image has
 *              a hard link from the root group.
 *
 *---------------------------------------------------------------------------
 */
/* cppcheck-suppress constParameterCallback */
herr_t geotiff_link_specific(void *obj, const H5VL_loc_params_t *loc_params,
                             H5VL_link_specific_args_t *args, hid_t __attribute__((unused)) dxpl_id,
                             void __attribute__((unused)) * *req)
{
    herr_t ret_value = SUCCEED;
    const char *link_name = NULL;

    /* obj could be file, group, or dataset - we need the file */
    /* For simplicity, try to extract file pointer based on common structure pattern */
    if (!obj || !loc_params || !args)
        FUNC_GOTO_ERROR(H5E_ARGS, H5E_BADVALUE, FAIL, "Invalid arguments to link_specific");

    switch (args->op_type) {
        case H5VL_LINK_EXISTS: {
            /* Get the link name from loc_params */
            if (loc_params->type == H5VL_OBJECT_BY_NAME) {
                link_name = loc_params->loc_data.loc_by_name.name;
            } else {
                FUNC_GOTO_ERROR(H5E_LINK, H5E_BADVALUE, FAIL,
                                "Link exists check requires name-based location");
            }

            if (!link_name)
                FUNC_GOTO_ERROR(H5E_LINK, H5E_BADVALUE, FAIL, "No link name provided");

            /* Currently only "image0" exists as a link/dataset
             * Future: check for "image1", "image2", etc. for multi-page TIFFs
             */
            *args->args.exists.exists = (strcmp(link_name, "image0") == 0);

            break;
        }

        case H5VL_LINK_ITER: {
            H5VL_link_iterate_args_t *iter_args = &args->args.iterate;

            /* TODO: For now, we only have one link: "image0"
             * Future: iterate over multiple images for multi-page TIFFs
             */

            assert(iter_args);
            assert(iter_args->idx_p);

            /* Only iterate if we haven't reached the end */
            if (*iter_args->idx_p > 0) {
                /* Already past the only link we have */
                break;
            }

            /* Call the user's callback for "image0" */
            if (iter_args->op) {
                H5L_info2_t link_info;
                herr_t cb_ret;

                /* Fill in minimal link info */
                memset(&link_info, 0, sizeof(H5L_info2_t));
                /* Consider all GeoTIFF "links" to be hard links */
                link_info.type = H5L_TYPE_HARD;
                link_info.corder_valid = true;
                link_info.corder = 0;
                link_info.cset = H5T_CSET_ASCII;

                /* TODO: Fill in link_info.u.token if needed */
                /* For now, leave token as zeros */

                /* Call user's callback
                 * Signature: herr_t (*op)(hid_t group, const char *name, const H5L_info2_t *info,
                 * void *op_data)
                 * Note: We pass 0 as group hid since we don't have a proper group ID
                 */
                cb_ret = iter_args->op(0, "image0", &link_info, iter_args->op_data);

                /* Update index */
                if (iter_args->idx_p)
                    *iter_args->idx_p = *iter_args->idx_p + 1;

                /* Check callback return value */
                if (cb_ret < 0) {
                    FUNC_GOTO_ERROR(H5E_LINK, H5E_BADITER, FAIL,
                                    "Iterator callback returned error");
                } else if (cb_ret > 0) {
                    /* Callback requested early termination */
                    ret_value = cb_ret;
                    goto done;
                }
            }

            break;
        }

        case H5VL_LINK_DELETE:
            FUNC_GOTO_ERROR(H5E_LINK, H5E_UNSUPPORTED, FAIL,
                            "Link deletion is not supported in read-only GeoTIFF VOL connector");
            break;

        default:
            FUNC_GOTO_ERROR(H5E_LINK, H5E_UNSUPPORTED, FAIL, "Unsupported link specific operation");
    }

done:
    return ret_value;
}
