This work-in-progress document describes the mapping between GeoTIFF file data and metadata and HDF5-level objects.

## Files

The top-level GeoTIFF file maps to an HDF5 file.

## Images

Each distinct image within the GeoTIFF file is considered an HDF5 dataset. Each image is assigned an HDF5 native datatype that matches the bits-per-sample and signed/unsigned qualities of the image's sample format.

### Dataset Naming

Images are exposed as HDF5 datasets using a zero-indexed naming convention: `image0`, `image1`, `image2`, etc. Dataset names provided to `H5Dopen` and other operations must be of the form `imageN`. Any other name will fail due to not precisely specifying an image within the GeoTIFF file.

### Discovering the Number of Images

To determine how many images (TIFF directories) are present in a GeoTIFF file, there are two methods.

1. Read from the scalar `num_images` attribute on a file. This is an attribute with a single native unisnged 64-bit integer describing the number of images/directories in the GeoTIFF file.

2. Use `H5Gget_info()` on the file ID. The `nlinks` field in the returned `H5G_info_t` structure indicates the total number of images available in the file. For example, a single-image TIFF will report `nlinks = 1` (corresponding to `image0`), while a multi-image TIFF with three directories will report `nlinks = 3` (corresponding to `image0`, `image1`, and `image2`).

### RGB(A) Images

Grayscale GeoTIFF images (1 sample per pixel) are represented in HDF5 as two-dimensional datasets with shape `[height, width]`.

GeoTIFF images with RGB color data (3 samples per pixel) are represented in HDF5 as three-dimensional datasets with shape `[height, width, 3]`, where the third dimension distinguishes among the three RGB color channels (red, green, blue).

Similarly, GeoTIFF images with RGBA color data (4 samples per pixel) are represented in HDF5 as three-dimensional datasets with shape `[height, width, 4]`, where the third dimension distinguishes among the four color channels (red, green, blue, alpha).

#### Planar Configuration Limitations

The VOL connector **only supports PLANARCONFIG_CONTIG** (interleaved pixel data). Images with PLANARCONFIG_SEPARATE (where RGB channels are stored as separate planes) are not supported and will fail to open. This is a design limitation as the connector expects all color channels for a pixel to be stored contiguously in memory.

#### Photometric Interpretation Limitations

The VOL connector supports the following photometric interpretations:
- **PHOTOMETRIC_MINISBLACK**: Grayscale images where 0 is black
- **PHOTOMETRIC_MINISWHITE**: Grayscale images where 0 is white
- **PHOTOMETRIC_RGB**: RGB color images

Unsupported photometric interpretations include:
- PHOTOMETRIC_PALETTE (indexed/palette color)
- PHOTOMETRIC_CIELAB (CIE L\*a\*b\* color space)
- PHOTOMETRIC_YCBCR (YCbCr color space)
- Other specialized color spaces

#### Data Type Limitations

The VOL connector supports integer data types (8, 16, 32, and 64-bit signed and unsigned). Floating-point sample formats and complex data types are not currently supported.

### Multi-Resolution Images

TIFF and GeoTIFF files may contain multiple resolution levels (also called "overviews" or "image pyramids") stored as separate TIFF directories. The GeoTIFF VOL connector treats each TIFF directory as an independent HDF5 dataset with no special handling for multi-resolution relationships.

This design reflects the fact that: (1) the TIFF `SUBFILETYPE` tag merely hints that an image is reduced-resolution without specifying which image it's derived from, (2) there is no explicit parent-child association between directories in the TIFF format, and (3) each directory appears as a separate directory in TIFF with fully duplicated data. Any ambiguity in determining which reduced-resolution images correspond to which full-resolution images is inherent to the TIFF format itself, not introduced by the VOL connector.

## (TBD) Metadata & Tags
