cmake_minimum_required (VERSION 3.9)
project (geotiff_vol VERSION 1.0.0 DESCRIPTION "HDF5 VOL connector for GeoTIFF files" LANGUAGES C)

# Enable testing
enable_testing ()
include (CTest)

# Find HDF5
find_package (HDF5 1.10 COMPONENTS C REQUIRED)

# Check if HDF5 has VOL support (requires 1.15+)
if(HDF5_VERSION VERSION_LESS "1.15.0")
    message(WARNING "HDF5 VOL connector requires HDF5 1.15.0 or newer. Found ${HDF5_VERSION}. "
                   "Build may fail due to missing VOL API. "
                   "Consider using HDF5 develop branch from https://github.com/HDFGroup/hdf5.git")
endif()

link_directories (${HDF5_LIBRARY_DIRS})
include_directories (${HDF5_INCLUDE_DIR})
set (LINK_LIBS ${LINK_LIBS} ${HDF5_C_${LIB_TYPE}_LIBRARY})

# It's really easy to pick up the wrong HDF5 library if you set the path
# wrong. Turn this on for added confirmation that you got it right.
#message (DEPRECATION "Include: ${HDF5_INCLUDE_DIR}")

add_subdirectory (src)
add_subdirectory (test)

