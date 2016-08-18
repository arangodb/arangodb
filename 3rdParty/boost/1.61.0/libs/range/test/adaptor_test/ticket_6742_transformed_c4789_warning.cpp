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
#include <boost/phoenix/core.hpp>
#include <boost/phoenix/operator.hpp>
#include <boost/range/adaptor/transformed.hpp>
#include <boost/range/algorithm_ext/push_back.hpp>
#include <boost/test/test_tools.hpp>
#include <boost/test/unit_test.hpp>
#include <iterator>
#include <iostream>
#include <vector>

namespace
{
    struct test_struct
    {
        double x;
        double y;
    };

    struct get_x
    {
        typedef double result_type;
        double operator()(const test_struct& s) const
        {
            return s.x;
        }
    };

    void range_transformed_warning()
    {
        using namespace boost::phoenix::arg_names;
        using namespace boost::adaptors;

        test_struct t;
        t.x = 2.0;
        t.y = -4.0;
        std::vector<test_struct> v(10u, t);

        std::vector<double> output1;
        boost::push_back(output1, v | transformed((&arg1)->*& test_struct::x));

        std::vector<double> output2;
        boost::push_back(output2, v | transformed(get_x()));

        BOOST_CHECK_EQUAL_COLLECTIONS(
                    output1.begin(), output1.end(),
                    output2.begin(), output2.end());
    }
} // anonymous namespace

boost::unit_test::test_suite*
init_unit_test_suite(int argc, char* argv[])
{
    boost::unit_test::test_suite* test
        = BOOST_TEST_SUITE( "Range adaptors - transformed warning" );

    test->add(BOOST_TEST_CASE(&range_transformed_warning));

    return test;
}
