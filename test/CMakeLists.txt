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
    add_test (test_runner_read test_runner "${PROJECT_SOURCE_DIR}/test/sample.tif")
    set_tests_properties(test_runner_read PROPERTIES
        ENVIRONMENT "HDF5_PLUGIN_PATH=${PROJECT_BINARY_DIR}/src")
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
