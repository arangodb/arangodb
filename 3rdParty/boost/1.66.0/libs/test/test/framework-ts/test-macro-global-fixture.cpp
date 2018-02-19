//  (C) Copyright Raffi Enficiaud 2016.
//  Distributed under the Boost Software License, Version 1.0.
//  (See accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)

//  See http://www.boost.org/libs/test for the library home page.

// checks issue https://svn.boost.org/trac/boost/ticket/5563

#define BOOST_TEST_MODULE test_macro_in_global_fixture
#include <boost/test/unit_test.hpp>
#include <boost/test/unit_test_log.hpp>
#include <boost/test/results_collector.hpp>
#include <boost/test/unit_test_suite.hpp>
#include <boost/test/framework.hpp>
#include <boost/test/unit_test_parameters.hpp>
#include <boost/test/utils/nullstream.hpp>
typedef boost::onullstream onullstream_type;
#include <boost/test/utils/algorithm.hpp>

// BOOST
#include <boost/lexical_cast.hpp>

// our special logger for tests
#include "logger-for-tests.hpp"

// STL
#include <iostream>
#include <ios>

using boost::test_tools::output_test_stream;
using namespace boost::unit_test;
namespace utf = boost::unit_test;


template < void (*function_to_call)() >
struct GlobalFixtureWithCtor {
    GlobalFixtureWithCtor() {
        BOOST_TEST_MESSAGE("GlobalFixtureWithCtor: ctor");
        (*function_to_call)();
        // having a message is ok
        // although it should break existing logger consistency
    }
    ~GlobalFixtureWithCtor() {
        BOOST_TEST_MESSAGE("GlobalFixtureWithCtor: dtor");
    }
};

template < void (*function_to_call)() >
struct GlobalFixtureWithSetup {
    GlobalFixtureWithSetup() {
        BOOST_TEST_MESSAGE("GlobalFixtureWithSetup ctor");
    }
    virtual ~GlobalFixtureWithSetup() {
        BOOST_TEST_MESSAGE("GlobalFixtureWithSetup dtor");
    }

    virtual void setup() {
        BOOST_TEST_MESSAGE("GlobalFixtureWithSetup::setup-calling function");
        (*function_to_call)();
        BOOST_TEST_MESSAGE("GlobalFixtureWithSetup::setup-calling function done");
    }
};

template < void (*function_to_call)() >
struct GlobalFixtureWithTeardown {
    GlobalFixtureWithTeardown() {
        BOOST_TEST_MESSAGE("GlobalFixtureWithTeardown ctor");
    }
    virtual ~GlobalFixtureWithTeardown() {
        BOOST_TEST_MESSAGE("GlobalFixtureWithTeardown dtor");
    }

    virtual void teardown() {
        BOOST_TEST_MESSAGE("GlobalFixtureWithTeardown::teardown-calling function");
        (*function_to_call)();
        BOOST_TEST_MESSAGE("GlobalFixtureWithTeardown::teardown-calling function done");
    }
};

template <class global_fixture_t >
void check_global_fixture(
    output_test_stream& output,
    output_format log_format,
    test_unit_id id,
    bool bt_module_failed = false,
    bool has_setup_error = false,
    log_level ll = log_successful_tests )
{
    boost::unit_test::unit_test_log.set_format(log_format);
    boost::unit_test::unit_test_log.set_stream(output);
    boost::unit_test::unit_test_log.set_threshold_level(ll);

    // output before fixture registration
    output << "* " << log_format << "-format  *******************************************************************";
    output << std::endl;

    // register this as a global fixture
    boost::unit_test::ut_detail::global_fixture_impl<global_fixture_t> fixture_stack_element;
    framework::finalize_setup_phase( id );

    bool setup_error_caught = false;
    try {
        framework::run( id, false ); // do not continue the test tree to have the test_log_start/end
    }
    catch (framework::setup_error&) {
        BOOST_TEST_MESSAGE("Framework setup_error caught");
        setup_error_caught = true;
    }

    output << std::endl;

    // we do not want the result of the comparison go to the "output" stream
    boost::unit_test::unit_test_log.set_format(OF_CLF);
    boost::unit_test::unit_test_log.set_stream(std::cout);

    BOOST_TEST( setup_error_caught == has_setup_error );
    BOOST_TEST( bt_module_failed == (( results_collector.results( id ).result_code() != 0 ) || setup_error_caught ));
    BOOST_TEST( output.match_pattern(true) ); // flushes the stream at the end of the comparison.
}

template <class global_fixture_t>
void check_global_fixture(
    output_test_stream& output,
    test_suite* ts,
    bool bt_module_failed = false,
    bool has_setup_error = false)
{
    ts->p_default_status.value = test_unit::RS_ENABLED;

    check_global_fixture<global_fixture_t>( output, OF_CLF, ts->p_id, bt_module_failed, has_setup_error );
    check_global_fixture<global_fixture_t>( output, OF_XML, ts->p_id, bt_module_failed, has_setup_error );
    check_global_fixture<global_fixture_t>( output, OF_JUNIT, ts->p_id, bt_module_failed, has_setup_error, log_successful_tests );
    check_global_fixture<global_fixture_t>( output, OF_JUNIT, ts->p_id, bt_module_failed, has_setup_error, log_cpp_exception_errors ); // should branch to the log log_all_errors
}

struct guard {
    ~guard()
    {
        boost::unit_test::unit_test_log.set_format( runtime_config::get<output_format>( runtime_config::btrt_log_format ) );
        boost::unit_test::unit_test_log.set_stream( std::cout );
    }
};

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

BOOST_AUTO_TEST_CASE( some_test )
{
    guard G;
    ut_detail::ignore_unused_variable_warning( G );
#line 275
    test_suite* ts_1 = BOOST_TEST_SUITE( "1 test cases inside" );
        ts_1->add( BOOST_TEST_CASE( good_foo ) );

    test_suite* ts_1b = BOOST_TEST_SUITE( "1 bad test case inside" );
        ts_1b->add( BOOST_TEST_CASE( bad_foo ), 1 );

    test_suite* ts_main = BOOST_TEST_SUITE( "Fake Test Suite Hierarchy" );
        ts_main->add( BOOST_TEST_CASE( bad_foo ) );
        ts_main->add( BOOST_TEST_CASE( very_bad_foo ) );
        ts_main->add( BOOST_TEST_CASE( very_bad_exception ) );
        ts_main->add( ts_1 );
        ts_main->add( ts_1b );

    // we need another tree
    test_suite* ts2_0 = BOOST_TEST_SUITE( "0 test cases inside" );
    test_suite* ts2_1 = BOOST_TEST_SUITE( "1 test cases inside" );
        ts2_1->add( BOOST_TEST_CASE( good_foo ) );

    test_suite* ts_main_no_error = BOOST_TEST_SUITE( "Fake Test Suite Hierarchy no errors" );
        ts_main_no_error->add( ts2_0 );
        ts_main_no_error->add( BOOST_TEST_CASE( almost_good_foo ) );
        ts_main_no_error->add( ts2_1 );

#define PATTERN_FILE_NAME "global-fixtures-test.pattern"

    std::string pattern_file_name(
        framework::master_test_suite().argc == 1
            ? (runtime_config::save_pattern() ? PATTERN_FILE_NAME : "./baseline-outputs/" PATTERN_FILE_NAME )
            : framework::master_test_suite().argv[1] );


    output_test_stream_for_loggers test_output( pattern_file_name,
                                                !runtime_config::save_pattern(),
                                                true,
                                                __FILE__ );

    // legacy API, we test that we catch exceptions in the ctor, and tests
    // in the suite are running or not depending on that
    test_output << "***********************" << std::endl;
    test_output << "*********************** GlobalFixtureWithCtor<&good_foo>" << std::endl;
    test_output << "***********************" << std::endl;
    check_global_fixture< GlobalFixtureWithCtor<&good_foo> >( test_output, ts_main, true );
    check_global_fixture< GlobalFixtureWithCtor<&good_foo> >( test_output, ts_main_no_error, false);

    test_output << "***********************" << std::endl;
    test_output << "*********************** GlobalFixtureWithCtor<&almost_good_foo>" << std::endl;
    test_output << "***********************" << std::endl;
    check_global_fixture< GlobalFixtureWithCtor<&almost_good_foo> >( test_output, ts_main, true );
    check_global_fixture< GlobalFixtureWithCtor<&almost_good_foo> >( test_output, ts_main_no_error, false );

    test_output << "***********************" << std::endl;
    test_output << "*********************** GlobalFixtureWithCtor<&bad_foo>" << std::endl;
    test_output << "***********************" << std::endl;
    check_global_fixture< GlobalFixtureWithCtor<&bad_foo> >( test_output, ts_main, true ); // should fail the module
    check_global_fixture< GlobalFixtureWithCtor<&bad_foo> >( test_output, ts_main_no_error, true );

    test_output << "***********************" << std::endl;
    test_output << "*********************** GlobalFixtureWithCtor<&very_bad_foo>" << std::endl;
    test_output << "***********************" << std::endl;
    check_global_fixture< GlobalFixtureWithCtor<&very_bad_foo> >( test_output, ts_main, true ); // should fail the module
    check_global_fixture< GlobalFixtureWithCtor<&very_bad_foo> >( test_output, ts_main_no_error, true );

    test_output << "***********************" << std::endl;
    test_output << "*********************** GlobalFixtureWithCtor<&very_bad_exception>" << std::endl;
    test_output << "***********************" << std::endl;
    check_global_fixture< GlobalFixtureWithCtor<&very_bad_exception> >( test_output, ts_main, true ); // should fail the module
    check_global_fixture< GlobalFixtureWithCtor<&very_bad_exception> >( test_output, ts_main_no_error, true );

    // here we test only for the setup function, tests should not be
    // executed when setup fails, setup should be allowed to fail

    // setup does not fail
    test_output << "***********************" << std::endl;
    test_output << "*********************** GlobalFixtureWithSetup<&good_foo>" << std::endl;
    test_output << "***********************" << std::endl;
    check_global_fixture< GlobalFixtureWithSetup<&good_foo> >( test_output, ts_main, true );
    check_global_fixture< GlobalFixtureWithSetup<&good_foo> >( test_output, ts_main_no_error, false );

    // setup does not fail, with messages
    test_output << "***********************" << std::endl;
    test_output << "*********************** GlobalFixtureWithSetup<&almost_good_foo>" << std::endl;
    test_output << "***********************" << std::endl;
    check_global_fixture< GlobalFixtureWithSetup<&almost_good_foo> >( test_output, ts_main, true );
    check_global_fixture< GlobalFixtureWithSetup<&almost_good_foo> >( test_output, ts_main_no_error, false );

    // setup fails
    test_output << "***********************" << std::endl;
    test_output << "*********************** GlobalFixtureWithSetup<&bad_foo>" << std::endl;
    test_output << "***********************" << std::endl;
    check_global_fixture< GlobalFixtureWithSetup<&bad_foo> >( test_output, ts_main, true );
    check_global_fixture< GlobalFixtureWithSetup<&bad_foo> >( test_output, ts_main_no_error, true );

    // setup fails badly
    test_output << "***********************" << std::endl;
    test_output << "*********************** GlobalFixtureWithSetup<&very_bad_foo>" << std::endl;
    test_output << "***********************" << std::endl;
    check_global_fixture< GlobalFixtureWithSetup<&very_bad_foo> >( test_output, ts_main, true );
    check_global_fixture< GlobalFixtureWithSetup<&very_bad_foo> >( test_output, ts_main_no_error, true );

    // setup fails with exception
    test_output << "***********************" << std::endl;
    test_output << "*********************** GlobalFixtureWithSetup<&very_bad_exception>" << std::endl;
    test_output << "***********************" << std::endl;
    check_global_fixture< GlobalFixtureWithSetup<&very_bad_exception> >( test_output, ts_main, true );
    check_global_fixture< GlobalFixtureWithSetup<&very_bad_exception> >( test_output, ts_main_no_error, true );

    // here we test only for the teardown function, tests should not be
    // executed when setup fails, setup should be allowed to fail

    // teardown does not fail
    test_output << "***********************" << std::endl;
    test_output << "*********************** GlobalFixtureWithTeardown<&good_foo>" << std::endl;
    test_output << "***********************" << std::endl;
    check_global_fixture< GlobalFixtureWithTeardown<&good_foo> >( test_output, ts_main, true );
    check_global_fixture< GlobalFixtureWithTeardown<&good_foo> >( test_output, ts_main_no_error, false );

    // teardown does not fail, with messages
    test_output << "***********************" << std::endl;
    test_output << "*********************** GlobalFixtureWithTeardown<&almost_good_foo>" << std::endl;
    test_output << "***********************" << std::endl;
    check_global_fixture< GlobalFixtureWithTeardown<&almost_good_foo> >( test_output, ts_main, true );
    check_global_fixture< GlobalFixtureWithTeardown<&almost_good_foo> >( test_output, ts_main_no_error, false );

    // teardown fails
    test_output << "***********************" << std::endl;
    test_output << "*********************** GlobalFixtureWithTeardown<&bad_foo>" << std::endl;
    test_output << "***********************" << std::endl;
    check_global_fixture< GlobalFixtureWithTeardown<&bad_foo> >( test_output, ts_main, true );
    check_global_fixture< GlobalFixtureWithTeardown<&bad_foo> >( test_output, ts_main_no_error, true );

    // teardown fails badly
    test_output << "***********************" << std::endl;
    test_output << "*********************** GlobalFixtureWithTeardown<&very_bad_foo>" << std::endl;
    test_output << "***********************" << std::endl;
    check_global_fixture< GlobalFixtureWithTeardown<&very_bad_foo> >( test_output, ts_main, true );
    check_global_fixture< GlobalFixtureWithTeardown<&very_bad_foo> >( test_output, ts_main_no_error, true );

    // teardown fails with exception
    test_output << "***********************" << std::endl;
    test_output << "*********************** GlobalFixtureWithTeardown<&very_bad_exception>" << std::endl;
    test_output << "***********************" << std::endl;
    check_global_fixture< GlobalFixtureWithTeardown<&very_bad_exception> >( test_output, ts_main, true );
    check_global_fixture< GlobalFixtureWithTeardown<&very_bad_exception> >( test_output, ts_main_no_error, true );

}
