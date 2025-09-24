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
#include "template_vol_connector.h"

#include <H5PLextern.h>
#include <assert.h>
#include <hdf5.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _MSC_VER
#ifndef strdup
#define strdup _strdup
#endif
#endif

/* GeoTIFF VOL connector initialization */
herr_t geotiff_init_connector(hid_t __attribute__((unused)) vipl_id)
{
    return 0;
}

/* GeoTIFF VOL connector termination */
herr_t geotiff_term_connector(void)
{
    return 0;
}

/* Simple introspect opt_query function that reports no optional operations are supported */
herr_t geotiff_introspect_opt_query(void __attribute__((unused)) * obj, H5VL_subclass_t subcls,
                                    int opt_type, uint64_t __attribute__((unused)) * flags)
{
    /* We don't support any optional operations */
    (void) subcls;
    (void) opt_type;
    return 0;
}

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
        NULL, /* create       */
        NULL, /* copy         */
        NULL, /* move         */
        NULL, /* get          */
        NULL, /* specific     */
        NULL  /* optional     */
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
        NULL,                        /* get_conn_cls  */
        NULL,                        /* get_cap_flags */
        geotiff_introspect_opt_query /* opt_query     */
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

/* These two functions are necessary to load this plugin using
 * the HDF5 library.
 */

/* Helper function to get HDF5 type from TIFF sample format and bits per sample */
hid_t geotiff_get_hdf5_type_from_tiff(uint16_t sample_format, uint16_t bits_per_sample)
{
    switch (sample_format) {
        case SAMPLEFORMAT_UINT:
            switch (bits_per_sample) {
                case 8:
                    return H5T_NATIVE_UCHAR;
                case 16:
                    return H5T_NATIVE_USHORT;
                case 32:
                    return H5T_NATIVE_UINT;
                case 64:
                    return H5T_NATIVE_UINT64;
                default:
                    return H5T_NATIVE_UCHAR;
            }
        case SAMPLEFORMAT_INT:
            switch (bits_per_sample) {
                case 8:
                    return H5T_NATIVE_CHAR;
                case 16:
                    return H5T_NATIVE_SHORT;
                case 32:
                    return H5T_NATIVE_INT;
                case 64:
                    return H5T_NATIVE_INT64;
                default:
                    return H5T_NATIVE_CHAR;
            }
        case SAMPLEFORMAT_IEEEFP:
            switch (bits_per_sample) {
                case 32:
                    return H5T_NATIVE_FLOAT;
                case 64:
                    return H5T_NATIVE_DOUBLE;
                default:
                    return H5T_NATIVE_FLOAT;
            }
        default:
            return H5T_NATIVE_UCHAR;
    }
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
    geotiff_file_t *file;

    /* We only support read-only access for GeoTIFF files */
    /* H5F_ACC_RDONLY is 0, so we need to check that no write flags are set */
    if (flags & H5F_ACC_RDWR) {
        return NULL;
    }

    file = (geotiff_file_t *) malloc(sizeof(geotiff_file_t));
    if (!file)
        return NULL;

    file->tiff = TIFFOpen(name, "r");
    if (!file->tiff) {
        free(file);
        return NULL;
    }

    file->gtif = GTIFNew(file->tiff);
    if (!file->gtif) {
        TIFFClose(file->tiff);
        free(file);
        return NULL;
    }

    file->filename = strdup(name);
    file->flags = flags;
    file->plist_id = fapl_id;

    /* Parse GeoTIFF metadata */
    geotiff_parse_geotiff_tags(file);

    return file;
}

// cppcheck-suppress constParameterCallback
herr_t geotiff_file_get(void *file, H5VL_file_get_args_t *args,
                        hid_t __attribute__((unused)) dxpl_id, void __attribute__((unused)) * *req)
{
    const geotiff_file_t *f = (const geotiff_file_t *) file;

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
            return -1;
    }

    return 0;
}

herr_t geotiff_file_close(void *file, hid_t __attribute__((unused)) dxpl_id,
                          void __attribute__((unused)) * *req)
{
    geotiff_file_t *f = (geotiff_file_t *) file;

    if (f) {
        if (f->gtif)
            GTIFFree(f->gtif);
        if (f->tiff)
            TIFFClose(f->tiff);
        if (f->filename)
            free(f->filename);
        free(f);
    }

    return 0;
}

/* Dataset operations */
void *geotiff_dataset_open(void *obj, const H5VL_loc_params_t __attribute__((unused)) * loc_params,
                           const char *name, hid_t __attribute__((unused)) dapl_id,
                           hid_t __attribute__((unused)) dxpl_id,
                           void __attribute__((unused)) * *req)
{
    geotiff_file_t *file = (geotiff_file_t *) obj;
    geotiff_dataset_t *dset;
    uint32_t width, height;
    uint16_t samples_per_pixel, bits_per_sample, sample_format;
    hsize_t dims[3];

    if (!file || !name)
        return NULL;

    dset = (geotiff_dataset_t *) malloc(sizeof(geotiff_dataset_t));
    if (!dset)
        return NULL;

    dset->file = file;
    dset->name = strdup(name);
    dset->data = NULL;
    dset->data_size = 0;
    dset->is_image = 0;

    if (strcmp(name, "image") == 0) {
        dset->is_image = 1;

        if (!TIFFGetField(file->tiff, TIFFTAG_IMAGEWIDTH, &width) ||
            !TIFFGetField(file->tiff, TIFFTAG_IMAGELENGTH, &height)) {
            free(dset->name);
            free(dset);
            return NULL;
        }

        /* Validate image dimensions */
        if (width == 0 || height == 0 || width > 65535 || height > 65535) {
            free(dset->name);
            free(dset);
            return NULL;
        }

        TIFFGetFieldDefaulted(file->tiff, TIFFTAG_SAMPLESPERPIXEL, &samples_per_pixel);
        TIFFGetFieldDefaulted(file->tiff, TIFFTAG_BITSPERSAMPLE, &bits_per_sample);
        TIFFGetFieldDefaulted(file->tiff, TIFFTAG_SAMPLEFORMAT, &sample_format);

        dset->type_id = geotiff_get_hdf5_type_from_tiff(sample_format, bits_per_sample);

        if (samples_per_pixel > 1) {
            dims[0] = height;
            dims[1] = width;
            dims[2] = samples_per_pixel;
            dset->space_id = H5Screate_simple(3, dims, NULL);
        } else {
            dims[0] = height;
            dims[1] = width;
            dset->space_id = H5Screate_simple(2, dims, NULL);
        }

        if (dset->space_id < 0) {
            free(dset->name);
            free(dset);
            return NULL;
        }

        if (geotiff_read_image_data(file, dset) < 0) {
            H5Sclose(dset->space_id);
            free(dset->name);
            free(dset);
            return NULL;
        }
    }

    return dset;
}

herr_t geotiff_dataset_read(size_t __attribute__((unused)) count, void *dset[],
                            hid_t __attribute__((unused)) mem_type_id[],
                            hid_t __attribute__((unused)) mem_space_id[],
                            hid_t __attribute__((unused)) file_space_id[],
                            hid_t __attribute__((unused)) dxpl_id, void *buf[],
                            void __attribute__((unused)) * *req)
{
    const geotiff_dataset_t *d = (const geotiff_dataset_t *) dset[0];

    if (!d || !d->data || !buf[0])
        return -1;

    memcpy(buf[0], d->data, d->data_size);

    return 0;
}

// cppcheck-suppress constParameterCallback
herr_t geotiff_dataset_get(void *dset, H5VL_dataset_get_args_t *args,
                           hid_t __attribute__((unused)) dxpl_id,
                           void __attribute__((unused)) * *req)
{
    const geotiff_dataset_t *d = (const geotiff_dataset_t *) dset;

    switch (args->op_type) {
        case H5VL_DATASET_GET_SPACE:
            args->args.get_space.space_id = d->space_id;
            break;
        case H5VL_DATASET_GET_TYPE:
            args->args.get_type.type_id = d->type_id;
            break;
        default:
            return -1;
    }

    return 0;
}

herr_t geotiff_dataset_close(void *dset, hid_t __attribute__((unused)) dxpl_id,
                             void __attribute__((unused)) * *req)
{
    geotiff_dataset_t *d = (geotiff_dataset_t *) dset;

    if (d) {
        if (d->name)
            free(d->name);
        if (d->data)
            free(d->data);
        free(d);
    }

    return 0;
}

/* Group operations */
void *geotiff_group_open(void *obj, const H5VL_loc_params_t __attribute__((unused)) * loc_params,
                         const char *name, hid_t __attribute__((unused)) gapl_id,
                         hid_t __attribute__((unused)) dxpl_id, void __attribute__((unused)) * *req)
{
    geotiff_file_t *file = (geotiff_file_t *) obj;
    geotiff_group_t *grp;

    if (!file || !name)
        return NULL;

    if (strcmp(name, "/") != 0)
        return NULL;

    grp = (geotiff_group_t *) malloc(sizeof(geotiff_group_t));
    if (!grp)
        return NULL;

    grp->file = file;
    grp->name = strdup(name);

    return grp;
}

herr_t geotiff_group_get(void __attribute__((unused)) * obj,
                         H5VL_group_get_args_t __attribute__((unused)) * args,
                         hid_t __attribute__((unused)) dxpl_id, void __attribute__((unused)) * *req)
{
    return 0;
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

    return 0;
}

/* Attribute operations */
void *geotiff_attr_open(void *obj, const H5VL_loc_params_t __attribute__((unused)) * loc_params,
                        const char *name, hid_t __attribute__((unused)) aapl_id,
                        hid_t __attribute__((unused)) dxpl_id, void __attribute__((unused)) * *req)
{
    geotiff_file_t *file = (geotiff_file_t *) obj;
    geotiff_attr_t *attr;

    if (!file || !name)
        return NULL;

    attr = (geotiff_attr_t *) malloc(sizeof(geotiff_attr_t));
    if (!attr)
        return NULL;

    attr->file = file;
    attr->name = strdup(name);
    attr->data = NULL;
    attr->data_size = 0;
    attr->type_id = H5T_NATIVE_CHAR;
    attr->space_id = H5Screate(H5S_SCALAR);

    return attr;
}

// cppcheck-suppress constParameterCallback
herr_t geotiff_attr_read(void *attr, hid_t __attribute__((unused)) mem_type_id, void *buf,
                         hid_t __attribute__((unused)) dxpl_id, void __attribute__((unused)) * *req)
{
    const geotiff_attr_t *a = (const geotiff_attr_t *) attr;

    if (!a || !buf)
        return -1;

    if (a->data && a->data_size > 0) {
        memcpy(buf, a->data, a->data_size);
    }

    return 0;
}

// cppcheck-suppress constParameterCallback
herr_t geotiff_attr_get(void *obj, H5VL_attr_get_args_t *args,
                        hid_t __attribute__((unused)) dxpl_id, void __attribute__((unused)) * *req)
{
    const geotiff_attr_t *a = (const geotiff_attr_t *) obj;

    switch (args->op_type) {
        case H5VL_ATTR_GET_SPACE:
            args->args.get_space.space_id = a->space_id;
            break;
        case H5VL_ATTR_GET_TYPE:
            args->args.get_type.type_id = a->type_id;
            break;
        default:
            return -1;
    }

    return 0;
}

herr_t geotiff_attr_close(void *attr, hid_t __attribute__((unused)) dxpl_id,
                          void __attribute__((unused)) * *req)
{
    geotiff_attr_t *a = (geotiff_attr_t *) attr;

    if (a) {
        if (a->name)
            free(a->name);
        if (a->data)
            free(a->data);
        free(a);
    }

    return 0;
}

/* Helper function to read image data from TIFF */
herr_t geotiff_read_image_data(geotiff_file_t *file, geotiff_dataset_t *dset)
{
    uint32_t width, height;
    uint16_t samples_per_pixel, bits_per_sample;
    tsize_t scanline_size;
    uint32_t row;
    unsigned char *image_data;

    if (!file || !file->tiff || !dset) {
        return -1;
    }

    if (!TIFFGetField(file->tiff, TIFFTAG_IMAGEWIDTH, &width) ||
        !TIFFGetField(file->tiff, TIFFTAG_IMAGELENGTH, &height)) {
        return -1;
    }

    /* Validate reasonable image dimensions */
    if (width == 0 || height == 0 || width > 65535 || height > 65535) {
        return -1;
    }

    TIFFGetFieldDefaulted(file->tiff, TIFFTAG_SAMPLESPERPIXEL, &samples_per_pixel);
    TIFFGetFieldDefaulted(file->tiff, TIFFTAG_BITSPERSAMPLE, &bits_per_sample);

    scanline_size = TIFFScanlineSize(file->tiff);
    if (scanline_size <= 0) {
        return -1;
    }

    /* Validate reasonable data size to prevent memory issues */
    size_t total_size = (size_t) height * (size_t) scanline_size;
    if (total_size > 100 * 1024 * 1024) { /* 100MB limit */
        return -1;
    }

    dset->data_size = total_size;
    dset->data = malloc(dset->data_size);

    if (!dset->data)
        return -1;

    image_data = (unsigned char *) dset->data;

    for (row = 0; row < height; row++) {
        if (TIFFReadScanline(file->tiff, image_data + row * scanline_size, row, 0) < 0) {
            free(dset->data);
            dset->data = NULL;
            dset->data_size = 0;
            return -1;
        }
    }

    return 0;
}

/* Helper function to parse GeoTIFF tags */
herr_t geotiff_parse_geotiff_tags(geotiff_file_t *file)
{
    geocode_t model_type;
    geocode_t pcs_code, gcs_code;
    /* Optionally read tie points / pixel scale via TIFF tags in the future */

    if (!file || !file->gtif)
        return -1;

    /* Get model type */
    if (GTIFKeyGet(file->gtif, GTModelTypeGeoKey, &model_type, 0, 1)) {
        printf("Model Type: %d\n", model_type);
    }

    /* Get PCS code */
    if (GTIFKeyGet(file->gtif, ProjectedCSTypeGeoKey, &pcs_code, 0, 1)) {
        printf("Projected CS: %d\n", pcs_code);
    }

    /* Get GCS code */
    if (GTIFKeyGet(file->gtif, GeographicTypeGeoKey, &gcs_code, 0, 1)) {
        printf("Geographic CS: %d\n", gcs_code);
    }

    /* Skipping citation retrieval for portability */

    /* Skipping tie points and pixel scale retrieval here due to tag differences across platforms */

    return 0;
}

H5PL_type_t H5PLget_plugin_type(void)
{
    return H5PL_TYPE_VOL;
}
const void *H5PLget_plugin_info(void)
{
    return &geotiff_class_g;
}
