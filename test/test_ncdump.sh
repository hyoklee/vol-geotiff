#!/bin/bash

# Test script for netCDF tools with GeoTIFF VOL connector

echo "Testing netCDF tools with GeoTIFF VOL connector"

# Paths to the custom-built HDF5 and netcdf-c
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
VOL_BUILD="${SCRIPT_DIR}/../build-x86/src"
HDF5_LIB="${HOME}/hdf5/install-x86/lib"
NCDUMP="${HOME}/netcdf-c/build-x86/ncdump/ncdump"

# Set plugin path for HDF5 VOL connector
export HDF5_PLUGIN_PATH="${VOL_BUILD}"
export HDF5_VOL_CONNECTOR=geotiff_vol_connector
export LD_LIBRARY_PATH="${HDF5_LIB}:${VOL_BUILD}:${LD_LIBRARY_PATH}"

# Test file - accept as argument or use default
GEOTIFF_FILE="${1:-EMIT_L2B_FRCOVPV_001_20260311T195709_2607013_014.tif}"

if [ ! -f "$GEOTIFF_FILE" ]; then
    echo "GeoTIFF file not found: $GEOTIFF_FILE"
    exit 1
fi

echo "Testing ncdump with GeoTIFF file: $GEOTIFF_FILE"
"${NCDUMP}" -h "$GEOTIFF_FILE" || echo "ncdump header failed"

echo ""
echo "Dumping lat0 values:"
"${NCDUMP}" -v lat0 "$GEOTIFF_FILE" || echo "ncdump lat0 failed"

echo ""
echo "Dumping lon0 values:"
"${NCDUMP}" -v lon0 "$GEOTIFF_FILE" || echo "ncdump lon0 failed"

echo ""
echo "netCDF tools testing completed"