# GeoTIFF VOL Connector Examples

This directory contains C examples demonstrating how to use the GeoTIFF VOL connector to read GeoTIFF files through the HDF5 API and visualize them using gnuplot. These examples are built as part of the normal build process - see `doc/usage.md`.

## Simple Demo

This demo shows basic VOL connector operations:
- Creates a test GeoTIFF
- Opens it via the VOL connector
- Displays dimensions and metadata
- Reads and prints pixel data

To execute it, run the `simple_demo` executable from the build directory.

## gnuplot Visualization Demo

The demo will:
- Create `demo_rings_gnuplot.tif` with colorful concentric rings
- Read it back through the HDF5 VOL connector
- Generate gnuplot data files (`gnuplot_rgb.dat`, `gnuplot_gray.dat`)
- Create a gnuplot script (`gnuplot_commands.gp`)
- Invoke gnuplot to generate visualizations (`demo_rgb.png`, `demo_gray.png`)

From the build directory:

To execute it, run the `gnuplot_demo` executable from the build directory.

## Troubleshooting

If the demo fails to find the VOL connector:
```bash
export HDF5_PLUGIN_PATH=/path/to/vol-geotiff/build/src
```
