Naming Conventions
==================

Description of established naming conventions used in source code of GIL,
tests and examples.

Concrete Types
--------------

Concrete (non-generic) GIL types follow this naming convention::

  ColorSpace + BitDepth + [f | s]+ [c] + [_planar] + [_step] + ClassType + _t

where:

- ``ColorSpace`` indicates layout and ordering of components.
  For example, ``rgb``, ``bgr``, ``cmyk``, ``rgba``.

- ``BitDepth`` indicates the bit depth of the color channel.
  For example, ``8``,``16``,``32``.

- By default, type of channel is unsigned integral.
  The ``s`` tag indicates signed integral.
  The ``f`` tag indicates a floating point type, which is always signed.

- By default, objects operate on mutable pixels.
  The ``c`` tag indicates object operating over immutable pixels.

- ``_planar`` indicates planar organization (as opposed to interleaved).

- ``_step`` indicates special image views, locators and iterators which
  traverse the data in non-trivial way. For example, backwards or every other
  pixel.

- ``ClassType`` is ``_image`` (image), ``_view`` (image view), ``_loc`` (pixel
  2D locator) ``_ptr`` (pixel iterator), ``_ref`` (pixel reference),
  ``_pixel`` (pixel value).

- ``_t`` suffix indicaes it is a name of a type.

For example:

.. code-block:: cpp

  bgr8_image_t             a;    // 8-bit interleaved BGR image
  cmyk16_pixel_t           b;    // 16-bit CMYK pixel value;
  cmyk16c_planar_ref_t     c(b); // const reference to a 16-bit planar CMYK pixel.
  rgb32f_planar_step_ptr_t d;    // step pointer to a 32-bit planar RGB pixel.
