// Boost.Range library
//
//  Copyright Neil Groves 2011. Use, modification and
//  distribution is subject to the Boost Software License, Version
//  1.0. (See accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)
//
//
// For more information, see http://www.boost.org/libs/range/
//
#include <boost/range/iterator_range.hpp>
#include <boost/unordered_map.hpp>

#include <boost/test/test_tools.hpp>
#include <boost/test/unit_test.hpp>

namespace boost
{
    namespace
    {
        // Ticket 10336 - compilation error in iterator_range and unordered_map
        void test_ticket_10336()
        {
            typedef boost::unordered_map<int,int> container_t;
            typedef container_t::const_iterator citer_t;
            typedef boost::iterator_range<citer_t> rng_t;
            
            const container_t c;
            rng_t rng(c.begin(), c.end());
        }
    }
}

boost::unit_test::test_suite*
init_unit_test_suite(int argc, char* argv[])
{
    boost::unit_test::test_suite* test
        = BOOST_TEST_SUITE( "RangeTestSuite.ticket_10336" );

    test->add( BOOST_TEST_CASE( &boost::test_ticket_10336 ) );

    return test;
}
