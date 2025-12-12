# Build the simple demo
add_executable(simple_demo simple_demo.c)
target_include_directories(simple_demo PRIVATE "${PROJECT_SOURCE_DIR}/src" ${HDF5_INCLUDE_DIRS})

# Link with HDF5, TIFF, GeoTIFF, and math libraries
message(STATUS "EXAMPLES: Building simple_demo with _have_config = ${_have_config}")
message(STATUS "EXAMPLES: TIFF_INCLUDE_DIRS = ${TIFF_INCLUDE_DIRS}")
message(STATUS "EXAMPLES: GEOTIFF_INCLUDE_DIRS = ${GEOTIFF_INCLUDE_DIRS}")
if (_have_config)
    message(STATUS "EXAMPLES: Using CONFIG targets")
    if(NOT WIN32)
        target_link_libraries(simple_demo PRIVATE HDF5::HDF5 TIFF::TIFF ${_GEOTIFF_TARGET} m)
    else()
        target_link_libraries(simple_demo PRIVATE HDF5::HDF5 TIFF::TIFF ${_GEOTIFF_TARGET})
    endif()
    # On Windows/vcpkg, explicitly add include directories to ensure headers are found
    if(GEOTIFF_INCLUDE_DIRS)
        target_include_directories(simple_demo PRIVATE ${GEOTIFF_INCLUDE_DIRS})
    endif()
    if(TIFF_INCLUDE_DIRS)
        target_include_directories(simple_demo PRIVATE ${TIFF_INCLUDE_DIRS})
    endif()
else()
    message(STATUS "EXAMPLES: Using manual libraries, adding include dirs")
    if(NOT WIN32)
        target_link_libraries(simple_demo PRIVATE HDF5::HDF5 ${TIFF_LIBRARIES} ${GEOTIFF_LIBRARIES} m)
    else()
        target_link_libraries(simple_demo PRIVATE HDF5::HDF5 ${TIFF_LIBRARIES} ${GEOTIFF_LIBRARIES})
    endif()
    target_include_directories(simple_demo PRIVATE ${TIFF_INCLUDE_DIRS} ${GEOTIFF_INCLUDE_DIRS})
    target_compile_options(simple_demo PRIVATE ${TIFF_CFLAGS_OTHER} ${GEOTIFF_CFLAGS_OTHER})
endif()

# Build the gnuplot demo
add_executable(gnuplot_demo gnuplot_demo.c)
target_include_directories(gnuplot_demo PRIVATE "${PROJECT_SOURCE_DIR}/src" ${HDF5_INCLUDE_DIRS})

# Link with HDF5, TIFF, GeoTIFF, and math libraries
if (_have_config)
    if(NOT WIN32)
        target_link_libraries(gnuplot_demo PRIVATE HDF5::HDF5 TIFF::TIFF ${_GEOTIFF_TARGET} m)
    else()
        target_link_libraries(gnuplot_demo PRIVATE HDF5::HDF5 TIFF::TIFF ${_GEOTIFF_TARGET})
    endif()
    # On Windows/vcpkg, explicitly add include directories to ensure headers are found
    if(GEOTIFF_INCLUDE_DIRS)
        target_include_directories(gnuplot_demo PRIVATE ${GEOTIFF_INCLUDE_DIRS})
    endif()
    if(TIFF_INCLUDE_DIRS)
        target_include_directories(gnuplot_demo PRIVATE ${TIFF_INCLUDE_DIRS})
    endif()
else()
    if(NOT WIN32)
        target_link_libraries(gnuplot_demo PRIVATE HDF5::HDF5 ${TIFF_LIBRARIES} ${GEOTIFF_LIBRARIES} m)
    else()
        target_link_libraries(gnuplot_demo PRIVATE HDF5::HDF5 ${TIFF_LIBRARIES} ${GEOTIFF_LIBRARIES})
    endif()
    target_include_directories(gnuplot_demo PRIVATE ${TIFF_INCLUDE_DIRS} ${GEOTIFF_INCLUDE_DIRS})
    target_compile_options(gnuplot_demo PRIVATE ${TIFF_CFLAGS_OTHER} ${GEOTIFF_CFLAGS_OTHER})
endif()

# Install the example programs
install(TARGETS simple_demo gnuplot_demo
        RUNTIME DESTINATION bin)
