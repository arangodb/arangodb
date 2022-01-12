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
#include <boost/test/execution_monitor.hpp>

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

void very_bad_exception() {
    BOOST_TEST_INFO("Context value=something");
    BOOST_TEST_INFO("Context value2=something different");
    BOOST_ERROR( "with some message" );

    BOOST_TEST_INFO("exception context should be shown");
    throw local_exception();
}

void bad_foo2() { bad_foo(); } // tests with clashing names

void timeout_foo()
{
    using boost::execution_exception;
    execution_exception::location dummy;
    throw execution_exception(
          execution_exception::timeout_error,
          "fake  timeout",
          dummy);
}

void context_mixed_foo()  {
    BOOST_TEST_CONTEXT("some context") {
        BOOST_TEST( true );
    }

    BOOST_TEST_INFO("other context");
    throw local_exception();
}

//____________________________________________________________________________//

void check( output_test_stream& output,
            output_format log_format,
            test_unit_id id,
            log_level ll = log_successful_tests,
            output_format additional_format = OF_INVALID)
{
    {
      log_setup_teardown holder(output, log_format, ll, invalid_log_level, additional_format);
      output << "* " << log_format << "-format  *******************************************************************";
      output << std::endl;
      framework::finalize_setup_phase( id );
      framework::run( id, false ); // do not continue the test tree to have the test_log_start/end
      output << std::endl;
    }

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

struct save_arguments {
    static bool save_pattern() {
      if(m_first) {
            m_save_pattern = runtime_config::save_pattern();
            m_first = false;
      }
      return m_save_pattern;
    }

    static const std::vector<std::string>& saved_argv() {
        if(argv.empty()) {
            for(size_t i = 0; i < static_cast<size_t>(framework::master_test_suite().argc); i++) {
                argv.push_back(framework::master_test_suite().argv[i]);
            }
        }
        return argv;
    }

    static std::string get_pattern_file(const std::string &to_retrieve) {
        if(argv.empty()) {
            for(size_t i = 0; i < static_cast<size_t>(framework::master_test_suite().argc); i++) {
                argv.push_back(framework::master_test_suite().argv[i]);
            }
        }

        for(size_t i = 0; i < static_cast<size_t>(framework::master_test_suite().argc); i++) {
            if(argv[i].find(to_retrieve) != std::string::npos)
                return argv[i];
        }

        return "";
    }

    static std::vector<std::string> argv;
    static bool m_first;
    static bool m_save_pattern;
};

bool save_arguments::m_save_pattern = false;
bool save_arguments::m_first = true;
std::vector<std::string> save_arguments::argv = std::vector<std::string>();

//____________________________________________________________________________//


BOOST_AUTO_TEST_CASE( test_logs )
{
    guard G;
    boost::ignore_unused( G );

#define PATTERN_FILE_NAME "log-formatter-test.pattern"

    std::string pattern_file_name(
            // we cannot use runtime_config::save_arguments() as one of the test is mutating
            // the arguments for testing purposes
            // we have to inspect argv as b2 may run the program from an unknown location
            save_arguments::saved_argv().size() <= 1
            ? (save_arguments::save_pattern()
               ? PATTERN_FILE_NAME
               : "./baseline-outputs/" PATTERN_FILE_NAME )
            : save_arguments::get_pattern_file(PATTERN_FILE_NAME));

    output_test_stream_for_loggers test_output( pattern_file_name,
                                                !save_arguments::save_pattern(),
                                                true,
                                                __FILE__ );

#line 157
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
        test_case* tc2 = BOOST_TEST_CASE_NAME( bad_foo2 , "bad_foo2<int>" ); // this is for skipped message GH-253
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

    test_suite* ts_timeout = BOOST_TEST_SUITE( "Timeout" );
        ts_timeout->add( BOOST_TEST_CASE( good_foo ) );
        test_case * tc_timeout = BOOST_TEST_CASE( timeout_foo );
        ts_timeout->add( tc_timeout );

    test_suite* ts_timeout_nested = BOOST_TEST_SUITE( "Timeout-nested" );
        ts_timeout_nested->add( BOOST_TEST_CASE( good_foo ) );
        test_suite* ts_timeout_internal = BOOST_TEST_SUITE( "Timeout" );
          ts_timeout_internal->add( BOOST_TEST_CASE( good_foo ) );
          test_case * tc_timeout_internal = BOOST_TEST_CASE( timeout_foo );
          ts_timeout_internal->add( tc_timeout_internal );
        ts_timeout_nested->add( ts_timeout_internal );
        ts_timeout_nested->add( BOOST_TEST_CASE_NAME( good_foo, "good_foo2" ) );

    check( test_output, ts_1 );

    check( test_output, ts_1b );

    check( test_output, ts_1c );

    check( test_output, ts_2 );

    check( test_output, ts_3 );

    check( test_output, ts_4 );

    ts_1->add( BOOST_TEST_CASE_NAME( bad_foo, "bad<bool>" ) );
    ts_3->depends_on( ts_1 );

    check( test_output, ts_main );

    check( test_output, ts_timeout );

    check( test_output, ts_timeout_nested );
}


BOOST_AUTO_TEST_CASE( test_logs_junit_info_closing_tags )
{
    guard G;
    boost::ignore_unused( G );

#define PATTERN_FILE_NAME_JUNIT "log-formatter-test.pattern.junit"

    std::string pattern_file_name(
            // we cannot use runtime_config::save_arguments() as one of the test is mutating
            // the arguments for testing purposes
            // we have to inspect argv as b2 may run the program from an unknown location
            save_arguments::saved_argv().size() <= 1
            ?  (save_arguments::save_pattern()
                ? PATTERN_FILE_NAME_JUNIT
                : "./baseline-outputs/" PATTERN_FILE_NAME_JUNIT )
            : save_arguments::get_pattern_file(PATTERN_FILE_NAME_JUNIT));

    output_test_stream_for_loggers test_output( pattern_file_name,
                                                !save_arguments::save_pattern(),
                                                true,
                                                __FILE__ );

#line 249
    test_suite* ts_main = BOOST_TEST_SUITE( "1 test cases inside" );
    ts_main->add( BOOST_TEST_CASE( almost_good_foo ) );

    ts_main->p_default_status.value = test_unit::RS_ENABLED;
    char const* argv[] = { "a.exe", "--run_test=*", "--build_info" };
    int argc = sizeof(argv)/sizeof(argv[0]);
    boost::unit_test::runtime_config::init( argc, (char**)argv );
    boost::unit_test::framework::impl::setup_for_execution( *ts_main );

    check( test_output, OF_JUNIT, ts_main->p_id, log_successful_tests );

    test_suite* ts_timeout = BOOST_TEST_SUITE( "Timeout" );
        ts_timeout->add( BOOST_TEST_CASE( good_foo ) );
        test_case * tc_timeout = BOOST_TEST_CASE( timeout_foo );
        ts_timeout->add( tc_timeout );

    check( test_output, OF_JUNIT, ts_timeout->p_id, log_successful_tests );


    test_suite* ts_account_failures = BOOST_TEST_SUITE( "1 junit failure is not error" );
        ts_account_failures->add( BOOST_TEST_CASE( bad_foo ) );
        ts_account_failures->add( BOOST_TEST_CASE( very_bad_foo ) );
        ts_account_failures->add( BOOST_TEST_CASE( good_foo ) );
        ts_account_failures->add( BOOST_TEST_CASE( bad_foo2 ) );

    char const* argv2[] = { "a.exe", "--run_test=*", "--build_info=false"  };
    int argc2 = sizeof(argv2)/sizeof(argv2[0]);
    boost::unit_test::runtime_config::init( argc2, (char**)argv2 );
    boost::unit_test::framework::impl::setup_for_execution( *ts_account_failures );

    check( test_output, OF_JUNIT, ts_account_failures->p_id, log_messages );
}


BOOST_AUTO_TEST_CASE( test_logs_context )
{
    guard G;
    boost::ignore_unused( G );

#define PATTERN_FILE_NAME_CONTEXT "log-formatter-context-test.pattern"

    std::string pattern_file_name(
            // we cannot use runtime_config::save_arguments() as one of the test is mutating
            // the arguments for testing purposes
            // we have to inspect argv as b2 may run the program from an unknown location
            save_arguments::saved_argv().size() <= 1
            ?  (save_arguments::save_pattern()
                ? PATTERN_FILE_NAME_CONTEXT
                : "./baseline-outputs/" PATTERN_FILE_NAME_CONTEXT )
            : save_arguments::get_pattern_file(PATTERN_FILE_NAME_CONTEXT));

    output_test_stream_for_loggers test_output( pattern_file_name,
                                                !save_arguments::save_pattern(),
                                                true,
                                                __FILE__ );

#line 330
    test_suite* ts_1 = BOOST_TEST_SUITE( "1 test cases inside" );
        ts_1->add( BOOST_TEST_CASE( context_mixed_foo ) );

    ts_1->p_default_status.value = test_unit::RS_ENABLED;
    check( test_output, OF_CLF, ts_1->p_id, log_successful_tests );
    check( test_output, OF_CLF, ts_1->p_id, log_cpp_exception_errors );
    check( test_output, OF_CLF, ts_1->p_id, log_successful_tests, OF_JUNIT );
    check( test_output, OF_CLF, ts_1->p_id, log_cpp_exception_errors, OF_JUNIT );

    check( test_output, OF_XML, ts_1->p_id, log_successful_tests );
    check( test_output, OF_XML, ts_1->p_id, log_cpp_exception_errors );
    check( test_output, OF_JUNIT, ts_1->p_id, log_successful_tests );
    check( test_output, OF_JUNIT, ts_1->p_id, log_cpp_exception_errors );

}


// EOF
