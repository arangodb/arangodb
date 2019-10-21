//  (C) Copyright Tony Lewis 2016.
//  Distributed under the Boost Software License, Version 1.0.
//  (See accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)

//  See http://www.boost.org/libs/test for the library home page.
//
//  Description : checks that datasets over tuples are supported
//  see issue https://svn.boost.org/trac/boost/ticket/12241
// ***************************************************************************

#define BOOST_TEST_MODULE boost_test_tuple_prob

#include <boost/test/unit_test.hpp>
#include <boost/test/data/test_case.hpp>
#include <boost/test/data/monomorphic.hpp>

#include <tuple>
#include <vector>
#include <iostream>

const std::vector< std::tuple<int, int>> values = {
        std::tuple<int, int>{  1, 11 },
        std::tuple<int, int>{  2, 12 },
        std::tuple<int, int>{  3, 13 },
};

BOOST_DATA_TEST_CASE( test1, boost::unit_test::data::make( values ), var1, var2 ) {
        std::cout << var1 << ", " << var2 << "\n";
}
