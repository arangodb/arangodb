/*=============================================================================
    Copyright (c) 2001-2007 Joel de Guzman

    Distributed under the Boost Software License, Version 1.0. (See accompanying
    file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
==============================================================================*/
#include <boost/phoenix/bind/bind_function.hpp>
#include <boost/phoenix/core/argument.hpp>

#include <iostream>

using namespace boost::phoenix;
using namespace boost::phoenix::placeholders;

void foo(int n)
{
    std::cout << n << std::endl;
}

int main()
{
    bind(&foo, arg1)(4);
}
