Image View
==========

.. contents::
   :local:
   :depth: 2

Overview
--------

An image view is a generalization of STL range concept to multiple dimensions.
Similar to ranges (and iterators), image views are shallow, don't own the
underlying data and don't propagate their constness over the data.
For example, a constant image view cannot be resized, but may allow modifying
the pixels. For pixel-immutable operations, use constant-value image view
(also called non-mutable image view). Most general N-dimensional views satisfy
the following concept:

.. code-block:: cpp

  concept RandomAccessNDImageViewConcept<Regular View>
  {
    typename value_type;      // for pixel-based views, the pixel type
    typename reference;       // result of dereferencing
    typename difference_type; // result of operator-(iterator,iterator) (1-dimensional!)
    typename const_t;  where RandomAccessNDImageViewConcept<View>; // same as View, but over immutable values
    typename point_t;  where PointNDConcept<point_t>; // N-dimensional point
    typename locator;  where RandomAccessNDLocatorConcept<locator>; // N-dimensional locator.
    typename iterator; where RandomAccessTraversalConcept<iterator>; // 1-dimensional iterator over all values
    typename reverse_iterator; where RandomAccessTraversalConcept<reverse_iterator>;
    typename size_type;       // the return value of size()

    // Equivalent to RandomAccessNDLocatorConcept::axis
    template <size_t D> struct axis {
        typename coord_t = point_t::axis<D>::coord_t;
        typename iterator; where RandomAccessTraversalConcept<iterator>;   // iterator along D-th axis.
        where SameType<coord_t, iterator::difference_type>;
        where SameType<iterator::value_type,value_type>;
    };

    // Defines the type of a view similar to this type, except it invokes Deref upon dereferencing
    template <PixelDereferenceAdaptorConcept Deref> struct add_deref {
        typename type;        where RandomAccessNDImageViewConcept<type>;
        static type make(const View& v, const Deref& deref);
    };

    static const size_t num_dimensions = point_t::num_dimensions;

    // Create from a locator at the top-left corner and dimensions
    View::View(const locator&, const point_type&);

    size_type        View::size()       const; // total number of elements
    reference        operator[](View, const difference_type&) const; // 1-dimensional reference
    iterator         View::begin()      const;
    iterator         View::end()        const;
    reverse_iterator View::rbegin()     const;
    reverse_iterator View::rend()       const;
    iterator         View::at(const point_t&);
    point_t          View::dimensions() const; // number of elements along each dimension
    bool             View::is_1d_traversable() const;   // Does an iterator over the first dimension visit each value?

    // iterator along a given dimension starting at a given point
    template <size_t D> View::axis<D>::iterator View::axis_iterator(const point_t&) const;

    reference operator()(View,const point_t&) const;
  };

  concept MutableRandomAccessNDImageViewConcept<RandomAccessNDImageViewConcept View>
  {
    where Mutable<reference>;
  };

Two-dimensional image views have the following extra requirements:

.. code-block:: cpp

  concept RandomAccess2DImageViewConcept<RandomAccessNDImageViewConcept View>
  {
    where num_dimensions==2;

    typename x_iterator = axis<0>::iterator;
    typename y_iterator = axis<1>::iterator;
    typename x_coord_t  = axis<0>::coord_t;
    typename y_coord_t  = axis<1>::coord_t;
    typename xy_locator = locator;

    x_coord_t View::width()  const;
    y_coord_t View::height() const;

    // X-navigation
    x_iterator View::x_at(const point_t&) const;
    x_iterator View::row_begin(y_coord_t) const;
    x_iterator View::row_end  (y_coord_t) const;

    // Y-navigation
    y_iterator View::y_at(const point_t&) const;
    y_iterator View::col_begin(x_coord_t) const;
    y_iterator View::col_end  (x_coord_t) const;

    // navigating in 2D
    xy_locator View::xy_at(const point_t&) const;

    // (x,y) versions of all methods taking point_t
    View::View(x_coord_t,y_coord_t,const locator&);
    iterator View::at(x_coord_t,y_coord_t) const;
    reference operator()(View,x_coord_t,y_coord_t) const;
    xy_locator View::xy_at(x_coord_t,y_coord_t) const;
    x_iterator View::x_at(x_coord_t,y_coord_t) const;
    y_iterator View::y_at(x_coord_t,y_coord_t) const;
  };

  concept MutableRandomAccess2DImageViewConcept<RandomAccess2DImageViewConcept View>
    : MutableRandomAccessNDImageViewConcept<View> {};

Image views that GIL typically uses operate on value types that model
``PixelValueConcept`` and have some additional requirements:

.. code-block:: cpp

  concept ImageViewConcept<RandomAccess2DImageViewConcept View>
  {
    where PixelValueConcept<value_type>;
    where PixelIteratorConcept<x_iterator>;
    where PixelIteratorConcept<y_iterator>;
    where x_coord_t == y_coord_t;

    typename coord_t = x_coord_t;

    std::size_t View::num_channels() const;
  };


  concept MutableImageViewConcept<ImageViewConcept View>
    : MutableRandomAccess2DImageViewConcept<View>
  {};

Two image views are compatible if they have compatible pixels and the same
number of dimensions:

.. code-block:: cpp

  concept ViewsCompatibleConcept<ImageViewConcept V1, ImageViewConcept V2>
  {
    where PixelsCompatibleConcept<V1::value_type, V2::value_type>;
    where V1::num_dimensions == V2::num_dimensions;
  };

Compatible views must also have the same dimensions (i.e. the same width and
height). Many algorithms taking multiple views require that they be pairwise
compatible.

.. seealso::

   - `RandomAccessNDImageViewConcept<View> <reference/structboost_1_1gil_1_1_random_access_n_d_image_view_concept.html>`_
   - `MutableRandomAccessNDImageViewConcept<View> <reference/structboost_1_1gil_1_1_mutable_random_access_n_d_image_view_concept.html>`_
   - `RandomAccess2DImageViewConcept<View> <reference/structboost_1_1gil_1_1_random_access2_d_image_view_concept.html>`_
   - `MutableRandomAccess2DImageViewConcept<View> <reference/structboost_1_1gil_1_1_mutable_random_access2_d_image_view_concept.html>`_
   - `ImageViewConcept<View> <reference/structboost_1_1gil_1_1_image_view_concept.html>`_
   - `MutableImageViewConcept<View> <reference/structboost_1_1gil_1_1_mutable_image_view_concept.html>`_
   - `ViewsCompatibleConcept<View1,View2> <reference/structboost_1_1gil_1_1_views_compatible_concept.html>`_

Models
------

GIL provides a model for ``ImageViewConcept`` called ``image_view``. It is
templated over a model of ``PixelLocatorConcept``. (If instantiated with a
model of ``MutablePixelLocatorConcept``, it models
``MutableImageViewConcept``). Synopsis:

.. code-block:: cpp

  // Locator models PixelLocatorConcept, could be MutablePixelLocatorConcept
  template <typename Locator>
  class image_view
  {
  public:
    typedef Locator xy_locator;
    typedef iterator_from_2d<Locator> iterator;
    ...
  private:
    xy_locator _pixels;     // 2D pixel locator at the top left corner of the image view range
    point_t    _dimensions; // width and height
  };

Image views are lightweight objects. A regular interleaved view is typically
16 bytes long - two integers for the width and height (inside dimensions) one
for the number of bytes between adjacent rows (inside the locator) and one
pointer to the beginning of the pixel block.

Algorithms
----------

GIL provides algorithms constructing views from raw data or other views.

Creating Views from Raw Pixels
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Standard image views can be constructed from raw data of any supported color
space, bit depth, channel ordering or planar vs. interleaved structure.
Interleaved views are constructed using ``interleaved_view``, supplying the
image dimensions, number of bytes per row, and a pointer to the first pixel:

.. code-block:: cpp

  // Iterator models pixel iterator (e.g. rgb8_ptr_t or rgb8c_ptr_t)
  template <typename Iterator>
  image_view<...> interleaved_view(ptrdiff_t width, ptrdiff_t height, Iterator pixels, ptrdiff_t rowsize)

Planar views are defined for every color space and take each plane separately.
Here is the RGB one:

.. code-block:: cpp

  // Iterator models channel iterator (e.g. bits8* or bits8 const*)
  template <typename Iterator>
  image_view<...> planar_rgb_view(
      ptrdiff_t width, ptrdiff_t height,
      IC r, IC g, IC b, ptrdiff_t rowsize);

Note that the supplied pixel/channel iterators could be constant (read-only),
in which case the returned view is a constant-value (immutable) view.

Creating Image Views from Other Image Views
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

It is possible to construct one image view from another by changing some
policy of how image data is interpreted. The result could be a view whose type
is derived from the type of the source. GIL uses the following metafunctions
to get the derived types:

.. code-block:: cpp

  // Some result view types
  template <typename View>
  struct dynamic_xy_step_type : public dynamic_y_step_type<typename dynamic_x_step_type<View>::type> {};

  template <typename View>
  struct dynamic_xy_step_transposed_type : public dynamic_xy_step_type<typename transposed_type<View>::type> {};

  // color and bit depth converted view to match pixel type P
  template <typename SrcView, // Models ImageViewConcept
          typename DstP,    // Models PixelConcept
          typename ColorConverter=gil::default_color_converter>
  struct color_converted_view_type
  {
    typedef ... type;     // image view adaptor with value type DstP, over SrcView
  };

  // single-channel view of the N-th channel of a given view
  template <typename SrcView>
  struct nth_channel_view_type
  {
    typedef ... type;
  };

GIL Provides the following view transformations:

.. code-block:: cpp

  // flipped upside-down, left-to-right, transposed view
  template <typename View> typename dynamic_y_step_type<View>::type             flipped_up_down_view(const View& src);
  template <typename View> typename dynamic_x_step_type<View>::type             flipped_left_right_view(const View& src);
  template <typename View> typename dynamic_xy_step_transposed_type<View>::type transposed_view(const View& src);

  // rotations
  template <typename View> typename dynamic_xy_step_type<View>::type            rotated180_view(const View& src);
  template <typename View> typename dynamic_xy_step_transposed_type<View>::type rotated90cw_view(const View& src);
  template <typename View> typename dynamic_xy_step_transposed_type<View>::type rotated90ccw_view(const View& src);

  // view of an axis-aligned rectangular area within an image
  template <typename View> View                                                 subimage_view(const View& src,
             const View::point_t& top_left, const View::point_t& dimensions);

  // subsampled view (skipping pixels in X and Y)
  template <typename View> typename dynamic_xy_step_type<View>::type            subsampled_view(const View& src,
             const View::point_t& step);

  template <typename View, typename P>
  color_converted_view_type<View,P>::type                                       color_converted_view(const View& src);
  template <typename View, typename P, typename CCV> // with a custom color converter
  color_converted_view_type<View,P,CCV>::type                                   color_converted_view(const View& src);

  template <typename View>
  nth_channel_view_type<View>::view_t                                           nth_channel_view(const View& view, int n);

The implementations of most of these view factory methods are straightforward.
Here is, for example, how the flip views are implemented. The flip upside-down
view creates a view whose first pixel is the bottom left pixel of the original
view and whose y-step is the negated step of the source.

.. code-block:: cpp

  template <typename View>
  typename dynamic_y_step_type<View>::type flipped_up_down_view(const View& src)
  {
    gil_function_requires<ImageViewConcept<View> >();
    typedef typename dynamic_y_step_type<View>::type RView;
    return RView(src.dimensions(),typename RView::xy_locator(src.xy_at(0,src.height()-1),-1));
  }

The call to ``gil_function_requires`` ensures (at compile time) that the
template parameter is a valid model of ``ImageViewConcept``. Using it
generates easier to track compile errors, creates no extra code and has no
run-time performance impact. We are using the ``boost::concept_check library``,
but wrapping it in ``gil_function_requires``, which performs the check if the
``BOOST_GIL_USE_CONCEPT_CHECK`` is set. It is unset by default, because there
is a significant increase in compile time when using concept checks. We will
skip ``gil_function_requires`` in the code examples in this guide for the sake
of succinctness.

Image views can be freely composed (see section :doc:`metafunctions` for
explanation of the typedefs ``rgb16_image_t`` and ``gray16_step_view_t)``:

.. code-block:: cpp

  rgb16_image_t img(100,100);    // an RGB interleaved image

  // grayscale view over the green (index 1) channel of img
  gray16_step_view_t green=nth_channel_view(view(img),1);

  // 50x50 view of the green channel of img, upside down and taking every other pixel in X and in Y
  gray16_step_view_t ud_fud=flipped_up_down_view(subsampled_view(green,2,2));

As previously stated, image views are fast, constant-time, shallow views over
the pixel data. The above code does not copy any pixels; it operates on the
pixel data allocated when ``img`` was created.

STL-Style Algorithms on Image Views
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Image views provide 1D iteration of their pixels via ``begin()`` and ``end()``
methods, which makes it possible to use STL algorithms with them. However,
using nested loops over X and Y is in many cases more efficient.
The algorithms in this section resemble STL algorithms, but they abstract away
the nested loops and take views (as opposed to ranges) as input.

.. code-block:: cpp

  // Equivalents of std::copy and std::uninitialized_copy
  // where ImageViewConcept<V1>, MutableImageViewConcept<V2>, ViewsCompatibleConcept<V1,V2>
  template <typename V1, typename V2>
  void copy_pixels(const V1& src, const V2& dst);
  template <typename V1, typename V2>
  void uninitialized_copy_pixels(const V1& src, const V2& dst);

  // Equivalents of std::fill and std::uninitialized_fill
  // where MutableImageViewConcept<V>, PixelConcept<Value>, PixelsCompatibleConcept<Value,V::value_type>
  template <typename V, typename Value>
  void fill_pixels(const V& dst, const Value& val);
  template <typename V, typename Value>
  void uninitialized_fill_pixels(const V& dst, const Value& val);

  // Equivalent of std::for_each
  // where ImageViewConcept<V>, boost::UnaryFunctionConcept<F>
  // where PixelsCompatibleConcept<V::reference, F::argument_type>
  template <typename V, typename F>
  F for_each_pixel(const V& view, F fun);
  template <typename V, typename F>
  F for_each_pixel_position(const V& view, F fun);

  // Equivalent of std::generate
  // where MutableImageViewConcept<V>, boost::UnaryFunctionConcept<F>
  // where PixelsCompatibleConcept<V::reference, F::argument_type>
  template <typename V, typename F>
  void generate_pixels(const V& dst, F fun);

  // Equivalent of std::transform with one source
  // where ImageViewConcept<V1>, MutableImageViewConcept<V2>
  // where boost::UnaryFunctionConcept<F>
  // where PixelsCompatibleConcept<V1::const_reference, F::argument_type>
  // where PixelsCompatibleConcept<F::result_type, V2::reference>
  template <typename V1, typename V2, typename F>
  F transform_pixels(const V1& src, const V2& dst, F fun);
  template <typename V1, typename V2, typename F>
  F transform_pixel_positions(const V1& src, const V2& dst, F fun);

  // Equivalent of std::transform with two sources
  // where ImageViewConcept<V1>, ImageViewConcept<V2>, MutableImageViewConcept<V3>
  // where boost::BinaryFunctionConcept<F>
  // where PixelsCompatibleConcept<V1::const_reference, F::first_argument_type>
  // where PixelsCompatibleConcept<V2::const_reference, F::second_argument_type>
  // where PixelsCompatibleConcept<F::result_type, V3::reference>
  template <typename V1, typename V2, typename V3, typename F>
  F transform_pixels(const V1& src1, const V2& src2, const V3& dst, F fun);
  template <typename V1, typename V2, typename V3, typename F>
  F transform_pixel_positions(const V1& src1, const V2& src2, const V3& dst, F fun);

  // Copies a view into another, color converting the pixels if needed, with the default or user-defined color converter
  // where ImageViewConcept<V1>, MutableImageViewConcept<V2>
  // V1::value_type must be convertible to V2::value_type.
  template <typename V1, typename V2>
  void copy_and_convert_pixels(const V1& src, const V2& dst);
  template <typename V1, typename V2, typename ColorConverter>
  void copy_and_convert_pixels(const V1& src, const V2& dst, ColorConverter ccv);

  // Equivalent of std::equal
  // where ImageViewConcept<V1>, ImageViewConcept<V2>, ViewsCompatibleConcept<V1,V2>
  template <typename V1, typename V2>
  bool equal_pixels(const V1& view1, const V2& view2);

Algorithms that take multiple views require that they have the same
dimensions. ``for_each_pixel_position`` and ``transform_pixel_positions`` pass
pixel locators, as opposed to pixel references, to their function objects.
This allows for writing algorithms that use pixel neighbours, as the tutorial
demonstrates.

Most of these algorithms check whether the image views are 1D-traversable.
A 1D-traversable image view has no gaps at the end of the rows.
In other words, if an x_iterator of that view is advanced past the last pixel
in a row it will move to the first pixel of the next row. When image views are
1D-traversable, the algorithms use a single loop and run more efficiently.
If one or more of the input views are not 1D-traversable, the algorithms
fall-back to an X-loop nested inside a Y-loop.

The algorithms typically delegate the work to their corresponding STL
algorithms. For example, ``copy_pixels`` calls ``std::copy`` either for each
row, or, when the images are 1D-traversable, once for all pixels.

In addition, overloads are sometimes provided for the STL algorithms.
For example, ``std::copy`` for planar iterators is overloaded to perform
``std::copy`` for each of the planes. ``std::copy`` over bitwise-copyable
pixels results in ``std::copy`` over unsigned char, which STL
implements via ``memmove``.

As a result ``copy_pixels`` may result in a single call to ``memmove`` for
interleaved 1D-traversable views, or one per each plane of planar
1D-traversable views, or one per each row of interleaved non-1D-traversable
images, etc.

GIL also provides some beta-versions of image processing algorithms, such as
resampling and convolution in a numerics extension available on
http://stlab.adobe.com/gil/download.html. This code is in early stage of
development and is not optimized for speed
