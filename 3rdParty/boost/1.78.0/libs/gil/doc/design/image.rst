Image
=====

.. contents::
   :local:
   :depth: 2

Overview
--------

An image is a container that owns the pixels of a given image view
It allocates them in its constructor and deletes them in the destructor.
It has a deep assignment operator and copy constructor. Images are used
rarely, just when data ownership is important. Most STL algorithms operate on
ranges, not containers. Similarly most GIL algorithms operate on image views
(which images provide).

In the most general form images are N-dimensional and satisfy the following
concept:

.. code-block:: cpp

  concept RandomAccessNDImageConcept<typename Img> : Regular<Img>
  {
    typename view_t; where MutableRandomAccessNDImageViewConcept<view_t>;
    typename const_view_t = view_t::const_t;
    typename point_t      = view_t::point_t;
    typename value_type   = view_t::value_type;
    typename allocator_type;

    Img::Img(point_t dims, std::size_t alignment=0);
    Img::Img(point_t dims, value_type fill_value, std::size_t alignment);

    void Img::recreate(point_t new_dims, std::size_t alignment=0);
    void Img::recreate(point_t new_dims, value_type fill_value, std::size_t alignment);

    const point_t&        Img::dimensions() const;
    const const_view_t&   const_view(const Img&);
    const view_t&         view(Img&);
  };

Two-dimensional images have additional requirements:

.. code-block:: cpp

  concept RandomAccess2DImageConcept<RandomAccessNDImageConcept Img>
  {
    typename x_coord_t = const_view_t::x_coord_t;
    typename y_coord_t = const_view_t::y_coord_t;

    Img::Img(x_coord_t width, y_coord_t height, std::size_t alignment=0);
    Img::Img(x_coord_t width, y_coord_t height, value_type fill_value, std::size_t alignment);

    x_coord_t Img::width() const;
    y_coord_t Img::height() const;

    void Img::recreate(x_coord_t width, y_coord_t height, std::size_t alignment=1);
    void Img::recreate(x_coord_t width, y_coord_t height, value_type fill_value, std::size_t alignment);
  };

GIL images have views that model ``ImageViewConcept`` and operate on pixels.

.. code-block:: cpp

  concept ImageConcept<RandomAccess2DImageConcept Img>
  {
    where MutableImageViewConcept<view_t>;
    typename coord_t  = view_t::coord_t;
  };

Images, unlike locators and image views, don't have 'mutable' set of concepts
because immutable images are not very useful.

.. seealso::

  - `RandomAccessNDImageConcept<Image> <reference/structboost_1_1gil_1_1_random_access_n_d_image_concept.html>`_
  - `RandomAccess2DImageConcept<Image> <reference/structboost_1_1gil_1_1_random_access2_d_image_concept.html>`_
  - `ImageConcept<Image> <reference/structboost_1_1gil_1_1_image_concept.html>`_

Models
------

GIL provides a class, ``image``, which is templated over the value type
(the pixel) and models ``ImageConcept``:

.. code-block:: cpp

    template
    <
        typename Pixel, // Models PixelValueConcept
        bool IsPlanar,  // planar or interleaved image
        typename A=std::allocator<unsigned char>
    >
   class image;

The image constructor takes an alignment parameter which allows for
constructing images that are word-aligned or 8-byte aligned. The alignment is
specified in bytes. The default value for alignment is 0, which means there is
no padding at the end of rows. Many operations are faster using such
1D-traversable images, because ``image_view::x_iterator`` can be used to
traverse the pixels, instead of the more complicated ``image_view::iterator``.
Note that when alignment is 0, packed images are aligned to the bit - i.e.
there are no padding bits at the end of rows of packed images.
