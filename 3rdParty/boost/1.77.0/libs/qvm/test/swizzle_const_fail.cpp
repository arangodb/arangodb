//Copyright (c) 2008-2016 Emil Dotchevski and Reverge Studios, Inc.

//Distributed under the Boost Software License, Version 1.0. (See accompanying
//file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <boost/qvm/vec.hpp>
#include <boost/qvm/vec_operations.hpp>
#include <boost/qvm/swizzle2.hpp>

int
main()
    {
    using namespace boost::qvm;
    vec<float,2> v;
    vec<float,2> const & cv=v;
    XY(cv)*=2;
    return 1;
    }
