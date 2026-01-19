# Build the GeoTIFF test (now includes test file generation)
add_executable (test_runner test_runner.c plugin_tests.c geotiff_tests.c)
target_include_directories (test_runner PRIVATE "${PROJECT_SOURCE_DIR}/src" ${HDF5_INCLUDE_DIRS} ${GEOTIFF_INCLUDE_DIRS})

# Define the plugin path for the tests to use
target_compile_definitions(test_runner PRIVATE
    GEOTIFF_VOL_PLUGIN_PATH="${PROJECT_BINARY_DIR}/src")

# Link test_runner with required libraries
target_link_libraries (test_runner PRIVATE HDF5::HDF5 Tiff::Tiff GeoTiff::GeoTiff)

# Add individual test cases
# Use generator expression to automatically select Debug or Release plugin directory
# Add the test executable and src directories to PATH so Windows can find the DLLs

# Derive vcpkg bin path from TIFF_LIBRARY for runtime DLL discovery
# TIFF_LIBRARY is e.g. C:/vcpkg.ms/installed/x64-windows/lib/tiff.lib
# We need: C:/vcpkg.ms/installed/x64-windows/bin (Release) or .../debug/bin (Debug)
if(TIFF_LIBRARY)
    get_filename_component(_TIFF_LIB_DIR "${TIFF_LIBRARY}" DIRECTORY)  # .../lib
    get_filename_component(_VCPKG_INSTALLED "${_TIFF_LIB_DIR}" DIRECTORY)  # .../x64-windows
    set(_VCPKG_BIN_PATH "$<IF:$<CONFIG:Debug>,${_VCPKG_INSTALLED}/debug/bin,${_VCPKG_INSTALLED}/bin>")
else()
    set(_VCPKG_BIN_PATH "")
endif()

set(TEST_ENVIRONMENT "HDF5_PLUGIN_PATH=${PROJECT_BINARY_DIR}/src/$<CONFIG>\;${PROJECT_BINARY_DIR}/src;PATH=$<TARGET_FILE_DIR:test_runner>\;${PROJECT_BINARY_DIR}/src/$<CONFIG>\;${PROJECT_BINARY_DIR}/src\;${_VCPKG_BIN_PATH}\;$ENV{PATH};HDF5_VOL_CONNECTOR=")
set(TEST_WORKING_DIRECTORY "$<TARGET_FILE_DIR:test_runner>")

# Add the GeoTIFF test (if a sample GeoTIFF file exists)
if(EXISTS "${PROJECT_SOURCE_DIR}/test/sample.tif")
    add_test(NAME test_runner_read COMMAND test_runner real_file_test "${PROJECT_SOURCE_DIR}/test/sample.tif")
    set_tests_properties(test_runner_read PROPERTIES
        ENVIRONMENT "${TEST_ENVIRONMENT}"
        WORKING_DIRECTORY "${TEST_WORKING_DIRECTORY}")
endif()

# Helper macro to add test with proper environment and working directory
macro(add_geotiff_test TEST_NAME)
    add_test(NAME ${TEST_NAME} COMMAND test_runner ${TEST_NAME} ${ARGN})
    set_tests_properties(${TEST_NAME} PROPERTIES
        ENVIRONMENT "${TEST_ENVIRONMENT}"
        WORKING_DIRECTORY "${TEST_WORKING_DIRECTORY}")
endmacro()

# Plugin registration tests
add_geotiff_test(test_getters)
add_geotiff_test(test_multiple_registration)
add_geotiff_test(test_registration_by_name)
add_geotiff_test(test_registration_by_value)

# GeoTIFF functionality tests
add_geotiff_test(open_grayscale)
add_geotiff_test(open_rgb)
add_geotiff_test(read_grayscale)
add_geotiff_test(read_rgb)
add_geotiff_test(band_read_rgb)
add_geotiff_test(point_read_grayscale)
add_geotiff_test(point_read_rgb)
add_geotiff_test(multi_image_read)
add_geotiff_test(dataset_error_handling)

# Datatype conversion tests - all permutations
set(DTYPE_NAMES "UCHAR" "USHORT" "UINT" "UINT64" "CHAR" "SHORT" "INT" "INT64" "FLOAT" "DOUBLE")

foreach(MEM_TYPE ${DTYPE_NAMES})
    foreach(FILE_TYPE ${DTYPE_NAMES})
        if(NOT "${MEM_TYPE}" STREQUAL "${FILE_TYPE}")
            set(TEST_NAME "dtype_conv_${MEM_TYPE}_${FILE_TYPE}")
            add_geotiff_test(${TEST_NAME})
        endif()
    endforeach()
endforeach()

# Link tests
add_geotiff_test(link_exists_grayscale)
add_geotiff_test(link_exists_rgb)
add_geotiff_test(link_iterate_grayscale)
add_geotiff_test(link_iterate_rgb)

# Feature tests
add_geotiff_test(unsupported_features)
add_geotiff_test(group_get_info)

# Tiled TIFF tests
add_geotiff_test(tiled_read_grayscale)
add_geotiff_test(tiled_read_rgb)

# Reference counting tests
add_geotiff_test(refcount_close_file_before_dataset)
add_geotiff_test(refcount_close_dataset_before_attribute)
add_geotiff_test(refcount_close_file_with_multiple_children)

# Attribute tests
add_geotiff_test(num_images_attribute)

# Real file comprehensive test
add_geotiff_test(real_file_comprehensive)

# GDAL test files - individual CTest instances for each .tif file
set(GDAL_TEST_FILES
    "3376.tif"
    "arcgis_geodataxform_coeffx_coeffy.tif"
    "bigtiff_four_strip_be_short.tif"
    "bigtiff_four_strip_short.tif"
    "bigtiff_one_strip_long.tif"
    "byte_gcp.tif"
    "byte_inconsistent_georef.tif"
    "byte_nogeoref.tif"
    "byte_point.tif"
    "byte.tif"
    "byte_with_ovr.tif"
    # "cielab.tif" # colorpsace not yet supported
    # "float32.tif" # floats not yet supported
    # "float64.tif" # floats not yet supported
    "GTModelTypeGeoKey_only.tif"
    "int16.tif"
    "int32.tif"
    "int64.tif"
    # "minfloat.tif" # floats not yet supported
    "ModelTiepointTag_z_non_zero_but_ModelPixelScaleTag_z_zero.tif"
    "PIA26616.tif"
    "stefan_full_greyalpha.tif"
    "stefan_full_rgba.tif"
    # "test_average_palette.tif" # palette color model not supported
    "uint16.tif"
    "uint32.tif"
    "uint64.tif"
    "utmsmall.tif"
    "WGS_1984_Web_Mercator_Auxiliary_Sphere.tif"
    "WGS_1984_Web_Mercator.tif"
)

foreach(TEST_FILE ${GDAL_TEST_FILES})
    # Get filename without extension for test name
    string(REPLACE ".tif" "" TEST_NAME_BASE "${TEST_FILE}")
    # Replace any special characters that might cause issues with underscores
    string(REGEX REPLACE "[^a-zA-Z0-9_]" "_" TEST_NAME_SAFE "${TEST_NAME_BASE}")
    set(TEST_NAME "gdal_${TEST_NAME_SAFE}")

    # Check if file exists in test directory
    if(EXISTS "${PROJECT_SOURCE_DIR}/test/${TEST_FILE}")
        # Create a wrapper test that calls test_runner with the real_file_test and filename
        # Use generator expression to automatically select Debug or Release plugin directory
        # Add the test executable and src directories to PATH so Windows can find the DLLs
        add_test(NAME ${TEST_NAME}
                 COMMAND ${CMAKE_COMMAND} -E env
                         "HDF5_PLUGIN_PATH=${PROJECT_BINARY_DIR}/src/$<CONFIG>\;${PROJECT_BINARY_DIR}/src"
                         "PATH=$<TARGET_FILE_DIR:test_runner>\;${PROJECT_BINARY_DIR}/src/$<CONFIG>\;${PROJECT_BINARY_DIR}/src\;${_VCPKG_BIN_PATH}\;$ENV{PATH}"
                         "HDF5_VOL_CONNECTOR="
                         $<TARGET_FILE:test_runner> real_file_test "${PROJECT_SOURCE_DIR}/test/${TEST_FILE}")
        set_tests_properties(${TEST_NAME} PROPERTIES
            WORKING_DIRECTORY "${TEST_WORKING_DIRECTORY}")
    endif()
endforeach()
