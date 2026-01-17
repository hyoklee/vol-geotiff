# Build the simple demo
add_executable(simple_demo simple_demo.c)
target_include_directories(simple_demo PRIVATE "${PROJECT_SOURCE_DIR}/src" ${HDF5_INCLUDE_DIRS})

# Link with HDF5, TIFF, GeoTIFF, and math libraries
# Note: On Windows/MSVC, the math library is built-in, so we don't link against 'm'
if(UNIX)
    target_link_libraries(simple_demo PRIVATE HDF5::HDF5 Tiff::Tiff GeoTiff::GeoTiff m)
else()
    target_link_libraries(simple_demo PRIVATE HDF5::HDF5 Tiff::Tiff GeoTiff::GeoTiff)
endif()

# Build the gnuplot demo
add_executable(gnuplot_demo gnuplot_demo.c)
target_include_directories(gnuplot_demo PRIVATE "${PROJECT_SOURCE_DIR}/src" ${HDF5_INCLUDE_DIRS})

# Link with HDF5, TIFF, GeoTIFF, and math libraries
# Note: On Windows/MSVC, the math library is built-in, so we don't link against 'm'
if(UNIX)
    target_link_libraries(gnuplot_demo PRIVATE HDF5::HDF5 Tiff::Tiff GeoTiff::GeoTiff m)
else()
    target_link_libraries(gnuplot_demo PRIVATE HDF5::HDF5 Tiff::Tiff GeoTiff::GeoTiff)
endif()

# Install the example programs
install(TARGETS simple_demo gnuplot_demo
        RUNTIME DESTINATION bin)
