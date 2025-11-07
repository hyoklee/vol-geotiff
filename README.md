# HDF5 VOL Connector for GeoTIFF 0.1.0

[![CI](https://github.com/LifeBoatLLC/DataFormats-VOLS/workflows/CI/badge.svg)](https://github.com/LifeBoatLLC/DataFormats-VOLS/actions/workflows/ci.yml)
[![Code Quality](https://github.com/LifeBoatLLC/DataFormats-VOLS/workflows/Code%20Quality/badge.svg)](https://github.com/LifeBoatLLC/DataFormats-VOLS/actions/workflows/code-quality.yml)
[![Documentation](https://github.com/LifeBoatLLC/DataFormats-VOLS/workflows/Documentation/badge.svg)](https://github.com/LifeBoatLLC/DataFormats-VOLS/actions/workflows/docs.yml)
[![License](https://img.shields.io/badge/License-Lifeboat-blue.svg)](COPYING)

This project implements an HDF5 Virtual Object Layer (VOL) connector that enables read-only operations on GeoTIFF files through HDF5 tools and netCDF-C. This allows users to access GeoTIFF data using familiar HDF5 tools like h5dump, h5ls, and netCDF tools like ncdump.

## Features

- **GeoTIFF File Access**: Read GeoTIFF files through the HDF5 API
- **Image Data Access**: Access raster image data as HDF5 datasets
- **HDF5 Tool Compatibility**: Use h5dump, h5ls, h5stat with GeoTIFF files
- **netCDF-C Compatibility**: Use ncdump and other netCDF tools with GeoTIFF files

- **(TBD) Metadata Extraction**: Parse and expose GeoTIFF spatial metadata

## Architecture

See the work-in-progress architecture.md file for the mapping between GeoTIFF file structure and HDF5 concepts.

## Dependencies

The GeoTIFF VOL connector has the following dependencies:

- **HDF5 develop branch (1.14+/2.x)** with VOL support
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

This project uses CMake as the build system. 

### CMake Build
1. Create a build directory:
   ```bash
   mkdir build && cd build
   ```

2. Configure with CMake (point to HDF5 install):
   ```bash
   cmake .. -DCMAKE_PREFIX_PATH=/opt/hdf5/
   ```

   Or specify HDF5 location explicitly:
   ```bash
   cmake .. -DHDF5_DIR=/opt/hdf5/install
   ```

3. Build the connector:
   ```bash
   make -j$(nproc)
   ```

4. (Optional) Install the connector:
   ```bash
   sudo make install
   ```

5. Run tests:
   ```bash
   make test
   ```

### Alternative CMake Configuration

This project requires HDF5 develop (1.14+/2.x). Ensure your `CMAKE_PREFIX_PATH` and/or `HDF5_DIR` point to that install.

## Usage

### Environment Setup

Set the HDF5 plugin path to include the built connector:
```bash
export HDF5_PLUGIN_PATH=/path/to/DataFormats-VOLS/build/src
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
#include "geotiff_vol_connector.h"
#include <hdf5.h>

int main() {
    hid_t vol_id, fapl_id, file_id, dset_id;

    // Tell the library where to find the GeoTIFF VOL connector library
    // (May be skipped if HDF5_VOL_CONNECTOR/HDF5_PLUGIN_PATH are defined in env)
    H5PLappend(GEOTIFF_VOL_PLUGIN_PATH);

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

- Single-image files with grayscale/RGB images

- (TBD) Multiple bit depths
- (TBD) Multiple sample formats (unsigned int, signed int, floating point)
- (TBD) Single and multi-band images
- (TBD) Various compression schemes (through libtiff)

### (TBD) Spatial Metadata
- (TBD) Coordinate Reference Systems (CRS)
- (TBD) Geotransformation parameters
- (TBD) Tie points and pixel scale
- (TBD) Geographic and projected coordinate systems
- (TBD) Datum and ellipsoid information

## Testing

The project includes several test programs:

1. **test_runner**: GeoTIFF-specific functionality tests, run by make test/ctest
2. (WIP) **test_ncdump**: netCDF tools integration test
3. (WIP) **test_h5tools.sh**: HDF5 tools integration test

Or run tests with a sample GeoTIFF file:
```bash
cd test
./test_runner 
(WIP) ./test_h5tools.sh <GeoTIFF filename>
(WIP) ./test_ncdump.sh <GeoTIFF filename>
```

## License

This project is licensed under the same terms as HDF5. See the COPYING file for details.

## Acknowledgments

- The HDF Group for the HDF5 library and VOL interface
- The GDAL project for GeoTIFF format specifications
- libgeotiff developers for the GeoTIFF library
