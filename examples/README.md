# GeoTIFF VOL Connector Examples

This directory contains C examples demonstrating how to use the GeoTIFF VOL connector to read GeoTIFF files through the HDF5 API and visualize them using gnuplot.

## Overview

The demo workflow:
1. Creates a visually distinctive 256×256 RGB GeoTIFF image with colorful concentric rings
2. Reads the GeoTIFF file through the HDF5 VOL connector (as if it were an HDF5 file)
3. Extracts image data and writes it to gnuplot-compatible data files
4. Generates gnuplot scripts to visualize the data
5. Produces PNG visualizations

## Files

- **`gnuplot_demo.c`**: Complete demo that creates a GeoTIFF, reads it via VOL connector, and generates gnuplot visualizations
- **`simple_demo.c`**: Simple example showing basic VOL connector usage
- **`CMakeLists.txt`**: Build configuration for the demo programs
- **`demo_rings.tif`**: Pre-generated demo GeoTIFF file for testing

## Quick Start

### Build the Examples

From the project root directory:

```bash
mkdir -p build && cd build
cmake ..
make
```

This builds all example programs in `build/examples/`.

### Run the gnuplot Demo

From the build directory:

```bash
cd examples
./gnuplot_demo
```

The demo will:
- Create `demo_rings_gnuplot.tif` with colorful concentric rings
- Read it back through the HDF5 VOL connector
- Generate gnuplot data files (`gnuplot_rgb.dat`, `gnuplot_gray.dat`)
- Create a gnuplot script (`gnuplot_commands.gp`)
- Invoke gnuplot to generate visualizations (`demo_rgb.png`, `demo_gray.png`)

### Run the Simple Demo

From the build directory:

```bash
cd examples
./simple_demo
```

This demonstrates basic VOL connector operations:
- Creates a test GeoTIFF
- Opens it via the VOL connector
- Displays dimensions and metadata
- Reads and prints pixel data

## Requirements

### Build Dependencies
- GCC or Clang compiler
- CMake 3.9+
- libtiff
- libgeotiff
- HDF5 1.14.0+ with VOL support

### Runtime Dependencies
- gnuplot (for visualization demo)

Install gnuplot on Ubuntu/Debian:
```bash
sudo apt-get install gnuplot
```

Install gnuplot on macOS:
```bash
brew install gnuplot
```

## Output Files

The gnuplot demo creates:

1. **`demo_rings_gnuplot.tif`**: 256×256 RGB GeoTIFF with colorful concentric rings
   - Format: RGB (3 channels, 8-bit per channel)
   - Pattern: Rainbow-colored concentric rings from center
   - Geographic metadata: WGS84, California area coordinates
   - Pixel scale: 0.01 degrees/pixel

2. **`gnuplot_rgb.dat`**: Binary matrix data for RGB visualization (256×256×3)

3. **`gnuplot_gray.dat`**: Binary matrix data for grayscale visualization (256×256)

4. **`gnuplot_commands.gp`**: Gnuplot script to generate visualizations

5. **`demo_rgb.png`**: RGB visualization showing the full color image

6. **`demo_gray.png`**: Grayscale visualization showing intensity

## How It Works

The examples demonstrate the key concept of the GeoTIFF VOL connector: **GeoTIFF files can be read as if they were HDF5 files**.

1. **Register the VOL connector**: The VOL connector is registered by name
2. **Open GeoTIFF as HDF5**: Use `H5Fopen()` to open a `.tif` file
3. **Access images as datasets**: Each TIFF directory appears as `image0`, `image1`, etc.
4. **Read with standard HDF5 API**: Use `H5Dread()` to read pixel data
5. **Process and visualize**: Export data in formats suitable for visualization tools

## Customization

You can modify the examples to:
- Change image size (modify `IMAGE_SIZE`)
- Use different color patterns
- Read your own GeoTIFF files
- Export data in different formats
- Create custom gnuplot visualizations

## Troubleshooting

If the demo fails to find the VOL connector:
```bash
export HDF5_PLUGIN_PATH=/path/to/vol-geotiff/build/src
```

If gnuplot is not found:
- Ensure gnuplot is installed and in your PATH
- On some systems, you may need to install additional gnuplot terminals for PNG output
