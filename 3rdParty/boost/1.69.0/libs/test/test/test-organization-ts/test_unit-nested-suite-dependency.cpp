//  (C) Copyright Raffi Enficiaud 2017.
//  Distributed under the Boost Software License, Version 1.0.
//  (See accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)

//  See http://www.boost.org/libs/test for the library home page.
//
//! @file
//! checking the nested suites activation, ticket trac #13149
// *****************************************************************************

#define BOOST_TEST_MODULE test_nested_dependency
#include <boost/test/unit_test.hpp>
#include <boost/test/unit_test_parameters.hpp>
#include <boost/test/tree/test_case_counter.hpp>
#include <boost/test/tree/traverse.hpp>

// initial reproducing snippet on the corresponding ticket
#if 0
BOOST_AUTO_TEST_SUITE(suite1, *boost::unit_test::depends_on("suite2"))
  BOOST_AUTO_TEST_SUITE(suite1_nested)
    BOOST_AUTO_TEST_CASE(suite1_test1)
    {
      BOOST_CHECK(true);
    }
  BOOST_AUTO_TEST_SUITE_END()
BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE(suite2)
  BOOST_AUTO_TEST_CASE(suite2_test2)
  {
    BOOST_CHECK(true);
  }
BOOST_AUTO_TEST_SUITE_END()
#endif

void suite1_test1()
{
  BOOST_CHECK(true);
}

void suite2_test2()
{
  BOOST_CHECK(true);
}

BOOST_AUTO_TEST_CASE( some_test )
{
    using namespace boost::unit_test;

    // test tree
    test_suite* master_ts = BOOST_TEST_SUITE("local master");
    test_suite* t_suite1 = BOOST_TEST_SUITE( "suite1" );
    test_suite* t_suite1_nested = BOOST_TEST_SUITE( "suite1_nested" );
    t_suite1_nested->add( BOOST_TEST_CASE( suite1_test1 ) );
    t_suite1->add(t_suite1_nested);

    test_suite* t_suite2 = BOOST_TEST_SUITE( "suite2" );
    t_suite2->add( BOOST_TEST_CASE( suite2_test2 ) );

    master_ts->add(t_suite1);
    master_ts->add(t_suite2);

    // dependencies
    t_suite1->depends_on( t_suite2 );

    // running
    char const* argv[] = { "dummy-test-module.exe", "--log_level=all", "--run_test=suite1/suite1_nested"};
    int argc = sizeof(argv)/sizeof(argv[0]);

    master_ts->p_default_status.value = test_unit::RS_ENABLED;
    framework::finalize_setup_phase( master_ts->p_id );

    runtime_config::init( argc, (char**)argv );
    framework::impl::setup_for_execution( *master_ts );

    // no need to run
    //framework::run( master_ts );

    // we should have 2 test cases enabled with this run parameters
    test_case_counter tcc;
    traverse_test_tree( master_ts->p_id, tcc );
    BOOST_TEST( tcc.p_count == 2 );
}
