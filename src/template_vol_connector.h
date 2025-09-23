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

#include <geo_keyp.h>
#include <geo_tiffp.h>
#include <geotiff.h>
#include <hdf5.h>
#include <tiffio.h>

/* The value must be between 256 and 65535 (inclusive) */
#define GEOTIFF_VOL_CONNECTOR_VALUE ((H5VL_class_value_t) 12203)
#define GEOTIFF_VOL_CONNECTOR_NAME "geotiff_vol_connector"

/* GeoTIFF VOL file object structure */
typedef struct geotiff_file_t {
    TIFF *tiff;         /* TIFF file handle */
    GTIF *gtif;         /* GeoTIFF handle */
    char *filename;     /* File name */
    unsigned int flags; /* File access flags */
    hid_t plist_id;     /* Property list ID */
} geotiff_file_t;

/* GeoTIFF VOL dataset object structure */
typedef struct geotiff_dataset_t {
    geotiff_file_t *file; /* Parent file */
    char *name;           /* Dataset name */
    hid_t type_id;        /* HDF5 datatype */
    hid_t space_id;       /* HDF5 dataspace */
    void *data;           /* Cached data */
    size_t data_size;     /* Data size in bytes */
    int is_image;         /* Is this an image dataset */
} geotiff_dataset_t;

/* GeoTIFF VOL group object structure */
typedef struct geotiff_group_t {
    geotiff_file_t *file; /* Parent file */
    char *name;           /* Group name */
} geotiff_group_t;

/* GeoTIFF VOL attribute object structure */
typedef struct geotiff_attr_t {
    geotiff_file_t *file; /* Parent file */
    char *name;           /* Attribute name */
    hid_t type_id;        /* HDF5 datatype */
    hid_t space_id;       /* HDF5 dataspace */
    void *data;           /* Attribute data */
    size_t data_size;     /* Data size in bytes */
} geotiff_attr_t;

/* Function prototypes */
herr_t geotiff_init_connector(void);
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

/* Helper functions */
herr_t geotiff_read_image_data(geotiff_file_t *file, geotiff_dataset_t *dset);
herr_t geotiff_parse_geotiff_tags(geotiff_file_t *file);
hid_t geotiff_get_hdf5_type_from_tiff(uint16 sample_format, uint16 bits_per_sample);

#endif /* _geotiff_vol_connector_H */
