// Boost.Range library
//
//  Copyright Neil Groves 2009. Use, modification and
//  distribution is subject to the Boost Software License, Version
//  1.0. (See accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)
//
//
// For more information, see http://www.boost.org/libs/range/
//
//[indexed_example

//<-
#include <boost/config.hpp>
//->

#include <boost/range/adaptor/indexed.hpp>
#include <boost/assign.hpp>
#include <iterator>
#include <iostream>
#include <vector>

//<-
#include <boost/test/test_tools.hpp>
#include <boost/test/unit_test.hpp>

namespace 
{

template<class Iterator1, class Iterator2>
void check_element_and_index(
        Iterator1 test_first,
        Iterator1 test_last,
        Iterator2 reference_first,
        Iterator2 reference_last)
{
    BOOST_CHECK_EQUAL( std::distance(test_first, test_last),
                       std::distance(reference_first, reference_last) );

    std::ptrdiff_t reference_index = 0;

    Iterator1 test_it = test_first;
    Iterator2 reference_it = reference_first;
    for (; test_it != test_last; ++test_it, ++reference_it, ++reference_index)
    {
        BOOST_CHECK_EQUAL(test_it->value(), *reference_it);
        BOOST_CHECK_EQUAL(test_it->index(), reference_index);
    }
}

template<class SinglePassRange1, class SinglePassRange2>
void check_element_and_index(
    const SinglePassRange1& test_rng,
    const SinglePassRange2& reference_rng)
{
    check_element_and_index(
        boost::begin(test_rng), boost::end(test_rng),
        boost::begin(reference_rng), boost::end(reference_rng));
}
//->

//<-
void indexed_example_test()
//->
//=int main(int argc, const char* argv[])
{
    using namespace boost::assign;
    using namespace boost::adaptors;

    std::vector<int> input;
    input += 10,20,30,40,50,60,70,80,90;

//<-
#ifndef BOOST_NO_CXX11_RANGE_BASED_FOR
//->
    for (const auto& element : input | indexed(0))
    {
        std::cout << "Element = " << element.value()
                  << " Index = " << element.index()
                  << std::endl;
    }
//<-
#endif // C++11 has range for loop
//->
    
//=    return 0;
//=}
//]
    check_element_and_index(
        input | indexed(0),
        input);
}
}

boost::unit_test::test_suite*
init_unit_test_suite(int argc, char* argv[])
{
    boost::unit_test::test_suite* test
        = BOOST_TEST_SUITE( "RangeTestSuite.adaptor.indexed_example" );

    test->add( BOOST_TEST_CASE( &indexed_example_test ) );

    return test;
}
