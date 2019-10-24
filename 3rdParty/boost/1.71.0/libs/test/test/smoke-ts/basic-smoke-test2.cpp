//  (C) Copyright Raffi Enficiaud 2016.
//  Distributed under the Boost Software License, Version 1.0.
//  (See accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)

//  See http://www.boost.org/libs/test for the library home page.

#define BOOST_TEST_MODULE basic_smoke_test2
#include <boost/test/included/unit_test.hpp>
#include <boost/mpl/list.hpp>

#include <iostream>


// trac 12531
namespace ns
{
    struct X {};
}

typedef boost::mpl::list<ns::X> test_types;

BOOST_AUTO_TEST_CASE_TEMPLATE(test, T, test_types)
{
}
