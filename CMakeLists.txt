cmake_minimum_required (VERSION 3.9)
project (geotiff_vol VERSION 1.0.0 DESCRIPTION "HDF5 VOL connector for GeoTIFF files" LANGUAGES C)

# Enable testing
enable_testing ()
include (CTest)

# Embed the paths to TIFF/GEOTIFF libraries in the built library
# so that HDF5 can find them at runtime
set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)

# === HDF5 setup ===
# Find HDF5 and set up alias target
find_package(HDF5 COMPONENTS C CONFIG)

if(NOT HDF5_FOUND)
    message(FATAL_ERROR "HDF5 CONFIG package not found. Set CMAKE_PREFIX_PATH or HDF5_DIR to your HDF5 install.")
endif()

# For now, require shared library
if (NOT TARGET hdf5-shared)
    message(FATAL_ERROR "hdf5-shared target not found in HDF5 package.")
endif()

# Create alias target to paper over specific HDF5 provided
add_library(HDF5::HDF5 ALIAS hdf5-shared)

# Extract HDF5 include directories for use by targets/tests
get_target_property(HDF5_INCLUDE_DIRS HDF5::HDF5 INTERFACE_INCLUDE_DIRECTORIES)

# === TIFF Setup ===
# Find TIFF - try config first, then allow CMake's FindTIFF.cmake module
find_package(TIFF CONFIG)
if(NOT TIFF_FOUND)
    find_package(TIFF)  # Try to use FindTIFF module
endif()

# Determine which TIFF target is available
set(_TIFF_TARGET "")
if(TARGET TIFF::TIFF)
    set(_TIFF_TARGET TIFF::TIFF)
    message(STATUS "Found TIFF target: ${_TIFF_TARGET}")
endif()

# If no TIFF target was found via config, try pkg-config then manual discovery
if(NOT _TIFF_TARGET)
    find_package(PkgConfig QUIET)
    if(PKG_CONFIG_FOUND)
        message(STATUS "Attempting pkg-config discovery of libtiff")
        pkg_check_modules(TIFF QUIET IMPORTED_TARGET libtiff-4)
        if(TIFF_FOUND)
            set(_TIFF_TARGET "PkgConfig::TIFF")
            message(STATUS "Found TIFF via pkg-config: ${_TIFF_TARGET}")
        endif()
    endif()
endif()

if(NOT _TIFF_TARGET)
    # Manual discovery for TIFF in standard system locations
    message(STATUS "Attempting manual discovery of libtiff")
    find_path(TIFF_INCLUDE_DIR tiffio.h)
    find_library(TIFF_LIBRARY NAMES tiff)

    if(NOT TIFF_INCLUDE_DIR OR NOT TIFF_LIBRARY)
        message(FATAL_ERROR "libtiff not found. Ensure TIFF is installed (e.g., via apt/brew) or provide headers/libs.")
    endif()

    message(STATUS "Found TIFF via manual discovery: ${TIFF_LIBRARY}")
endif()

# Create unified TIFF target for all potential TIFF sources
add_library(Tiff::Tiff INTERFACE IMPORTED)
if(_TIFF_TARGET)
    # Wrap the found target
    target_link_libraries(Tiff::Tiff INTERFACE ${_TIFF_TARGET})
else()
    # Wrap manual discovery results
    set_target_properties(Tiff::Tiff PROPERTIES
        INTERFACE_LINK_LIBRARIES "${TIFF_LIBRARY}"
        INTERFACE_INCLUDE_DIRECTORIES "${TIFF_INCLUDE_DIR}")
endif()

# === GeoTIFF Setup ===
# Find GeoTIFF
find_package(GeoTIFF CONFIG)
if(NOT GeoTIFF_FOUND)
    find_package(geotiff CONFIG)
endif()

# Determine which GeoTIFF target is available
set(_GEOTIFF_TARGET "")
foreach(geotiff_candidate IN ITEMS geotiff::geotiff GeoTIFF::geotiff GeoTIFF::GeoTIFF geotiff_library)
    if(TARGET ${geotiff_candidate})
        set(_GEOTIFF_TARGET ${geotiff_candidate})
        message(STATUS "Found GeoTIFF target: ${_GEOTIFF_TARGET}")
        # break()
    endif()
endforeach()
if(NOT _GEOTIFF_TARGET)
    message(STATUS "No GeoTIFF CMake target found")
endif()

# If no GeoTIFF target was found via config, try pkg-config then manual discovery
if(NOT _GEOTIFF_TARGET)
    find_package(PkgConfig QUIET)
    if(PKG_CONFIG_FOUND)
        message(STATUS "Attempting pkg-config discovery of libgeotiff")
        pkg_check_modules(GEOTIFF QUIET IMPORTED_TARGET libgeotiff)
        if(GEOTIFF_FOUND)
            set(_GEOTIFF_TARGET "PkgConfig::GEOTIFF")
            message(STATUS "Found GeoTIFF via pkg-config: ${_GEOTIFF_TARGET}")
        endif()
    endif()
endif()

if(NOT _GEOTIFF_TARGET)
    # Manual discovery for GeoTIFF in standard system locations
    message(STATUS "Attempting manual discovery of libgeotiff")
    find_path(GEOTIFF_INCLUDE_DIR geotiff/geotiff.h
        PATHS /usr/include /opt/homebrew/include /usr/local/include)
    find_library(GEOTIFF_LIBRARY NAMES geotiff
        PATHS /usr/lib /usr/lib/x86_64-linux-gnu /opt/homebrew/lib /usr/local/lib)

    if(NOT GEOTIFF_INCLUDE_DIR OR NOT GEOTIFF_LIBRARY)
        message(FATAL_ERROR "libgeotiff not found. Ensure GeoTIFF is installed (e.g., via apt/brew) or provide headers/libs.")
    endif()

    message(STATUS "Found GeoTIFF via manual discovery: ${GEOTIFF_LIBRARY}")
endif()

# Create unified GeoTIFF target for all potential GeoTIFF sources
add_library(GeoTiff::GeoTiff INTERFACE IMPORTED)
if(_GEOTIFF_TARGET)
    # Wrap the found target
    target_link_libraries(GeoTiff::GeoTiff INTERFACE ${_GEOTIFF_TARGET})

    # Extract and adjust include directories from the imported GeoTIFF target
    get_target_property(GEOTIFF_INCLUDE_DIRS ${_GEOTIFF_TARGET} INTERFACE_INCLUDE_DIRECTORIES)
    if(GEOTIFF_INCLUDE_DIRS)
        set(_GEOTIFF_ADJUSTED_INCLUDES "")
        foreach(_dir ${GEOTIFF_INCLUDE_DIRS})
            if(EXISTS "${_dir}/geotiff/geotiff.h")
                list(APPEND _GEOTIFF_ADJUSTED_INCLUDES "${_dir}/geotiff")
            else()
                list(APPEND _GEOTIFF_ADJUSTED_INCLUDES "${_dir}")
            endif()
        endforeach()
        set(GEOTIFF_INCLUDE_DIRS "${_GEOTIFF_ADJUSTED_INCLUDES}")
    endif()
else()
    # Wrap manual discovery results
    set(GEOTIFF_INCLUDE_DIRS ${GEOTIFF_INCLUDE_DIR})
    # Add common geotiff subdirectory to include path if it exists
    if(EXISTS "${GEOTIFF_INCLUDE_DIR}/geotiff")
        list(APPEND GEOTIFF_INCLUDE_DIRS "${GEOTIFF_INCLUDE_DIR}/geotiff")
    endif()

    set_target_properties(GeoTiff::GeoTiff PROPERTIES
        INTERFACE_LINK_LIBRARIES "${GEOTIFF_LIBRARY}"
        INTERFACE_INCLUDE_DIRECTORIES "${GEOTIFF_INCLUDE_DIRS}")
endif()

# Build options
option (GEOTIFF_VOL_BUILD_EXAMPLES "Build example programs" ON)

add_subdirectory (src)
add_subdirectory (test)

if (GEOTIFF_VOL_BUILD_EXAMPLES)
    add_subdirectory (examples)
endif()
