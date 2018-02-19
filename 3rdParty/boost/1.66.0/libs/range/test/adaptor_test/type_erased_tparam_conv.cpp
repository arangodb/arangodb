// Boost.Range library
//
//  Copyright Neil Groves 2014. Use, modification and
//  distribution is subject to the Boost Software License, Version
//  1.0. (See accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)
//
#include <boost/range/adaptor/type_erased.hpp>
#include "type_erased_test.hpp"

#include <boost/test/unit_test.hpp>

#include <algorithm>
#include <vector>

namespace boost_range_adaptor_type_erased_test
{
    namespace
    {

void template_parameter_conversion()
{
    typedef boost::any_range<
                int
              , boost::random_access_traversal_tag
              , int&
              , std::ptrdiff_t
    > source_range_type;

    typedef boost::any_range<
                int
              , boost::single_pass_traversal_tag
              , const int&
              , std::ptrdiff_t
    > target_range_type;

    source_range_type source;

    // Converting via construction
    target_range_type t1(source);

    // Converting via assignment
    target_range_type t2;
    t2 = source;

    // Converting via construction to a type with a reference type
    // that is a value
    typedef boost::any_range<
                int
              , boost::single_pass_traversal_tag
              , int
              , std::ptrdiff_t
            > target_range2_type;

    target_range2_type t3(source);
    target_range2_type t4;
    t4 = source;
}

    } // anonymous namespace
} // namespace boost_range_adaptor_type_erased_test

boost::unit_test::test_suite*
init_unit_test_suite(int argc, char* argv[])
{
    boost::unit_test::test_suite* test =
        BOOST_TEST_SUITE("RangeTestSuite.adaptor.type_erased_tparam_conv");

    test->add(BOOST_TEST_CASE(
        &boost_range_adaptor_type_erased_test::template_parameter_conversion));

    return test;
}

