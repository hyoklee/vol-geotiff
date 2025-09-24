# CMake generated Testfile for 
# Source directory: /home/hyoklee/vol-geotiff/test
# Build directory: /home/hyoklee/vol-geotiff/build/test
# 
# This file includes the relevant testing commands required for 
# testing this directory and lists subdirectories to be tested as well.
add_test(vol_plugin "vol_plugin")
set_tests_properties(vol_plugin PROPERTIES  ENVIRONMENT "HDF5_PLUGIN_PATH=/home/hyoklee/vol-geotiff/build/src" _BACKTRACE_TRIPLES "/home/hyoklee/vol-geotiff/test/CMakeLists.txt;12;add_test;/home/hyoklee/vol-geotiff/test/CMakeLists.txt;0;")
add_test(test_geotiff_read "test_geotiff_read" "/home/hyoklee/vol-geotiff/test/sample.tif")
set_tests_properties(test_geotiff_read PROPERTIES  ENVIRONMENT "HDF5_PLUGIN_PATH=/home/hyoklee/vol-geotiff/build/src" _BACKTRACE_TRIPLES "/home/hyoklee/vol-geotiff/test/CMakeLists.txt;18;add_test;/home/hyoklee/vol-geotiff/test/CMakeLists.txt;0;")
