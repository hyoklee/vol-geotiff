#!/bin/bash

# Test script for netCDF tools with GeoTIFF VOL connector

echo "Testing netCDF tools with GeoTIFF VOL connector"

# Set plugin path for HDF5 VOL connector
export HDF5_PLUGIN_PATH="../src"
export HDF5_VOL_CONNECTOR=geotiff_vol_connector

# Test file (should be a GeoTIFF file)
GEOTIFF_FILE="sample.tif"

if [ ! -f "$GEOTIFF_FILE" ]; then
    echo "Creating a simple test GeoTIFF file..."
    echo "Please provide a GeoTIFF file named $GEOTIFF_FILE for testing"
    exit 1
fi

echo "Testing ncdump with GeoTIFF file: $GEOTIFF_FILE"
ncdump -h "$GEOTIFF_FILE" || echo "ncdump header failed"

echo "Testing ncdump with variables:"
ncdump -v image "$GEOTIFF_FILE" || echo "ncdump variables failed"

echo "Testing ncinfo if available:"
which ncinfo > /dev/null 2>&1 && ncinfo "$GEOTIFF_FILE" || echo "ncinfo not available or failed"

echo "netCDF tools testing completed"