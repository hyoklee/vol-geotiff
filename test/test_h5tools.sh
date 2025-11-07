#!/bin/bash

# Test script for HDF5 tools with GeoTIFF VOL connector

echo "Testing HDF5 tools with GeoTIFF VOL connector"

# Check if being run from build directory
if [ ! -d "./src" ]; then
    echo "Error: This script must be run from the build directory"
    echo "Usage: cd build && ../test/test_h5tools.sh <geotiff_file.tif>"
    exit 1
fi

# Check if filename argument is provided
if [ -z "$1" ]; then
    echo "Usage: $0 <geotiff_file.tif>"
    exit 1
fi

# Validate that the filename ends with .tif
if [[ ! "$1" =~ \.tif$ ]]; then
    echo "Error: Filename must end with .tif"
    exit 1
fi

# Check if file exists
if [ ! -f "$1" ]; then
    echo "Error: File '$1' not found"
    exit 1
fi

# Set plugin path for HDF5 VOL connector using absolute path
export HDF5_PLUGIN_PATH="$(cd ./src && pwd)"

echo "Using HDF5_PLUGIN_PATH: $HDF5_PLUGIN_PATH"
echo "Testing h5ls with GeoTIFF file: $1"
h5ls --vol-name=geotiff_vol_connector "$1" || echo "h5ls failed"

echo "Testing h5dump with GeoTIFF file: $1"
h5dump --vol-name=geotiff_vol_connector "$1" || echo "h5dump failed"

echo "Testing h5stat with GeoTIFF file: $1"
h5stat --vol-name=geotiff_vol_connector "$1" || echo "h5stat failed"

echo "HDF5 tools testing completed"