//  (C) Copyright Raffi Enficiaud 2019.
//  Distributed under the Boost Software License, Version 1.0.
//  (See accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)

//  See http://www.boost.org/libs/test for the library home page.
//
// ***************************************************************************

// Boost.Test

#define BOOST_TEST_MODULE junit count skipped tests
#include <boost/test/included/unit_test.hpp>

// our special logger for tests
#include "logger-for-tests.hpp"

// STL
#include <iostream>
#include <ios>

using boost::test_tools::output_test_stream;
using namespace boost::unit_test;
namespace utf = boost::unit_test;
namespace tt = boost::test_tools;


struct skip_with_message
{
    static bool default_enabled; // to control the status from outside
  
    std::string message;
    skip_with_message(std::string m)
    : message(m) {}

    tt::assertion_result operator()(utf::test_unit_id)
    {
        tt::assertion_result ans(skip_with_message::default_enabled);
        ans.message() << "test is skipped because " << message;
        return ans;
    }
};
bool skip_with_message::default_enabled = false;

void test_1()
{
    BOOST_CHECK_EQUAL( 2 + 2, 4 );
}

void test_2()
{
    BOOST_CHECK_EQUAL( 0, 1 );
}

void test_3()
{
    BOOST_CHECK_EQUAL( 0, 1 );
}

void check( output_test_stream& output,
            output_format log_format,
            test_unit_id id,
            log_level ll = log_successful_tests )
{
    class reset_status : public test_tree_visitor {
    private:
        virtual bool    visit( test_unit const& tu )
        {
            const_cast<test_unit&>(tu).p_default_status.value = test_case::RS_INHERIT;
            const_cast<test_unit&>(tu).p_run_status.value = test_case::RS_INVALID;
            return true;
        }
    } rstatus;

    {
      log_setup_teardown holder(output, log_format, ll);

      // reinit the default/run status otherwise we cannot apply the decorators
      // after the first run
      traverse_test_tree( id, rstatus, true );
      framework::get<test_suite>(id).p_default_status.value = test_unit::RS_ENABLED;

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
    check( output, OF_CLF, ts->p_id );
    check( output, OF_XML, ts->p_id );
    check( output, OF_JUNIT, ts->p_id, log_successful_tests );
    check( output, OF_JUNIT, ts->p_id, log_cpp_exception_errors );
}

struct guard {
    ~guard()
    {
        boost::unit_test::unit_test_log.set_format( runtime_config::get<output_format>( runtime_config::btrt_log_format ) );
        boost::unit_test::unit_test_log.set_stream( std::cout );
    }
};


BOOST_AUTO_TEST_CASE( test_logs )
{
    guard G;
    boost::ignore_unused( G );

#define PATTERN_FILE_NAME "log-count-skipped-tests.pattern"

    std::string pattern_file_name(
        framework::master_test_suite().argc <= 1
            ? (runtime_config::save_pattern() ? PATTERN_FILE_NAME : "./baseline-outputs/" PATTERN_FILE_NAME )
            : framework::master_test_suite().argv[1] );

    output_test_stream_for_loggers test_output( pattern_file_name,
                                                !runtime_config::save_pattern(),
                                                true,
                                                __FILE__ );

    test_case* tc1 = BOOST_TEST_CASE(test_1);
    test_case* tc2 = BOOST_TEST_CASE(test_2);
    test_case* tc3 = BOOST_TEST_CASE(test_3);
  
    // add decorators to the tests, should happen only once. The status will be reset in the check.
    decorator::collector_t* decorator_collector = &(*utf::precondition(skip_with_message("-some precondition-")));
    decorator_collector->store_in( *tc2 );
    decorator_collector->reset();

    decorator_collector = &(* utf::disabled());
    decorator_collector->store_in( *tc3 );
    decorator_collector->reset();

    test_suite* ts_main = BOOST_TEST_SUITE( "fake master test suite" );
        ts_main->add( tc1 );
        ts_main->add( tc2 );
        ts_main->add( tc3 );

    check( test_output, ts_main );
  
    // change precondition
    skip_with_message::default_enabled = true;
    check( test_output, ts_main );

    //
    // disabling sub suites and subtests
    test_suite* ts_main2 = BOOST_TEST_SUITE( "fake master test suite2" );
        ts_main2->add( tc1 ); // active
        ts_main2->add( tc2 ); // conditionally disabled
        ts_main2->add( tc3 ); // disabled

    // all disabled: count increases by 2
    test_suite* ts_sub1 = BOOST_TEST_SUITE( "child1" );
        ts_sub1->add( BOOST_TEST_CASE_NAME(test_1, "t1"));
        ts_sub1->add( BOOST_TEST_CASE_NAME(test_1, "t2"));

    test_case* tc_2_1 = BOOST_TEST_CASE(test_1); // disabled
    test_suite* ts_sub2 = BOOST_TEST_SUITE( "child2" ); // conditionally disabled
        ts_sub2->add( tc_2_1 );
        ts_sub2->add( BOOST_TEST_CASE_NAME(test_1, "t2"));
  
    ts_main2->add(ts_sub1);
    ts_main2->add(ts_sub2);
  

    decorator_collector = &(* utf::disabled());
    decorator_collector->store_in( *ts_sub1 );
    decorator_collector->reset();

    decorator_collector = &(* utf::disabled());
    decorator_collector->store_in( *tc_2_1 );
    decorator_collector->reset();
  
    decorator_collector = &(*utf::precondition(skip_with_message("-some precondition-")));
    decorator_collector->store_in( *ts_sub2 );
    decorator_collector->reset();


    skip_with_message::default_enabled = false;
    check( test_output, ts_main2 );
    // count disabled = 2 (main) + 2 (ts_sub1) + 2 (ts_sub2)
  
    // change precondition
    skip_with_message::default_enabled = true;
    check( test_output, ts_main2 );
    // count disabled = 1 (main) + 2 (ts_sub1) + 1 (ts_sub2)
}
