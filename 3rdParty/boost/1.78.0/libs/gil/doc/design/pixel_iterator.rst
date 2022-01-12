Pixel Iterator
==============

.. contents::
   :local:
   :depth: 2

Overview
--------

Pixel iterators are random traversal iterators whose ``value_type
models`` ``PixelValueConcept``.

Fundamental Iterator
--------------------

Pixel iterators provide metafunctions to determine whether they are mutable
(i.e. whether they allow for modifying the pixel they refer to), to get the
immutable (read-only) type of the iterator, and to determine whether they are
plain iterators or adaptors over another pixel iterator:

.. code-block:: cpp

  concept PixelIteratorConcept<RandomAccessTraversalIteratorConcept Iterator>
      : PixelBasedConcept<Iterator>
  {
    where PixelValueConcept<value_type>;
    typename const_iterator_type<It>::type;
        where PixelIteratorConcept<const_iterator_type<It>::type>;
    static const bool  iterator_is_mutable<It>::value;
    static const bool  is_iterator_adaptor<It>::value;   // is it an iterator adaptor
  };

  template <typename Iterator>
  concept MutablePixelIteratorConcept : PixelIteratorConcept<Iterator>, MutableRandomAccessIteratorConcept<Iterator> {};

.. seealso::

  - `PixelIteratorConcept<Iterator> <reference/group___pixel_iterator_concept_pixel_iterator.html>`_
  - `MutablePixelIteratorConcept<Iterator> <reference/structboost_1_1gil_1_1_mutable_pixel_iterator_concept.html>`_

Models
^^^^^^

A built-in pointer to pixel, ``pixel<ChannelValue,Layout>*``, is GIL model for
pixel iterator over interleaved homogeneous pixels. Similarly,
``packed_pixel<PixelData,ChannelRefVec,Layout>*`` is GIL model for an iterator
over interleaved packed pixels.

For planar homogeneous pixels, GIL provides the class
``planar_pixel_iterator``, templated over a channel iterator and color space.
Here is how the standard mutable and read-only planar RGB iterators over
unsigned char are defined:

.. code-block:: cpp

  template <typename ChannelPtr, typename ColorSpace>
  struct planar_pixel_iterator;

  // GIL provided typedefs
  typedef planar_pixel_iterator<const bits8*, rgb_t> rgb8c_planar_ptr_t;
  typedef planar_pixel_iterator<      bits8*, rgb_t> rgb8_planar_ptr_t;

``planar_pixel_iterator`` also models ``HomogeneousColorBaseConcept`` (it
subclasses from ``homogeneous_color_base``) and, as a result, all color base
algorithms apply to it. The element type of its color base is a channel
iterator. For example, GIL implements ``operator++`` of planar iterators
approximately like this:

.. code-block:: cpp

  template <typename T>
  struct inc : public std::unary_function<T,T>
  {
    T operator()(T x) const { return ++x; }
  };

  template <typename ChannelPtr, typename ColorSpace>
  planar_pixel_iterator<ChannelPtr,ColorSpace>&
  planar_pixel_iterator<ChannelPtr,ColorSpace>::operator++()
  {
    static_transform(*this,*this,inc<ChannelPtr>());
    return *this;
  }

Since ``static_transform`` uses compile-time recursion, incrementing an
instance of ``rgb8_planar_ptr_t`` amounts to three pointer increments.
GIL also uses the class ``bit_aligned_pixel_iterator`` as a model for a pixel
iterator over bit-aligned pixels. Internally it keeps track of the current
byte and the bit offset.

Iterator Adaptor
----------------

Iterator adaptor is an iterator that wraps around another iterator. Its
``is_iterator_adaptor`` metafunction must evaluate to true, and it needs to
provide a member method to return the base iterator, a metafunction to get its
type, and a metafunction to rebind to another base iterator:

.. code-block:: cpp

  concept IteratorAdaptorConcept<RandomAccessTraversalIteratorConcept Iterator>
  {
    where SameType<is_iterator_adaptor<Iterator>::type, mpl::true_>;

    typename iterator_adaptor_get_base<Iterator>;
        where Metafunction<iterator_adaptor_get_base<Iterator> >;
        where boost_concepts::ForwardTraversalConcept<iterator_adaptor_get_base<Iterator>::type>;

    typename another_iterator;
    typename iterator_adaptor_rebind<Iterator,another_iterator>::type;
        where boost_concepts::ForwardTraversalConcept<another_iterator>;
        where IteratorAdaptorConcept<iterator_adaptor_rebind<Iterator,another_iterator>::type>;

    const iterator_adaptor_get_base<Iterator>::type& Iterator::base() const;
  };

  template <boost_concepts::Mutable_ForwardIteratorConcept Iterator>
  concept MutableIteratorAdaptorConcept : IteratorAdaptorConcept<Iterator> {};

.. seealso::

  - `IteratorAdaptorConcept<Iterator> <reference/structboost_1_1gil_1_1_iterator_adaptor_concept.html>`_
  - `MutableIteratorAdaptorConcept<Iterator> <reference/structboost_1_1gil_1_1_mutable_iterator_adaptor_concept.html>`_

Models
^^^^^^

GIL provides several models of ``IteratorAdaptorConcept``:

- ``memory_based_step_iterator<Iterator>``: An iterator adaptor that changes
  the fundamental step of the base iterator
  (see :ref:`design/pixel_iterator:Step Iterator`)

- ``dereference_iterator_adaptor<Iterator,Fn>``: An iterator that applies a
  unary function ``Fn`` upon dereferencing. It is used, for example, for
  on-the-fly color conversion. It can be used to construct a shallow image
  "view" that pretends to have a different color space or channel depth.
  See :doc:`image_view` for more. The unary function ``Fn`` must
  model ``PixelDereferenceAdaptorConcept`` (see below).

Pixel Dereference Adaptor
-------------------------

Pixel dereference adaptor is a unary function that can be applied upon
dereferencing a pixel iterator. Its argument type could be anything (usually a
``PixelConcept``) and the result type must be convertible to ``PixelConcept``:

.. code-block:: cpp

  template <boost::UnaryFunctionConcept D>
  concept PixelDereferenceAdaptorConcept:
      DefaultConstructibleConcept<D>,
      CopyConstructibleConcept<D>,
      AssignableConcept<D>
  {
    typename const_t;         where PixelDereferenceAdaptorConcept<const_t>;
    typename value_type;      where PixelValueConcept<value_type>;
    typename reference;       where PixelConcept<remove_reference<reference>::type>;  // may be mutable
    typename const_reference;   // must not be mutable
    static const bool D::is_mutable;

    where Convertible<value_type, result_type>;
  };

Models
^^^^^^

GIL provides several models of ``PixelDereferenceAdaptorConcept``:

* ``color_convert_deref_fn``: a function object that performs color conversion

* ``detail::nth_channel_deref_fn``: a function object that returns a grayscale
  pixel corresponding to the n-th channel of a given pixel

* ``deref_compose``: a function object that composes two models of
  ``PixelDereferenceAdaptorConcept``. Similar to ``std::unary_compose``,
  except it needs to pull the additional typedefs required by
  ``PixelDereferenceAdaptorConcept``

GIL uses pixel dereference adaptors to implement image views that perform
color conversion upon dereferencing, or that return the N-th channel of the
underlying pixel. They can be used to model virtual image views that perform
an arbitrary function upon dereferencing, for example a view of the Mandelbrot
set. ``dereference_iterator_adaptor<Iterator,Fn>`` is an iterator wrapper over
a pixel iterator ``Iterator`` that invokes the given dereference iterator
adaptor ``Fn`` upon dereferencing.

Step Iterator
-------------

Sometimes we want to traverse pixels with a unit step other than the one
provided by the fundamental pixel iterators. Examples where this would be
useful:

* a single-channel view of the red channel of an RGB interleaved image
* left-to-right flipped image (step = -fundamental_step)
* subsampled view, taking every N-th pixel (step = N*fundamental_step)
* traversal in vertical direction (step = number of bytes per row)
* any combination of the above (steps are multiplied)

Step iterators are forward traversal iterators that allow changing the step
between adjacent values:

.. code-block:: cpp

  concept StepIteratorConcept<boost_concepts::ForwardTraversalConcept Iterator>
  {
    template <Integral D> void Iterator::set_step(D step);
  };

  concept MutableStepIteratorConcept<boost_concepts::Mutable_ForwardIteratorConcept Iterator>
      : StepIteratorConcept<Iterator>
  {};

GIL currently provides a step iterator whose ``value_type models``
``PixelValueConcept``. In addition, the step is specified in memory units
(which are bytes or bits). This is necessary, for example, when implementing
an iterator navigating along a column of pixels - the size of a row of pixels
may sometimes not be divisible by the size of a pixel; for example rows may be
word-aligned.

To advance in bytes/bits, the base iterator must model
``MemoryBasedIteratorConcept``. A memory-based iterator has an inherent memory
unit, which is either a bit or a byte. It must supply functions returning the
number of bits per memory unit (1 or 8), the current step in memory units, the
memory-unit distance between two iterators, and a reference a given distance
in memunits away. It must also supply a function that advances an iterator a
given distance in memory units. ``memunit_advanced`` and
``memunit_advanced_ref`` have a default implementation but some iterators may
supply a more efficient version:

.. code-block:: cpp

  concept MemoryBasedIteratorConcept
  <
      boost_concepts::RandomAccessTraversalConcept Iterator
  >
  {
    typename byte_to_memunit<Iterator>; where metafunction<byte_to_memunit<Iterator> >;
    std::ptrdiff_t      memunit_step(const Iterator&);
    std::ptrdiff_t      memunit_distance(const Iterator& , const Iterator&);
    void                memunit_advance(Iterator&, std::ptrdiff_t diff);
    Iterator            memunit_advanced(const Iterator& p, std::ptrdiff_t diff) { Iterator tmp; memunit_advance(tmp,diff); return tmp; }
    Iterator::reference memunit_advanced_ref(const Iterator& p, std::ptrdiff_t diff) { return *memunit_advanced(p,diff); }
  };

It is useful to be able to construct a step iterator over another iterator.
More generally, given a type, we want to be able to construct an equivalent
type that allows for dynamically specified horizontal step:

.. code-block:: cpp

  concept HasDynamicXStepTypeConcept<typename T>
  {
    typename dynamic_x_step_type<T>;
        where Metafunction<dynamic_x_step_type<T> >;
  };

All models of pixel iterators, locators and image views that GIL provides
support ``HasDynamicXStepTypeConcept``.

.. seealso::

  - `StepIteratorConcept<Iterator> <reference/structboost_1_1gil_1_1_step_iterator_concept.html>`_
  - `MutableStepIteratorConcept<Iterator> <reference/structboost_1_1gil_1_1_mutable_step_iterator_concept.html>`_
  - `MemoryBasedIteratorConcept<Iterator> <reference/structboost_1_1gil_1_1_memory_based_iterator_concept.html>`_
  - `HasDynamicXStepTypeConcept<T> <reference/structboost_1_1gil_1_1_has_dynamic_x_step_type_concept.html>`_

Models
^^^^^^

All standard memory-based iterators GIL currently provides model
``MemoryBasedIteratorConcept``. GIL provides the class
``memory_based_step_iterator`` which models ``PixelIteratorConcept``,
``StepIteratorConcept``, and ``MemoryBasedIteratorConcept``. It takes the base
iterator as a template parameter (which must model ``PixelIteratorConcept``
and ``MemoryBasedIteratorConcept``) and allows changing the step dynamically.
GIL implementation contains the base iterator and a ``ptrdiff_t`` denoting the
number of memory units (bytes or bits) to skip for a unit step. It may also be
used with a negative number. GIL provides a function to create a step iterator
from a base iterator and a step:

.. code-block:: cpp

  // Iterator models MemoryBasedIteratorConcept, HasDynamicXStepTypeConcept
  template <typename Iterator>
  typename dynamic_x_step_type<Iterator>::type make_step_iterator(Iterator const& it, std::ptrdiff_t step);

GIL also provides a model of an iterator over a virtual array of pixels,
``position_iterator``. It is a step iterator that keeps track of the pixel
position and invokes a function object to get the value of the pixel upon
dereferencing. It models ``PixelIteratorConcept`` and ``StepIteratorConcept``
but not ``MemoryBasedIteratorConcept``.
