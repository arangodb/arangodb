Concepts
========

All constructs in GIL are models of GIL concepts. A *concept* is a set of
requirements that a type (or a set of related types) must fulfill to be used
correctly in generic algorithms. The requirements include syntactic and
algorithmic guarantees. For example, GIL class ``pixel`` is a model of GIL
``PixelConcept``. The user may substitute the pixel class with one of their
own, and, as long as it satisfies the requirements of ``PixelConcept``,
all other GIL classes and algorithms can be used with it.
See more about concepts is avaialble at
`Generic Programming in ConceptC++ <https://web.archive.org/web/20160324115943/http://www.generic-programming.org/languages/conceptcpp/>`_

In this document we will use a syntax for defining concepts that is described
in the C++ standard proposal paper
`[N2081] Concepts <http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2006/n2081.pdf>`_.

Here are some common concepts that will be used in GIL.
Most of them are defined at the
`ConceptC++ Concept Web <https://web.archive.org/web/20160326060858/http://www.generic-programming.org/languages/conceptcpp/concept_web.php>`_:

.. code-block:: cpp

  auto concept DefaultConstructible<typename T>
  {
      T::T();
  };

  auto concept CopyConstructible<typename T>
  {
      T::T(T);
      T::~T();
  };

  auto concept Assignable<typename T, typename U = T>
  {
      typename result_type;
      result_type operator=(T&, U);
  };

  auto concept EqualityComparable<typename T, typename U = T>
  {
      bool operator==(T x, U y);
      bool operator!=(T x, U y) { return !(x==y); }
  };

  concept SameType<typename T, typename U> { /* unspecified */ };
  template<typename T> concept_map SameType<T, T> { /* unspecified */ };

  auto concept Swappable<typename T>
  {
      void swap(T& t, T& u);
  };

Here are some additional basic concepts that GIL needs:

.. code-block:: cpp

  auto concept Regular<typename T> :
      DefaultConstructible<T>,
      CopyConstructible<T>,
      EqualityComparable<T>,
      Assignable<T>,
      Swappable<T>
  {};

  auto concept Metafunction<typename T>
  {
      typename type;
  };
