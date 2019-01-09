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
// Credit goes to Eric Niebler for providing an example to demonstrate this
// issue. This has been trivially modified to create this test case.
//
#include <boost/range/adaptor/strided.hpp>
#include <boost/range/adaptor/reversed.hpp>
#include <boost/range/algorithm_ext/push_back.hpp>

#include <boost/test/test_tools.hpp>
#include <boost/test/unit_test.hpp>

#include <list>
#include <vector>
#include <numeric>

namespace boost
{
    namespace
    {

void ticket_9519_strided_reversed_test()
{
    using namespace boost::adaptors;

    std::vector<int> vi;
    for (int i = 0; i < 50; ++i)
    {
        vi.push_back(i);
    }

    std::vector<int> output;
    boost::push_back(output, vi | strided(3) | reversed);

    std::list<int> reference;
    for (int i = 0; i < 50; i += 3)
    {
        reference.push_front(i);
    }

    BOOST_CHECK_EQUAL_COLLECTIONS(output.begin(), output.end(),
                                  reference.begin(), reference.end());
}

    } // anonymous namespace
} // namespace boost

boost::unit_test::test_suite*
init_unit_test_suite(int argc, char* argv[])
{
    boost::unit_test::test_suite* test
        = BOOST_TEST_SUITE(
            "RangeTestSuite.adaptor.ticket_9519_strided_reversed");

    test->add(BOOST_TEST_CASE(&boost::ticket_9519_strided_reversed_test));

    return test;
}

