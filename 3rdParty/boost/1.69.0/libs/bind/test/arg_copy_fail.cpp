//
//  arg_copy_fail.cpp - arg<1> to arg<2>
//
//  Copyright 2016 Peter Dimov
//
//  Distributed under the Boost Software License, Version 1.0.
//  See accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt
//

#include <boost/bind/arg.hpp>

//

int main()
{
    boost::arg<1> a1(( boost::arg<2>() ));
    (void)a1;
}
