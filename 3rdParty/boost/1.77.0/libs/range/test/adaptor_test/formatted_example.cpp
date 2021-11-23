// Boost.Range library
//
//  Copyright Neil Groves 2014. Use, modification and
//  distribution is subject to the Boost Software License, Version
//  1.0. (See accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)
//
//
// For more information, see http://www.boost.org/libs/range/
//
//[formatted_example
#include <boost/range/adaptor/formatted.hpp>
#include <boost/assign.hpp>
#include <iterator>
#include <iostream>
#include <string>
#include <sstream>
#include <vector>

//<-
#include <boost/test/test_tools.hpp>
#include <boost/test/unit_test.hpp>

#include <boost/range/algorithm_ext/push_back.hpp>

namespace
{
void formatted_example_test()
//->
//=int main(int argc, const char* argv[])
{
    using namespace boost::assign;

    std::vector<int> input;
    input += 1,2,3,4,5;
    
    std::cout << boost::adaptors::format(input) << std::endl;

    // Alternatively this can be written:
    // std::cout << (input | boost::adaptors::formatted()) << std::endl;

//=    return 0;
//=}
//]
    std::ostringstream test;
    test << boost::adaptors::format(input);

    BOOST_CHECK_EQUAL(test.str(), "{1,2,3,4,5}");
}
}

boost::unit_test::test_suite*
init_unit_test_suite(int argc, char* argv[])
{
    boost::unit_test::test_suite* test
        = BOOST_TEST_SUITE( "RangeTestSuite.adaptor.formatted_example" );

    test->add( BOOST_TEST_CASE( &formatted_example_test ) );

    return test;
}
