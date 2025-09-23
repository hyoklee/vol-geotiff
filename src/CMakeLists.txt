# Set the GeoTIFF VOL connector name
set (GEOTIFF_VOL_NAME geotiff_vol_connector)

# Find required libraries
find_package(PkgConfig REQUIRED)
pkg_check_modules(TIFF REQUIRED libtiff-4)

# Find libgeotiff manually since it doesn't provide pkg-config
find_path(GEOTIFF_INCLUDE_DIR geotiff/geotiff.h PATHS /usr/include)
find_library(GEOTIFF_LIBRARY NAMES geotiff PATHS /usr/lib /usr/lib/x86_64-linux-gnu)

if(NOT GEOTIFF_INCLUDE_DIR OR NOT GEOTIFF_LIBRARY)
    message(FATAL_ERROR "libgeotiff not found. Please install libgeotiff-dev")
endif()

set(GEOTIFF_LIBRARIES ${GEOTIFF_LIBRARY})
set(GEOTIFF_INCLUDE_DIRS ${GEOTIFF_INCLUDE_DIR})

# Build the GeoTIFF VOL connector
add_library (${GEOTIFF_VOL_NAME} SHARED template_vol_connector.c)
set_target_properties (${GEOTIFF_VOL_NAME} PROPERTIES VERSION ${PROJECT_VERSION})
set_target_properties (${GEOTIFF_VOL_NAME} PROPERTIES SOVERSION 1)
set_target_properties (${GEOTIFF_VOL_NAME} PROPERTIES PUBLIC_HEADER "template_vol_connector.h")

# Link libraries
target_link_libraries(${GEOTIFF_VOL_NAME} ${TIFF_LIBRARIES} ${GEOTIFF_LIBRARIES})
target_include_directories(${GEOTIFF_VOL_NAME} PRIVATE ${TIFF_INCLUDE_DIRS} ${GEOTIFF_INCLUDE_DIRS})
target_compile_options(${GEOTIFF_VOL_NAME} PRIVATE ${TIFF_CFLAGS_OTHER} ${GEOTIFF_CFLAGS_OTHER})

