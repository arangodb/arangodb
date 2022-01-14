// Copyright (C) 2003 Rational Discovery LLC
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <boost/python/module.hpp>
#include <stdexcept>

using namespace boost::python;

BOOST_PYTHON_MODULE(module_init_exception_ext)
{
    throw std::runtime_error("Module init failed");
}
