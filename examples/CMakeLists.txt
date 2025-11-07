# Build the simple demo
add_executable(simple_demo simple_demo.c)
target_include_directories(simple_demo PRIVATE "${PROJECT_SOURCE_DIR}/src" ${HDF5_INCLUDE_DIRS})

# Link with HDF5, TIFF, GeoTIFF, and math libraries
if (_have_config)
    target_link_libraries(simple_demo PRIVATE HDF5::HDF5 TIFF::TIFF ${_GEOTIFF_TARGET} m)
else()
    target_link_libraries(simple_demo PRIVATE HDF5::HDF5 ${TIFF_LIBRARIES} ${GEOTIFF_LIBRARIES} m)
    target_include_directories(simple_demo PRIVATE ${TIFF_INCLUDE_DIRS} ${GEOTIFF_INCLUDE_DIRS})
    target_compile_options(simple_demo PRIVATE ${TIFF_CFLAGS_OTHER} ${GEOTIFF_CFLAGS_OTHER})
endif()

# Build the gnuplot demo
add_executable(gnuplot_demo gnuplot_demo.c)
target_include_directories(gnuplot_demo PRIVATE "${PROJECT_SOURCE_DIR}/src" ${HDF5_INCLUDE_DIRS})

# Link with HDF5, TIFF, GeoTIFF, and math libraries
if (_have_config)
    target_link_libraries(gnuplot_demo PRIVATE HDF5::HDF5 TIFF::TIFF ${_GEOTIFF_TARGET} m)
else()
    target_link_libraries(gnuplot_demo PRIVATE HDF5::HDF5 ${TIFF_LIBRARIES} ${GEOTIFF_LIBRARIES} m)
    target_include_directories(gnuplot_demo PRIVATE ${TIFF_INCLUDE_DIRS} ${GEOTIFF_INCLUDE_DIRS})
    target_compile_options(gnuplot_demo PRIVATE ${TIFF_CFLAGS_OTHER} ${GEOTIFF_CFLAGS_OTHER})
endif()

# Install the example programs
install(TARGETS simple_demo gnuplot_demo
        RUNTIME DESTINATION bin)
