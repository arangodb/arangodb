//  (C) Copyright Raffi Enficiaud 2017.
//  Distributed under the Boost Software License, Version 1.0.
//  (See accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)

//  See http://www.boost.org/libs/test for the library home page.

#define BOOST_TEST_MODULE message in dataset

#include <boost/test/unit_test.hpp>

#include <boost/test/unit_test_suite.hpp>
#include <boost/test/unit_test_log.hpp>
#include <boost/test/results_collector.hpp>
#include <boost/test/data/monomorphic.hpp>
#include <boost/test/data/test_case.hpp>

#include <boost/test/framework.hpp>
#include <boost/test/unit_test_parameters.hpp>
#include <boost/test/utils/nullstream.hpp>

#include "logger-for-tests.hpp"

// STL
#include <iostream>
#include <ios>


using boost::test_tools::output_test_stream;
using namespace boost::unit_test;



#line 34
std::string filenames[] = { "util/test_image1.jpg", "util/test_image2.jpg" };
BOOST_DATA_TEST_CASE(test_update,
                     boost::unit_test::data::make(filenames))
{
    std::string field_name = "Volume";
    int         value = 100;

    BOOST_TEST_MESSAGE("Testing update :");
    BOOST_TEST_MESSAGE("Update " << field_name << " with " << value);
}

void check_pattern_loggers(
    output_test_stream& output,
    output_format log_format,
    test_unit_id id,
    bool bt_module_failed = false,
    log_level ll = log_successful_tests )
{
    {
      log_setup_teardown holder(output, log_format, ll);

      // output before fixture registration
      output << "* " << log_format << "-format  *******************************************************************";
      output << std::endl;

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
    }

    BOOST_TEST( bt_module_failed == (( results_collector.results( id ).result_code() != 0 ) ));
    BOOST_TEST( output.match_pattern(true) ); // flushes the stream at the end of the comparison.
}

void check_pattern_loggers(
    output_test_stream& output,
    test_suite* ts,
    bool bt_module_failed = false)
{
    ts->p_default_status.value = test_unit::RS_ENABLED;

    check_pattern_loggers( output, OF_CLF, ts->p_id, bt_module_failed );
    check_pattern_loggers( output, OF_XML, ts->p_id, bt_module_failed );
    check_pattern_loggers( output, OF_JUNIT, ts->p_id, bt_module_failed, log_successful_tests );
    check_pattern_loggers( output, OF_JUNIT, ts->p_id, bt_module_failed, log_cpp_exception_errors ); // should branch to the log log_all_errors
}

struct guard {
    ~guard()
    {
        boost::unit_test::unit_test_log.set_format( runtime_config::get<output_format>( runtime_config::btrt_log_format ) );
        boost::unit_test::unit_test_log.set_stream( std::cout );
    }
};


//____________________________________________________________________________//


BOOST_AUTO_TEST_CASE( messages_in_datasets )
{
    guard G;
    boost::ignore_unused( G );

#define PATTERN_FILE_NAME "messages-in-datasets-test.pattern"

    std::string pattern_file_name(
        framework::master_test_suite().argc == 1
            ? (runtime_config::save_pattern() ? PATTERN_FILE_NAME : "./baseline-outputs/" PATTERN_FILE_NAME )
            : framework::master_test_suite().argv[1] );

    output_test_stream_for_loggers test_output( pattern_file_name,
                                                !runtime_config::save_pattern(),
                                                true,
                                                __FILE__ );

    auto dataset = boost::unit_test::data::make(filenames);

    test_unit_generator const& generator = boost::unit_test::data::ds_detail::test_case_gen<test_updatecase, decltype(dataset)>(
        "fake_name",
        __FILE__,
        200,
        std::forward<decltype(dataset)>(dataset) );
    test_suite* ts = BOOST_TEST_SUITE( "fake_datatest_case" );
    while(test_unit *tu = generator.next()) {
        ts->add(tu);
    }

    check_pattern_loggers(test_output, ts);
}
