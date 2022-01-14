Color Base
==========

.. contents::
   :local:
   :depth: 2

Overview
--------

A color base is a container of color elements. The most common use of color
base is in the implementation of a pixel, in which case the color elements are
channel values. The color base concept, however, can be used in other
scenarios. For example, a planar pixel has channels that are not contiguous in
memory. Its reference is a proxy class that uses a color base whose elements
are channel references. Its iterator uses a color base whose elements are
channel iterators.

Color base models must satisfy the following concepts:

.. code-block:: cpp

  concept ColorBaseConcept<typename T>
      : CopyConstructible<T>, EqualityComparable<T>
  {
    // a GIL layout (the color space and element permutation)
    typename layout_t;

    // The type of K-th element
    template <int K> struct kth_element_type;
        where Metafunction<kth_element_type>;

    // The result of at_c
    template <int K> struct kth_element_const_reference_type;
        where Metafunction<kth_element_const_reference_type>;

    template <int K> kth_element_const_reference_type<T,K>::type at_c(T);

    template <ColorBaseConcept T2> where { ColorBasesCompatibleConcept<T,T2> }
        T::T(T2);
    template <ColorBaseConcept T2> where { ColorBasesCompatibleConcept<T,T2> }
        bool operator==(const T&, const T2&);
    template <ColorBaseConcept T2> where { ColorBasesCompatibleConcept<T,T2> }
        bool operator!=(const T&, const T2&);

  };

  concept MutableColorBaseConcept<ColorBaseConcept T>
      : Assignable<T>, Swappable<T>
  {
    template <int K> struct kth_element_reference_type;
        where Metafunction<kth_element_reference_type>;

    template <int K> kth_element_reference_type<T,K>::type at_c(T);

    template <ColorBaseConcept T2> where { ColorBasesCompatibleConcept<T,T2> }
        T& operator=(T&, const T2&);
  };

  concept ColorBaseValueConcept<typename T> : MutableColorBaseConcept<T>, Regular<T>
  {
  };

  concept HomogeneousColorBaseConcept<ColorBaseConcept CB>
  {
    // For all K in [0 ... size<C1>::value-1):
    //     where SameType<kth_element_type<K>::type, kth_element_type<K+1>::type>;
    kth_element_const_reference_type<0>::type dynamic_at_c(const CB&, std::size_t n) const;
  };

  concept MutableHomogeneousColorBaseConcept<MutableColorBaseConcept CB>
      : HomogeneousColorBaseConcept<CB>
  {
    kth_element_reference_type<0>::type dynamic_at_c(const CB&, std::size_t n);
  };

  concept HomogeneousColorBaseValueConcept<typename T>
      : MutableHomogeneousColorBaseConcept<T>, Regular<T>
  {
  };

  concept ColorBasesCompatibleConcept<ColorBaseConcept C1, ColorBaseConcept C2>
  {
    where SameType<C1::layout_t::color_space_t, C2::layout_t::color_space_t>;
    // also, for all K in [0 ... size<C1>::value):
    //     where Convertible<kth_semantic_element_type<C1,K>::type, kth_semantic_element_type<C2,K>::type>;
    //     where Convertible<kth_semantic_element_type<C2,K>::type, kth_semantic_element_type<C1,K>::type>;
  };

A color base must have an associated layout (which consists of a color space,
as well as an ordering of the channels). There are two ways to index the
elements of a color base: A physical index corresponds to the way they are
ordered in memory, and a semantic index corresponds to the way the elements
are ordered in their color space. For example, in the RGB color space the
elements are ordered as ``{red_t, green_t, blue_t}``. For a color base with
a BGR layout, the first element in physical ordering is the blue element,
whereas the first semantic element is the red one.  Models of
``ColorBaseConcept`` are required to provide the ``at_c<K>(ColorBase)``
function, which allows for accessing the elements based on their physical
order. GIL provides a ``semantic_at_c<K>(ColorBase)`` function (described
later) which can operate on any model of ColorBaseConcept and returns the
corresponding semantic element.

Two color bases are *compatible* if they have the same color space and their
elements (paired semantically) are convertible to each other.

Models
------

GIL provides a model for a homogeneous color base (a color base whose elements
all have the same type).

.. code-block:: cpp

  namespace detail
  {
    template <typename Element, typename Layout, int K>
    struct homogeneous_color_base;
  }

It is used in the implementation of GIL's pixel, planar pixel reference and
planar pixel iterator. Another model of ``ColorBaseConcept`` is
``packed_pixel`` - it is a pixel whose channels are bit ranges.

See the :doc:`pixel` section for more.

Algorithms
----------

GIL provides the following functions and metafunctions operating on color
bases:

.. code-block:: cpp

  // Metafunction returning an mpl::int_ equal to the number of elements in the color base
  template <class ColorBase> struct size;

  // Returns the type of the return value of semantic_at_c<K>(color_base)
  template <class ColorBase, int K> struct kth_semantic_element_reference_type;
  template <class ColorBase, int K> struct kth_semantic_element_const_reference_type;

  // Returns a reference to the element with K-th semantic index.
  template <class ColorBase, int K>
  typename kth_semantic_element_reference_type<ColorBase,K>::type       semantic_at_c(ColorBase& p)
  template <class ColorBase, int K>
  typename kth_semantic_element_const_reference_type<ColorBase,K>::type semantic_at_c(const ColorBase& p)

  // Returns the type of the return value of get_color<Color>(color_base)
  template <typename Color, typename ColorBase> struct color_reference_t;
  template <typename Color, typename ColorBase> struct color_const_reference_t;

  // Returns a reference to the element corresponding to the given color
  template <typename ColorBase, typename Color>
  typename color_reference_t<Color,ColorBase>::type get_color(ColorBase& cb, Color=Color());
  template <typename ColorBase, typename Color>
  typename color_const_reference_t<Color,ColorBase>::type get_color(const ColorBase& cb, Color=Color());

  // Returns the element type of the color base. Defined for homogeneous color bases only
  template <typename ColorBase> struct element_type;
  template <typename ColorBase> struct element_reference_type;
  template <typename ColorBase> struct element_const_reference_type;

GIL also provides the following algorithms which operate on color bases.
Note that they all pair the elements semantically:

.. code-block:: cpp

  // Equivalents to std::equal, std::copy, std::fill, std::generate
  template <typename CB1,typename CB2>   bool static_equal(const CB1& p1, const CB2& p2);
  template <typename Src,typename Dst>   void static_copy(const Src& src, Dst& dst);
  template <typename CB, typename Op>    void static_generate(CB& dst,Op op);

  // Equivalents to std::transform
  template <typename CB ,             typename Dst,typename Op> Op static_transform(      CB&,Dst&,Op);
  template <typename CB ,             typename Dst,typename Op> Op static_transform(const CB&,Dst&,Op);
  template <typename CB1,typename CB2,typename Dst,typename Op> Op static_transform(      CB1&,      CB2&,Dst&,Op);
  template <typename CB1,typename CB2,typename Dst,typename Op> Op static_transform(const CB1&,      CB2&,Dst&,Op);
  template <typename CB1,typename CB2,typename Dst,typename Op> Op static_transform(      CB1&,const CB2&,Dst&,Op);
  template <typename CB1,typename CB2,typename Dst,typename Op> Op static_transform(const CB1&,const CB2&,Dst&,Op);

  // Equivalents to std::for_each
  template <typename CB1,                          typename Op> Op static_for_each(      CB1&,Op);
  template <typename CB1,                          typename Op> Op static_for_each(const CB1&,Op);
  template <typename CB1,typename CB2,             typename Op> Op static_for_each(      CB1&,      CB2&,Op);
  template <typename CB1,typename CB2,             typename Op> Op static_for_each(      CB1&,const CB2&,Op);
  template <typename CB1,typename CB2,             typename Op> Op static_for_each(const CB1&,      CB2&,Op);
  template <typename CB1,typename CB2,             typename Op> Op static_for_each(const CB1&,const CB2&,Op);
  template <typename CB1,typename CB2,typename CB3,typename Op> Op static_for_each(      CB1&,      CB2&,      CB3&,Op);
  template <typename CB1,typename CB2,typename CB3,typename Op> Op static_for_each(      CB1&,      CB2&,const CB3&,Op);
  template <typename CB1,typename CB2,typename CB3,typename Op> Op static_for_each(      CB1&,const CB2&,      CB3&,Op);
  template <typename CB1,typename CB2,typename CB3,typename Op> Op static_for_each(      CB1&,const CB2&,const CB3&,Op);
  template <typename CB1,typename CB2,typename CB3,typename Op> Op static_for_each(const CB1&,      CB2&,      CB3&,Op);
  template <typename CB1,typename CB2,typename CB3,typename Op> Op static_for_each(const CB1&,      CB2&,const CB3&,Op);
  template <typename CB1,typename CB2,typename CB3,typename Op> Op static_for_each(const CB1&,const CB2&,      CB3&,Op);
  template <typename CB1,typename CB2,typename CB3,typename Op> Op static_for_each(const CB1&,const CB2&,const CB3&,Op);

  // The following algorithms are only defined for homogeneous color bases:
  // Equivalent to std::fill
  template <typename HCB, typename Element> void static_fill(HCB& p, const Element& v);

  // Equivalents to std::min_element and std::max_element
  template <typename HCB> typename element_const_reference_type<HCB>::type static_min(const HCB&);
  template <typename HCB> typename element_reference_type<HCB>::type       static_min(      HCB&);
  template <typename HCB> typename element_const_reference_type<HCB>::type static_max(const HCB&);
  template <typename HCB> typename element_reference_type<HCB>::type       static_max(      HCB&);

These algorithms are designed after the corresponding STL algorithms, except
that instead of ranges they take color bases and operate on their elements.
In addition, they are implemented with a compile-time recursion (thus the
prefix "static\_"). Finally, they pair the elements semantically instead of
based on their physical order in memory.

For example, here is the implementation of ``static_equal``:

.. code-block:: cpp

  namespace detail
  {
    template <int K> struct element_recursion
    {
      template <typename P1,typename P2>
      static bool static_equal(const P1& p1, const P2& p2)
      {
        return element_recursion<K-1>::static_equal(p1,p2) &&
               semantic_at_c<K-1>(p1)==semantic_at_c<N-1>(p2);
      }
    };
    template <> struct element_recursion<0>
    {
      template <typename P1,typename P2>
      static bool static_equal(const P1&, const P2&) { return true; }
    };
  }

  template <typename P1,typename P2>
  bool static_equal(const P1& p1, const P2& p2)
  {
    gil_function_requires<ColorSpacesCompatibleConcept<P1::layout_t::color_space_t,P2::layout_t::color_space_t> >();
    return detail::element_recursion<size<P1>::value>::static_equal(p1,p2);
  }

This algorithm is used when invoking ``operator==`` on two pixels, for
example. By using semantic accessors we are properly comparing an RGB pixel to
a BGR pixel. Notice also that all of the above algorithms taking more than one
color base require that they all have the same color space.
