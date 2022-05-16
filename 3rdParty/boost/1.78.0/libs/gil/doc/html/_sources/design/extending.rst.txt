Extending
=========

.. contents::
   :local:
   :depth: 2

Overview
--------

You can define your own pixel iterators, locators, image views,
images, channel types, color spaces and algorithms. You can make
virtual images that live on the disk, inside a jpeg file, somewhere on
the internet, or even fully-synthetic images such as the Mandelbrot
set. As long as they properly model the corresponding concepts, they
will work with any existing GIL code. Most such extensions require no
changes to the library and can thus be supplied in another module.

Defining new color spaces
-------------------------

Each color space is in a separate file. To add a new color space, just
copy one of the existing ones (like rgb.hpp) and change it
accordingly. If you want color conversion support, you will have to
provide methods to convert between it and the existing color spaces
(see color_convert.h). For convenience you may want to provide useful
typedefs for pixels, pointers, references and images with the new
color space (see typedefs.h).

Defining new channel types
--------------------------

Most of the time you don't need to do anything special to use a new
channel type. You can just use it:

.. code-block:: cpp

  typedef pixel<double,rgb_layout_t>   rgb64_pixel_t;    // 64 bit RGB pixel
  typedef rgb64_pixel*                 rgb64_pixel_ptr_t;// pointer to 64-bit interleaved data
  typedef image_type<double,rgb_layout_t>::type rgb64_image_t;    // 64-bit interleaved image

If you want to use your own channel class, you will need to provide a
specialization of ``channel_traits`` for it (see channel.hpp). If you
want to do conversion between your and existing channel types, you
will need to provide an overload of ``channel_convert``.

Overloading color conversion
----------------------------

Suppose you want to provide your own color conversion. For example,
you may want to implement higher quality color conversion using color
profiles. Typically you may want to redefine color conversion only in
some instances and default to GIL's color conversion in all other
cases. Here is, for example, how to overload color conversion so that
color conversion to gray inverts the result but everything else
remains the same:

.. code-block:: cpp

  // make the default use GIL's default
  template <typename SrcColorSpace, typename DstColorSpace>
  struct my_color_converter_impl
  : public default_color_converter_impl<SrcColorSpace,DstColorSpace> {};

  // provide specializations only for cases you care about
  // (in this case, if the destination is grayscale, invert it)
  template <typename SrcColorSpace>
  struct my_color_converter_impl<SrcColorSpace,gray_t>
  {
    template <typename SrcP, typename DstP>  // Model PixelConcept
    void operator()(const SrcP& src, DstP& dst) const
    {
        default_color_converter_impl<SrcColorSpace,gray_t>()(src,dst);
        get_color(dst,gray_color_t())=channel_invert(get_color(dst,gray_color_t()));
    }
  };

  // create a color converter object that dispatches to your own implementation
  struct my_color_converter
  {
    template <typename SrcP, typename DstP>  // Model PixelConcept
    void operator()(const SrcP& src,DstP& dst) const
    {
        typedef typename color_space_type<SrcP>::type SrcColorSpace;
        typedef typename color_space_type<DstP>::type DstColorSpace;
        my_color_converter_impl<SrcColorSpace,DstColorSpace>()(src,dst);
    }
  };

GIL color conversion functions take the color converter as an
optional parameter. You can pass your own color converter:

.. code-block:: cpp

  color_converted_view<gray8_pixel_t>(img_view,my_color_converter());

Defining new image views
------------------------

You can provide your own pixel iterators, locators and views,
overriding either the mechanism for getting from one pixel to the next
or doing an arbitrary pixel transformation on dereference. For
example, let's look at the implementation of ``color_converted_view``
(an image factory method that, given any image view, returns a new,
otherwise identical view, except that color conversion is performed on
pixel access). First we need to define a model of
``PixelDereferenceAdaptorConcept``; a function object that will be
called when we dereference a pixel iterator. It will call
``color_convert`` to convert to the destination pixel type:

.. code-block:: cpp

  template <typename SrcConstRefP,  // const reference to the source pixel
          typename DstP>          // Destination pixel value (models PixelValueConcept)
  class color_convert_deref_fn
  {
  public:
    typedef color_convert_deref_fn const_t;
    typedef DstP                value_type;
    typedef value_type          reference;      // read-only dereferencing
    typedef const value_type&   const_reference;
    typedef SrcConstRefP        argument_type;
    typedef reference           result_type;
    static bool constexpr is_mutable = false;

    result_type operator()(argument_type srcP) const {
        result_type dstP;
        color_convert(srcP,dstP);
        return dstP;
    }
  };

We then use the ``add_deref`` member struct of image views to construct the
type of a view that invokes a given function object (``deref_t``) upon
dereferencing. In our case, it performs color conversion:

.. code-block:: cpp

  template <typename SrcView, typename DstP>
  struct color_converted_view_type
  {
  private:
    typedef typename SrcView::const_t::reference src_pix_ref;  // const reference to pixel in SrcView
    typedef color_convert_deref_fn<src_pix_ref, DstP> deref_t; // the dereference adaptor that performs color conversion
    typedef typename SrcView::template add_deref<deref_t> add_ref_t;
  public:
    typedef typename add_ref_t::type type; // the color converted view type
    static type make(const SrcView& sv) { return add_ref_t::make(sv, deref_t()); }
  };

Finally our ``color_converted_view`` code simply creates color-converted view
from the source view:

.. code-block:: cpp

  template <typename DstP, typename View> inline
  typename color_converted_view_type<View,DstP>::type color_convert_view(const View& src)
  {
    return color_converted_view_type<View,DstP>::make(src);
  }

(The actual color convert view transformation is slightly more
complicated, as it takes an optional color conversion object, which
allows users to specify their own color conversion methods). See the
GIL tutorial for an example of creating a virtual image view that
defines the Mandelbrot set.
