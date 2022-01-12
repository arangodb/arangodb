// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)
#include <boost/python/module.hpp>
#include <boost/python/def.hpp>
#include <boost/python/object.hpp>
#include <boost/python/class.hpp>
#include <boost/assert.hpp>
#include <boost/cstdint.hpp>

using namespace boost::python;

struct BOOST_ALIGNMENT(32) X
{
    int x;
    BOOST_ALIGNMENT(32) float f;
    X(int n, float _f) : x(n), f(_f)
    {
        BOOST_ASSERT((reinterpret_cast<uintptr_t>(&f) % 32) == 0);
    }
};

int x_function(X& x) { return x.x;}
float f_function(X& x) { return x.f;}

BOOST_PYTHON_MODULE(aligned_class_ext)
{
    class_<X>("X", init<int, float>());
    def("x_function", x_function);
    def("f_function", f_function);
}

#include "module_tail.cpp"
