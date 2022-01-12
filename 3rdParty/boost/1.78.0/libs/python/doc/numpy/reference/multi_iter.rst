multi_iter
==========

.. contents :: Table of Contents

A ``multi_iter`` is a Python object, intended to be used as an iterator  It should generally only be used in loops.

 ``<boost/python/numpy/ufunc.hpp>`` contains the class definitions for ``multi_iter``


synopsis
--------

::

  namespace boost 
  {
  namespace python
  {
  namespace numpy 
  {

  class multi_iter : public object 
  {
  public:
    void next();
    bool not_done() const;
    char * get_data(int n) const;
    int const get_nd() const;
    Py_intptr_t const * get_shape() const;
    Py_intptr_t const shape(int n) const;
  };


  multi_iter make_multi_iter(object const & a1);
  multi_iter make_multi_iter(object const & a1, object const & a2);
  multi_iter make_multi_iter(object const & a1, object const & a2, object const & a3);

  }
  }
  }


constructors
------------

::

  multi_iter make_multi_iter(object const & a1);
  multi_iter make_multi_iter(object const & a1, object const & a2);
  multi_iter make_multi_iter(object const & a1, object const & a2, object const & a3);

:Returns: A Python iterator object broadcasting over one, two or three sequences as supplied

accessors
---------

::

  void next();

:Effects: Increments the iterator

::

  bool not_done() const;

:Returns: boolean value indicating whether the iterator is at its end

::

  char * get_data(int n) const;

:Returns: a pointer to the element of the nth broadcasted array.

::

  int const get_nd() const;

:Returns: the number of dimensions of the broadcasted array expression

::

  Py_intptr_t const * get_shape() const;

:Returns: the shape of the broadcasted array expression as an array of integers.

::

  Py_intptr_t const shape(int n) const;

:Returns: the shape of the broadcasted array expression in the nth dimension.
	    

