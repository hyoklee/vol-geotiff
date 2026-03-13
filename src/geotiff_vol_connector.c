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
#include <H5VLconnector_passthru.h>
#include <assert.h>
#include <geo_normalize.h>
#include <geovalues.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <xtiffio.h>

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

#define SIZE_LIMIT_BYTES (100 * 1024 * 1024) /* 100 MB size limit for image data */

/* GDAL metadata tag numbers */
#define TIFFTAG_GDAL_METADATA 42112
#define TIFFTAG_GDAL_NODATA 42113
#define MAX_GDAL_METADATA_ITEMS 128

/* Structure to hold a single GDAL metadata key-value pair */
typedef struct {
    char *key;
    char *value;
} gdal_metadata_item_t;

/* Structure to hold all GDAL metadata for a file */
struct gdal_metadata_t {
    gdal_metadata_item_t items[MAX_GDAL_METADATA_ITEMS];
    int count;
};

static hbool_t H5_geotiff_initialized_g = FALSE;

/* Register GDAL metadata tags with libtiff so TIFFGetField works for them */
static TIFFExtendProc geotiff_parent_extender_g = NULL;

static void geotiff_gdal_tag_extender(TIFF *tiff)
{
    static const TIFFFieldInfo gdal_fields[] = {
        {TIFFTAG_GDAL_METADATA, -1, -1, TIFF_ASCII, FIELD_CUSTOM, 1, 0, (char *) "GDAL_METADATA"},
        {TIFFTAG_GDAL_NODATA, -1, -1, TIFF_ASCII, FIELD_CUSTOM, 1, 0, (char *) "GDAL_NODATA"},
    };
    TIFFMergeFieldInfo(tiff, gdal_fields, sizeof(gdal_fields) / sizeof(gdal_fields[0]));
    if (geotiff_parent_extender_g)
        (*geotiff_parent_extender_g)(tiff);
}

/* Identifiers for HDF5's error API */
hid_t H5_geotiff_err_stack_g = H5I_INVALID_HID;
hid_t H5_geotiff_err_class_g = H5I_INVALID_HID;

/* Helper functions */
static herr_t geotiff_read_image_data(geotiff_object_t *dset_obj);
static herr_t geotiff_get_hdf5_type_from_tiff(uint16_t sample_format, uint16_t bits_per_sample,
                                              hid_t *type_id);
herr_t geotiff_object_get(void *obj, const H5VL_loc_params_t *loc_params,
                          H5VL_object_get_args_t *args, hid_t dxpl_id, void **req);
void *geotiff_object_open(void *obj, const H5VL_loc_params_t *loc_params, H5I_type_t *opened_type,
                          hid_t dxpl_id, void **req);
herr_t geotiff_attr_specific(void *obj, const H5VL_loc_params_t *loc_params,
                             H5VL_attr_specific_args_t *args, hid_t dxpl_id, void **req);

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
        NULL,                  /* create       */
        geotiff_attr_open,     /* open         */
        geotiff_attr_read,     /* read         */
        NULL,                  /* write        */
        geotiff_attr_get,      /* get          */
        geotiff_attr_specific, /* specific     */
        NULL,                  /* optional     */
        geotiff_attr_close     /* close        */
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
        geotiff_object_open, /* open         */
        NULL,                /* copy         */
        geotiff_object_get,  /* get          */
        NULL,                /* specific     */
        NULL                 /* optional     */
    },
    {
        /* introscpect_cls */
        geotiff_introspect_get_conn_cls,  /* get_conn_cls  */
        geotiff_introspect_get_cap_flags, /* get_cap_flags */
        geotiff_introspect_opt_query      /* opt_query     */
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

/* Parse GDAL metadata XML string (from TIFFTAG_GDAL_METADATA) into key-value pairs.
 * Format: <GDALMetadata><Item name="key">value</Item>...</GDALMetadata>
 * Returns allocated gdal_metadata_t or NULL on failure. */
static gdal_metadata_t *parse_gdal_metadata(const char *xml)
{
    gdal_metadata_t *meta = NULL;
    const char *p;
    if (!xml)
        return NULL;

    meta = (gdal_metadata_t *) calloc(1, sizeof(gdal_metadata_t));
    if (!meta)
        return NULL;

    p = xml;
    while (meta->count < MAX_GDAL_METADATA_ITEMS) {
        /* Find next <Item */
        const char *item_start = strstr(p, "<Item");
        if (!item_start)
            break;

        /* Find name=" */
        const char *name_attr = strstr(item_start, "name=\"");
        if (!name_attr) {
            p = item_start + 5;
            continue;
        }
        name_attr += 6; /* skip name=" */

        /* Find closing " of name */
        const char *name_end = strchr(name_attr, '"');
        if (!name_end) {
            p = item_start + 5;
            continue;
        }

        /* Find > closing the opening tag */
        const char *tag_end = strchr(item_start, '>');
        if (!tag_end) {
            p = item_start + 5;
            continue;
        }
        tag_end++; /* skip > */

        /* Check for self-closing tag /> - skip empty items */
        if (*(tag_end - 2) == '/') {
            p = tag_end;
            continue;
        }

        /* Find </Item> */
        const char *close_tag = strstr(tag_end, "</Item>");
        if (!close_tag) {
            p = tag_end;
            continue;
        }

        /* Extract key and value */
        size_t key_len = (size_t) (name_end - name_attr);
        size_t val_len = (size_t) (close_tag - tag_end);

        meta->items[meta->count].key = (char *) malloc(key_len + 1);
        meta->items[meta->count].value = (char *) malloc(val_len + 1);
        if (!meta->items[meta->count].key || !meta->items[meta->count].value) {
            free(meta->items[meta->count].key);
            free(meta->items[meta->count].value);
            break;
        }
        memcpy(meta->items[meta->count].key, name_attr, key_len);
        meta->items[meta->count].key[key_len] = '\0';
        memcpy(meta->items[meta->count].value, tag_end, val_len);
        meta->items[meta->count].value[val_len] = '\0';
        meta->count++;

        p = close_tag + 7; /* skip </Item> */
    }
    return meta;
}

static void free_gdal_metadata(gdal_metadata_t *meta)
{
    if (!meta)
        return;
    for (int i = 0; i < meta->count; i++) {
        free(meta->items[i].key);
        free(meta->items[i].value);
    }
    free(meta);
}

/* Add a key-value string item to gdal_metadata_t if space allows. */
static void gdal_meta_add(gdal_metadata_t *meta, const char *key, const char *value)
{
    if (!meta || !key || !value)
        return;
    if (meta->count >= MAX_GDAL_METADATA_ITEMS)
        return;
    meta->items[meta->count].key = strdup(key);
    meta->items[meta->count].value = strdup(value);
    if (meta->items[meta->count].key && meta->items[meta->count].value)
        meta->count++;
    else {
        free(meta->items[meta->count].key);
        free(meta->items[meta->count].value);
        meta->items[meta->count].key = NULL;
        meta->items[meta->count].value = NULL;
    }
}

/* Append geo-derived attributes (origin, pixel size, CRS, corner coordinates)
 * to an existing gdal_metadata_t, reading directly from the TIFF/GeoTIFF tags. */
static void geotiff_add_geo_attrs(TIFF *tiff, gdal_metadata_t *meta)
{
    char buf[512];
    uint32_t width = 0, height = 0;
    uint16_t pixscale_count = 0, tiepoint_count = 0;
    double *pixscale = NULL, *tiepoints = NULL;

    if (!tiff || !meta)
        return;

    TIFFGetField(tiff, TIFFTAG_IMAGEWIDTH, &width);
    TIFFGetField(tiff, TIFFTAG_IMAGELENGTH, &height);

    if (!TIFFGetField(tiff, TIFFTAG_GEOPIXELSCALE, &pixscale_count, &pixscale) ||
        pixscale_count < 2)
        return;
    if (!TIFFGetField(tiff, TIFFTAG_GEOTIEPOINTS, &tiepoint_count, &tiepoints) ||
        tiepoint_count < 6)
        return;

    /* Geotransform (GDAL convention):
     *   ulx = tiepoint_geo_x - tiepoint_pixel_x * scale_x
     *   uly = tiepoint_geo_y + tiepoint_pixel_y * scale_y   (scale_y positive in tag)
     *   pixel_y negative for north-up image */
    double scale_x = pixscale[0];
    double scale_y = pixscale[1]; /* positive in tag */
    double tie_pix_x = tiepoints[0];
    double tie_pix_y = tiepoints[1];
    double tie_geo_x = tiepoints[3];
    double tie_geo_y = tiepoints[4];

    double ulx = tie_geo_x - tie_pix_x * scale_x;
    double uly = tie_geo_y + tie_pix_y * scale_y;
    double px = scale_x;
    double py = -scale_y; /* negative = north-up */

    /* GeoTransform (six-element GDAL string) */
    snprintf(buf, sizeof(buf), "%.15g %.15g 0 %.15g 0 %.15g", ulx, px, uly, py);
    gdal_meta_add(meta, "GeoTransform", buf);

    /* Origin and pixel size as separate human-readable attributes */
    snprintf(buf, sizeof(buf), "(%.15g, %.15g)", ulx, uly);
    gdal_meta_add(meta, "origin", buf);

    snprintf(buf, sizeof(buf), "(%.15g, %.15g)", px, py);
    gdal_meta_add(meta, "pixel_size", buf);

    /* Corner coordinates */
    double lrx = ulx + width * px;
    double lry = uly + height * py;
    double cx = ulx + (width / 2.0) * px;
    double cy = uly + (height / 2.0) * py;

    snprintf(buf, sizeof(buf), "(%.7f, %.7f)", ulx, uly);
    gdal_meta_add(meta, "CornerCoordinates_UpperLeft", buf);

    snprintf(buf, sizeof(buf), "(%.7f, %.7f)", ulx, lry);
    gdal_meta_add(meta, "CornerCoordinates_LowerLeft", buf);

    snprintf(buf, sizeof(buf), "(%.7f, %.7f)", lrx, uly);
    gdal_meta_add(meta, "CornerCoordinates_UpperRight", buf);

    snprintf(buf, sizeof(buf), "(%.7f, %.7f)", lrx, lry);
    gdal_meta_add(meta, "CornerCoordinates_LowerRight", buf);

    snprintf(buf, sizeof(buf), "(%.7f, %.7f)", cx, cy);
    gdal_meta_add(meta, "CornerCoordinates_Center", buf);

    /* Coordinate system via GeoTIFF keys */
    GTIF *gtif = GTIFNew(tiff);
    if (gtif) {
        GTIFDefn defn;
        if (GTIFGetDefn(gtif, &defn) && defn.DefnSet) {
            char *crs_name = NULL;
            if (defn.Model == ModelTypeGeographic) {
                GTIFGetGCSInfo(defn.GCS, &crs_name, NULL, NULL, NULL);
                if (crs_name) {
                    snprintf(buf, sizeof(buf), "EPSG:%d (%s)", (int) defn.GCS, crs_name);
                    free(crs_name);
                } else {
                    snprintf(buf, sizeof(buf), "EPSG:%d", (int) defn.GCS);
                }
            } else if (defn.Model == ModelTypeProjected) {
                GTIFGetPCSInfo(defn.PCS, &crs_name, NULL, NULL, NULL);
                if (crs_name) {
                    snprintf(buf, sizeof(buf), "EPSG:%d (%s)", (int) defn.PCS, crs_name);
                    free(crs_name);
                } else {
                    snprintf(buf, sizeof(buf), "EPSG:%d", (int) defn.PCS);
                }
            } else {
                snprintf(buf, sizeof(buf), "Unknown CRS (model=%d)", (int) defn.Model);
            }
            gdal_meta_add(meta, "coordinate_system", buf);

            /* PROJ.4 string */
            char *proj4 = GTIFGetProj4Defn(&defn);
            if (proj4) {
                gdal_meta_add(meta, "proj4", proj4);
                free(proj4);
            }
        }

        /* AREA_OR_POINT from GTRasterTypeGeoKey (1025) */
        unsigned short raster_type = 0;
        if (GTIFKeyGetSHORT(gtif, GTRasterTypeGeoKey, &raster_type, 0, 1) == 1) {
            gdal_meta_add(meta, "AREA_OR_POINT",
                          raster_type == RasterPixelIsPoint ? "Point" : "Area");
        }

        GTIFFree(gtif);
    }

    /* COMPRESSION from TIFF tag */
    uint16_t compression = 0;
    if (TIFFGetField(tiff, TIFFTAG_COMPRESSION, &compression)) {
        const char *comp_str = "UNKNOWN";
        switch (compression) {
            case COMPRESSION_NONE:
                comp_str = "NONE";
                break;
            case COMPRESSION_LZW:
                comp_str = "LZW";
                break;
            case COMPRESSION_OJPEG:
                comp_str = "OJPEG";
                break;
            case COMPRESSION_JPEG:
                comp_str = "JPEG";
                break;
            case COMPRESSION_DEFLATE:
                comp_str = "DEFLATE";
                break;
            case COMPRESSION_ADOBE_DEFLATE:
                comp_str = "DEFLATE";
                break;
            case COMPRESSION_PACKBITS:
                comp_str = "PACKBITS";
                break;
            default:
                snprintf(buf, sizeof(buf), "%u", (unsigned) compression);
                comp_str = buf;
                break;
        }
        gdal_meta_add(meta, "COMPRESSION", comp_str);
    }

    /* INTERLEAVE from PlanarConfig tag (GDAL reports BAND for single-band or separate-plane) */
    uint16_t planar_config = 0;
    uint16_t samples_per_pixel = 1;
    TIFFGetField(tiff, TIFFTAG_SAMPLESPERPIXEL, &samples_per_pixel);
    if (TIFFGetField(tiff, TIFFTAG_PLANARCONFIG, &planar_config)) {
        bool is_band = (planar_config == PLANARCONFIG_SEPARATE) || (samples_per_pixel <= 1);
        gdal_meta_add(meta, "INTERLEAVE", is_band ? "BAND" : "PIXEL");
    }

    /* LAYOUT=COG if file is tiled */
    uint32_t tile_width = 0;
    if (TIFFGetField(tiff, TIFFTAG_TILEWIDTH, &tile_width) && tile_width > 0) {
        gdal_meta_add(meta, "LAYOUT", "COG");
    }

    /* NoData value from GDAL NODATA tag (42113) */
    const char *nodata_str = NULL;
    if (TIFFGetField(tiff, TIFFTAG_GDAL_NODATA, &nodata_str) && nodata_str) {
        gdal_meta_add(meta, "NoData", nodata_str);
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
    geotiff_object_t *file_obj = NULL;
    geotiff_object_t *ret_value = NULL;

    geotiff_file_t *file = NULL; /* Convenience pointer */

    /* We only support read-only access for GeoTIFF files */
    if (flags != H5F_ACC_RDONLY)
        FUNC_GOTO_ERROR(H5E_FILE, H5E_UNSUPPORTED, NULL,
                        "GeoTIFF VOL connector only supports read-only access");

    if ((file_obj = (geotiff_object_t *) calloc(1, sizeof(geotiff_object_t))) == NULL)
        FUNC_GOTO_ERROR(H5E_FILE, H5E_CANTALLOC, NULL,
                        "Failed to allocate memory for GeoTIFF file struct");

    file_obj->obj_type = H5I_FILE;
    file = &file_obj->u.file;
    /* Parent file pointers points to itself */
    file_obj->parent_file = file_obj;
    file_obj->ref_count = 1;

    if ((file->tiff = XTIFFOpen(name, "r")) == NULL)
        FUNC_GOTO_ERROR(H5E_FILE, H5E_CANTOPENFILE, NULL, "Failed to open GeoTIFF file: %s", name);

    if ((file->filename = strdup(name)) == NULL)
        FUNC_GOTO_ERROR(H5E_FILE, H5E_CANTALLOC, NULL, "Failed to duplicate filename string");

    file->flags = flags;
    file->plist_id = fapl_id;

    /* Try to read and parse GDAL metadata (tag 42112) */
    {
        char *gdal_xml = NULL;
        if (TIFFGetField(file->tiff, TIFFTAG_GDAL_METADATA, &gdal_xml) && gdal_xml)
            file->gdal_meta = parse_gdal_metadata(gdal_xml);
    }

    /* Ensure gdal_meta exists, then append geo-derived attributes
     * (origin, pixel size, corner coordinates, CRS). */
    if (!file->gdal_meta)
        file->gdal_meta = (gdal_metadata_t *) calloc(1, sizeof(gdal_metadata_t));
    if (file->gdal_meta)
        geotiff_add_geo_attrs(file->tiff, file->gdal_meta);

    ret_value = file_obj;

done:
    if (!ret_value) {
        if (file_obj) {
            H5E_BEGIN_TRY
            {
                geotiff_file_close(file_obj, dxpl_id, req);
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
    const geotiff_object_t *o = (const geotiff_object_t *) file;
    const geotiff_file_t *f = &o->u.file;
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
        case H5VL_FILE_GET_FCPL:
            if ((args->args.get_fcpl.fcpl_id = H5Pcreate(H5P_FILE_CREATE)) < 0)
                FUNC_GOTO_ERROR(H5E_FILE, H5E_CANTGET, FAIL,
                                "Failed to create file creation property list");
            break;
        case H5VL_FILE_GET_FAPL:
            if ((args->args.get_fapl.fapl_id = H5Pcreate(H5P_FILE_ACCESS)) < 0)
                FUNC_GOTO_ERROR(H5E_FILE, H5E_CANTGET, FAIL,
                                "Failed to create file access property list");
            break;
        case H5VL_FILE_GET_INTENT:
            *args->args.get_intent.flags = H5F_ACC_RDONLY;
            break;
        case H5VL_FILE_GET_FILENO:
            *args->args.get_fileno.fileno = 1;
            break;
        case H5VL_FILE_GET_OBJ_COUNT:
            *args->args.get_obj_count.count = 0;
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
    geotiff_object_t *o = (geotiff_object_t *) file;
    herr_t ret_value = SUCCEED;

    assert(o);

    if (o->ref_count == 0)
        FUNC_GOTO_ERROR(H5E_FILE, H5E_CANTCLOSEFILE, FAIL,
                        "GeoTIFF file already closed (ref_count is 0)");

    o->ref_count--;

    if (o->ref_count == 0) {
        if (o->u.file.tiff)
            TIFFClose(o->u.file.tiff);
        if (o->u.file.filename)
            free(o->u.file.filename);
        if (o->u.file.gdal_meta)
            free_gdal_metadata(o->u.file.gdal_meta);
        free(o);
    }

done:
    return ret_value;
}

/* Dataset operations */
void *geotiff_dataset_open(void *obj, const H5VL_loc_params_t __attribute__((unused)) * loc_params,
                           const char *name, hid_t __attribute__((unused)) dapl_id,
                           hid_t __attribute__((unused)) dxpl_id,
                           void __attribute__((unused)) * *req)
{
    geotiff_object_t *obj_in = (geotiff_object_t *) obj;
    /* dataset_open may be called with a file OR a group as the parent object */
    geotiff_object_t *file_obj = (obj_in->obj_type == H5I_FILE) ? obj_in : obj_in->parent_file;
    geotiff_object_t *dset_obj = NULL;
    geotiff_object_t *ret_value = NULL;

    geotiff_dataset_t *dset = NULL;           /* Convenience pointer */
    geotiff_file_t *file = &file_obj->u.file; /* Convenience pointer */

    uint32_t width = 0;
    uint32_t height = 0;
    uint16_t samples_per_pixel = 0;
    uint16_t bits_per_sample = 0;
    uint16_t sample_format = 0;

    uint16_t num_dirs = 0;
    hsize_t dims[3] = {0, 0, 0};

    H5T_class_t dtype_class = H5T_NO_CLASS;

    if (!file_obj || !name) {
        FUNC_GOTO_ERROR(H5E_ARGS, H5E_BADVALUE, NULL, "Invalid file or dataset name");
    }

    if ((dset_obj = (geotiff_object_t *) malloc(sizeof(geotiff_object_t))) == NULL) {
        FUNC_GOTO_ERROR(H5E_DATASET, H5E_CANTALLOC, NULL,
                        "Failed to allocate memory for GeoTIFF dataset struct");
    }

    dset_obj->obj_type = H5I_DATASET;
    dset_obj->parent_file = file_obj->parent_file;
    dset_obj->ref_count = 1; /* Initialize dataset's own ref count */
    /* Increment file reference count since this dataset holds a reference */
    file_obj->ref_count++;

    dset = &dset_obj->u.dataset;

    if ((dset->name = strdup(name)) == NULL) {
        FUNC_GOTO_ERROR(H5E_DATASET, H5E_CANTALLOC, NULL,
                        "Failed to duplicate dataset name string");
    }

    dset->data = NULL;
    dset->data_size = 0;
    dset->is_image = false;
    dset->is_latlon = false;
    dset->is_lat = false;
    dset->space_id = H5I_INVALID_HID;
    dset->type_id = H5I_INVALID_HID;
    dset->gtif = NULL;
    dset->directory_index = -1;

    /* Parse dataset name: "imageN", "latN", or "lonN" */
    int image_index = 0;
    if (sscanf(name, "/image%d", &image_index) == 1 || sscanf(name, "image%d", &image_index) == 1) {
        if (image_index < 0)
            FUNC_GOTO_ERROR(H5E_DATASET, H5E_BADVALUE, NULL,
                            "Invalid image index %d (must be non-negative)", image_index);
    } else if (sscanf(name, "/lat%d", &image_index) == 1 ||
               sscanf(name, "lat%d", &image_index) == 1) {
        dset->is_latlon = true;
        dset->is_lat = true;
    } else if (sscanf(name, "/lon%d", &image_index) == 1 ||
               sscanf(name, "lon%d", &image_index) == 1) {
        dset->is_latlon = true;
        dset->is_lat = false;
    } else {
        FUNC_GOTO_ERROR(H5E_DATASET, H5E_BADVALUE, NULL,
                        "Invalid dataset name '%s', expected 'imageN', 'latN', or 'lonN'", name);
    }

    /* --- lat/lon coordinate dataset path --- */
    if (dset->is_latlon) {
        uint16_t pixscale_count = 0, tiepoint_count = 0;
        double *pixscale = NULL, *tiepoints = NULL;
        uint32_t img_width = 0, img_height = 0;
        double ulx, uly, px, py;

        num_dirs = (uint16_t) TIFFNumberOfDirectories(file->tiff);
        if (image_index < 0 || (unsigned) image_index >= (unsigned) num_dirs)
            FUNC_GOTO_ERROR(H5E_DATASET, H5E_NOTFOUND, NULL, "Directory %d not found (file has %d)",
                            image_index, (int) num_dirs);

        if (!TIFFSetDirectory(file->tiff, (uint16_t) image_index))
            FUNC_GOTO_ERROR(H5E_DATASET, H5E_CANTGET, NULL, "Failed to set TIFF directory to %d",
                            image_index);

        TIFFGetField(file->tiff, TIFFTAG_IMAGEWIDTH, &img_width);
        TIFFGetField(file->tiff, TIFFTAG_IMAGELENGTH, &img_height);

        if (TIFFGetField(file->tiff, TIFFTAG_GEOPIXELSCALE, &pixscale_count, &pixscale) &&
            pixscale_count >= 2 &&
            TIFFGetField(file->tiff, TIFFTAG_GEOTIEPOINTS, &tiepoint_count, &tiepoints) &&
            tiepoint_count >= 6) {
            /* GeoTIFF tags present in this directory */
            double scale_x = pixscale[0];
            double scale_y = pixscale[1];
            ulx = tiepoints[3] - tiepoints[0] * scale_x;
            uly = tiepoints[4] + tiepoints[1] * scale_y;
            px = scale_x;
            py = -scale_y;
        } else {
            /* Overview directory: derive geotransform from main image (directory 0) */
            uint16_t main_psc = 0, main_tpc = 0;
            double *main_ps = NULL, *main_tp = NULL;
            uint32_t main_w = 0, main_h = 0;

            if (!TIFFSetDirectory(file->tiff, 0))
                FUNC_GOTO_ERROR(H5E_DATASET, H5E_CANTGET, NULL,
                                "Failed to set TIFF directory to 0 for geotransform");
            TIFFGetField(file->tiff, TIFFTAG_IMAGEWIDTH, &main_w);
            TIFFGetField(file->tiff, TIFFTAG_IMAGELENGTH, &main_h);
            if (!TIFFGetField(file->tiff, TIFFTAG_GEOPIXELSCALE, &main_psc, &main_ps) ||
                main_psc < 2 ||
                !TIFFGetField(file->tiff, TIFFTAG_GEOTIEPOINTS, &main_tpc, &main_tp) ||
                main_tpc < 6)
                FUNC_GOTO_ERROR(H5E_DATASET, H5E_CANTGET, NULL,
                                "No geotransform in main TIFF directory");

            double main_scale_x = main_ps[0];
            double main_scale_y = main_ps[1];
            ulx = main_tp[3] - main_tp[0] * main_scale_x;
            uly = main_tp[4] + main_tp[1] * main_scale_y;
            /* Scale pixel size proportionally to the overview dimensions */
            px = main_scale_x * ((double) main_w / (double) img_width);
            py = -main_scale_y * ((double) main_h / (double) img_height);
        }

        dset->directory_index = image_index;

        /* Detect CRS type from directory 0 (has definitive GeoTIFF keys) */
        bool is_geographic = true;
        GTIFDefn latlon_defn;
        memset(&latlon_defn, 0, sizeof(latlon_defn));
        if (TIFFSetDirectory(file->tiff, 0)) {
            GTIF *tmp_gtif = GTIFNew(file->tiff);
            if (tmp_gtif) {
                if (GTIFGetDefn(tmp_gtif, &latlon_defn) && latlon_defn.DefnSet)
                    is_geographic = (latlon_defn.Model == ModelTypeGeographic);
                GTIFFree(tmp_gtif);
            }
        }

        /* Always generate 2D lat/lon arrays [height, width] */
        {
            hsize_t h = (hsize_t) img_height;
            hsize_t w = (hsize_t) img_width;
            hsize_t total = h * w;

            double *X = (double *) malloc(total * sizeof(double));
            double *Y = (double *) malloc(total * sizeof(double));
            if (!X || !Y) {
                free(X);
                free(Y);
                FUNC_GOTO_ERROR(H5E_RESOURCE, H5E_CANTALLOC, NULL,
                                "Failed to allocate coordinate arrays");
            }

            /* Compute coordinates for each pixel using affine transform */
            for (hsize_t row = 0; row < h; row++) {
                for (hsize_t col = 0; col < w; col++) {
                    X[row * w + col] = ulx + (double) col * px;
                    Y[row * w + col] = uly + (double) row * py;
                }
            }

            /* For projected CRS, convert to geographic (lon, lat) in-place */
            if (!is_geographic) {
                if (!GTIFProj4ToLatLong(&latlon_defn, (int) total, X, Y)) {
                    free(X);
                    free(Y);
                    FUNC_GOTO_ERROR(H5E_DATASET, H5E_CANTGET, NULL,
                                    "GTIFProj4ToLatLong failed for projected CRS");
                }
            }

            /* X is longitude, Y is latitude */
            dset->data_size = total * sizeof(double);
            if ((dset->data = malloc(dset->data_size)) == NULL) {
                free(X);
                free(Y);
                FUNC_GOTO_ERROR(H5E_RESOURCE, H5E_CANTALLOC, NULL,
                                "Failed to allocate 2D lat/lon output array");
            }

            double *vals = (double *) dset->data;
            if (dset->is_lat)
                memcpy(vals, Y, dset->data_size);
            else
                memcpy(vals, X, dset->data_size);
            free(X);
            free(Y);

            hsize_t dims[2] = {h, w};
            if ((dset->space_id = H5Screate_simple(2, dims, NULL)) < 0)
                FUNC_GOTO_ERROR(H5E_DATASPACE, H5E_CANTCREATE, NULL,
                                "Failed to create 2D lat/lon dataspace");
        }

        if ((dset->type_id = H5Tcopy(H5T_NATIVE_DOUBLE)) < 0)
            FUNC_GOTO_ERROR(H5E_DATATYPE, H5E_CANTCOPY, NULL,
                            "Failed to copy double type for lat/lon");

        ret_value = dset_obj;
        goto done;
    }

    /* Check if this image directory exists in the TIFF file */
    /* Manual cast because some versions of TIFF return tdir_t */
    num_dirs = (uint16_t) TIFFNumberOfDirectories(file->tiff);
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
            /* Allow alpha channel (extra_sample_count == 1) for RGBA or grayscale+alpha */
            if ((samples_per_pixel == 4 || samples_per_pixel == 2) && extra_sample_count == 1) {
                /* Check that the extra sample is an alpha channel */
                if (extra_samples[0] != EXTRASAMPLE_ASSOCALPHA &&
                    extra_samples[0] != EXTRASAMPLE_UNASSALPHA)
                    FUNC_GOTO_ERROR(H5E_DATASET, H5E_UNSUPPORTED, NULL,
                                    "Unsupported extra sample type: %d (expected alpha channel)",
                                    extra_samples[0]);
            } else if (extra_sample_count > 0) {
                FUNC_GOTO_ERROR(H5E_DATASET, H5E_UNSUPPORTED, NULL,
                                "Unsupported configuration: %d extra samples found (only alpha "
                                "channel for RGBA or grayscale+alpha is supported)",
                                extra_sample_count);
            }
        }

        /* Validate bits per sample is byte-aligned */
        if (bits_per_sample % 8 != 0)
            FUNC_GOTO_ERROR(H5E_DATASET, H5E_UNSUPPORTED, NULL,
                            "Unsupported bits per sample: %d (must be a multiple of 8). 1-bit and "
                            "other sub-byte formats are not supported.",
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
    } else if (samples_per_pixel == 2 || samples_per_pixel == 3 || samples_per_pixel == 4) {
        /* Grayscale+Alpha, RGB, or RGBA: 3D dataspace [height, width, samples] */
        dims[0] = height;
        dims[1] = width;
        dims[2] = samples_per_pixel;
        if ((dset->space_id = H5Screate_simple(3, dims, NULL)) < 0) {
            FUNC_GOTO_ERROR(H5E_DATASET, H5E_CANTCREATE, NULL,
                            "Failed to create dataspace for multi-sample dataset");
        }
    } else {
        FUNC_GOTO_ERROR(H5E_DATASET, H5E_UNSUPPORTED, NULL,
                        "Unsupported samples per pixel: %d (only 1, 2, 3, or 4 supported)",
                        samples_per_pixel);
    }

    if (geotiff_read_image_data(dset_obj) < 0)
        FUNC_GOTO_ERROR(H5E_DATASET, H5E_CANTGET, NULL, "failed to read image data");

    ret_value = dset_obj;

done:
    if (!ret_value) {
        if (dset)
            H5E_BEGIN_TRY
            {
                geotiff_dataset_close(dset_obj, dxpl_id, req);
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
    const geotiff_object_t *dset_obj = (const geotiff_object_t *) dset[0];
    const geotiff_dataset_t *d = NULL; /* Convenience pointer */

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

    assert(dset_obj);
    d = (const geotiff_dataset_t *) &dset_obj->u.dataset;

    if (!buf[0])
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
    const geotiff_object_t *o = (const geotiff_object_t *) dset;
    const geotiff_dataset_t *d = &o->u.dataset;

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

        case H5VL_DATASET_GET_DCPL:
            /* Return a default dataset creation property list */
            if ((args->args.get_dcpl.dcpl_id = H5Pcreate(H5P_DATASET_CREATE)) < 0)
                FUNC_GOTO_ERROR(H5E_DATASET, H5E_CANTGET, FAIL,
                                "Failed to create default dataset creation property list");
            break;

        case H5VL_DATASET_GET_DAPL:
            /* Return a default dataset access property list */
            if ((args->args.get_dapl.dapl_id = H5Pcreate(H5P_DATASET_ACCESS)) < 0)
                FUNC_GOTO_ERROR(H5E_DATASET, H5E_CANTGET, FAIL,
                                "Failed to create default dataset access property list");
            break;

        default:
            FUNC_GOTO_ERROR(H5E_DATASET, H5E_UNSUPPORTED, FAIL,
                            "Unsupported dataset get operation");
            break;
    }
done:
    return ret_value;
}

herr_t geotiff_dataset_close(void *dset, hid_t dxpl_id, void **req)
{
    geotiff_object_t *d = (geotiff_object_t *) dset;
    herr_t ret_value = SUCCEED;

    assert(d);

    /* Use FUNC_DONE_ERROR to try to complete resource release after failure */
    if (!d->parent_file)
        FUNC_DONE_ERROR(H5E_DATASET, H5E_BADVALUE, FAIL,
                        "Dataset has no valid parent file reference");

    /* Decrement dataset's ref count */
    if (d->ref_count == 0)
        FUNC_DONE_ERROR(H5E_DATASET, H5E_CANTCLOSEOBJ, FAIL,
                        "Dataset already closed (ref_count is 0)");

    d->ref_count--;

    /* Only do the real close when ref_count reaches 0 */
    if (d->ref_count == 0) {
        if (d->u.dataset.gtif)
            GTIFFree(d->u.dataset.gtif);
        if (d->u.dataset.name)
            free(d->u.dataset.name);
        if (d->u.dataset.data)
            free(d->u.dataset.data);
        if (d->u.dataset.space_id != H5I_INVALID_HID)
            if (H5Sclose(d->u.dataset.space_id) < 0)
                FUNC_DONE_ERROR(H5E_DATASET, H5E_CLOSEERROR, FAIL, "Failed to close dataspace");
        if (d->u.dataset.type_id != H5I_INVALID_HID)
            if (H5Tclose(d->u.dataset.type_id) < 0)
                FUNC_DONE_ERROR(H5E_DATASET, H5E_CLOSEERROR, FAIL, "Failed to close datatype");

        /* Decrement parent file's reference count */
        if (geotiff_file_close(d->parent_file, dxpl_id, req) < 0)
            FUNC_DONE_ERROR(H5E_DATASET, H5E_CLOSEERROR, FAIL,
                            "Failed to close dataset file object");

        free(d);
    }

    return ret_value;
}

/* Group operations */
void *geotiff_group_open(void *obj, const H5VL_loc_params_t __attribute__((unused)) * loc_params,
                         const char *name, hid_t __attribute__((unused)) gapl_id,
                         hid_t __attribute__((unused)) dxpl_id, void __attribute__((unused)) * *req)
{
    geotiff_object_t *file = (geotiff_object_t *) obj;
    geotiff_object_t *grp_obj = NULL;
    geotiff_object_t *ret_value = NULL;

    geotiff_group_t *grp = NULL; /* Convenience pointer */

    if (!file || !name)
        FUNC_GOTO_ERROR(H5E_ARGS, H5E_BADVALUE, NULL, "Invalid file or group name");

    if (strcmp(name, "/") != 0)
        FUNC_GOTO_ERROR(H5E_ARGS, H5E_UNSUPPORTED, NULL,
                        "GeoTIFF VOL connector currently only supports root group '/'");

    if ((grp_obj = (geotiff_object_t *) calloc(1, sizeof(geotiff_object_t))) == NULL)
        FUNC_GOTO_ERROR(H5E_SYM, H5E_CANTALLOC, NULL,
                        "Failed to allocate memory for GeoTIFF group struct");

    grp_obj->obj_type = H5I_GROUP;
    grp_obj->parent_file = file->parent_file;
    grp_obj->ref_count = 1; /* Initialize group's own ref count */
    /* Increment file reference count since this group holds a reference */
    grp_obj->parent_file->ref_count++;

    grp = &grp_obj->u.group;
    if ((grp->name = strdup(name)) == NULL)
        FUNC_GOTO_ERROR(H5E_SYM, H5E_CANTALLOC, NULL, "Failed to duplicate group name string");

    ret_value = grp_obj;
done:
    if (!ret_value && grp) {
        H5E_BEGIN_TRY
        {
            geotiff_group_close(grp_obj, dxpl_id, req);
        }
        H5E_END_TRY;
    }

    return ret_value;
}

herr_t geotiff_group_get(void *obj, H5VL_group_get_args_t *args,
                         hid_t __attribute__((unused)) dxpl_id, void __attribute__((unused)) * *req)
{
    geotiff_object_t *o = (geotiff_object_t *) obj;
    const geotiff_group_t *grp = (const geotiff_group_t *) &o->u.group; /* Convenience pointer */
    herr_t ret_value = SUCCEED;

    if (!args)
        FUNC_GOTO_ERROR(H5E_ARGS, H5E_BADVALUE, FAIL, "invalid arguments");

    switch (args->op_type) {
        case H5VL_GROUP_GET_INFO: {
            H5G_info_t *ginfo = args->args.get_info.ginfo;
            uint16_t num_dirs = 0;

            if (!grp || !ginfo)
                FUNC_GOTO_ERROR(H5E_ARGS, H5E_BADVALUE, FAIL, "invalid group or info pointer");

            if (!o->parent_file || !o->parent_file->u.file.tiff)
                FUNC_GOTO_ERROR(H5E_ARGS, H5E_BADVALUE, FAIL, "invalid file object");

            /* Get number of TIFF directories (images) in the file */
            /* Manual cast because some versions of TIFF return tdir_t */
            num_dirs = (uint16_t) TIFFNumberOfDirectories(o->parent_file->u.file.tiff);

            /* Fill in group info structure */
            ginfo->storage_type = H5G_STORAGE_TYPE_COMPACT;
            ginfo->nlinks = (hsize_t) num_dirs * 3; /* imageN, latN, lonN per directory */
            ginfo->max_corder = -1;                 /* No creation order tracking */
            ginfo->mounted = false;                 /* No files mounted on this group */

            break;
        }

        case H5VL_GROUP_GET_GCPL: {
            if ((args->args.get_gcpl.gcpl_id = H5Pcreate(H5P_GROUP_CREATE)) < 0)
                FUNC_GOTO_ERROR(H5E_VOL, H5E_CANTGET, FAIL,
                                "Failed to copy default group creation property list");
            break;
        }

        default: {
            FUNC_GOTO_ERROR(H5E_VOL, H5E_UNSUPPORTED, FAIL, "unknown group get operation");
        }
    }

done:
    return ret_value;
}

herr_t geotiff_group_close(void *grp, hid_t dxpl_id, void **req)
{
    geotiff_object_t *o = (geotiff_object_t *) grp;
    geotiff_group_t *g = &o->u.group; /* Convenience pointer */
    herr_t ret_value = SUCCEED;

    assert(g);

    /* Use FUNC_DONE_ERROR to try to complete resource release after failure */

    /* Decrement group's ref count */
    if (o->ref_count == 0)
        FUNC_DONE_ERROR(H5E_SYM, H5E_CANTCLOSEOBJ, FAIL, "Group already closed (ref_count is 0)");

    o->ref_count--;

    /* Only do the real close when ref_count reaches 0 */
    if (o->ref_count == 0) {
        if (g->name)
            free(g->name);

        /* Decrement parent file's reference count */
        if (geotiff_file_close(o->parent_file, dxpl_id, req) < 0)
            FUNC_DONE_ERROR(H5E_SYM, H5E_CLOSEERROR, FAIL, "Failed to close group file object");

        free(o);
    }

    return ret_value;
}

/* Attribute operations */
void *geotiff_attr_open(void *obj, const H5VL_loc_params_t *loc_params, const char *name,
                        hid_t __attribute__((unused)) aapl_id,
                        hid_t __attribute__((unused)) dxpl_id, void __attribute__((unused)) * *req)
{
    geotiff_object_t *parent_obj = NULL;
    geotiff_object_t *attr_obj = NULL;
    geotiff_object_t *ret_value = NULL;

    geotiff_attr_t *attr = NULL; /* Convenience pointer */

    if (!obj || !name || !loc_params)
        FUNC_GOTO_ERROR(H5E_ATTR, H5E_BADVALUE, NULL, "Invalid object or attribute name");

    parent_obj = (geotiff_object_t *) obj;

    /* Determine the type of the parent object */
    if (loc_params->type != H5VL_OBJECT_BY_SELF)
        FUNC_GOTO_ERROR(H5E_ATTR, H5E_UNSUPPORTED, NULL,
                        "Unsupported location parameter type for attribute open");

    if ((attr_obj = (geotiff_object_t *) calloc(1, sizeof(geotiff_object_t))) == NULL)
        FUNC_GOTO_ERROR(H5E_ATTR, H5E_CANTALLOC, NULL,
                        "Failed to allocate memory for GeoTIFF attribute struct");

    attr_obj->obj_type = H5I_ATTR;
    attr_obj->parent_file = parent_obj->parent_file;
    attr_obj->ref_count = 1; /* Initialize attribute's own ref count */
    /* Increment file reference count since this attribute holds a reference */
    attr_obj->parent_file->ref_count++;
    /* Increment parent object's reference count since this attribute holds a reference */
    parent_obj->ref_count++;
    attr = &attr_obj->u.attr;

    if ((attr->name = strdup(name)) == NULL)
        FUNC_GOTO_ERROR(H5E_ATTR, H5E_CANTALLOC, NULL, "Failed to duplicate attribute name string");

    attr->parent = obj;
    attr->space_id = H5I_INVALID_HID;
    attr->type_id = H5I_INVALID_HID;
    if (parent_obj->obj_type == H5I_FILE && strcmp(name, "num_images") == 0) {
        /* Special "num_images" attribute on file object */
        /* Create scalar dataspace for the count */
        if ((attr->space_id = H5Screate(H5S_SCALAR)) < 0)
            FUNC_GOTO_ERROR(H5E_ATTR, H5E_CANTCREATE, NULL,
                            "Failed to create scalar dataspace for num_images attribute");

        /* Use native uint64 type for the count */
        attr->type_id = H5T_NATIVE_UINT64;
    } else if (parent_obj->obj_type == H5I_GROUP || parent_obj->obj_type == H5I_FILE) {
        /* Check if this is a GDAL metadata attribute on the root group */
        const geotiff_file_t *file = &parent_obj->parent_file->u.file;
        const gdal_metadata_t *meta = file->gdal_meta;
        bool found = false;

        if (meta) {
            for (int i = 0; i < meta->count; i++) {
                if (strcmp(meta->items[i].key, name) == 0) {
                    found = true;
                    /* Create scalar dataspace */
                    if ((attr->space_id = H5Screate(H5S_SCALAR)) < 0)
                        FUNC_GOTO_ERROR(H5E_ATTR, H5E_CANTCREATE, NULL,
                                        "Failed to create scalar dataspace for metadata attribute");
                    /* Use variable-length string type */
                    hid_t str_type = H5Tcopy(H5T_C_S1);
                    if (str_type < 0)
                        FUNC_GOTO_ERROR(H5E_ATTR, H5E_CANTCOPY, NULL, "Failed to copy string type");
                    if (H5Tset_size(str_type, H5T_VARIABLE) < 0) {
                        H5Tclose(str_type);
                        FUNC_GOTO_ERROR(H5E_ATTR, H5E_CANTSET, NULL,
                                        "Failed to set variable string size");
                    }
                    attr->type_id = str_type;
                    break;
                }
            }
        }
        if (!found)
            FUNC_GOTO_ERROR(H5E_ATTR, H5E_NOTFOUND, NULL, "Metadata attribute '%s' not found",
                            name);
    } else if (parent_obj->obj_type == H5I_DATASET && parent_obj->u.dataset.is_image) {
        if ((attr->space_id = H5Screate(H5S_SCALAR)) < 0)
            FUNC_GOTO_ERROR(H5E_ATTR, H5E_CANTCREATE, NULL,
                            "Failed to create scalar dataspace for image dataset attribute");
        if (strcmp(name, "coordinates") == 0) {
            hid_t str_type = H5Tcopy(H5T_C_S1);
            if (str_type < 0)
                FUNC_GOTO_ERROR(H5E_ATTR, H5E_CANTCOPY, NULL, "Failed to copy string type");
            if (H5Tset_size(str_type, H5T_VARIABLE) < 0) {
                H5Tclose(str_type);
                FUNC_GOTO_ERROR(H5E_ATTR, H5E_CANTSET, NULL, "Failed to set variable string size");
            }
            attr->type_id = str_type;
        } else if (strcmp(name, "_FillValue") == 0) {
            if ((attr->type_id = H5Tcopy(parent_obj->u.dataset.type_id)) < 0)
                FUNC_GOTO_ERROR(H5E_ATTR, H5E_CANTCOPY, NULL,
                                "Failed to copy dataset type for _FillValue attribute");
        } else {
            FUNC_GOTO_ERROR(H5E_ATTR, H5E_NOTFOUND, NULL, "Unknown attribute '%s' on image dataset",
                            name);
        }
    } else if (parent_obj->obj_type == H5I_DATASET && parent_obj->u.dataset.is_latlon) {
        if (strcmp(name, "units") != 0)
            FUNC_GOTO_ERROR(H5E_ATTR, H5E_NOTFOUND, NULL,
                            "Unknown attribute '%s' on lat/lon dataset", name);
        if ((attr->space_id = H5Screate(H5S_SCALAR)) < 0)
            FUNC_GOTO_ERROR(H5E_ATTR, H5E_CANTCREATE, NULL,
                            "Failed to create scalar dataspace for units attribute");
        hid_t str_type = H5Tcopy(H5T_C_S1);
        if (str_type < 0)
            FUNC_GOTO_ERROR(H5E_ATTR, H5E_CANTCOPY, NULL, "Failed to copy string type");
        if (H5Tset_size(str_type, H5T_VARIABLE) < 0) {
            H5Tclose(str_type);
            FUNC_GOTO_ERROR(H5E_ATTR, H5E_CANTSET, NULL, "Failed to set variable string size");
        }
        attr->type_id = str_type;
    } else {
        /* Unknown attribute - report error */
        FUNC_GOTO_ERROR(H5E_ATTR, H5E_NOTFOUND, NULL, "Unknown attribute '%s' on object type %d",
                        name, parent_obj->obj_type);
    }

    ret_value = attr_obj;
done:
    if (!ret_value && attr_obj) {
        H5E_BEGIN_TRY
        {
            geotiff_attr_close(attr_obj, dxpl_id, req);
        }
        H5E_END_TRY;
    }
    return ret_value;
}

// cppcheck-suppress constParameterCallback
herr_t geotiff_attr_read(void *attr, hid_t __attribute__((unused)) mem_type_id, void *buf,
                         hid_t __attribute__((unused)) dxpl_id, void __attribute__((unused)) * *req)
{
    const geotiff_object_t *o = (const geotiff_object_t *) attr;
    const geotiff_attr_t *a = NULL; /* Convenience pointer */
    herr_t ret_value = SUCCEED;

    assert(o);

    a = &o->u.attr;

    if (!buf)
        FUNC_GOTO_ERROR(H5E_ATTR, H5E_BADVALUE, FAIL, "Invalid attribute or buffer");

    if (strcmp(a->name, "num_images") == 0) {
        /* Handle the num_images attribute on file object */
        const geotiff_object_t *parent_obj = (const geotiff_object_t *) a->parent;

        if (parent_obj->obj_type != H5I_FILE)
            FUNC_GOTO_ERROR(H5E_ATTR, H5E_BADVALUE, FAIL,
                            "num_images attribute only valid on file objects");

        if (!parent_obj->u.file.tiff)
            FUNC_GOTO_ERROR(H5E_ATTR, H5E_BADVALUE, FAIL, "Invalid TIFF file handle");

        /* Get number of directories and write to buffer */
        uint64_t num_dirs = (uint64_t) TIFFNumberOfDirectories(parent_obj->u.file.tiff);
        *((uint64_t *) buf) = num_dirs;
    } else if (strcmp(a->name, "_FillValue") == 0) {
        const geotiff_object_t *parent_obj = (const geotiff_object_t *) a->parent;
        if (parent_obj->obj_type != H5I_DATASET || !parent_obj->u.dataset.is_image)
            FUNC_GOTO_ERROR(H5E_ATTR, H5E_BADVALUE, FAIL,
                            "_FillValue attribute only valid on image datasets");
        const geotiff_file_t *file = &parent_obj->parent_file->u.file;
        const char *nodata_str = NULL;
        if (file->gdal_meta) {
            for (int i = 0; i < file->gdal_meta->count; i++) {
                if (strcmp(file->gdal_meta->items[i].key, "NoData") == 0) {
                    nodata_str = file->gdal_meta->items[i].value;
                    break;
                }
            }
        }
        if (!nodata_str)
            FUNC_GOTO_ERROR(H5E_ATTR, H5E_NOTFOUND, FAIL,
                            "No NoData value found in GeoTIFF metadata");
        hid_t type_id = parent_obj->u.dataset.type_id;
        H5T_class_t cls = H5Tget_class(type_id);
        size_t sz = H5Tget_size(type_id);
        double dval = strtod(nodata_str, NULL);
        if (cls == H5T_FLOAT) {
            if (sz == 4) {
                float fv = (float) dval;
                memcpy(buf, &fv, 4);
            } else {
                memcpy(buf, &dval, 8);
            }
        } else if (cls == H5T_INTEGER) {
            H5T_sign_t sgn = H5Tget_sign(type_id);
            if (sgn == H5T_SGN_NONE) {
                unsigned long long uv = (unsigned long long) dval;
                switch (sz) {
                    case 1:
                        *(uint8_t *) buf = (uint8_t) uv;
                        break;
                    case 2:
                        *(uint16_t *) buf = (uint16_t) uv;
                        break;
                    case 4:
                        *(uint32_t *) buf = (uint32_t) uv;
                        break;
                    default:
                        *(uint64_t *) buf = (uint64_t) uv;
                        break;
                }
            } else {
                long long sv = (long long) dval;
                switch (sz) {
                    case 1:
                        *(int8_t *) buf = (int8_t) sv;
                        break;
                    case 2:
                        *(int16_t *) buf = (int16_t) sv;
                        break;
                    case 4:
                        *(int32_t *) buf = (int32_t) sv;
                        break;
                    default:
                        *(int64_t *) buf = (int64_t) sv;
                        break;
                }
            }
        } else {
            FUNC_GOTO_ERROR(H5E_ATTR, H5E_UNSUPPORTED, FAIL,
                            "Unsupported type class for _FillValue");
        }
    } else if (strcmp(a->name, "coordinates") == 0) {
        const geotiff_object_t *parent_obj = (const geotiff_object_t *) a->parent;
        if (parent_obj->obj_type != H5I_DATASET || !parent_obj->u.dataset.is_image)
            FUNC_GOTO_ERROR(H5E_ATTR, H5E_BADVALUE, FAIL,
                            "coordinates attribute only valid on image datasets");
        int idx = parent_obj->u.dataset.directory_index;
        char coord_val[32];
        snprintf(coord_val, sizeof(coord_val), "lat%d lon%d", idx, idx);
        char *str_copy = strdup(coord_val);
        if (!str_copy)
            FUNC_GOTO_ERROR(H5E_ATTR, H5E_CANTALLOC, FAIL,
                            "Failed to duplicate coordinates string");
        *((char **) buf) = str_copy;
    } else if (strcmp(a->name, "units") == 0) {
        const geotiff_object_t *parent_obj = (const geotiff_object_t *) a->parent;
        if (parent_obj->obj_type != H5I_DATASET || !parent_obj->u.dataset.is_latlon)
            FUNC_GOTO_ERROR(H5E_ATTR, H5E_BADVALUE, FAIL,
                            "units attribute only valid on lat/lon datasets");
        const char *units = parent_obj->u.dataset.is_lat ? "degrees_north" : "degrees_east";
        char *str_copy = strdup(units);
        if (!str_copy)
            FUNC_GOTO_ERROR(H5E_ATTR, H5E_CANTALLOC, FAIL, "Failed to duplicate units string");
        *((char **) buf) = str_copy;
    } else {
        /* GDAL metadata string attribute */
        const geotiff_object_t *parent_obj = (const geotiff_object_t *) a->parent;
        const geotiff_file_t *file = &parent_obj->parent_file->u.file;
        const gdal_metadata_t *meta = file->gdal_meta;
        const char *val = NULL;

        if (meta) {
            for (int i = 0; i < meta->count; i++) {
                if (strcmp(meta->items[i].key, a->name) == 0) {
                    val = meta->items[i].value;
                    break;
                }
            }
        }

        if (!val)
            FUNC_GOTO_ERROR(H5E_ATTR, H5E_NOTFOUND, FAIL,
                            "Metadata attribute '%s' not found for read", a->name);

        /* For variable-length strings, HDF5 will free the buffer with H5free_memory()/free().
         * Return a malloc-allocated copy so free_gdal_metadata() doesn't double-free. */
        char *str_copy = strdup(val);
        if (!str_copy)
            FUNC_GOTO_ERROR(H5E_ATTR, H5E_CANTALLOC, FAIL,
                            "Failed to duplicate attribute value string");
        *((char **) buf) = str_copy;
    }

done:
    return ret_value;
}

// cppcheck-suppress constParameterCallback
herr_t geotiff_attr_get(void *obj, H5VL_attr_get_args_t *args,
                        hid_t __attribute__((unused)) dxpl_id, void __attribute__((unused)) * *req)
{
    const geotiff_object_t *o = (const geotiff_object_t *) obj;
    const geotiff_attr_t *a = &o->u.attr;

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
        case H5VL_ATTR_GET_NAME: {
            const char *name = a->name;
            size_t name_len = name ? strlen(name) : 0;
            if (args->args.get_name.buf && args->args.get_name.buf_size > 0) {
                size_t ncopy = name_len < args->args.get_name.buf_size - 1
                                   ? name_len
                                   : args->args.get_name.buf_size - 1;
                memcpy(args->args.get_name.buf, name, ncopy);
                args->args.get_name.buf[ncopy] = '\0';
            }
            if (args->args.get_name.attr_name_len)
                *args->args.get_name.attr_name_len = name_len;
            break;
        }
        case H5VL_ATTR_GET_INFO: {
            H5A_info_t *ainfo = args->args.get_info.ainfo;
            if (!ainfo)
                FUNC_GOTO_ERROR(H5E_ATTR, H5E_BADVALUE, FAIL, "NULL ainfo pointer");
            memset(ainfo, 0, sizeof(H5A_info_t));
            ainfo->corder_valid = false;
            ainfo->cset = H5T_CSET_ASCII;
            if (a->name) {
                /* Look up value length */
                const geotiff_object_t *parent_obj = (const geotiff_object_t *) a->parent;
                const geotiff_file_t *file = &parent_obj->parent_file->u.file;
                const gdal_metadata_t *meta = file->gdal_meta;
                if (meta) {
                    for (int i = 0; i < meta->count; i++) {
                        if (strcmp(meta->items[i].key, a->name) == 0) {
                            ainfo->data_size = strlen(meta->items[i].value) + 1;
                            break;
                        }
                    }
                }
            }
            break;
        }
        default:
            FUNC_GOTO_ERROR(H5E_ATTR, H5E_UNSUPPORTED, FAIL, "Unsupported attribute get operation");
            break;
    }

done:
    return ret_value;
}

herr_t geotiff_attr_specific(void *obj,
                             const H5VL_loc_params_t __attribute__((unused)) * loc_params,
                             H5VL_attr_specific_args_t *args, hid_t __attribute__((unused)) dxpl_id,
                             void __attribute__((unused)) * *req)
{
    geotiff_object_t *o = (geotiff_object_t *) obj;
    herr_t ret_value = SUCCEED;

    if (!obj || !args)
        FUNC_GOTO_ERROR(H5E_ARGS, H5E_BADVALUE, FAIL, "Invalid arguments");

    switch (args->op_type) {
        case H5VL_ATTR_ITER: {
            H5VL_attr_iterate_args_t *iter = &args->args.iterate;
            const geotiff_file_t *file = &o->parent_file->u.file;
            const gdal_metadata_t *meta = file->gdal_meta;
            hid_t loc_id = H5I_INVALID_HID;

            if (!iter)
                FUNC_GOTO_ERROR(H5E_ATTR, H5E_BADVALUE, FAIL, "Invalid iterator args");

            hsize_t start_idx = iter->idx ? *iter->idx : 0;

            /* For dataset objects, expose per-dataset attributes */
            if (o->obj_type == H5I_DATASET) {
                /* Build attribute list for this dataset type */
                static const char *latlon_attrs[] = {"units"};
                static const char *image_attr_names[] = {"coordinates", "_FillValue"};

                const char **attr_list = NULL;
                int num_attrs = 0;
                const char *nodata_str = NULL;

                if (o->u.dataset.is_latlon) {
                    attr_list = latlon_attrs;
                    num_attrs = 1;
                } else if (o->u.dataset.is_image) {
                    /* Check if NoData exists */
                    if (meta) {
                        for (int k = 0; k < meta->count; k++) {
                            if (strcmp(meta->items[k].key, "NoData") == 0) {
                                nodata_str = meta->items[k].value;
                                break;
                            }
                        }
                    }
                    attr_list = image_attr_names;
                    num_attrs = nodata_str ? 2 : 1;
                } else {
                    break;
                }

                if (start_idx >= (hsize_t) num_attrs)
                    break;

                o->ref_count++;
                if ((loc_id = H5VLwrap_register((void *) o, H5I_DATASET)) < 0) {
                    o->ref_count--;
                    FUNC_GOTO_ERROR(H5E_ATTR, H5E_CANTREGISTER, FAIL,
                                    "Failed to wrap dataset for attribute iteration");
                }

                for (hsize_t i = start_idx; i < (hsize_t) num_attrs; i++) {
                    if (iter->idx)
                        *iter->idx = i;
                    if (!iter->op)
                        continue;
                    H5A_info_t ainfo;
                    memset(&ainfo, 0, sizeof(H5A_info_t));
                    ainfo.data_size = strlen(attr_list[i]) + 1;
                    herr_t cb_ret = iter->op(loc_id, attr_list[i], &ainfo, iter->op_data);
                    if (iter->idx)
                        *iter->idx = i + 1;
                    if (cb_ret < 0) {
                        H5Idec_ref(loc_id);
                        FUNC_GOTO_ERROR(H5E_ATTR, H5E_BADITER, FAIL,
                                        "Attribute iterator callback returned error");
                    } else if (cb_ret > 0) {
                        H5Idec_ref(loc_id);
                        ret_value = cb_ret;
                        goto done;
                    }
                }
                H5Idec_ref(loc_id);
                break;
            }

            /* Only iterate file-level metadata on file or group objects */
            if (o->obj_type != H5I_FILE && o->obj_type != H5I_GROUP)
                break;

            if (!meta || meta->count == 0)
                break; /* No attributes */

            /* Create a valid HID for the object using H5VLwrap_register so the
             * user's callback can call H5Aopen(loc_id, attr_name, ...).
             * Increment ref_count BEFORE registering so that H5Idec_ref(loc_id)
             * doesn't free the object (it closes the HID but leaves the object
             * alive for the caller who still holds the original gid reference). */
            {
                H5I_type_t htype = (o->obj_type == H5I_FILE) ? H5I_FILE : H5I_GROUP;
                o->ref_count++; /* extra ref so H5Idec_ref won't free us */
                if ((loc_id = H5VLwrap_register((void *) o, htype)) < 0) {
                    o->ref_count--; /* undo extra ref on failure */
                    FUNC_GOTO_ERROR(H5E_ATTR, H5E_CANTREGISTER, FAIL,
                                    "Failed to wrap object for attribute iteration");
                }
            }

            for (hsize_t i = start_idx; i < (hsize_t) meta->count; i++) {
                H5A_info_t ainfo;
                herr_t cb_ret;

                memset(&ainfo, 0, sizeof(H5A_info_t));
                ainfo.corder_valid = false;
                ainfo.corder = (H5O_msg_crt_idx_t) i;
                ainfo.cset = H5T_CSET_ASCII;
                ainfo.data_size = strlen(meta->items[i].value) + 1;

                if (iter->idx)
                    *iter->idx = i;

                if (!iter->op)
                    continue;

                cb_ret = iter->op(loc_id, meta->items[i].key, &ainfo, iter->op_data);
                if (iter->idx)
                    *iter->idx = i + 1;

                if (cb_ret < 0) {
                    H5Idec_ref(loc_id);
                    FUNC_GOTO_ERROR(H5E_ATTR, H5E_BADITER, FAIL,
                                    "Attribute iterator callback returned error");
                } else if (cb_ret > 0) {
                    ret_value = cb_ret;
                    H5Idec_ref(loc_id);
                    goto done;
                }
            }

            H5Idec_ref(loc_id);
            break;
        }

        case H5VL_ATTR_EXISTS: {
            const geotiff_file_t *file = &o->parent_file->u.file;
            const gdal_metadata_t *meta = file->gdal_meta;
            const char *name = args->args.exists.name;
            bool found = false;

            if (o->obj_type == H5I_DATASET && o->u.dataset.is_latlon) {
                found = (strcmp(name, "units") == 0);
            } else if (o->obj_type == H5I_DATASET && o->u.dataset.is_image) {
                if (strcmp(name, "coordinates") == 0) {
                    found = true;
                } else if (strcmp(name, "_FillValue") == 0) {
                    if (meta) {
                        for (int k = 0; k < meta->count; k++) {
                            if (strcmp(meta->items[k].key, "NoData") == 0) {
                                found = true;
                                break;
                            }
                        }
                    }
                }
            } else if ((o->obj_type == H5I_FILE || o->obj_type == H5I_GROUP) && meta) {
                for (int i = 0; i < meta->count; i++) {
                    if (strcmp(meta->items[i].key, name) == 0) {
                        found = true;
                        break;
                    }
                }
            }
            *args->args.exists.exists = found;
            break;
        }

        default:
            FUNC_GOTO_ERROR(H5E_ATTR, H5E_UNSUPPORTED, FAIL,
                            "Unsupported attr specific operation %d", args->op_type);
    }

done:
    return ret_value;
}

herr_t geotiff_attr_close(void *attr, hid_t dxpl_id, void **req)
{
    geotiff_object_t *o = (geotiff_object_t *) attr;
    geotiff_attr_t *a = &o->u.attr;
    geotiff_object_t *parent_obj = NULL;
    herr_t ret_value = SUCCEED;

    assert(a);

    /* Decrement attribute's ref count */
    if (o->ref_count == 0)
        FUNC_DONE_ERROR(H5E_ATTR, H5E_CANTCLOSEOBJ, FAIL,
                        "Attribute already closed (ref_count is 0)");

    o->ref_count--;

    /* Only do the real close when ref_count reaches 0 */
    if (o->ref_count == 0) {
        /* Use FUNC_DONE_ERROR to try to complete resource release after failure */
        if (a->name)
            free(a->name);
        if (a->space_id != H5I_INVALID_HID)
            if (H5Sclose(a->space_id) < 0)
                FUNC_DONE_ERROR(H5E_ATTR, H5E_CLOSEERROR, FAIL,
                                "Failed to close attribute dataspace");
        /* Only close type_id if it's not a predefined type (like H5T_NATIVE_*) */
        if (a->type_id != H5I_INVALID_HID && a->type_id != H5T_NATIVE_CHAR &&
            a->type_id != H5T_NATIVE_UINT64)
            if (H5Tclose(a->type_id) < 0)
                FUNC_DONE_ERROR(H5E_ATTR, H5E_CLOSEERROR, FAIL,
                                "Failed to close attribute datatype");

        /* Close parent object (dataset, group, or file) */
        parent_obj = (geotiff_object_t *) a->parent;
        if (parent_obj) {
            switch (parent_obj->obj_type) {
                case H5I_FILE:
                    if (geotiff_file_close(parent_obj, dxpl_id, req) < 0)
                        FUNC_DONE_ERROR(H5E_ATTR, H5E_CLOSEERROR, FAIL,
                                        "Failed to close attribute's parent file");
                    break;
                case H5I_DATASET:
                    if (geotiff_dataset_close(parent_obj, dxpl_id, req) < 0)
                        FUNC_DONE_ERROR(H5E_ATTR, H5E_CLOSEERROR, FAIL,
                                        "Failed to close attribute's parent dataset");
                    break;
                case H5I_GROUP:
                    if (geotiff_group_close(parent_obj, dxpl_id, req) < 0)
                        FUNC_DONE_ERROR(H5E_ATTR, H5E_CLOSEERROR, FAIL,
                                        "Failed to close attribute's parent group");
                    break;
                default:
                    FUNC_DONE_ERROR(H5E_ATTR, H5E_BADVALUE, FAIL, "Invalid parent object type");
            }
        }

        /* Also decrement the file reference count */
        if (geotiff_file_close(o->parent_file, dxpl_id, req) < 0)
            FUNC_DONE_ERROR(H5E_ATTR, H5E_CLOSEERROR, FAIL,
                            "Failed to close attribute file object");

        free(o);
    }

    return ret_value;
}

/* Helper function to read hyperslab selection (bands/regions) from GeoTIFF */
herr_t geotiff_read_hyperslab(const geotiff_object_t *dset_obj, const hsize_t *start,
                              const hsize_t __attribute__((unused)) * stride, const hsize_t *count,
                              const hsize_t *block, int ndims,
                              hid_t __attribute__((unused)) mem_type_id, void *buf)
{
    geotiff_object_t *file_obj = dset_obj->parent_file;
    geotiff_file_t *file = NULL; /* Convenience pointer */

    uint32_t width, height;
    uint16_t samples_per_pixel, bits_per_sample, sample_format;
    size_t elem_size;
    tsize_t scanline_size;
    herr_t ret_value = SUCCEED;
    unsigned char *scanline_buf = NULL;
    unsigned char *output = (unsigned char *) buf;
    hsize_t row_start, row_count, col_start, col_count, band_start, band_count;
    hsize_t band_idx;

    assert(file_obj);
    file = &file_obj->u.file;

    if (!file->tiff || !buf)
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
static herr_t geotiff_read_image_data(geotiff_object_t *dset_obj)
{
    geotiff_dataset_t *dset = NULL; /* Convenience pointer */
    geotiff_file_t *file = NULL;    /* Convenience pointer */

    assert(dset_obj);
    dset = &dset_obj->u.dataset;

    assert(dset_obj->parent_file);
    file = &dset_obj->parent_file->u.file;

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
    if (total_size > SIZE_LIMIT_BYTES)
        FUNC_GOTO_ERROR(H5E_DATASET, H5E_CANTGET, FAIL,
                        "total read size of %zu bytes exceeds limit of %zu bytes", total_size,
                        (size_t) SIZE_LIMIT_BYTES);

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
                size_t elem_size = bits_per_sample / 8;
                for (uint32_t ty = 0; ty < actual_tile_height; ty++) {
                    uint32_t image_row = tile_row + ty;
                    size_t tile_row_offset =
                        (size_t) ty * tile_width * samples_per_pixel * elem_size;
                    size_t image_row_offset =
                        (size_t) image_row * (size_t) scanline_size +
                        (size_t) tile_col * samples_per_pixel * elem_size;
                    size_t copy_bytes = actual_tile_width * samples_per_pixel * elem_size;

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

    /* Register GDAL metadata tags with libtiff tag extender */
    geotiff_parent_extender_g = TIFFSetTagExtender(geotiff_gdal_tag_extender);

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
    }

    return ret_value;
}

herr_t geotiff_introspect_get_conn_cls(void GEOTIFF_UNUSED_PARAM *obj,
                                       H5VL_get_conn_lvl_t GEOTIFF_UNUSED_PARAM lvl,
                                       const H5VL_class_t GEOTIFF_UNUSED_PARAM **conn_cls)
{
    herr_t ret_value = SUCCEED;

    assert(conn_cls);

#ifdef _MSC_VER
    GEOTIFF_UNUSED(obj);
    GEOTIFF_UNUSED(lvl);
#endif

    /* Retrieve the VOL connector class */
    *conn_cls = &geotiff_class_g;

    return ret_value;
}

/* Open a named object (used by H5Oopen).  Dispatches to dataset_open or
 * group_open based on the object name. */
void *geotiff_object_open(void *obj, const H5VL_loc_params_t *loc_params, H5I_type_t *opened_type,
                          hid_t dxpl_id, void **req)
{
    void *ret_value = NULL;

    if (!obj || !loc_params || !opened_type)
        FUNC_GOTO_ERROR(H5E_OHDR, H5E_BADVALUE, NULL, "Invalid arguments");

    if (loc_params->type == H5VL_OBJECT_BY_NAME) {
        const char *name = loc_params->loc_data.loc_by_name.name;
        int image_idx = -1;

        if (!name || strcmp(name, "/") == 0 || strcmp(name, ".") == 0) {
            /* Root group */
            ret_value = geotiff_group_open(obj, loc_params, "/", H5P_DEFAULT, dxpl_id, req);
            *opened_type = H5I_GROUP;
        } else if (sscanf(name, "image%d", &image_idx) == 1 ||
                   sscanf(name, "/image%d", &image_idx) == 1 ||
                   sscanf(name, "lat%d", &image_idx) == 1 ||
                   sscanf(name, "/lat%d", &image_idx) == 1 ||
                   sscanf(name, "lon%d", &image_idx) == 1 ||
                   sscanf(name, "/lon%d", &image_idx) == 1) {
            /* Dataset (image, lat, or lon) */
            ret_value = geotiff_dataset_open(obj, loc_params, name, H5P_DEFAULT, dxpl_id, req);
            *opened_type = H5I_DATASET;
        } else {
            FUNC_GOTO_ERROR(H5E_OHDR, H5E_NOTFOUND, NULL, "Unknown object name: %s", name);
        }
    } else if (loc_params->type == H5VL_OBJECT_BY_SELF) {
        /* Return the object itself (used by H5Oopen with ".") */
        geotiff_object_t *o = (geotiff_object_t *) obj;
        o->ref_count++;
        *opened_type = o->obj_type;
        ret_value = obj;
    } else {
        FUNC_GOTO_ERROR(H5E_OHDR, H5E_UNSUPPORTED, NULL,
                        "Unsupported location parameter type for object open");
    }

done:
    return ret_value;
}

herr_t geotiff_object_get(void *obj, const H5VL_loc_params_t *loc_params,
                          H5VL_object_get_args_t *args, hid_t __attribute__((unused)) dxpl_id,
                          void __attribute__((unused)) * *req)
{
    const geotiff_object_t *o = (const geotiff_object_t *) obj;
    herr_t ret_value = SUCCEED;

    if (!obj || !args)
        FUNC_GOTO_ERROR(H5E_ARGS, H5E_BADVALUE, FAIL, "Invalid arguments");

    switch (args->op_type) {
        case H5VL_OBJECT_GET_INFO: {
            H5O_info2_t *oinfo = args->args.get_info.oinfo;
            if (!oinfo)
                FUNC_GOTO_ERROR(H5E_ARGS, H5E_BADVALUE, FAIL, "NULL oinfo pointer");

            memset(oinfo, 0, sizeof(H5O_info2_t));
            oinfo->fileno = 1;
            oinfo->rc = 1;

            /* Determine object type from location params or object type */
            if (loc_params && loc_params->type == H5VL_OBJECT_BY_NAME) {
                const char *name = loc_params->loc_data.loc_by_name.name;
                if (!name || strcmp(name, "/") == 0 || strcmp(name, ".") == 0)
                    oinfo->type = H5O_TYPE_GROUP;
                else
                    oinfo->type = H5O_TYPE_DATASET;
            } else {
                switch (o->obj_type) {
                    case H5I_FILE:
                    case H5I_GROUP:
                        oinfo->type = H5O_TYPE_GROUP;
                        break;
                    case H5I_DATASET:
                        oinfo->type = H5O_TYPE_DATASET;
                        break;
                    default:
                        oinfo->type = H5O_TYPE_UNKNOWN;
                        break;
                }
            }

            /* Report the number of GDAL metadata attributes if this is root group */
            if (oinfo->type == H5O_TYPE_GROUP) {
                const geotiff_file_t *file = &o->parent_file->u.file;
                if (file->gdal_meta)
                    oinfo->num_attrs = (hsize_t) file->gdal_meta->count;
            }
            break;
        }

        case H5VL_OBJECT_GET_TYPE: {
            H5O_type_t *obj_type = args->args.get_type.obj_type;
            if (!obj_type)
                FUNC_GOTO_ERROR(H5E_ARGS, H5E_BADVALUE, FAIL, "NULL obj_type pointer");
            switch (o->obj_type) {
                case H5I_FILE:
                case H5I_GROUP:
                    *obj_type = H5O_TYPE_GROUP;
                    break;
                case H5I_DATASET:
                    *obj_type = H5O_TYPE_DATASET;
                    break;
                default:
                    *obj_type = H5O_TYPE_UNKNOWN;
                    break;
            }
            break;
        }

        default:
            FUNC_GOTO_ERROR(H5E_VOL, H5E_UNSUPPORTED, FAIL, "Unsupported object get operation %d",
                            args->op_type);
    }

done:
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
    geotiff_object_t *o = (geotiff_object_t *) obj;
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

            {
                const geotiff_file_t *file = &((geotiff_object_t *) obj)->parent_file->u.file;
                int image_index = -1;
                bool exists = false;

                if ((sscanf(link_name, "image%d", &image_index) == 1 ||
                     sscanf(link_name, "/image%d", &image_index) == 1 ||
                     sscanf(link_name, "lat%d", &image_index) == 1 ||
                     sscanf(link_name, "/lat%d", &image_index) == 1 ||
                     sscanf(link_name, "lon%d", &image_index) == 1 ||
                     sscanf(link_name, "/lon%d", &image_index) == 1) &&
                    image_index >= 0) {
                    uint16_t num_dirs = (uint16_t) TIFFNumberOfDirectories(file->tiff);
                    exists = ((unsigned) image_index < (unsigned) num_dirs);
                }
                *args->args.exists.exists = exists;
            }
            break;
        }

        case H5VL_LINK_ITER: {
            H5VL_link_iterate_args_t *iter_args = &args->args.iterate;
            const geotiff_file_t *file = &((geotiff_object_t *) obj)->parent_file->u.file;
            uint16_t num_dirs;
            hid_t loc_id = H5I_INVALID_HID;
            hsize_t start_idx;

            assert(iter_args);

            if (!iter_args->op)
                break; /* No callback, nothing to do */

            if (!file->tiff)
                FUNC_GOTO_ERROR(H5E_LINK, H5E_BADVALUE, FAIL, "Invalid TIFF file handle");

            num_dirs = (uint16_t) TIFFNumberOfDirectories(file->tiff);

            /* Wrap the object as a valid HID for the callback's group argument */
            o->ref_count++;
            loc_id =
                H5VLwrap_register((void *) o, (o->obj_type == H5I_FILE) ? H5I_FILE : H5I_GROUP);
            if (loc_id < 0) {
                o->ref_count--;
                FUNC_GOTO_ERROR(H5E_LINK, H5E_CANTREGISTER, FAIL,
                                "Failed to wrap object for link iteration");
            }

            start_idx = iter_args->idx_p ? *iter_args->idx_p : 0;

            for (hsize_t i = start_idx; i < (hsize_t) num_dirs * 3; i++) {
                char dset_name[32];
                H5L_info2_t linfo;
                herr_t cb_ret;
                unsigned dir = (unsigned) (i / 3);
                unsigned kind = (unsigned) (i % 3); /* 0=image, 1=lat, 2=lon */

                if (kind == 0)
                    snprintf(dset_name, sizeof(dset_name), "image%u", dir);
                else if (kind == 1)
                    snprintf(dset_name, sizeof(dset_name), "lat%u", dir);
                else
                    snprintf(dset_name, sizeof(dset_name), "lon%u", dir);

                memset(&linfo, 0, sizeof(H5L_info2_t));
                linfo.type = H5L_TYPE_HARD;
                linfo.corder_valid = true;
                linfo.corder = (int64_t) i;
                linfo.cset = H5T_CSET_ASCII;
                memset(&linfo.u.token, 0, sizeof(linfo.u.token));
                linfo.u.token.__data[0] = (uint8_t) (i & 0xFF);

                if (iter_args->idx_p)
                    *iter_args->idx_p = i;

                cb_ret = iter_args->op(loc_id, dset_name, &linfo, iter_args->op_data);

                if (iter_args->idx_p)
                    *iter_args->idx_p = i + 1;

                if (cb_ret < 0) {
                    H5Idec_ref(loc_id);
                    FUNC_GOTO_ERROR(H5E_LINK, H5E_BADITER, FAIL,
                                    "Link iterator callback returned error");
                } else if (cb_ret > 0) {
                    ret_value = cb_ret;
                    H5Idec_ref(loc_id);
                    goto done;
                }
            }

            H5Idec_ref(loc_id);
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

herr_t geotiff_introspect_get_cap_flags(const void GEOTIFF_UNUSED_PARAM *info, uint64_t *cap_flags)
{
    herr_t ret_value = SUCCEED;

    assert(cap_flags);

#ifdef _MSC_VER
    GEOTIFF_UNUSED(info);
#endif

    /* Set capability flags for the GeoTIFF VOL connector */

    /* Basic flags are not entirely accurate,
       since dataset/attr/group creation is architecturally unsupported */
    *cap_flags = H5VL_CAP_FLAG_FILE_BASIC | H5VL_CAP_FLAG_ATTR_BASIC | H5VL_CAP_FLAG_DATASET_BASIC |
                 H5VL_CAP_FLAG_GROUP_BASIC;

    return ret_value;
}
