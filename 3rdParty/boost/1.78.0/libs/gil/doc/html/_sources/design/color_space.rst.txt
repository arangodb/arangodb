Color Space and Layout
======================

.. contents::
   :local:
   :depth: 2

Overview
--------

A color space captures the set and interpretation of channels comprising a
pixel. In Boost.GIL, color space is defined as an MPL random access sequence
containing the types of all elements in the color space.

Two color spaces are considered *compatible* if they are equal (i.e. have the
same set of colors in the same order).

.. seealso::

  - `ColorSpaceConcept<ColorSpace> <reference/structboost_1_1gil_1_1_color_space_concept.html>`_
  - `ColorSpacesCompatibleConcept<ColorSpace1,ColorSpace2> <reference/structboost_1_1gil_1_1_color_spaces_compatible_concept.html>`_
  - `ChannelMappingConcept<Mapping> <reference/structboost_1_1gil_1_1_channel_mapping_concept.html>`_

Models
------

GIL currently provides the following color spaces:

- ``gray_t``
- ``rgb_t``
- ``rgba_t``
- ``cmyk_t``

It also provides unnamed N-channel color spaces of two to five channels:

- ``devicen_t<2>``
- ``devicen_t<3>``
- ``devicen_t<4>``
- ``devicen_t<5>``

Besides the standard layouts, it also provides:

- ``bgr_layout_t``
- ``bgra_layout_t``
- ``abgr_layout_t``
- ``argb_layout_t``

As an example, here is how GIL defines the RGBA color space::

.. code-block:: cpp

  struct red_t {};
  struct green_t {};
  struct blue_t {};
  struct alpha_t {};
  rgba_t = using mpl::vector4<red_t, green_t, blue_t, alpha_t>;

The ordering of the channels in the color space definition specifies their
semantic order. For example, ``red_t`` is the first semantic channel of
``rgba_t``. While there is a unique semantic ordering of the channels in a
color space, channels may vary in their physical ordering in memory

The mapping of channels is specified by ``ChannelMappingConcept``, which is
an MPL random access sequence of integral types.
A color space and its associated mapping are often used together.

Thus they are grouped in GIL's layout:

.. code-block:: cpp

  template
  <
      typename ColorSpace,
      typename ChannelMapping = mpl::range_c<int, 0, mpl::size<ColorSpace>::value>
  >
  struct layout
  {
    using color_space_t = ColorSpace;
    using channel_mapping_t = ChannelMapping;
  };

Here is how to create layouts for the RGBA color space:

.. code-block:: cpp

  using rgba_layout_t = layout<rgba_t>; // default ordering is 0,1,2,3...
  using bgra_layout_t = layout<rgba_t, mpl::vector4_c<int,2,1,0,3>>;
  using argb_layout_t = layout<rgba_t, mpl::vector4_c<int,1,2,3,0>>;
  using abgr_layout_t = layout<rgba_t, mpl::vector4_c<int,3,2,1,0>>;
