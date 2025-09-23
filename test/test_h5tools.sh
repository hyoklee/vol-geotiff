#!/bin/bash

# Test script for HDF5 tools with GeoTIFF VOL connector

echo "Testing HDF5 tools with GeoTIFF VOL connector"

# Set plugin path
export HDF5_PLUGIN_PATH="../src"

# Test file (should be a GeoTIFF file)
GEOTIFF_FILE="sample.tif"

if [ ! -f "$GEOTIFF_FILE" ]; then
    echo "Creating a simple test GeoTIFF file..."
    # This would need GDAL or similar tools to create a proper GeoTIFF
    echo "Please provide a GeoTIFF file named $GEOTIFF_FILE for testing"
    exit 1
fi

echo "Testing h5ls with GeoTIFF file: $GEOTIFF_FILE"
h5ls --vol-name=geotiff_vol_connector "$GEOTIFF_FILE" || echo "h5ls failed"

echo "Testing h5dump with GeoTIFF file: $GEOTIFF_FILE"
h5dump --vol-name=geotiff_vol_connector "$GEOTIFF_FILE" || echo "h5dump failed"

echo "Testing h5stat with GeoTIFF file: $GEOTIFF_FILE"
h5stat --vol-name=geotiff_vol_connector "$GEOTIFF_FILE" || echo "h5stat failed"

echo "HDF5 tools testing completed"