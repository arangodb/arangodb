// Copyright 2018 Peter Dimov
//
// Distributed under the Boost Software License, Version 1.0.
//
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt

#include "lib2_throw.hpp"
#include <boost/throw_exception.hpp>

void lib2::f()
{
    boost::throw_exception( lib2::exception() );
}
