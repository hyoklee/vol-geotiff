# Set the GeoTIFF VOL connector name
set (GEOTIFF_VOL_NAME geotiff_vol_connector)

# Prefer vcpkg CONFIG packages for TIFF and libgeotiff; fall back to pkg-config/manual
set(_have_config FALSE)
find_package(TIFF CONFIG QUIET)
# vcpkg recommends using GeoTIFF (note the case) as the package name
find_package(GeoTIFF CONFIG QUIET)
# Some ports may still use lowercase name
if (NOT GeoTIFF_FOUND)
    find_package(geotiff CONFIG QUIET)
endif()
# Figure out which GeoTIFF target name exists
set(_GEOTIFF_TARGET "")
if (TARGET geotiff::geotiff)
    set(_GEOTIFF_TARGET geotiff::geotiff)
elseif (TARGET GeoTIFF::geotiff)
    set(_GEOTIFF_TARGET GeoTIFF::geotiff)
elseif (TARGET GeoTIFF::GeoTIFF)
    set(_GEOTIFF_TARGET GeoTIFF::GeoTIFF)
endif()

if (TARGET TIFF::TIFF AND _GEOTIFF_TARGET)
    set(_have_config TRUE)
    # Extract include directories from the imported GeoTIFF target for use by tests
    get_target_property(GEOTIFF_INCLUDE_DIRS ${_GEOTIFF_TARGET} INTERFACE_INCLUDE_DIRECTORIES)
    # Propagate to parent scope so tests can include headers
    set(GEOTIFF_INCLUDE_DIRS "${GEOTIFF_INCLUDE_DIRS}" PARENT_SCOPE)
endif()

if (NOT _have_config)
    message(STATUS "Falling back to alternate discovery for TIFF/geotiff (vcpkg HINTS, pkg-config last)")
    # Try to find TIFF without pkg-config to support Windows easily
    if (NOT TARGET TIFF::TIFF)
        set(_TIFF_INC_HINT "")
        set(_TIFF_LIB_HINT "")
        if (DEFINED VCPKG_ROOT AND DEFINED VCPKG_TARGET_TRIPLET)
            set(_TIFF_INC_HINT "${VCPKG_ROOT}/installed/${VCPKG_TARGET_TRIPLET}/include")
            set(_TIFF_LIB_HINT "${VCPKG_ROOT}/installed/${VCPKG_TARGET_TRIPLET}/lib")
        endif()
        find_path(TIFF_INCLUDE_DIR tiffio.h HINTS ${_TIFF_INC_HINT})
        find_library(TIFF_LIBRARY NAMES tiff HINTS ${_TIFF_LIB_HINT})
        if (TIFF_INCLUDE_DIR AND TIFF_LIBRARY)
            set(TIFF_INCLUDE_DIRS ${TIFF_INCLUDE_DIR})
            set(TIFF_LIBRARIES ${TIFF_LIBRARY})
        else()
            # Last resort: pkg-config
            find_package(PkgConfig REQUIRED)
            pkg_check_modules(TIFF REQUIRED libtiff-4)
        endif()
    endif()

    # Try to locate GeoTIFF headers/libs with vcpkg hints if available
    set(_VCPKG_INC_HINT "")
    set(_VCPKG_LIB_HINT "")
    if (DEFINED VCPKG_ROOT AND DEFINED VCPKG_TARGET_TRIPLET)
        set(_VCPKG_INC_HINT "${VCPKG_ROOT}/installed/${VCPKG_TARGET_TRIPLET}/include")
        set(_VCPKG_LIB_HINT "${VCPKG_ROOT}/installed/${VCPKG_TARGET_TRIPLET}/lib")
    endif()
    if (EXISTS "${_VCPKG_INC_HINT}/geotiff/geotiff.h")
        set(GEOTIFF_INCLUDE_DIR "${_VCPKG_INC_HINT}")
    endif()
    find_path(GEOTIFF_INCLUDE_DIR geotiff/geotiff.h
              HINTS ${_VCPKG_INC_HINT}
              PATHS /usr/include)
    find_library(GEOTIFF_LIBRARY NAMES geotiff
                 HINTS ${_VCPKG_LIB_HINT}
                 PATHS /usr/lib /usr/lib/x86_64-linux-gnu)

    if(NOT GEOTIFF_INCLUDE_DIR OR NOT GEOTIFF_LIBRARY)
        message(FATAL_ERROR "libgeotiff not found. Ensure GeoTIFF is installed (e.g., via vcpkg) or provide headers/libs.")
    endif()

    set(GEOTIFF_LIBRARIES ${GEOTIFF_LIBRARY})
    set(GEOTIFF_INCLUDE_DIRS ${GEOTIFF_INCLUDE_DIR})
    # Propagate to parent scope so tests can include headers
    set(GEOTIFF_INCLUDE_DIRS "${GEOTIFF_INCLUDE_DIRS}" PARENT_SCOPE)
    message(STATUS "Using GeoTIFF include dir: ${GEOTIFF_INCLUDE_DIRS}")
    message(STATUS "Using GeoTIFF library: ${GEOTIFF_LIBRARIES}")
endif()

# Build the GeoTIFF VOL connector
add_library (${GEOTIFF_VOL_NAME} SHARED template_vol_connector.c)
set_target_properties (${GEOTIFF_VOL_NAME} PROPERTIES VERSION ${PROJECT_VERSION})
set_target_properties (${GEOTIFF_VOL_NAME} PROPERTIES SOVERSION 1)
set_target_properties (${GEOTIFF_VOL_NAME} PROPERTIES PUBLIC_HEADER "template_vol_connector.h")

# Link libraries
if (_have_config)
    target_include_directories(${GEOTIFF_VOL_NAME} PRIVATE ${HDF5_INCLUDE_DIRS})
    target_link_libraries(${GEOTIFF_VOL_NAME} PRIVATE TIFF::TIFF ${_GEOTIFF_TARGET} HDF5::HDF5)
else()
    target_link_libraries(${GEOTIFF_VOL_NAME} PRIVATE ${TIFF_LIBRARIES} ${GEOTIFF_LIBRARIES} HDF5::HDF5)
    target_include_directories(${GEOTIFF_VOL_NAME} PRIVATE ${HDF5_INCLUDE_DIRS} ${TIFF_INCLUDE_DIRS} ${GEOTIFF_INCLUDE_DIRS})
    target_compile_options(${GEOTIFF_VOL_NAME} PRIVATE ${TIFF_CFLAGS_OTHER} ${GEOTIFF_CFLAGS_OTHER})
endif()
