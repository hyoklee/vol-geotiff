# Build the test
add_executable (vol_plugin vol_plugin.c)
target_include_directories (vol_plugin PRIVATE "${PROJECT_SOURCE_DIR}/src" ${GEOTIFF_INCLUDE_DIRS})
target_link_libraries (vol_plugin PRIVATE HDF5::HDF5)

# Build the GeoTIFF test
add_executable (test_geotiff_read test_geotiff_read.c)
target_include_directories (test_geotiff_read PRIVATE "${PROJECT_SOURCE_DIR}/src" ${GEOTIFF_INCLUDE_DIRS})
target_link_libraries (test_geotiff_read PRIVATE HDF5::HDF5)

# Build the GeoTIFF band reading test
add_executable (test_band_read test_band_read.c)
target_include_directories (test_band_read PRIVATE "${PROJECT_SOURCE_DIR}/src" ${GEOTIFF_INCLUDE_DIRS})
target_link_libraries (test_band_read PRIVATE HDF5::HDF5)

# Build the h5dump-like VOL connector test
add_executable (test_h5dump_vol test_h5dump_vol.c)
target_include_directories (test_h5dump_vol PRIVATE "${PROJECT_SOURCE_DIR}/src" ${GEOTIFF_INCLUDE_DIRS})
target_link_libraries (test_h5dump_vol PRIVATE HDF5::HDF5)

# Add the test
add_test (vol_plugin vol_plugin)
set_tests_properties(vol_plugin PROPERTIES
    ENVIRONMENT "HDF5_PLUGIN_PATH=${PROJECT_BINARY_DIR}/src")

# Add the GeoTIFF test (if a sample GeoTIFF file exists)
if(EXISTS "${PROJECT_SOURCE_DIR}/test/sample.tif")
    add_test (test_geotiff_read test_geotiff_read "${PROJECT_SOURCE_DIR}/test/sample.tif")
    set_tests_properties(test_geotiff_read PROPERTIES
        ENVIRONMENT "HDF5_PLUGIN_PATH=${PROJECT_BINARY_DIR}/src")

    add_test (test_band_read test_band_read "${PROJECT_SOURCE_DIR}/test/sample.tif")
    set_tests_properties(test_band_read PROPERTIES
        ENVIRONMENT "HDF5_PLUGIN_PATH=${PROJECT_BINARY_DIR}/src")
endif()
