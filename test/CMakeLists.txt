# Build the test
add_executable (vol_plugin vol_plugin.c)
target_include_directories (vol_plugin PRIVATE "${PROJECT_SOURCE_DIR}/src" ${GEOTIFF_INCLUDE_DIRS})
target_link_libraries (vol_plugin PRIVATE HDF5::HDF5)

# Build the GeoTIFF test
add_executable (test_geotiff_read test_geotiff_read.c)
target_include_directories (test_geotiff_read PRIVATE "${PROJECT_SOURCE_DIR}/src" ${GEOTIFF_INCLUDE_DIRS})
target_link_libraries (test_geotiff_read PRIVATE HDF5::HDF5)

# Add the test
add_test (vol_plugin vol_plugin)
set_tests_properties(vol_plugin PROPERTIES
    ENVIRONMENT "HDF5_PLUGIN_PATH=${PROJECT_BINARY_DIR}/src")

# Add the GeoTIFF test (if a sample GeoTIFF file exists)
if(EXISTS "${PROJECT_SOURCE_DIR}/test/sample.tif")
    add_test (test_geotiff_read test_geotiff_read "${PROJECT_SOURCE_DIR}/test/sample.tif")
    set_tests_properties(test_geotiff_read PROPERTIES
        ENVIRONMENT "HDF5_PLUGIN_PATH=${PROJECT_BINARY_DIR}/src")
endif()
