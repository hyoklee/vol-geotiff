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

#ifndef _geotiff_vol_connector_H
#define _geotiff_vol_connector_H

/* Include geotiff headers (CMake adds the appropriate include path) */
#include "geotiff_vol_err.h" /* Error reporting macros */
#include <geotiff.h>
#include <hdf5.h>
#include <stdint.h>
#include <tiffio.h>

/* Cross-platform unused parameter macro */
#ifdef _MSC_VER
#define GEOTIFF_UNUSED(x) (void) (x)
#define GEOTIFF_UNUSED_PARAM
#else
#define GEOTIFF_UNUSED(x)
#define GEOTIFF_UNUSED_PARAM __attribute__((unused))
#endif

/* The value must be between 256 and 65535 (inclusive) */
#define GEOTIFF_VOL_CONNECTOR_VALUE ((H5VL_class_value_t) 12203)
#define GEOTIFF_VOL_CONNECTOR_NAME "geotiff_vol_connector"

/* Forward declaration for GDAL metadata type used in geotiff_file_t */
typedef struct gdal_metadata_t gdal_metadata_t;

/* GeoTIFF VOL file object structure */
typedef struct geotiff_file_t {
    TIFF *tiff;                 /* TIFF file handle - shared across datasets */
    char *filename;             /* File name */
    unsigned int flags;         /* File access flags */
    hid_t plist_id;             /* Property list ID */
    gdal_metadata_t *gdal_meta; /* Parsed GDAL metadata (tag 42112) */
    /* NOTE: For thread safety with multi-image files, each dataset should
     * have its own TIFF handle via TIFFOpen(). Currently using shared handle. */
} geotiff_file_t;

/* Forward declaration for unified object type */
typedef struct geotiff_object_t geotiff_object_t;

/* GeoTIFF VOL dataset object structure */
typedef struct geotiff_dataset_t {
    char *name;          /* Dataset name */
    int directory_index; /* TIFF directory index for multi-image files */
    GTIF *gtif;          /* GeoTIFF handle for this directory's geo keys */
    hid_t type_id;       /* HDF5 datatype */
    hid_t space_id;      /* HDF5 dataspace */
    void *data;          /* Cached data */
    size_t data_size;    /* Data size in bytes */
    bool is_image;       /* Is this an image dataset */
    bool is_latlon;      /* Is this a latN or lonN coordinate dataset */
    bool is_lat;         /* True for latN, false for lonN (only valid if is_latlon) */
} geotiff_dataset_t;

/* GeoTIFF VOL group object structure */
typedef struct geotiff_group_t {
    char *name; /* Group name */
} geotiff_group_t;

/* GeoTIFF VOL attribute object structure */
typedef struct geotiff_attr_t {
    void *parent;   /* Parent object (dataset, group, or file) */
    char *name;     /* Attribute name */
    hid_t type_id;  /* HDF5 datatype */
    hid_t space_id; /* HDF5 dataspace */
} geotiff_attr_t;

/* Unified GeoTIFF VOL object structure */
struct geotiff_object_t {
    geotiff_object_t *parent_file; /* Parent file (never NULL after open) */
    H5I_type_t obj_type;           /* HDF5 object type identifier */
    size_t ref_count;              /* Reference count for child objects */
    union {
        geotiff_file_t file;
        geotiff_dataset_t dataset;
        geotiff_group_t group;
        geotiff_attr_t attr;
    } u;
};

/* Function prototypes (HDF5 develop expects hid_t vipl_id) */
herr_t geotiff_init_connector(hid_t vipl_id);
herr_t geotiff_term_connector(void);

/* File operations */
void *geotiff_file_create(const char *name, unsigned flags, hid_t fcpl_id, hid_t fapl_id,
                          hid_t dxpl_id, void **req);
void *geotiff_file_open(const char *name, unsigned flags, hid_t fapl_id, hid_t dxpl_id, void **req);
herr_t geotiff_file_get(void *file, H5VL_file_get_args_t *args, hid_t dxpl_id, void **req);
herr_t geotiff_file_close(void *file, hid_t dxpl_id, void **req);

/* Dataset operations */
void *geotiff_dataset_open(void *obj, const H5VL_loc_params_t *loc_params, const char *name,
                           hid_t dapl_id, hid_t dxpl_id, void **req);
herr_t geotiff_dataset_read(size_t count, void *dset[], hid_t mem_type_id[], hid_t mem_space_id[],
                            hid_t file_space_id[], hid_t dxpl_id, void *buf[], void **req);
herr_t geotiff_dataset_get(void *dset, H5VL_dataset_get_args_t *args, hid_t dxpl_id, void **req);
herr_t geotiff_dataset_close(void *dset, hid_t dxpl_id, void **req);

/* Group operations */
void *geotiff_group_open(void *obj, const H5VL_loc_params_t *loc_params, const char *name,
                         hid_t gapl_id, hid_t dxpl_id, void **req);
herr_t geotiff_group_get(void *obj, H5VL_group_get_args_t *args, hid_t dxpl_id, void **req);
herr_t geotiff_group_close(void *grp, hid_t dxpl_id, void **req);

/* Attribute operations */
void *geotiff_attr_open(void *obj, const H5VL_loc_params_t *loc_params, const char *name,
                        hid_t aapl_id, hid_t dxpl_id, void **req);
herr_t geotiff_attr_read(void *attr, hid_t mem_type_id, void *buf, hid_t dxpl_id, void **req);
herr_t geotiff_attr_get(void *obj, H5VL_attr_get_args_t *args, hid_t dxpl_id, void **req);
herr_t geotiff_attr_close(void *attr, hid_t dxpl_id, void **req);

/* Link operations */
herr_t geotiff_link_specific(void *obj, const H5VL_loc_params_t *loc_params,
                             H5VL_link_specific_args_t *args, hid_t dxpl_id, void **req);

/* Attribute specific operation */
herr_t geotiff_attr_specific(void *obj, const H5VL_loc_params_t *loc_params,
                             H5VL_attr_specific_args_t *args, hid_t dxpl_id, void **req);

/* Object get operation */
herr_t geotiff_object_get(void *obj, const H5VL_loc_params_t *loc_params,
                          H5VL_object_get_args_t *args, hid_t dxpl_id, void **req);

/* Helper functions */
herr_t geotiff_read_hyperslab(const geotiff_object_t *dset_obj, const hsize_t *start,
                              const hsize_t *stride, const hsize_t *count, const hsize_t *block,
                              int ndims, hid_t mem_type_id, void *buf);

herr_t geotiff_introspect_opt_query(void *obj, H5VL_subclass_t subcls, int opt_type,
                                    uint64_t *flags);

herr_t geotiff_introspect_get_conn_cls(void GEOTIFF_UNUSED_PARAM *obj,
                                       H5VL_get_conn_lvl_t GEOTIFF_UNUSED_PARAM lvl,
                                       const H5VL_class_t GEOTIFF_UNUSED_PARAM **conn_cls);

herr_t geotiff_introspect_get_cap_flags(const void GEOTIFF_UNUSED_PARAM *info, uint64_t *cap_flags);
#endif /* _geotiff_vol_connector_H */
