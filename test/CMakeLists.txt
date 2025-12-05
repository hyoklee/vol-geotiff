# Build the GeoTIFF test (now includes test file generation)
add_executable (test_runner test_runner.c plugin_tests.c geotiff_tests.c)
target_include_directories (test_runner PRIVATE "${PROJECT_SOURCE_DIR}/src" ${HDF5_INCLUDE_DIRS} ${GEOTIFF_INCLUDE_DIRS})

# Define the plugin path for the tests to use
target_compile_definitions(test_runner PRIVATE
    GEOTIFF_VOL_PLUGIN_PATH="${PROJECT_BINARY_DIR}/src")

# Link test_runner with required libraries
if (_have_config)
    target_link_libraries (test_runner PRIVATE HDF5::HDF5 TIFF::TIFF ${_GEOTIFF_TARGET})
else()
    target_link_libraries (test_runner PRIVATE HDF5::HDF5 ${TIFF_LIBRARIES} ${GEOTIFF_LIBRARIES})
endif()

# Add the GeoTIFF test (if a sample GeoTIFF file exists)
if(EXISTS "${PROJECT_SOURCE_DIR}/test/sample.tif")
    add_test(NAME test_runner_read
             COMMAND ${CMAKE_COMMAND} -E env
                     "HDF5_PLUGIN_PATH=${PROJECT_BINARY_DIR}/src/Release\;${PROJECT_BINARY_DIR}/src"
                     "PATH=C:/src/hdf5-dev/install/bin\;C:/src/vcpkg.microsoft/installed/x64-windows/bin\;$ENV{PATH}"
                     "HDF5_VOL_CONNECTOR="
                     $<TARGET_FILE:test_runner> real_file_test "${PROJECT_SOURCE_DIR}/test/sample.tif")
endif()

# Add individual test cases
set(TEST_ENVIRONMENT "HDF5_PLUGIN_PATH=${PROJECT_BINARY_DIR}/src/Release\;${PROJECT_BINARY_DIR}/src;PATH=C:/src/hdf5-dev/install/bin\;C:/src/vcpkg.microsoft/installed/x64-windows/bin\;$ENV{PATH};HDF5_VOL_CONNECTOR=")

# Plugin registration tests
add_test(NAME test_getters COMMAND test_runner test_getters)
set_tests_properties(test_getters PROPERTIES ENVIRONMENT "${TEST_ENVIRONMENT}")

add_test(NAME test_multiple_registration COMMAND test_runner test_multiple_registration)
set_tests_properties(test_multiple_registration PROPERTIES ENVIRONMENT "${TEST_ENVIRONMENT}")

add_test(NAME test_registration_by_name COMMAND test_runner test_registration_by_name)
set_tests_properties(test_registration_by_name PROPERTIES ENVIRONMENT "${TEST_ENVIRONMENT}")

add_test(NAME test_registration_by_value COMMAND test_runner test_registration_by_value)
set_tests_properties(test_registration_by_value PROPERTIES ENVIRONMENT "${TEST_ENVIRONMENT}")

# GeoTIFF functionality tests
add_test(NAME open_grayscale COMMAND test_runner open_grayscale)
set_tests_properties(open_grayscale PROPERTIES ENVIRONMENT "${TEST_ENVIRONMENT}")

add_test(NAME open_rgb COMMAND test_runner open_rgb)
set_tests_properties(open_rgb PROPERTIES ENVIRONMENT "${TEST_ENVIRONMENT}")

add_test(NAME read_grayscale COMMAND test_runner read_grayscale)
set_tests_properties(read_grayscale PROPERTIES ENVIRONMENT "${TEST_ENVIRONMENT}")

add_test(NAME read_rgb COMMAND test_runner read_rgb)
set_tests_properties(read_rgb PROPERTIES ENVIRONMENT "${TEST_ENVIRONMENT}")

add_test(NAME band_read_rgb COMMAND test_runner band_read_rgb)
set_tests_properties(band_read_rgb PROPERTIES ENVIRONMENT "${TEST_ENVIRONMENT}")

add_test(NAME point_read_grayscale COMMAND test_runner point_read_grayscale)
set_tests_properties(point_read_grayscale PROPERTIES ENVIRONMENT "${TEST_ENVIRONMENT}")

add_test(NAME point_read_rgb COMMAND test_runner point_read_rgb)
set_tests_properties(point_read_rgb PROPERTIES ENVIRONMENT "${TEST_ENVIRONMENT}")

add_test(NAME multi_image_read COMMAND test_runner multi_image_read)
set_tests_properties(multi_image_read PROPERTIES ENVIRONMENT "${TEST_ENVIRONMENT}")

add_test(NAME dataset_error_handling COMMAND test_runner dataset_error_handling)
set_tests_properties(dataset_error_handling PROPERTIES ENVIRONMENT "${TEST_ENVIRONMENT}")

# Datatype conversion tests - all permutations
set(DTYPE_NAMES "UCHAR" "USHORT" "UINT" "UINT64" "CHAR" "SHORT" "INT" "INT64" "FLOAT" "DOUBLE")

foreach(MEM_TYPE ${DTYPE_NAMES})
    foreach(FILE_TYPE ${DTYPE_NAMES})
        if(NOT "${MEM_TYPE}" STREQUAL "${FILE_TYPE}")
            set(TEST_NAME "dtype_conv_${MEM_TYPE}_${FILE_TYPE}")
            add_test(NAME ${TEST_NAME} COMMAND test_runner ${TEST_NAME})
            set_tests_properties(${TEST_NAME} PROPERTIES ENVIRONMENT "${TEST_ENVIRONMENT}")
        endif()
    endforeach()
endforeach()
add_test(NAME link_exists_grayscale COMMAND test_runner link_exists_grayscale)
set_tests_properties(link_exists_grayscale PROPERTIES ENVIRONMENT "${TEST_ENVIRONMENT}")

add_test(NAME link_exists_rgb COMMAND test_runner link_exists_rgb)
set_tests_properties(link_exists_rgb PROPERTIES ENVIRONMENT "${TEST_ENVIRONMENT}")

add_test(NAME link_iterate_grayscale COMMAND test_runner link_iterate_grayscale)
set_tests_properties(link_iterate_grayscale PROPERTIES ENVIRONMENT "${TEST_ENVIRONMENT}")

add_test(NAME link_iterate_rgb COMMAND test_runner link_iterate_rgb)
set_tests_properties(link_iterate_rgb PROPERTIES ENVIRONMENT "${TEST_ENVIRONMENT}")

add_test(NAME unsupported_features COMMAND test_runner unsupported_features)
set_tests_properties(unsupported_features PROPERTIES ENVIRONMENT "${TEST_ENVIRONMENT}")

add_test(NAME group_get_info COMMAND test_runner group_get_info)
set_tests_properties(group_get_info PROPERTIES ENVIRONMENT "${TEST_ENVIRONMENT}")

# Tiled TIFF tests
add_test(NAME tiled_read_grayscale COMMAND test_runner tiled_read_grayscale)
set_tests_properties(tiled_read_grayscale PROPERTIES ENVIRONMENT "${TEST_ENVIRONMENT}")

add_test(NAME tiled_read_rgb COMMAND test_runner tiled_read_rgb)
set_tests_properties(tiled_read_rgb PROPERTIES ENVIRONMENT "${TEST_ENVIRONMENT}")

# Reference counting tests
add_test(NAME refcount_close_file_before_dataset COMMAND test_runner refcount_close_file_before_dataset)
set_tests_properties(refcount_close_file_before_dataset PROPERTIES ENVIRONMENT "${TEST_ENVIRONMENT}")

add_test(NAME refcount_close_dataset_before_attribute COMMAND test_runner refcount_close_dataset_before_attribute)
set_tests_properties(refcount_close_dataset_before_attribute PROPERTIES ENVIRONMENT "${TEST_ENVIRONMENT}")

add_test(NAME refcount_close_file_with_multiple_children COMMAND test_runner refcount_close_file_with_multiple_children)
set_tests_properties(refcount_close_file_with_multiple_children PROPERTIES ENVIRONMENT "${TEST_ENVIRONMENT}")

add_test(NAME num_images_attribute COMMAND test_runner num_images_attribute)
set_tests_properties(num_images_attribute PROPERTIES ENVIRONMENT "${TEST_ENVIRONMENT}")

# Real file comprehensive test
add_test(NAME real_file_comprehensive COMMAND test_runner real_file_comprehensive)
set_tests_properties(real_file_comprehensive PROPERTIES ENVIRONMENT "${TEST_ENVIRONMENT}")

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
    "byte_zstd.tif"
    # "cielab.tif" # colorpsace ot yet supported
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
        add_test(NAME ${TEST_NAME}
                 COMMAND ${CMAKE_COMMAND} -E env
                         "HDF5_PLUGIN_PATH=${PROJECT_BINARY_DIR}/src/Release\;${PROJECT_BINARY_DIR}/src"
                         "PATH=C:/src/hdf5-dev/install/bin\;C:/src/vcpkg.microsoft/installed/x64-windows/bin\;$ENV{PATH}"
                         "HDF5_VOL_CONNECTOR="
                         $<TARGET_FILE:test_runner> real_file_test "${PROJECT_SOURCE_DIR}/test/${TEST_FILE}")
    endif()
endforeach()
