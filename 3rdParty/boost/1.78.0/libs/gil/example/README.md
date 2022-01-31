# Boost.GIL Examples

This directory contains

- examples of C++ programs using GIL
- configuration files for Boost.Build command line and CMake integration for popular IDEs.

We provide Boost.Build (`Jamfile`) and CMake (`CMakeLists.txt`)
configurations to build the examples.
See the [CONTRIBUTING.md](../CONTRIBUTING.md)
for details on how to run `b2` and `cmake` for Boost.GIL.

Each example is build as a separate executable.
Each executable generates its output as `out-<example_name>.jpg`.
For example, the `resize.cpp` example generates the image `out-resize.jpg`.

The following C++ examples are included:

1. `resize.cpp`
   Scales an image using bilinear or nearest-neighbour resampling.

2. `affine.cpp`
   Performs an arbitrary affine transformation on the image.

3. `convolution.cpp`
   Convolves the image with a Gaussian kernel.

4. `mandelbrot.cpp`
   Creates a synthetic image defining the Mandelbrot set.

5. `interleaved_ptr.cpp`
   Illustrates how to create a custom pixel reference and iterator.
   Creates a GIL image view over user-supplied data without the need to cast to GIL pixel type.

6. `x_gradient.cpp`
   Horizontal gradient, from the tutorial

7. `histogram.cpp`
   Algorithm to compute the histogram of an image

8. `packed_pixel.cpp`
   Illustrates how to create a custom pixel model - a pixel whose channel size is not divisible by bytes.

9. `dynamic_image.cpp`
   Example of using images whose type is instantiated at run time.
