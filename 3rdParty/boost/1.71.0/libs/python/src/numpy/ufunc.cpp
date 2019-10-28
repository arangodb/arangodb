// Copyright Jim Bosch 2010-2012.
// Copyright Stefan Seefeld 2016.
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#define BOOST_PYTHON_NUMPY_INTERNAL
#include <boost/python/numpy/internal.hpp>
#include <boost/python/numpy/ufunc.hpp>

namespace boost { namespace python {
namespace converter 
{
NUMPY_OBJECT_MANAGER_TRAITS_IMPL(PyArrayMultiIter_Type, numpy::multi_iter)
} // namespace boost::python::converter

namespace numpy 
{

multi_iter make_multi_iter(object const & a1)
{
  return multi_iter(python::detail::new_reference(PyArray_MultiIterNew(1, a1.ptr())));
}

  multi_iter make_multi_iter(object const & a1, object const & a2)
{
  return multi_iter(python::detail::new_reference(PyArray_MultiIterNew(2, a1.ptr(), a2.ptr())));
}

multi_iter make_multi_iter(object const & a1, object const & a2, object const & a3)
{
  return multi_iter(python::detail::new_reference(PyArray_MultiIterNew(3, a1.ptr(), a2.ptr(), a3.ptr())));
}

void multi_iter::next() 
{
  PyArray_MultiIter_NEXT(ptr());
}

bool multi_iter::not_done() const 
{
  return PyArray_MultiIter_NOTDONE(ptr());
}

char * multi_iter::get_data(int i) const 
{
  return reinterpret_cast<char*>(PyArray_MultiIter_DATA(ptr(), i));
}

int multi_iter::get_nd() const
{
  return reinterpret_cast<PyArrayMultiIterObject*>(ptr())->nd;
}

Py_intptr_t const * multi_iter::get_shape() const 
{
  return reinterpret_cast<PyArrayMultiIterObject*>(ptr())->dimensions;
}

Py_intptr_t multi_iter::shape(int n) const
{
  return reinterpret_cast<PyArrayMultiIterObject*>(ptr())->dimensions[n];
}

}}} // namespace boost::python::numpy
