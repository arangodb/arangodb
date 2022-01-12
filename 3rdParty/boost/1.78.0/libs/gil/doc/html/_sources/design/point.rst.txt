Point
=====

.. contents::
   :local:
   :depth: 2

Overview
--------

A point defines the location of a pixel inside an image. It can also be used
to describe the dimensions of an image. In most general terms, points are
N-dimensional and model the following concept:

.. code-block:: cpp

  concept PointNDConcept<typename T> : Regular<T>
  {
      // the type of a coordinate along each axis
      template <size_t K> struct axis; where Metafunction<axis>;

      const size_t num_dimensions;

      // accessor/modifier of the value of each axis.
      template <size_t K> const typename axis<K>::type& T::axis_value() const;
      template <size_t K>       typename axis<K>::type& T::axis_value();
  };

GIL uses a two-dimensional point, which is a refinement of ``PointNDConcept``
in which both dimensions are of the same type:

.. code-block:: cpp

  concept Point2DConcept<typename T> : PointNDConcept<T>
  {
      where num_dimensions == 2;
      where SameType<axis<0>::type, axis<1>::type>;

      typename value_type = axis<0>::type;

      const value_type& operator[](const T&, size_t i);
          value_type& operator[](      T&, size_t i);

      value_type x,y;
  };

.. seealso::

  - `PointNDConcept <reference/structboost_1_1gil_1_1_point_n_d_concept.html>`_
  - `Point2DConcept <reference/structboost_1_1gil_1_1_point2_d_concept.html>`_

Models
------

GIL provides a model of ``Point2DConcept``, ``point<T>`` where ``T`` is the
coordinate type.
