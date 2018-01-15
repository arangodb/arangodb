// Copyright Jim Bosch & Ankit Daftery 2010-2012.
// Copyright Stefan Seefeld 2016.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#include <boost/python/numpy.hpp>

namespace p = boost::python;
namespace np = boost::python::numpy;

np::ndarray zeros(p::tuple shape, np::dtype dt) { return np::zeros(shape, dt);}
np::ndarray array2(p::object obj, np::dtype dt) { return np::array(obj,dt);}
np::ndarray array1(p::object obj) { return np::array(obj);}
np::ndarray empty1(p::tuple shape, np::dtype dt) { return np::empty(shape,dt);}

np::ndarray c_empty(p::tuple shape, np::dtype dt)
{
  // convert 'shape' to a C array so we can test the corresponding
  // version of the constructor
  unsigned len = p::len(shape);
  Py_intptr_t *c_shape = new Py_intptr_t[len];
  for (unsigned i = 0; i != len; ++i)
    c_shape[i] = p::extract<Py_intptr_t>(shape[i]);
  np::ndarray result = np::empty(len, c_shape, dt);
  delete [] c_shape;
  return result;
}

np::ndarray transpose(np::ndarray arr) { return arr.transpose();}
np::ndarray squeeze(np::ndarray arr) { return arr.squeeze();}
np::ndarray reshape(np::ndarray arr,p::tuple tup) { return arr.reshape(tup);}

Py_intptr_t shape_index(np::ndarray arr,int k) { return arr.shape(k); }
Py_intptr_t strides_index(np::ndarray arr,int k) { return arr.strides(k); }

BOOST_PYTHON_MODULE(ndarray_ext)
{
  np::initialize();
  p::def("zeros", zeros);
  p::def("zeros_matrix", zeros, np::as_matrix<>());
  p::def("array", array2);
  p::def("array", array1);
  p::def("empty", empty1);
  p::def("c_empty", c_empty);
  p::def("transpose", transpose);
  p::def("squeeze", squeeze);
  p::def("reshape", reshape);
  p::def("shape_index", shape_index);
  p::def("strides_index", strides_index);
}
