#!/bin/bash
#
# convert_to_nc4.sh - Convert a GeoTIFF to NetCDF-4 using the vol-geotiff connector
#
# Usage: bash convert_to_nc4.sh [input.tif] [output.nc4]
#
# Steps:
#   1. Use vol-geotiff ncdump (with VOL connector) to dump the CDL header
#   2. Use system ncgen to create an nc4 skeleton from the CDL header
#   3. Use geotiff_fill_data to copy variable data (reads via VOL, writes via native HDF5)

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
VOL_BUILD="${SCRIPT_DIR}/../build-x86/src"
HDF5_LIB="${HOME}/hdf5/install-x86/lib"
NCDUMP="${HOME}/netcdf-c/build-x86/ncdump/ncdump"
NCGEN="/usr/bin/ncgen"

# Input / output
INPUT="${1:-${SCRIPT_DIR}/EMIT_L2B_FRCOVPV_001_20260311T195709_2607013_014.tif}"
OUTPUT="${2:-${SCRIPT_DIR}/EMIT_L2B_FRCOVPV_001_20260311T195709_2607013_014.nc4}"

if [ ! -f "$INPUT" ]; then
    echo "ERROR: input file not found: $INPUT"
    exit 1
fi

CDL_HEADER="/tmp/geotiff_header_$$.cdl"
trap "rm -f $CDL_HEADER" EXIT

echo "=== Step 1: Dump CDL header from GeoTIFF (via vol-geotiff connector) ==="
(
    export HDF5_PLUGIN_PATH="${VOL_BUILD}"
    export HDF5_VOL_CONNECTOR=geotiff_vol_connector
    export LD_LIBRARY_PATH="${HDF5_LIB}:${VOL_BUILD}:${LD_LIBRARY_PATH}"
    "${NCDUMP}" -h "$INPUT"
) > "$CDL_HEADER"

echo "  CDL header written to $CDL_HEADER"
head -5 "$CDL_HEADER"
echo "  ..."

echo ""
echo "=== Step 2: Create nc4 skeleton with system ncgen (no VOL connector) ==="
unset HDF5_VOL_CONNECTOR
"${NCGEN}" -k nc4 -o "$OUTPUT" "$CDL_HEADER"
echo "  Created skeleton: $OUTPUT"
ls -lh "$OUTPUT"

echo ""
echo "=== Step 3: Fill variable data from GeoTIFF into nc4 ==="
(
    export HDF5_PLUGIN_PATH="${VOL_BUILD}"
    export HDF5_VOL_CONNECTOR=geotiff_vol_connector
    export LD_LIBRARY_PATH="${HDF5_LIB}:${VOL_BUILD}:${LD_LIBRARY_PATH}"
    "${SCRIPT_DIR}/geotiff_fill_data" "$INPUT" "$OUTPUT"
)

echo ""
echo "=== Verifying output with system ncdump -h ==="
unset HDF5_VOL_CONNECTOR
/usr/bin/ncdump -h "$OUTPUT"

echo ""
echo "Conversion complete: $OUTPUT"
ls -lh "$OUTPUT"
