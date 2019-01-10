// Copyright Jim Bosch & Ankit Daftery 2010-2012.
// Copyright Stefan Seefeld 2016.
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <boost/python/numpy.hpp>

namespace p = boost::python;
namespace np = boost::python::numpy;

np::ndarray reshape(np::ndarray old_array, p::tuple shape)
{
  np::ndarray local_shape =  old_array.reshape(shape);
  return local_shape;
}

BOOST_PYTHON_MODULE(shapes_ext)
{
  np::initialize();
  p::def("reshape", reshape);
}
