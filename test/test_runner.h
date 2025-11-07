
#ifndef TEST_GEOTIFF_H
#define TEST_GEOTIFF_H

#include <hdf5.h>

#define GRAYSCALE_FILENAME "test_image_gray.tif"
#define RGB_FILENAME "test_image_rgb.tif"
#define WIDTH 32
#define HEIGHT 32

/* Helper function to generate tiled TIFFs */
int generate_tiled_tiff(const char *filename, int is_rgb, uint32_t width, uint32_t height,
                        uint32_t tile_width, uint32_t tile_height);

/* Plugin-handling tests */
herr_t test_getters(void);
herr_t test_multiple_registration(void);
herr_t test_registration_by_name(void);
herr_t test_registration_by_value(void);

/* GeoTIFF functionality tests */
int OpenGeoTIFFTest(const char *filename);
int ReadGeoTIFFTest(const char *filename);
int BandReadGeoTIFFTest(const char *filename);
int PointReadGeoTIFFTest(const char *filename);
int MultiImageReadGeoTIFFTest(void);
int DatasetErrorHandlingTest(const char *filename);
int DatatypeConversionTest(hid_t mem_type_id, hid_t file_type_id, const char *mem_type_name,
                           const char *file_type_name);
int LinkExistsTest(const char *filename);
int LinkIterateTest(const char *filename);
int UnsupportedFeaturesTest(void);
int GroupGetInfoTest(void);
int TiledTIFFReadTest(const char *filename, int is_rgb);

#endif // TEST_GEOTIFF_H