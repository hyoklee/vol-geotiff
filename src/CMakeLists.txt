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
    message(STATUS "Using CONFIG mode: _have_config = TRUE")
    # Extract include directories from the imported GeoTIFF target for use by tests
    get_target_property(GEOTIFF_INCLUDE_DIRS ${_GEOTIFF_TARGET} INTERFACE_INCLUDE_DIRECTORIES)
    # Propagate to parent scope so tests can include headers
    set(GEOTIFF_INCLUDE_DIRS "${GEOTIFF_INCLUDE_DIRS}" PARENT_SCOPE)
else()
    message(STATUS "Using pkg-config/manual mode: _have_config = FALSE")
endif()

if (NOT _have_config)
    message(STATUS "Falling back to alternate discovery for TIFF/geotiff (vcpkg HINTS, pkg-config last)")
    message(STATUS "DEBUG: _have_config = ${_have_config}")
    # Try to find TIFF without pkg-config to support Windows easily
    if (NOT TARGET TIFF::TIFF)
        set(_TIFF_INC_HINT "")
        set(_TIFF_LIB_HINT "")
        if (DEFINED VCPKG_ROOT AND DEFINED VCPKG_TARGET_TRIPLET)
            set(_TIFF_INC_HINT "${VCPKG_ROOT}/installed/${VCPKG_TARGET_TRIPLET}/include")
            set(_TIFF_LIB_HINT "${VCPKG_ROOT}/installed/${VCPKG_TARGET_TRIPLET}/lib")
        endif()
        find_path(TIFF_INCLUDE_DIR tiffio.h
                  HINTS ${_TIFF_INC_HINT})
        find_library(TIFF_LIBRARY NAMES tiff
                     HINTS ${_TIFF_LIB_HINT})
        if (TIFF_INCLUDE_DIR AND TIFF_LIBRARY)
            set(TIFF_INCLUDE_DIRS ${TIFF_INCLUDE_DIR})
            set(TIFF_LIBRARIES ${TIFF_LIBRARY})
            message(STATUS "Found TIFF via find_path/find_library")
            message(STATUS "TIFF_INCLUDE_DIRS: ${TIFF_INCLUDE_DIRS}")
        else()
            # Last resort: pkg-config
            message(STATUS "Trying pkg-config for TIFF...")
            find_package(PkgConfig REQUIRED)
            pkg_check_modules(TIFF REQUIRED libtiff-4)
            message(STATUS "pkg-config TIFF_INCLUDE_DIRS: ${TIFF_INCLUDE_DIRS}")
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
              HINTS ${_VCPKG_INC_HINT})
    find_library(GEOTIFF_LIBRARY NAMES geotiff
                 HINTS ${_VCPKG_LIB_HINT})

    if(GEOTIFF_INCLUDE_DIR AND GEOTIFF_LIBRARY)
        set(GEOTIFF_LIBRARIES ${GEOTIFF_LIBRARY})
        set(GEOTIFF_INCLUDE_DIRS ${GEOTIFF_INCLUDE_DIR})
        message(STATUS "Found GeoTIFF via find_path/find_library")
    else()
        # Last resort: pkg-config
        message(STATUS "Trying pkg-config for libgeotiff...")
        find_package(PkgConfig REQUIRED)
        pkg_check_modules(GEOTIFF REQUIRED libgeotiff)
        message(STATUS "pkg-config found GEOTIFF_FOUND: ${GEOTIFF_FOUND}")
        message(STATUS "pkg-config GEOTIFF_INCLUDE_DIRS: ${GEOTIFF_INCLUDE_DIRS}")
        message(STATUS "pkg-config GEOTIFF_LIBRARIES: ${GEOTIFF_LIBRARIES}")
        message(STATUS "pkg-config GEOTIFF_LIBRARY_DIRS: ${GEOTIFF_LIBRARY_DIRS}")

        # Check if headers are in a geotiff subdirectory or not
        # On macOS Homebrew, headers are at /opt/homebrew/include/geotiffio.h
        # On Linux, headers are at /usr/include/geotiff/geotiffio.h
        # We need to create a compatibility shim for macOS
        list(GET GEOTIFF_INCLUDE_DIRS 0 GEOTIFF_FIRST_INCLUDE)
        if(NOT EXISTS "${GEOTIFF_FIRST_INCLUDE}/geotiff/geotiffio.h")
            message(STATUS "Headers not in geotiff/ subdirectory, creating compatibility shim")
            file(MAKE_DIRECTORY "${PROJECT_BINARY_DIR}/geotiff_compat/geotiff")

            # Symlink all geotiff headers
            set(GEOTIFF_HEADERS
                geotiffio.h geo_normalize.h geotiff.h xtiffio.h geovalues.h
                geokeys.h geonames.h geo_tiffp.h cpl_serv.h geo_config.h
                geo_simpletags.h geo_keyp.h)

            foreach(HEADER ${GEOTIFF_HEADERS})
                if(EXISTS "${GEOTIFF_FIRST_INCLUDE}/${HEADER}")
                    file(CREATE_LINK "${GEOTIFF_FIRST_INCLUDE}/${HEADER}"
                         "${PROJECT_BINARY_DIR}/geotiff_compat/geotiff/${HEADER}" SYMBOLIC)
                endif()
            endforeach()

            # Prepend the compat directory to include paths
            set(GEOTIFF_INCLUDE_DIRS "${PROJECT_BINARY_DIR}/geotiff_compat;${GEOTIFF_INCLUDE_DIRS}")
            message(STATUS "Created geotiff compatibility shim at ${PROJECT_BINARY_DIR}/geotiff_compat")
        endif()

        # Fix library paths - pkg-config returns just library names, need full paths
        if(GEOTIFF_LIBRARY_DIRS)
            set(GEOTIFF_LIBRARIES "")
            foreach(LIB ${GEOTIFF_LIBRARIES})
                find_library(GEOTIFF_LIB_${LIB} NAMES ${LIB} PATHS ${GEOTIFF_LIBRARY_DIRS} NO_DEFAULT_PATH)
                if(GEOTIFF_LIB_${LIB})
                    list(APPEND GEOTIFF_LIBRARIES ${GEOTIFF_LIB_${LIB}})
                else()
                    list(APPEND GEOTIFF_LIBRARIES ${LIB})
                endif()
            endforeach()
            message(STATUS "Resolved GEOTIFF_LIBRARIES: ${GEOTIFF_LIBRARIES}")
        endif()
    endif()
    # Propagate to parent scope so tests can include headers
    set(GEOTIFF_INCLUDE_DIRS "${GEOTIFF_INCLUDE_DIRS}" PARENT_SCOPE)
    set(GEOTIFF_LIBRARIES "${GEOTIFF_LIBRARIES}" PARENT_SCOPE)
    set(TIFF_INCLUDE_DIRS "${TIFF_INCLUDE_DIRS}" PARENT_SCOPE)
    set(TIFF_LIBRARIES "${TIFF_LIBRARIES}" PARENT_SCOPE)
    set(TIFF_CFLAGS_OTHER "${TIFF_CFLAGS_OTHER}" PARENT_SCOPE)
    set(GEOTIFF_CFLAGS_OTHER "${GEOTIFF_CFLAGS_OTHER}" PARENT_SCOPE)
    message(STATUS "Using GeoTIFF include dir: ${GEOTIFF_INCLUDE_DIRS}")
    message(STATUS "Using GeoTIFF library: ${GEOTIFF_LIBRARIES}")
endif()

# Propagate _have_config to parent scope for test builds
set(_have_config "${_have_config}" PARENT_SCOPE)
set(_GEOTIFF_TARGET "${_GEOTIFF_TARGET}" PARENT_SCOPE)

# Build the GeoTIFF VOL connector
add_library (${GEOTIFF_VOL_NAME} SHARED geotiff_vol_connector.c)
set_target_properties (${GEOTIFF_VOL_NAME} PROPERTIES VERSION ${PROJECT_VERSION})
set_target_properties (${GEOTIFF_VOL_NAME} PROPERTIES SOVERSION 1)
set_target_properties (${GEOTIFF_VOL_NAME} PROPERTIES PUBLIC_HEADER "geotiff_vol_connector.h;geotiff_vol_err.h")

# Always add HDF5 include directories explicitly
target_include_directories(${GEOTIFF_VOL_NAME} PRIVATE ${HDF5_INCLUDE_DIRS})

# Link libraries
message(STATUS "Building ${GEOTIFF_VOL_NAME} with _have_config = ${_have_config}")
if (_have_config)
    message(STATUS "Using CONFIG targets: TIFF::TIFF and ${_GEOTIFF_TARGET}")
    target_link_libraries(${GEOTIFF_VOL_NAME} PRIVATE TIFF::TIFF ${_GEOTIFF_TARGET} HDF5::HDF5)
else()
    message(STATUS "Using manual libraries: ${TIFF_LIBRARIES} ${GEOTIFF_LIBRARIES}")
    message(STATUS "Adding include dirs: TIFF=${TIFF_INCLUDE_DIRS}")
    message(STATUS "Adding include dirs: GEOTIFF=${GEOTIFF_INCLUDE_DIRS}")
    target_link_libraries(${GEOTIFF_VOL_NAME} PRIVATE ${TIFF_LIBRARIES} ${GEOTIFF_LIBRARIES} HDF5::HDF5)
    target_include_directories(${GEOTIFF_VOL_NAME} PRIVATE ${TIFF_INCLUDE_DIRS} ${GEOTIFF_INCLUDE_DIRS})
    target_compile_options(${GEOTIFF_VOL_NAME} PRIVATE ${TIFF_CFLAGS_OTHER} ${GEOTIFF_CFLAGS_OTHER})
endif()

# Install rules
install(TARGETS ${GEOTIFF_VOL_NAME}
        LIBRARY DESTINATION lib
        PUBLIC_HEADER DESTINATION include)
