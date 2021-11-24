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

#include <vector>

namespace boost_range_adaptor_type_erased_test
{
    namespace
    {

void test_operator_brackets()
{
    typedef boost::adaptors::type_erased<> type_erased_t;

    std::vector<int> c;
    for (int i = 0; i < 10; ++i)
        c.push_back(i);

    typedef boost::any_range_type_generator<
                        std::vector<int> >::type any_range_type;

    BOOST_STATIC_ASSERT((
            boost::is_same<
                int,
                boost::range_value<any_range_type>::type
            >::value
    ));

    BOOST_STATIC_ASSERT((
            boost::is_same<
                boost::random_access_traversal_tag,
                boost::iterator_traversal<
                    boost::range_iterator<any_range_type>::type
                >::type
            >::value
    ));

    any_range_type rng = c | type_erased_t();

    for (int i = 0; i < 10; ++i)
    {
        BOOST_CHECK_EQUAL(rng[i], i);
    }
}

    } // anonymous namespace
} // namespace boost_range_adaptor_type_erased_test

boost::unit_test::test_suite*
init_unit_test_suite(int, char*[])
{
    boost::unit_test::test_suite* test
        = BOOST_TEST_SUITE("RangeTestSuite.adaptor.type_erased_brackets");

    test->add(
        BOOST_TEST_CASE(
            &boost_range_adaptor_type_erased_test::test_operator_brackets));

    return test;
}

