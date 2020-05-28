/*=============================================================================
    Copyright (c) 2001-2007 Joel de Guzman

    Distributed under the Boost Software License, Version 1.0. (See accompanying 
    file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
==============================================================================*/
#include <iostream>

#include <boost/phoenix/core/argument.hpp>
#include <boost/phoenix/core/value.hpp>
#include <boost/detail/lightweight_test.hpp>

using boost::phoenix::val;

int
main()
{
#ifndef BOOST_PHOENIX_NO_PREDEFINED_TERMINALS
    using boost::phoenix::arg_names::_1;
#else 
    boost::phoenix::arg_names::_1_type _1;
#endif

    int i1 = 1;

    BOOST_TEST(val(_1)(i1) == i1);

    return boost::report_errors();
}
