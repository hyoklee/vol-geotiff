# HDF5 VOL Connector for GeoTIFF 0.1.0

[![CI](https://github.com/LifeBoatLLC/DataFormats-VOLS/workflows/CI/badge.svg)](https://github.com/LifeBoatLLC/DataFormats-VOLS/actions/workflows/ci.yml)
[![Code Quality](https://github.com/LifeBoatLLC/DataFormats-VOLS/workflows/Code%20Quality/badge.svg)](https://github.com/LifeBoatLLC/DataFormats-VOLS/actions/workflows/code-quality.yml)
[![Documentation](https://github.com/LifeBoatLLC/DataFormats-VOLS/workflows/Documentation/badge.svg)](https://github.com/LifeBoatLLC/DataFormats-VOLS/actions/workflows/docs.yml)
[![License](https://img.shields.io/badge/License-Lifeboat-blue.svg)](COPYING)

This project implements an HDF5 Virtual Object Layer (VOL) connector that enables read-only operations on GeoTIFF files through HDF5.

## Features

- **GeoTIFF File Access**: Read GeoTIFF files through the HDF5 API
- **Image Data Access**: Access raster image data as HDF5 datasets
- **(TBD) HDF5 Tool Compatibility**: Use h5dump, h5ls, h5stat with GeoTIFF files

## Dependencies, Build, and Installation

See `doc/usage.md` for detailed instructions related to acquiring dependencing, building and installing the GeoTIFF VOL, and usage.

## Supported GeoTIFF Features

- Single or multi-image files with grayscale/RGB images
- Multiple bit depths
- Multiple sample formats (unsigned int, signed int, floating point)
- (TBD) Single and multi-band images
- (TBD) Various compression schemes (through libtiff)

## Testing

The project includes several test programs:

1. **test_runner**: GeoTIFF-specific functionality tests, run by make test/ctest in the build directory
2. (WIP) **test_h5tools.sh**: HDF5 tools integration test

## License

This project is licensed under the same terms as HDF5. See the COPYING file for details.

## Acknowledgments

- The HDF Group for the HDF5 library and VOL interface
- The GDAL project for GeoTIFF format specifications
- libgeotiff developers for the GeoTIFF library
