Pixel
=====

.. contents::
   :local:
   :depth: 2

Overview
--------

A pixel is a set of channels defining the color at a given point in an
image. Conceptually, a pixel is little more than a color base whose
elements model ``ChannelConcept``. All properties of pixels inherit
from color bases: pixels may be *homogeneous* if all of their channels
have the same type; otherwise they are called *heterogeneous*. The
channels of a pixel may be addressed using semantic or physical
indexing, or by color; all color-base algorithms work on pixels as
well. Two pixels are *compatible* if their color spaces are the same
and their channels, paired semantically, are compatible. Note that
constness, memory organization and reference/value are ignored. For
example, an 8-bit RGB planar reference is compatible to a constant
8-bit BGR interleaved pixel value. Most pairwise pixel operations
(copy construction, assignment, equality, etc.) are only defined for
compatible pixels.

Pixels (as well as other GIL constructs built on pixels, such as
iterators, locators, views and images) must provide metafunctions to
access their color space, channel mapping, number of channels, and
(for homogeneous pixels) the channel type:

.. code-block:: cpp

  concept PixelBasedConcept<typename T>
  {
    typename color_space_type<T>;
        where Metafunction<color_space_type<T> >;
        where ColorSpaceConcept<color_space_type<T>::type>;
    typename channel_mapping_type<T>;
        where Metafunction<channel_mapping_type<T> >;
        where ChannelMappingConcept<channel_mapping_type<T>::type>;
    typename is_planar<T>;
        where Metafunction<is_planar<T> >;
        where SameType<is_planar<T>::type, bool>;
  };

  concept HomogeneousPixelBasedConcept<PixelBasedConcept T>
  {
    typename channel_type<T>;
        where Metafunction<channel_type<T> >;
        where ChannelConcept<channel_type<T>::type>;
  };

Pixels model the following concepts:

.. code-block:: cpp

  concept PixelConcept<typename P> : ColorBaseConcept<P>, PixelBasedConcept<P>
  {
    where is_pixel<P>::value==true;
    // where for each K [0..size<P>::value-1]:
    //      ChannelConcept<kth_element_type<K> >;

    typename value_type;       where PixelValueConcept<value_type>;
    typename reference;        where PixelConcept<reference>;
    typename const_reference;  where PixelConcept<const_reference>;
    static const bool P::is_mutable;

    template <PixelConcept P2> where { PixelConcept<P,P2> }
        P::P(P2);
    template <PixelConcept P2> where { PixelConcept<P,P2> }
        bool operator==(const P&, const P2&);
    template <PixelConcept P2> where { PixelConcept<P,P2> }
        bool operator!=(const P&, const P2&);
  };

  concept MutablePixelConcept<typename P> : PixelConcept<P>, MutableColorBaseConcept<P>
  {
    where is_mutable==true;
  };

  concept HomogeneousPixelConcept<PixelConcept P> : HomogeneousColorBaseConcept<P>, HomogeneousPixelBasedConcept<P>
  {
    P::template element_const_reference_type<P>::type operator[](P p, std::size_t i) const { return dynamic_at_c(P,i); }
  };

  concept MutableHomogeneousPixelConcept<MutablePixelConcept P> : MutableHomogeneousColorBaseConcept<P>
  {
    P::template element_reference_type<P>::type operator[](P p, std::size_t i) { return dynamic_at_c(p,i); }
  };

  concept PixelValueConcept<typename P> : PixelConcept<P>, Regular<P>
  {
    where SameType<value_type,P>;
  };

  concept PixelsCompatibleConcept<PixelConcept P1, PixelConcept P2> : ColorBasesCompatibleConcept<P1,P2>
  {
    // where for each K [0..size<P1>::value):
    //    ChannelsCompatibleConcept<kth_semantic_element_type<P1,K>::type, kth_semantic_element_type<P2,K>::type>;
  };

A pixel is *convertible* to a second pixel if it is possible to
approximate its color in the form of the second pixel. Conversion is
an explicit, non-symmetric and often lossy operation (due to both
channel and color space approximation). Convertibility requires
modeling the following concept:

.. code-block:: cpp

  template <PixelConcept SrcPixel, MutablePixelConcept DstPixel>
  concept PixelConvertibleConcept
  {
    void color_convert(const SrcPixel&, DstPixel&);
  };

The distinction between ``PixelConcept`` and ``PixelValueConcept`` is
analogous to that for channels and color bases - pixel reference proxies model
both, but only pixel values model the latter.

.. seealso::

  - `PixelBasedConcept<P> <reference/structboost_1_1gil_1_1_pixel_based_concept.html>`_
  - `PixelConcept<Pixel> <reference/structboost_1_1gil_1_1_pixel_concept.html>`_
  - `MutablePixelConcept<Pixel> <reference/structboost_1_1gil_1_1_mutable_pixel_concept.html>`_
  - `PixelValueConcept<Pixel> <reference/structboost_1_1gil_1_1_pixel_value_concept.html>`_
  - `HomogeneousPixelConcept<Pixel> <reference/structboost_1_1gil_1_1_homogeneous_pixel_based_concept.html>`_
  - `MutableHomogeneousPixelConcept<Pixel> <reference/structboost_1_1gil_1_1_mutable_homogeneous_pixel_concept.html>`_
  - `HomogeneousPixelValueConcept<Pixel> <reference/structboost_1_1gil_1_1_homogeneous_pixel_value_concept.html>`_
  - `PixelsCompatibleConcept<Pixel1, Pixel2> <reference/structboost_1_1gil_1_1_pixels_compatible_concept.html>`_
  - `PixelConvertibleConcept<SrcPixel, DstPixel> <reference/structboost_1_1gil_1_1_pixel_convertible_concept.html>`_

Models
------

The most commonly used pixel is a homogeneous pixel whose values are
together in memory. For this purpose GIL provides the struct
``pixel``, templated over the channel value and layout:

.. code-block:: cpp

  // models HomogeneousPixelValueConcept
  template <typename ChannelValue, typename Layout> struct pixel;

  // Those typedefs are already provided by GIL
  typedef pixel<bits8, rgb_layout_t> rgb8_pixel_t;
  typedef pixel<bits8, bgr_layout_t> bgr8_pixel_t;

  bgr8_pixel_t bgr8(255,0,0);     // pixels can be initialized with the channels directly
  rgb8_pixel_t rgb8(bgr8);        // compatible pixels can also be copy-constructed

  rgb8 = bgr8;            // assignment and equality is defined between compatible pixels
  assert(rgb8 == bgr8);   // assignment and equality operate on the semantic channels

  // The first physical channels of the two pixels are different
  assert(at_c<0>(rgb8) != at_c<0>(bgr8));
  assert(dynamic_at_c(bgr8,0) != dynamic_at_c(rgb8,0));
  assert(rgb8[0] != bgr8[0]); // same as above (but operator[] is defined for pixels only)

Planar pixels have their channels distributed in memory. While they share the
same value type (``pixel``) with interleaved pixels, their reference type is a
proxy class containing references to each of the channels.
This is implemented with the struct ``planar_pixel_reference``:

.. code-block:: cpp

  // models HomogeneousPixel
  template <typename ChannelReference, typename ColorSpace> struct planar_pixel_reference;

  // Define the type of a mutable and read-only reference. (These typedefs are already provided by GIL)
  typedef planar_pixel_reference<      bits8&,rgb_t> rgb8_planar_ref_t;
  typedef planar_pixel_reference<const bits8&,rgb_t> rgb8c_planar_ref_t;

Note that, unlike the ``pixel`` struct, planar pixel references are templated
over the color space, not over the pixel layout. They always use a canonical
channel ordering. Ordering of their elements is unnecessary because their
elements are references to the channels.

Sometimes the channels of a pixel may not be byte-aligned. For example an RGB
pixel in '5-5-6' format is a 16-bit pixel whose red, green and blue channels
occupy bits [0..4],[5..9] and [10..15] respectively. GIL provides a model for
such packed pixel formats:

.. code-block:: cpp

  // define an rgb565 pixel
  typedef packed_pixel_type<uint16_t, mpl::vector3_c<unsigned,5,6,5>, rgb_layout_t>::type rgb565_pixel_t;

  function_requires<PixelValueConcept<rgb565_pixel_t> >();
  static_assert(sizeof(rgb565_pixel_t) == 2, "");

  // define a bgr556 pixel
  typedef packed_pixel_type<uint16_t, mpl::vector3_c<unsigned,5,6,5>, bgr_layout_t>::type bgr556_pixel_t;

  function_requires<PixelValueConcept<bgr556_pixel_t> >();

  // rgb565 is compatible with bgr556.
  function_requires<PixelsCompatibleConcept<rgb565_pixel_t,bgr556_pixel_t> >();

In some cases, the pixel itself may not be byte aligned. For example,
consider an RGB pixel in '2-3-2' format. Its size is 7 bits. GIL
refers to such pixels, pixel iterators and images as
"bit-aligned". Bit-aligned pixels (and images) are more complex than
packed ones. Since packed pixels are byte-aligned, we can use a C++
reference as the reference type to a packed pixel, and a C pointer as
an x_iterator over a row of packed pixels. For bit-aligned constructs
we need a special reference proxy class (bit_aligned_pixel_reference)
and iterator class (bit_aligned_pixel_iterator). The value type of
bit-aligned pixels is a packed_pixel. Here is how to use bit_aligned
pixels and pixel iterators:

.. code-block:: cpp

  // Mutable reference to a BGR232 pixel
  typedef const bit_aligned_pixel_reference<unsigned char, mpl::vector3_c<unsigned,2,3,2>, bgr_layout_t, true>  bgr232_ref_t;

  // A mutable iterator over BGR232 pixels
  typedef bit_aligned_pixel_iterator<bgr232_ref_t> bgr232_ptr_t;

  // BGR232 pixel value. It is a packed_pixel of size 1 byte. (The last bit is unused)
  typedef std::iterator_traits<bgr232_ptr_t>::value_type bgr232_pixel_t;
  static_assert(sizeof(bgr232_pixel_t) == 1, "");

  bgr232_pixel_t red(0,0,3); // = 0RRGGGBB, = 01100000 = 0x60

  // a buffer of 7 bytes fits exactly 8 BGR232 pixels.
  unsigned char pix_buffer[7];
  std::fill(pix_buffer,pix_buffer+7,0);

  // Fill the 8 pixels with red
  bgr232_ptr_t pix_it(&pix_buffer[0],0);  // start at bit 0 of the first pixel
  for (int i=0; i<8; ++i)
  {
    *pix_it++ = red;
  }
  // Result: 0x60 0x30 0x11 0x0C 0x06 0x83 0xC1

Algorithms
----------

Since pixels model ``ColorBaseConcept`` and ``PixelBasedConcept`` all
algorithms and metafunctions of color bases can work with them as well:

.. code-block:: cpp

  // This is how to access the first semantic channel (red)
  assert(semantic_at_c<0>(rgb8) == semantic_at_c<0>(bgr8));

  // This is how to access the red channel by name
  assert(get_color<red_t>(rgb8) == get_color<red_t>(bgr8));

  // This is another way of doing it (some compilers don't like the first one)
  assert(get_color(rgb8,red_t()) == get_color(bgr8,red_t()));

  // This is how to use the PixelBasedConcept metafunctions
  BOOST_MPL_ASSERT(num_channels<rgb8_pixel_t>::value == 3);
  BOOST_MPL_ASSERT((is_same<channel_type<rgb8_pixel_t>::type, bits8>));
  BOOST_MPL_ASSERT((is_same<color_space_type<bgr8_pixel_t>::type, rgb_t> ));
  BOOST_MPL_ASSERT((is_same<channel_mapping_type<bgr8_pixel_t>::type, mpl::vector3_c<int,2,1,0> > ));

  // Pixels contain just the three channels and nothing extra
  BOOST_MPL_ASSERT(sizeof(rgb8_pixel_t)==3);

  rgb8_planar_ref_t ref(bgr8);    // copy construction is allowed from a compatible mutable pixel type

  get_color<red_t>(ref) = 10;     // assignment is ok because the reference is mutable
  assert(get_color<red_t>(bgr8)==10);  // references modify the value they are bound to

  // Create a zero packed pixel and a full regular unpacked pixel.
  rgb565_pixel_t r565;
  rgb8_pixel_t rgb_full(255,255,255);

  // Convert all channels of the unpacked pixel to the packed one & assert the packed one is full
  get_color(r565,red_t())   = channel_convert<rgb565_channel0_t>(get_color(rgb_full,red_t()));
  get_color(r565,green_t()) = channel_convert<rgb565_channel1_t>(get_color(rgb_full,green_t()));
  get_color(r565,blue_t())  = channel_convert<rgb565_channel2_t>(get_color(rgb_full,blue_t()));
  assert(r565 == rgb565_pixel_t((uint16_t)65535));

GIL also provides the ``color_convert`` algorithm to convert between pixels of
different color spaces and channel types:

.. code-block:: cpp

  rgb8_pixel_t red_in_rgb8(255,0,0);
  cmyk16_pixel_t red_in_cmyk16;
  color_convert(red_in_rgb8,red_in_cmyk16);
