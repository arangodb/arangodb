Boost Generic Image Library
===========================

The Generic Image Library (GIL) is a C++11 library that abstracts image
representations from algorithms and allows writing code that can work on
a variety of images with performance similar to hand-writing for a specific
image type.

Quickstart
----------

.. toctree::
   :maxdepth: 1

   installation
   tutorial/video
   tutorial/histogram
   tutorial/gradient
   naming

Core Library Documentation
--------------------------

.. toctree::
   :maxdepth: 2

   design/index
   image_processing/index
   API Reference <./reference/index.html#://>

Extensions Documentation
------------------------

.. toctree::
   :maxdepth: 2

   io
   toolbox
   numeric

Examples
--------

* :download:`x_gradient.cpp <../example/x_gradient.cpp>`:
  Writing an algorithm that operates on generic images
* :download:`dynamic_image.cpp <../example/dynamic_image.cpp>`:
  Using images whose properties (color space, channel type) are specified
  at run time
* :download:`histogram.cpp <../example/histogram.cpp>`: Creating a histogram
* :download:`interleaved_ptr.cpp <../example/interleaved_ptr.cpp>`,
  :download:`interleaved_ptr.hpp <../example/interleaved_ptr.hpp>`,
  :download:`interleaved_ref.hpp <../example/interleaved_ref.hpp>`:
  Creating your own pixel reference and pixel iterator
* :download:`mandelbrot.cpp <../example/mandelbrot.cpp>`:
  Creating a synthetic image defined by a function
* :download:`packed_pixel.cpp <../example/packed_pixel.cpp>`:
  Defining bitmasks and images whose channels or pixels are not byte-aligned
* :download:`resize.cpp <../example/resize.cpp>`:
  Rescaling an image using bilinear sampling (requires the optional
  Numeric extension)
* :download:`affine.cpp <../example/affine.cpp>`:
  Applying an affine transformation to an image (requires the optional
  Numeric extension)
* :download:`convolution.cpp <../example/convolution.cpp>`:
  Blurring images (requires the optional Numeric extension)
