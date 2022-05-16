Installation
============

The latest version of GIL can be downloaded from https://github.com/boostorg/gil.

The GIL is a header-only library. Meaning, it consists of header files only,
it does not require Boost to be built and it does not require any libraries
to link against.

.. note::

   The exception to dependencies-free rule of GIL is the I/O extension
   which requires client libraries implementing popular image formats
   like libpng, libjpeg, etc.

In order to use GIL, including ``boost/gil.hpp`` and telling your compiler
where to find Boost and GIL headers should be sufficient for most projects.
