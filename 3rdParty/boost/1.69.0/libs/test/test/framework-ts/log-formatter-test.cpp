//  (C) Copyright Raffi Enficiaud 2016.
//  Distributed under the Boost Software License, Version 1.0.
//  (See accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)

//  See http://www.boost.org/libs/test for the library home page.
//
//  tests Unit Test Framework logging facilities against
//  pattern file
// ***************************************************************************

// Boost.Test
#define BOOST_TEST_MAIN
#include <boost/test/unit_test.hpp>
#include <boost/test/unit_test_log.hpp>
#include <boost/test/unit_test_suite.hpp>
#include <boost/test/framework.hpp>
#include <boost/test/unit_test_parameters.hpp>
#include <boost/test/utils/nullstream.hpp>
typedef boost::onullstream onullstream_type;

// BOOST
#include <boost/lexical_cast.hpp>

// our special logger for tests
#include "logger-for-tests.hpp"

// STL
#include <iostream>
#include <ios>

using boost::test_tools::output_test_stream;
using namespace boost::unit_test;



//____________________________________________________________________________//

// this one should generate a message as it does not execute any assertion
void good_foo() {}

void almost_good_foo()
{
    BOOST_TEST_WARN( 2>3 );
}

void bad_foo()  {
    BOOST_ERROR( "" );

    BOOST_TEST_MESSAGE("this is a message");
    BOOST_CHECK(true);

    BOOST_TEST_INFO("Context value=something");
    BOOST_TEST_INFO("Context value2=something different");
    BOOST_ERROR( "with some message" );

    BOOST_CHECK_MESSAGE( 1 == 2.3, "non sense" );
}

void very_bad_foo()  {
    BOOST_TEST_CONTEXT("some context") {
        BOOST_FAIL( "very_bad_foo is fatal" );
    }
}

struct local_exception {};

void very_bad_exception()  {
    BOOST_TEST_INFO("Context value=something");
    BOOST_TEST_INFO("Context value2=something different");
    BOOST_ERROR( "with some message" );

    BOOST_TEST_INFO("exception context should be shown");
    throw local_exception();
}

void bad_foo2() { bad_foo(); } // tests with clashing names

//____________________________________________________________________________//

void check( output_test_stream& output,
            output_format log_format,
            test_unit_id id,
            log_level ll = log_successful_tests )
{
    boost::unit_test::unit_test_log.set_format(log_format);
    boost::unit_test::unit_test_log.set_stream(output);
    boost::unit_test::unit_test_log.set_threshold_level(ll);

    output << "* " << log_format << "-format  *******************************************************************";
    output << std::endl;
    framework::finalize_setup_phase( id );
    framework::run( id, false ); // do not continue the test tree to have the test_log_start/end
    output << std::endl;

    boost::unit_test::unit_test_log.set_format(OF_CLF);
    boost::unit_test::unit_test_log.set_stream(std::cout);

    BOOST_TEST( output.match_pattern(true) ); // flushes the stream at the end of the comparison.
}

//____________________________________________________________________________//

void check( output_test_stream& output, test_suite* ts )
{
    ts->p_default_status.value = test_unit::RS_ENABLED;

    check( output, OF_CLF, ts->p_id );
    check( output, OF_XML, ts->p_id );
    check( output, OF_JUNIT, ts->p_id, log_successful_tests );
    check( output, OF_JUNIT, ts->p_id, log_cpp_exception_errors ); // should branch to the log log_all_errors
}

//____________________________________________________________________________//

struct guard {
    ~guard()
    {
        boost::unit_test::unit_test_log.set_format( runtime_config::get<output_format>( runtime_config::btrt_log_format ) );
        boost::unit_test::unit_test_log.set_stream( std::cout );
    }
};

//____________________________________________________________________________//


BOOST_AUTO_TEST_CASE( test_logs )
{
    guard G;
    ut_detail::ignore_unused_variable_warning( G );

#define PATTERN_FILE_NAME "log-formatter-test.pattern"

    std::string pattern_file_name(
        framework::master_test_suite().argc <= 1
            ? (runtime_config::save_pattern() ? PATTERN_FILE_NAME : "./baseline-outputs/" PATTERN_FILE_NAME )
            : framework::master_test_suite().argv[1] );

    output_test_stream_for_loggers test_output( pattern_file_name,
                                                !runtime_config::save_pattern(),
                                                true,
                                                __FILE__ );

#line 207
    test_suite* ts_0 = BOOST_TEST_SUITE( "0 test cases inside" );

    test_suite* ts_1 = BOOST_TEST_SUITE( "1 test cases inside" );
        ts_1->add( BOOST_TEST_CASE( good_foo ) );

    test_suite* ts_1b = BOOST_TEST_SUITE( "1 bad test case inside" );
        ts_1b->add( BOOST_TEST_CASE( bad_foo ), 1 );

    test_suite* ts_1c = BOOST_TEST_SUITE( "1 almost good test case inside" );
        ts_1c->add( BOOST_TEST_CASE( almost_good_foo ) );

    test_suite* ts_2 = BOOST_TEST_SUITE( "2 test cases inside" );
        ts_2->add( BOOST_TEST_CASE( good_foo ) );
        ts_2->add( BOOST_TEST_CASE( bad_foo ), 1 );

    test_suite* ts_3 = BOOST_TEST_SUITE( "3 test cases inside" );
        ts_3->add( BOOST_TEST_CASE( bad_foo ) );
        test_case* tc1 = BOOST_TEST_CASE( very_bad_foo );
        ts_3->add( tc1 );
        test_case* tc2 = BOOST_TEST_CASE( bad_foo2 );
        ts_3->add( tc2 );
        tc2->depends_on( tc1 );

    test_suite* ts_4 = BOOST_TEST_SUITE( "4 test cases inside" );
        ts_4->add( BOOST_TEST_CASE( bad_foo ) );
        ts_4->add( BOOST_TEST_CASE( very_bad_foo ) );
        ts_4->add( BOOST_TEST_CASE( very_bad_exception ) );
        ts_4->add( BOOST_TEST_CASE( bad_foo2 ) );

    test_suite* ts_main = BOOST_TEST_SUITE( "Fake Test Suite Hierarchy" );
        ts_main->add( ts_0 );
        ts_main->add( ts_1 );
        ts_main->add( ts_2 );
        ts_main->add( ts_3 );
        ts_main->add( ts_4 );

    check( test_output, ts_1 );

    check( test_output, ts_1b );

    check( test_output, ts_1c );

    check( test_output, ts_2 );

    check( test_output, ts_3 );

    check( test_output, ts_4 );

    ts_1->add( BOOST_TEST_CASE( bad_foo ) );
    ts_3->depends_on( ts_1 );

    check( test_output, ts_main );
}

//____________________________________________________________________________//

BOOST_AUTO_TEST_CASE( test_logs_junit_info_closing_tags )
{
    guard G;
    ut_detail::ignore_unused_variable_warning( G );

#define PATTERN_FILE_NAME_JUNIT "log-formatter-test.pattern.junit"

    std::string pattern_file_name(
        framework::master_test_suite().argc <= 2
            ? (runtime_config::save_pattern() ? PATTERN_FILE_NAME_JUNIT : "./baseline-outputs/" PATTERN_FILE_NAME_JUNIT )
            : framework::master_test_suite().argv[2] );

    output_test_stream_for_loggers test_output( pattern_file_name,
                                                !runtime_config::save_pattern(),
                                                true,
                                                __FILE__ );

#line 218
    test_suite* ts_main = BOOST_TEST_SUITE( "1 test cases inside" );
    ts_main->add( BOOST_TEST_CASE( almost_good_foo ) );

    ts_main->p_default_status.value = test_unit::RS_ENABLED;
    char const* argv[] = { "a.exe", "--run_test=*", "--build_info" };
    int argc = sizeof(argv)/sizeof(argv[0]);
    boost::unit_test::runtime_config::init( argc, (char**)argv );
    boost::unit_test::framework::impl::setup_for_execution( *ts_main );

    check( test_output, OF_JUNIT, ts_main->p_id, log_successful_tests );
}

// EOF
