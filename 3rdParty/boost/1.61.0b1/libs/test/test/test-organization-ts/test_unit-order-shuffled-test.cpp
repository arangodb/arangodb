//  (C) Copyright Raffi Enficiaud 2016.
//  Distributed under the Boost Software License, Version 1.0.
//  (See accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)

//  See http://www.boost.org/libs/test for the library home page.
//
/// @file
/// @brief tests order of the running unit tests under shuffling
// ***************************************************************************

// Boost.Test
#define BOOST_TEST_MODULE test unit order shuffled test
#include <boost/test/included/unit_test.hpp>
#include <boost/test/unit_test_log.hpp>
#include <boost/test/tree/visitor.hpp>
#include <boost/test/utils/string_cast.hpp>

#include <boost/test/utils/nullstream.hpp>
typedef boost::onullstream onullstream_type;
namespace ut = boost::unit_test;
namespace tt = boost::test_tools;

#include <cstddef>
#include <iostream>

//____________________________________________________________________________//

void some_test() {}
#define TC( name )                                                      \
boost::unit_test::make_test_case( boost::function<void ()>(some_test),  \
                                  BOOST_TEST_STRINGIZE( name ),         \
                                  __FILE__, __LINE__ )

//____________________________________________________________________________//

struct tu_order_collector : ut::test_observer {
    virtual void    test_unit_start( ut::test_unit const& tu )
    {
        //std::cout << "## TU: " << tu.full_name() << std::endl;
        m_order.push_back( tu.p_id );
    }

    std::vector<ut::test_unit_id> m_order;
};

//____________________________________________________________________________//

static tu_order_collector
run_tree( ut::test_suite* master )
{
    std::cout << "## TU: START" << std::endl;
    tu_order_collector c;
    ut::framework::register_observer( c );

    master->p_default_status.value = ut::test_unit::RS_ENABLED;
    ut::framework::finalize_setup_phase( master->p_id );
    ut::framework::impl::setup_for_execution( *master );

    onullstream_type    null_output;
    ut::unit_test_log.set_stream( null_output );

    ut::framework::run( master );
    ut::unit_test_log.set_stream( std::cout );

    ut::framework::deregister_observer( c );

    return c;
}

//____________________________________________________________________________//

struct test_tree {
    test_tree() {
        master = BOOST_TEST_SUITE( "master" );

        std::size_t nb_ts = (std::max)(3, std::rand() % 17);
        std::vector<ut::test_suite*> tsuites(1, master); // master is in there

        for(std::size_t s = 0; s < nb_ts; s++)
        {
          tsuites.push_back(BOOST_TEST_SUITE( "ts1_" +  boost::unit_test::utils::string_cast(s)));
          master->add( tsuites.back() );
        }

        std::size_t nb_ts2 = (std::max)(3, std::rand() % 11);
        for(std::size_t s = 0; s < nb_ts2; s++)
        {
          tsuites.push_back(BOOST_TEST_SUITE( "ts2_" +  boost::unit_test::utils::string_cast(s)));
          tsuites[std::rand() % nb_ts]->add( tsuites.back() ); // picking a random one in the first level
        }

        // generating N tests units, associating them to an aribtrary test suite
        for(std::size_t s = 0; s < 10; s++)
        {
            ut::test_case* tc = boost::unit_test::make_test_case(
                                    boost::function<void ()>(some_test),
                                    "tc_" +  boost::unit_test::utils::string_cast(s),
                                    __FILE__, __LINE__ );
            tsuites[std::rand() % tsuites.size()]->add(tc);
        }

    }

    ut::test_suite* master;
};

//____________________________________________________________________________//
BOOST_FIXTURE_TEST_CASE( test_no_seed, test_tree )
{
    // no seed set
    ut::runtime_config::s_arguments_store.template set<unsigned int>(ut::runtime_config::RANDOM_SEED, 0);

    tu_order_collector res1 = run_tree( master );
    tu_order_collector res2 = run_tree( master );

    BOOST_TEST( res1.m_order == res2.m_order, tt::per_element() );
}

BOOST_FIXTURE_TEST_CASE( test_seed_to_time, test_tree )
{
    // seed = 1 means current time is used.
    ut::runtime_config::s_arguments_store.template set<unsigned int>(ut::runtime_config::RANDOM_SEED, 1);

    tu_order_collector res1 = run_tree( master );
    tu_order_collector res2 = run_tree( master );

    BOOST_TEST( res1.m_order != res2.m_order ); // some elements might be the same, but not the full sequences
}

BOOST_FIXTURE_TEST_CASE( test_seed_identical, test_tree )
{
    // seed = 1 means current time is used.
    unsigned int seed = static_cast<unsigned int>( std::time( 0 ) );
    ut::runtime_config::s_arguments_store.template set<unsigned int>(ut::runtime_config::RANDOM_SEED, seed);
    tu_order_collector res1 = run_tree( master );

    ut::runtime_config::s_arguments_store.template set<unsigned int>(ut::runtime_config::RANDOM_SEED, seed);
    tu_order_collector res2 = run_tree( master );

    BOOST_TEST( res1.m_order == res2.m_order, tt::per_element() );

    // using time seed now
    ut::runtime_config::s_arguments_store.template set<unsigned int>(ut::runtime_config::RANDOM_SEED, 1);
    tu_order_collector res3 = run_tree( master );
    BOOST_TEST( res1.m_order != res3.m_order ); // some elements might be the same, but not the full sequences


}
