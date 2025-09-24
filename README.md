# HDF5 VOL Connector for GeoTIFF

[![CI](https://github.com/hyoklee/vol-geotiff/workflows/CI/badge.svg)](https://github.com/hyoklee/vol-geotiff/actions/workflows/ci.yml)
[![Code Quality](https://github.com/hyoklee/vol-geotiff/workflows/Code%20Quality/badge.svg)](https://github.com/hyoklee/vol-geotiff/actions/workflows/code-quality.yml)
[![Documentation](https://github.com/hyoklee/vol-geotiff/workflows/Documentation/badge.svg)](https://github.com/hyoklee/vol-geotiff/actions/workflows/docs.yml)
[![License](https://img.shields.io/badge/License-HDF5-blue.svg)](COPYING)

This project implements an HDF5 Virtual Object Layer (VOL) connector that enables reading GeoTIFF files through HDF5 tools and netCDF-C. This allows users to access GeoTIFF data using familiar HDF5 tools like h5dump, h5ls, and netCDF tools like ncdump.

## Features

- **GeoTIFF File Access**: Read GeoTIFF files through the HDF5 API
- **Image Data Access**: Access raster image data as HDF5 datasets
- **Metadata Extraction**: Parse and expose GeoTIFF spatial metadata
- **HDF5 Tool Compatibility**: Use h5dump, h5ls, h5stat with GeoTIFF files
- **netCDF-C Compatibility**: Use ncdump and other netCDF tools with GeoTIFF files
- **Multiple Data Types**: Support for various TIFF data types (uint8, uint16, uint32, float32, float64)

## Architecture

The GeoTIFF VOL connector maps GeoTIFF file structure to HDF5 concepts:

- **File**: GeoTIFF file (.tif/.tiff)
- **Root Group**: "/" represents the file root
- **Image Dataset**: "/image" contains the raster data
- **Attributes**: GeoTIFF metadata (coordinate system, geo-referencing, etc.)

## Dependencies

You will need the following to build the GeoTIFF VOL connector:

- **HDF5 develop branch (1.15+/2.x)** with VOL support
- **libtiff** (TIFF library)
- **libgeotiff** (GeoTIFF library)
- **CMake 3.9 or later**
- **pkg-config** for finding TIFF and GeoTIFF libraries

### Installing Dependencies

On Ubuntu/Debian (using HDF5 develop built from source):
```bash
sudo apt-get install -y build-essential cmake pkg-config libtiff5-dev libgeotiff-dev
git clone --depth 1 --branch develop https://github.com/HDFGroup/hdf5.git
cd hdf5 && mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release -DBUILD_SHARED_LIBS=ON -DHDF5_BUILD_TOOLS=ON -DHDF5_BUILD_TESTS=OFF -DCMAKE_INSTALL_PREFIX=/opt/hdf5-develop
make -j$(nproc) && sudo make install
```

On CentOS/RHEL:
```bash
sudo yum install hdf5-devel libtiff-devel libgeotiff-devel pkgconfig
```

On macOS with Homebrew (HDF5 develop built from source is required):
```bash
brew install cmake pkg-config libtiff libgeotiff
# Build and install HDF5 develop similarly to the Linux instructions above
```

## Building

This project uses CMake as the build system. Autotools support has been removed to simplify the build process and maintenance.

### CMake Build
1. Create a build directory:
   ```bash
   mkdir build && cd build
   ```

2. Configure with CMake (point to HDF5 develop install):

4. Run tests:
   ```bash
   make test
   ```

### Alternative CMake Configuration

This project requires HDF5 develop (1.15+/2.x). Ensure your `CMAKE_PREFIX_PATH` and/or `HDF5_DIR` point to that install.

## Usage

### Environment Setup

Set the HDF5 plugin path to include the built connector:
```bash
export HDF5_PLUGIN_PATH=/path/to/vol-geotiff/build/src
```

### Using with HDF5 Tools

List contents of a GeoTIFF file:
```bash
h5ls --vol-name=geotiff_vol_connector sample.tif
```

Dump GeoTIFF structure and data:
```bash
h5dump --vol-name=geotiff_vol_connector sample.tif
```

Get statistics:
```bash
h5stat --vol-name=geotiff_vol_connector sample.tif
```

### Using with netCDF Tools

Set the VOL connector environment variable:
```bash
export HDF5_VOL_CONNECTOR=geotiff_vol_connector
```

Dump header information:
```bash
ncdump -h sample.tif
```

Extract image data:
```bash
ncdump -v image sample.tif
```

### Programming Interface

```c
#include "template_vol_connector.h"
#include <hdf5.h>

int main() {
    hid_t vol_id, fapl_id, file_id, dset_id;

    // Register the GeoTIFF VOL connector
    vol_id = H5VLregister_connector_by_name(GEOTIFF_VOL_CONNECTOR_NAME, H5P_DEFAULT);

    // Create file access property list and set VOL connector
    fapl_id = H5Pcreate(H5P_FILE_ACCESS);
    H5Pset_vol(fapl_id, vol_id, NULL);

    // Open GeoTIFF file
    file_id = H5Fopen("sample.tif", H5F_ACC_RDONLY, fapl_id);

    // Open image dataset
    dset_id = H5Dopen2(file_id, "/image", H5P_DEFAULT);

    // Read data...

    // Cleanup
    H5Dclose(dset_id);
    H5Fclose(file_id);
    H5Pclose(fapl_id);
    H5VLunregister_connector(vol_id);

    return 0;
}
```

## Supported GeoTIFF Features

### Image Data
- Multiple bit depths (8, 16, 32, 64 bit)
- Multiple sample formats (unsigned int, signed int, floating point)
- Single and multi-band images
- Various compression schemes (through libtiff)

### Spatial Metadata
- Coordinate Reference Systems (CRS)
- Geotransformation parameters
- Tie points and pixel scale
- Geographic and projected coordinate systems
- Datum and ellipsoid information

### Limitations
- **Read-only**: The connector only supports reading GeoTIFF files
- **Single image**: Only the primary image is exposed as a dataset
- **Memory usage**: Large images are loaded entirely into memory
- **Complex projections**: Some advanced GeoTIFF features may not be fully supported

## Testing

The project includes several test programs:

1. **vol_plugin**: Basic VOL connector registration test
2. **test_geotiff_read**: GeoTIFF-specific functionality test
3. **test_h5tools.sh**: HDF5 tools integration test
4. **test_ncdump.sh**: netCDF tools integration test

Run tests with a sample GeoTIFF file:
```bash
cd test
./test_geotiff_read sample.tif
./test_h5tools.sh
./test_ncdump.sh
```

## Troubleshooting

### Plugin Not Found
- Ensure `HDF5_PLUGIN_PATH` is set correctly
- Verify the shared library was built successfully
- Check that HDF5 was compiled with plugin support

### Library Dependencies
- Ensure libtiff and libgeotiff are properly installed
- Check that pkg-config can find the required libraries
- Verify library versions are compatible

### Runtime Errors
- Check that the GeoTIFF file is valid and accessible
- Ensure sufficient memory for large images
- Verify HDF5 version is the develop branch (1.15+/2.x)

## Development and CI/CD

This project uses GitHub Actions for continuous integration and deployment:

### Automated Testing
- **CI Workflow**: Tests on Ubuntu (and macOS as applicable) with HDF5 develop only
- **Code Quality**: Static analysis, formatting checks, and security scans
- **Comprehensive Testing**: Weekly extensive testing across different configurations
- **Documentation**: Automatic API documentation generation and deployment

### Workflows
- `.github/workflows/ci.yml` - Main CI pipeline
- `.github/workflows/code-quality.yml` - Code quality checks
- `.github/workflows/docs.yml` - Documentation generation
- `.github/workflows/release.yml` - Automated releases
- `.github/workflows/comprehensive-test.yml` - Extended testing (HDF5 develop only)

### Development Process
1. All pull requests must pass CI checks
2. Code formatting is enforced via clang-format
3. Static analysis must pass without warnings
4. New features require tests and documentation
5. Releases are automatically built and published

## Contributing

Contributions are welcome! Please:

1. Fork the repository
2. Create a feature branch
3. Add tests for new functionality
4. Ensure all CI checks pass
5. Update documentation as needed
6. Submit a pull request

### Local Development
```bash
# Install pre-commit hooks (recommended)
pip install pre-commit
pre-commit install

# Run tests locally
mkdir build && cd build
cmake ..
make
make test

# Run code formatting
clang-format -i src/*.c src/*.h test/*.c test/*.h
```

### Pre-commit on Windows (PowerShell)
```powershell
# One-time setup: installs pre-commit and installs git hook
scripts/setup-precommit.ps1 -Install

# Run hooks on all files
pre-commit run --all-files
```

## License

This project is licensed under the same terms as HDF5. See the COPYING file for details.

## Acknowledgments

- The HDF Group for the HDF5 library and VOL interface
- The GDAL project for GeoTIFF format specifications
- libgeotiff developers for the GeoTIFF library
