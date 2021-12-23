// Boost.Range library
//
//  Copyright Neil Groves 2014. Use, modification and
//  distribution is subject to the Boost Software License, Version
//  1.0. (See accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)
//
// For more information, see http://www.boost.org/libs/range/
//

#include <boost/range/iterator_range_hash.hpp>

#include <boost/test/test_tools.hpp>
#include <boost/test/unit_test.hpp>

#include <vector>

namespace boost_range_test
{
    namespace
    {

void test_iterator_range_hash()
{
    std::vector<boost::int32_t> v;

    for (boost::int32_t i = 0; i < 10; ++i)
    {
        v.push_back(i);
    }

    std::size_t ref_output = boost::hash_range(v.begin(), v.end());

    boost::iterator_range<std::vector<boost::int32_t>::iterator> rng(v);

    std::size_t test_output = boost::hash_value(rng);

    BOOST_CHECK_EQUAL(ref_output, test_output);
}

    } // anonymous namespace
} // namespace boost_range_test

boost::unit_test::test_suite* init_unit_test_suite( int argc, char* argv[] )
{
    boost::unit_test::test_suite* test =
        BOOST_TEST_SUITE("Boost.Range iterator_range hash function");

    test->add(BOOST_TEST_CASE(&boost_range_test::test_iterator_range_hash));

    return test;
}
