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

#include <hdf5.h>
#include <H5PLextern.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>

/* GeoTIFF VOL connector initialization */
herr_t geotiff_init_connector(void) {
    return 0;
}

/* GeoTIFF VOL connector termination */
herr_t geotiff_term_connector(void) {
    return 0;
}

/* The VOL class struct */
static const H5VL_class_t geotiff_class_g = {
    3,                                              /* VOL class struct version */
    GEOTIFF_VOL_CONNECTOR_VALUE,                    /* value                    */
    GEOTIFF_VOL_CONNECTOR_NAME,                     /* name                     */
    1,                                              /* version                  */
    0,                                              /* capability flags         */
    geotiff_init_connector,                         /* initialize               */
    geotiff_term_connector,                         /* terminate                */
    {   /* info_cls */
        (size_t)0,                                  /* size    */
        NULL,                                       /* copy    */
        NULL,                                       /* compare */
        NULL,                                       /* free    */
        NULL,                                       /* to_str  */
        NULL,                                       /* from_str */
    },
    {   /* wrap_cls */
        NULL,                                       /* get_object   */
        NULL,                                       /* get_wrap_ctx */
        NULL,                                       /* wrap_object  */
        NULL,                                       /* unwrap_object */
        NULL,                                       /* free_wrap_ctx */
    },
    {   /* attribute_cls */
        NULL,                                       /* create       */
        geotiff_attr_open,                          /* open         */
        geotiff_attr_read,                          /* read         */
        NULL,                                       /* write        */
        geotiff_attr_get,                           /* get          */
        NULL,                                       /* specific     */
        NULL,                                       /* optional     */
        geotiff_attr_close                          /* close        */
    },
    {   /* dataset_cls */
        NULL,                                       /* create       */
        geotiff_dataset_open,                       /* open         */
        geotiff_dataset_read,                       /* read         */
        NULL,                                       /* write        */
        geotiff_dataset_get,                        /* get          */
        NULL,                                       /* specific     */
        NULL,                                       /* optional     */
        geotiff_dataset_close                       /* close        */
    },
    {   /* datatype_cls */
        NULL,                                       /* commit       */
        NULL,                                       /* open         */
        NULL,                                       /* get_size     */
        NULL,                                       /* specific     */
        NULL,                                       /* optional     */
        NULL                                        /* close        */
    },
    {   /* file_cls */
        geotiff_file_create,                        /* create       */
        geotiff_file_open,                          /* open         */
        geotiff_file_get,                           /* get          */
        NULL,                                       /* specific     */
        NULL,                                       /* optional     */
        geotiff_file_close                          /* close        */
    },
    {   /* group_cls */
        NULL,                                       /* create       */
        geotiff_group_open,                         /* open         */
        geotiff_group_get,                          /* get          */
        NULL,                                       /* specific     */
        NULL,                                       /* optional     */
        geotiff_group_close                         /* close        */
    },
    {   /* link_cls */
        NULL,                                       /* create       */
        NULL,                                       /* copy         */
        NULL,                                       /* move         */
        NULL,                                       /* get          */
        NULL,                                       /* specific     */
        NULL                                        /* optional     */
    },
    {   /* object_cls */
        NULL,                                       /* open         */
        NULL,                                       /* copy         */
        NULL,                                       /* get          */
        NULL,                                       /* specific     */
        NULL                                        /* optional     */
    },
    {   /* introscpect_cls */
        NULL,                                       /* get_conn_cls  */
        NULL,                                       /* get_cap_flags */
        NULL                                        /* opt_query     */
    },
    {   /* request_cls */
        NULL,                                       /* wait         */
        NULL,                                       /* notify       */
        NULL,                                       /* cancel       */
        NULL,                                       /* specific     */
        NULL,                                       /* optional     */
        NULL                                        /* free         */
    },
    {   /* blob_cls */
        NULL,                                       /* put          */
        NULL,                                       /* get          */
        NULL,                                       /* specific     */
        NULL                                        /* optional     */
    },
    {   /* token_cls */
        NULL,                                       /* cmp          */
        NULL,                                       /* to_str       */
        NULL                                        /* from_str     */
    },
    NULL                                            /* optional     */
};

/* These two functions are necessary to load this plugin using
 * the HDF5 library.
 */

/* Helper function to get HDF5 type from TIFF sample format and bits per sample */
hid_t geotiff_get_hdf5_type_from_tiff(uint16 sample_format, uint16 bits_per_sample) {
    switch (sample_format) {
        case SAMPLEFORMAT_UINT:
            switch (bits_per_sample) {
                case 8: return H5T_NATIVE_UCHAR;
                case 16: return H5T_NATIVE_USHORT;
                case 32: return H5T_NATIVE_UINT;
                case 64: return H5T_NATIVE_UINT64;
                default: return H5T_NATIVE_UCHAR;
            }
        case SAMPLEFORMAT_INT:
            switch (bits_per_sample) {
                case 8: return H5T_NATIVE_CHAR;
                case 16: return H5T_NATIVE_SHORT;
                case 32: return H5T_NATIVE_INT;
                case 64: return H5T_NATIVE_INT64;
                default: return H5T_NATIVE_CHAR;
            }
        case SAMPLEFORMAT_IEEEFP:
            switch (bits_per_sample) {
                case 32: return H5T_NATIVE_FLOAT;
                case 64: return H5T_NATIVE_DOUBLE;
                default: return H5T_NATIVE_FLOAT;
            }
        default:
            return H5T_NATIVE_UCHAR;
    }
}

/* File operations */
void *geotiff_file_create(const char *name, unsigned flags, hid_t fcpl_id, hid_t fapl_id, hid_t dxpl_id, void **req) {
    return NULL;
}

void *geotiff_file_open(const char *name, unsigned flags, hid_t fapl_id, hid_t dxpl_id, void **req) {
    geotiff_file_t *file;

    if (!(flags & H5F_ACC_RDONLY)) {
        return NULL;
    }

    file = (geotiff_file_t *)malloc(sizeof(geotiff_file_t));
    if (!file) return NULL;

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

herr_t geotiff_file_get(void *file, H5VL_file_get_args_t *args, hid_t dxpl_id, void **req) {
    geotiff_file_t *f = (geotiff_file_t *)file;

    switch (args->op_type) {
        case H5VL_FILE_GET_NAME:
            if (args->args.get_name.name && args->args.get_name.size > 0) {
                strncpy(args->args.get_name.name, f->filename, args->args.get_name.size - 1);
                args->args.get_name.name[args->args.get_name.size - 1] = '\0';
                *args->args.get_name.name_len = strlen(f->filename);
            }
            break;
        default:
            return -1;
    }

    return 0;
}

herr_t geotiff_file_close(void *file, hid_t dxpl_id, void **req) {
    geotiff_file_t *f = (geotiff_file_t *)file;

    if (f) {
        if (f->gtif) GTIFFree(f->gtif);
        if (f->tiff) TIFFClose(f->tiff);
        if (f->filename) free(f->filename);
        free(f);
    }

    return 0;
}

/* Dataset operations */
void *geotiff_dataset_open(void *obj, const H5VL_loc_params_t *loc_params, const char *name, hid_t dapl_id, hid_t dxpl_id, void **req) {
    geotiff_file_t *file = (geotiff_file_t *)obj;
    geotiff_dataset_t *dset;
    uint32 width, height;
    uint16 samples_per_pixel, bits_per_sample, sample_format;
    hsize_t dims[2];

    if (!file || !name) return NULL;

    dset = (geotiff_dataset_t *)malloc(sizeof(geotiff_dataset_t));
    if (!dset) return NULL;

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

        geotiff_read_image_data(file, dset);
    }

    return dset;
}

herr_t geotiff_dataset_read(size_t count, void *dset[], hid_t mem_type_id[], hid_t mem_space_id[], hid_t file_space_id[], hid_t dxpl_id, void *buf[], void **req) {
    geotiff_dataset_t *d = (geotiff_dataset_t *)dset[0];

    if (!d || !d->data || !buf[0]) return -1;

    memcpy(buf[0], d->data, d->data_size);

    return 0;
}

herr_t geotiff_dataset_get(void *dset, H5VL_dataset_get_args_t *args, hid_t dxpl_id, void **req) {
    geotiff_dataset_t *d = (geotiff_dataset_t *)dset;

    switch (args->op_type) {
        case H5VL_DATASET_GET_SPACE:
            *args->args.get_space.space_id = d->space_id;
            break;
        case H5VL_DATASET_GET_TYPE:
            *args->args.get_type.type_id = d->type_id;
            break;
        default:
            return -1;
    }

    return 0;
}

herr_t geotiff_dataset_close(void *dset, hid_t dxpl_id, void **req) {
    geotiff_dataset_t *d = (geotiff_dataset_t *)dset;

    if (d) {
        if (d->name) free(d->name);
        if (d->data) free(d->data);
        free(d);
    }

    return 0;
}

/* Group operations */
void *geotiff_group_open(void *obj, const H5VL_loc_params_t *loc_params, const char *name, hid_t gapl_id, hid_t dxpl_id, void **req) {
    geotiff_file_t *file = (geotiff_file_t *)obj;
    geotiff_group_t *grp;

    if (!file || !name) return NULL;

    if (strcmp(name, "/") != 0) return NULL;

    grp = (geotiff_group_t *)malloc(sizeof(geotiff_group_t));
    if (!grp) return NULL;

    grp->file = file;
    grp->name = strdup(name);

    return grp;
}

herr_t geotiff_group_get(void *obj, H5VL_group_get_args_t *args, hid_t dxpl_id, void **req) {
    return 0;
}

herr_t geotiff_group_close(void *grp, hid_t dxpl_id, void **req) {
    geotiff_group_t *g = (geotiff_group_t *)grp;

    if (g) {
        if (g->name) free(g->name);
        free(g);
    }

    return 0;
}

/* Attribute operations */
void *geotiff_attr_open(void *obj, const H5VL_loc_params_t *loc_params, const char *name, hid_t aapl_id, hid_t dxpl_id, void **req) {
    geotiff_file_t *file = (geotiff_file_t *)obj;
    geotiff_attr_t *attr;

    if (!file || !name) return NULL;

    attr = (geotiff_attr_t *)malloc(sizeof(geotiff_attr_t));
    if (!attr) return NULL;

    attr->file = file;
    attr->name = strdup(name);
    attr->data = NULL;
    attr->data_size = 0;
    attr->type_id = H5T_NATIVE_CHAR;
    attr->space_id = H5Screate(H5S_SCALAR);

    return attr;
}

herr_t geotiff_attr_read(void *attr, hid_t mem_type_id, void *buf, hid_t dxpl_id, void **req) {
    geotiff_attr_t *a = (geotiff_attr_t *)attr;

    if (!a || !buf) return -1;

    if (a->data && a->data_size > 0) {
        memcpy(buf, a->data, a->data_size);
    }

    return 0;
}

herr_t geotiff_attr_get(void *obj, H5VL_attr_get_args_t *args, hid_t dxpl_id, void **req) {
    geotiff_attr_t *a = (geotiff_attr_t *)obj;

    switch (args->op_type) {
        case H5VL_ATTR_GET_SPACE:
            *args->args.get_space.space_id = a->space_id;
            break;
        case H5VL_ATTR_GET_TYPE:
            *args->args.get_type.type_id = a->type_id;
            break;
        default:
            return -1;
    }

    return 0;
}

herr_t geotiff_attr_close(void *attr, hid_t dxpl_id, void **req) {
    geotiff_attr_t *a = (geotiff_attr_t *)attr;

    if (a) {
        if (a->name) free(a->name);
        if (a->data) free(a->data);
        free(a);
    }

    return 0;
}

/* Helper function to read image data from TIFF */
herr_t geotiff_read_image_data(geotiff_file_t *file, geotiff_dataset_t *dset) {
    uint32 width, height;
    uint16 samples_per_pixel, bits_per_sample;
    tsize_t scanline_size;
    uint32 row;
    unsigned char *image_data;

    if (!TIFFGetField(file->tiff, TIFFTAG_IMAGEWIDTH, &width) ||
        !TIFFGetField(file->tiff, TIFFTAG_IMAGELENGTH, &height)) {
        return -1;
    }

    TIFFGetFieldDefaulted(file->tiff, TIFFTAG_SAMPLESPERPIXEL, &samples_per_pixel);
    TIFFGetFieldDefaulted(file->tiff, TIFFTAG_BITSPERSAMPLE, &bits_per_sample);

    scanline_size = TIFFScanlineSize(file->tiff);
    dset->data_size = height * scanline_size;
    dset->data = malloc(dset->data_size);

    if (!dset->data) return -1;

    image_data = (unsigned char *)dset->data;

    for (row = 0; row < height; row++) {
        if (TIFFReadScanline(file->tiff, image_data + row * scanline_size, row, 0) < 0) {
            free(dset->data);
            dset->data = NULL;
            return -1;
        }
    }

    return 0;
}

/* Helper function to parse GeoTIFF tags */
herr_t geotiff_parse_geotiff_tags(geotiff_file_t *file) {
    geocode_t model_type;
    geocode_t pcs_code, gcs_code, datum_code, ellipsoid_code;
    short citation_length;
    char *citation = NULL;
    double tie_points[6], pixel_scale[3];
    short tie_count, scale_count;

    if (!file || !file->gtif) return -1;

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

    /* Get citation if available */
    citation_length = 0;
    if (GTIFKeyGet(file->gtif, GTCitationGeoKey, NULL, &citation_length, 0) && citation_length > 0) {
        citation = (char *)malloc(citation_length);
        if (citation) {
            if (GTIFKeyGet(file->gtif, GTCitationGeoKey, citation, &citation_length, citation_length)) {
                printf("Citation: %s\n", citation);
            }
            free(citation);
        }
    }

    /* Get tie points */
    tie_count = 0;
    if (GTIFKeyGet(file->gtif, GTModelTiepointTag, NULL, &tie_count, 0) && tie_count >= 6) {
        if (GTIFKeyGet(file->gtif, GTModelTiepointTag, tie_points, &tie_count, 6)) {
            printf("Tie Points: (%f, %f, %f) -> (%f, %f, %f)\n",
                   tie_points[0], tie_points[1], tie_points[2],
                   tie_points[3], tie_points[4], tie_points[5]);
        }
    }

    /* Get pixel scale */
    scale_count = 0;
    if (GTIFKeyGet(file->gtif, GTModelPixelScaleTag, NULL, &scale_count, 0) && scale_count >= 3) {
        if (GTIFKeyGet(file->gtif, GTModelPixelScaleTag, pixel_scale, &scale_count, 3)) {
            printf("Pixel Scale: %f, %f, %f\n", pixel_scale[0], pixel_scale[1], pixel_scale[2]);
        }
    }

    return 0;
}

H5PL_type_t H5PLget_plugin_type(void) {return H5PL_TYPE_VOL;}
const void *H5PLget_plugin_info(void) {return &geotiff_class_g;}

