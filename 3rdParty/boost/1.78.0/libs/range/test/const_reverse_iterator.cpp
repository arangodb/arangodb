// Boost.Range library
//
//  Copyright Neil Groves 2014. Use, modification and
//  distribution is subject to the Boost Software License, Version
//  1.0. (See accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)
//
// For more information, see http://www.boost.org/libs/range/
//

#define BOOST_ALLOW_DEPRECATED_HEADERS
#include <boost/range/const_reverse_iterator.hpp>
#include <boost/static_assert.hpp>
#include <boost/type_traits/is_same.hpp>

#include <boost/test/test_tools.hpp>
#include <boost/test/unit_test.hpp>

#include <vector>

namespace boost_range_test
{
    namespace
    {

void test_const_reverse_iterator()
{
    typedef std::vector<int> cont;

    BOOST_STATIC_ASSERT((
        boost::is_same<
            boost::reverse_iterator<cont::const_iterator>,
            boost::range_const_reverse_iterator<cont>::type
        >::value));

    BOOST_STATIC_ASSERT((
        boost::is_same<
            boost::reverse_iterator<cont::const_iterator>,
            boost::range_const_reverse_iterator<const cont>::type
        >::value));

#ifndef BOOST_NO_CXX11_RVALUE_REFERENCES
    BOOST_STATIC_ASSERT((
        boost::is_same<
            boost::reverse_iterator<cont::const_iterator>,
            boost::range_const_reverse_iterator<cont&&>::type
        >::value));
#endif
}

    } // anonymous namespace
} // namespace boost_range_test

boost::unit_test::test_suite* init_unit_test_suite( int argc, char* argv[] )
{
    boost::unit_test::test_suite* test =
        BOOST_TEST_SUITE(
                "Boost.Range range_const_reverse_iterator meta-function");

    test->add(BOOST_TEST_CASE(&boost_range_test::test_const_reverse_iterator));

    return test;
}
