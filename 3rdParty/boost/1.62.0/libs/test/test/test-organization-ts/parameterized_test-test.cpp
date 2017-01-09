//  (C) Copyright Gennadiy Rozental 2002-2015.
//  Distributed under the Boost Software License, Version 1.0.
//  (See accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)

//  See http://www.boost.org/libs/test for the library home page.
//
//  Description : tests parameterized tests
//  Note: this file should be compatible with C++03 compilers (features in boost.test v2)
// ***************************************************************************

// Boost.Test
#define BOOST_TEST_MAIN
#include <boost/test/unit_test.hpp>
#include <boost/test/parameterized_test.hpp>
#include <boost/test/unit_test_log.hpp>
#include <boost/test/results_collector.hpp>
#include <boost/test/utils/nullstream.hpp>
typedef boost::onullstream onullstream_type;

namespace ut = boost::unit_test;

// BOOST
#include <boost/scoped_ptr.hpp>

// STL
#include <list>
#include <iostream>

//____________________________________________________________________________//

void test0( int i )
{
    BOOST_TEST( (i%2 == 0) ); // amounts to BOOST_CHECK, for backward compatibility wrt. boost.test v2
}

//____________________________________________________________________________//

void test1( int i )
{
    BOOST_TEST( (i%2 == 0) );
    if( i%3 == 0 ) {
        throw 124;
    }
}

//____________________________________________________________________________//

static void
setup_tree( ut::test_suite* master_tu )
{
    master_tu->p_default_status.value = ut::test_unit::RS_ENABLED;
    ut::framework::finalize_setup_phase( master_tu->p_id );
}

//____________________________________________________________________________//

BOOST_AUTO_TEST_CASE( test_case1 )
{
    onullstream_type    null_output;
    ut::test_suite* test = BOOST_TEST_SUITE( "" );

    ut::unit_test_log.set_stream( null_output );
    int test_data[] = { 2, 2, 2 };
    test->add( BOOST_PARAM_TEST_CASE( &test0, (int*)test_data, (int*)test_data + sizeof(test_data)/sizeof(int) ) );

    setup_tree( test );
    ut::framework::run( test );
    ut::test_results const& tr = ut::results_collector.results( test->p_id );

    ut::unit_test_log.set_stream( std::cout );
    BOOST_TEST( tr.p_assertions_failed == 0U );
    BOOST_TEST( !tr.p_aborted );
}

//____________________________________________________________________________//

BOOST_AUTO_TEST_CASE( test_case2 )
{
    onullstream_type    null_output;
    ut::test_suite* test = BOOST_TEST_SUITE( "" );

    ut::unit_test_log.set_stream( null_output );
    int test_data[] = { 1, 2, 2 };
    test->add( BOOST_PARAM_TEST_CASE( &test0, (int*)test_data, (int*)test_data + sizeof(test_data)/sizeof(int) ) );

    setup_tree( test );
    ut::framework::run( test );
    ut::test_results const& tr = ut::results_collector.results( test->p_id );

    ut::unit_test_log.set_stream( std::cout );
    BOOST_TEST( tr.p_assertions_failed == 1U );
    BOOST_TEST( !tr.p_aborted );
}

//____________________________________________________________________________//

BOOST_AUTO_TEST_CASE( test_case3 )
{
    onullstream_type    null_output;
    ut::test_suite* test = BOOST_TEST_SUITE( "" );

    ut::unit_test_log.set_stream( null_output );
    int test_data[] = { 1, 1, 2 };
    test->add( BOOST_PARAM_TEST_CASE( &test0, (int*)test_data, (int*)test_data + sizeof(test_data)/sizeof(int) ) );

    setup_tree( test );
    ut::framework::run( test );
    ut::test_results const& tr = ut::results_collector.results( test->p_id );

    ut::unit_test_log.set_stream( std::cout );
    BOOST_TEST( tr.p_assertions_failed == 2U );
    BOOST_TEST( !tr.p_aborted );
}

//____________________________________________________________________________//

BOOST_AUTO_TEST_CASE( test_case4 )
{
    onullstream_type    null_output;
    ut::test_suite* test = BOOST_TEST_SUITE( "" );

    ut::unit_test_log.set_stream( null_output );
    int test_data[] = { 1, 1, 1 };
    test->add( BOOST_PARAM_TEST_CASE( &test0, (int*)test_data, (int*)test_data + sizeof(test_data)/sizeof(int) ) );

    setup_tree( test );
    ut::framework::run( test );
    ut::test_results const& tr = ut::results_collector.results( test->p_id );

    ut::unit_test_log.set_stream( std::cout );
    BOOST_TEST( tr.p_assertions_failed == 3U );
    BOOST_TEST( !tr.p_aborted );
}

//____________________________________________________________________________//

BOOST_AUTO_TEST_CASE( test_case5 )
{
    onullstream_type    null_output;
    ut::test_suite* test = BOOST_TEST_SUITE( "" );

    ut::unit_test_log.set_stream( null_output );
    int test_data[] = { 6, 6, 6 };
    test->add( BOOST_PARAM_TEST_CASE( &test1, (int*)test_data, (int*)test_data + sizeof(test_data)/sizeof(int) ) );

    setup_tree( test );
    ut::framework::run( test );
    ut::test_results const& tr = ut::results_collector.results( test->p_id );

    ut::unit_test_log.set_stream( std::cout );
    BOOST_TEST( tr.p_assertions_failed == 3U );
    BOOST_TEST( !tr.p_aborted );
    BOOST_TEST( !tr.passed() );
}

//____________________________________________________________________________//

BOOST_AUTO_TEST_CASE( test_case6 )
{
    onullstream_type    null_output;
    ut::test_suite* test = BOOST_TEST_SUITE( "" );

    ut::unit_test_log.set_stream( null_output );
    int test_data[] = { 0, 3, 9 };
    test->add( BOOST_PARAM_TEST_CASE( &test1, (int*)test_data, (int*)test_data + sizeof(test_data)/sizeof(int) ) );

    setup_tree( test );
    ut::framework::run( test );
    ut::test_results const& tr = ut::results_collector.results( test->p_id );

    ut::unit_test_log.set_stream( std::cout );
    BOOST_TEST( tr.p_assertions_failed == 5U );
    BOOST_TEST( !tr.p_aborted );
}

//____________________________________________________________________________//

BOOST_AUTO_TEST_CASE( test_case7 )
{
    onullstream_type    null_output;
    ut::test_suite* test = BOOST_TEST_SUITE( "" );

    ut::unit_test_log.set_stream( null_output );
    int test_data[] = { 2, 3, 9 };
    test->add( BOOST_PARAM_TEST_CASE( &test1, (int*)test_data, (int*)test_data + sizeof(test_data)/sizeof(int) ) );

    setup_tree( test );
    ut::framework::run( test );
    ut::test_results const& tr = ut::results_collector.results( test->p_id );

    ut::unit_test_log.set_stream( std::cout );
    BOOST_TEST( tr.p_assertions_failed == 4U );
    BOOST_TEST( !tr.p_aborted );
}

//____________________________________________________________________________//

BOOST_AUTO_TEST_CASE( test_case8 )
{
    onullstream_type    null_output;
    ut::test_suite* test = BOOST_TEST_SUITE( "" );

    ut::unit_test_log.set_stream( null_output );
    int test_data[] = { 3, 2, 6 };
    test->add( BOOST_PARAM_TEST_CASE( &test1, (int*)test_data, (int*)test_data + sizeof(test_data)/sizeof(int) ) );

    setup_tree( test );
    ut::framework::run( test );
    ut::test_results const& tr = ut::results_collector.results( test->p_id );

    ut::unit_test_log.set_stream( std::cout );
    BOOST_TEST( tr.p_assertions_failed == 3U );
    BOOST_TEST( !tr.p_aborted );
}

//____________________________________________________________________________//

// EOF
