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
#include <boost/test/included/unit_test.hpp>
#include <boost/test/unit_test_log.hpp>
#include <boost/test/tools/output_test_stream.hpp>
#include <boost/test/unit_test_suite.hpp>
#include <boost/test/framework.hpp>
#include <boost/test/unit_test_parameters.hpp>
#include <boost/test/utils/nullstream.hpp>
typedef boost::onullstream onullstream_type;
#include <boost/test/utils/algorithm.hpp>

// BOOST
#include <boost/lexical_cast.hpp>

// STL
#include <iostream>
#include <ios>

using boost::test_tools::output_test_stream;
using namespace boost::unit_test;
namespace utf = boost::unit_test;


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

struct log_guard {
    ~log_guard()
    {
        unit_test_log.set_stream( std::cout );
    }
};

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


//____________________________________________________________________________//

void check( output_test_stream& output, output_format log_format, test_unit_id id )
{
    boost::unit_test::unit_test_log.set_format(log_format);
    boost::unit_test::unit_test_log.set_stream(output);
    boost::unit_test::unit_test_log.set_threshold_level(log_successful_tests);

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
    check( output, OF_JUNIT, ts->p_id );
}

//____________________________________________________________________________//

struct guard {
    ~guard()
    {
        boost::unit_test::unit_test_log.set_format( runtime_config::get<output_format>( runtime_config::LOG_FORMAT ) );
        boost::unit_test::unit_test_log.set_stream( std::cout );
    }
};



class output_test_stream_for_loggers : public output_test_stream {

public:
    explicit output_test_stream_for_loggers(
        boost::unit_test::const_string    pattern_file_name = boost::unit_test::const_string(),
        bool                              match_or_save     = true,
        bool                              text_or_binary    = true )
    : output_test_stream(pattern_file_name, match_or_save, text_or_binary)
    {}

    static std::string normalize_path(const std::string &str) {
        const std::string to_look_for[] = {"\\"};
        const std::string to_replace[]  = {"/"};
        return utils::replace_all_occurrences_of(
                    str,
                    to_look_for, to_look_for + sizeof(to_look_for)/sizeof(to_look_for[0]),
                    to_replace, to_replace + sizeof(to_replace)/sizeof(to_replace[0])
              );
    }

    static std::string get_basename() {
        static std::string basename;

        if(basename.empty()) {
            basename = normalize_path(__FILE__);
            std::string::size_type basename_pos = basename.rfind('/');
            if(basename_pos != std::string::npos) {
                 basename = basename.substr(basename_pos+1);
            }
        }
        return basename;
    }

    virtual std::string get_stream_string_representation() const {
        std::string current_string = output_test_stream::get_stream_string_representation();

        std::string pathname_fixes;
        {
            static const std::string to_look_for[] = {normalize_path(__FILE__)};
            static const std::string to_replace[]  = {"xxx/" + get_basename() };
            pathname_fixes = utils::replace_all_occurrences_of(
                current_string,
                to_look_for, to_look_for + sizeof(to_look_for)/sizeof(to_look_for[0]),
                to_replace, to_replace + sizeof(to_replace)/sizeof(to_replace[0])
            );
        }

        std::string other_vars_fixes;
        {
            static const std::string to_look_for[] = {"time=\"*\"",
                                                      get_basename() + "(*):",
                                                      "unknown location(*):",
                                                      "; testing time: *us\n", // removing this is far more easier than adding a testing time
                                                      "; testing time: *ms\n",
                                                      "<TestingTime>*</TestingTime>",
                                                      "condition 2>3 is not satisfied\n",
                                                      "condition 2>3 is not satisfied]",
                                                      };
                                                      
            static const std::string to_replace[]  = {"time=\"0.1234\"",
                                                      get_basename() + ":*:" ,
                                                      "unknown location:*:",
                                                      "\n",
                                                      "\n",
                                                      "<TestingTime>ZZZ</TestingTime>",
                                                      "condition 2>3 is not satisfied [2 <= 3]\n",
                                                      "condition 2>3 is not satisfied [2 <= 3]]",
                                                      };

            other_vars_fixes = utils::replace_all_occurrences_with_wildcards(
                pathname_fixes,
                to_look_for, to_look_for + sizeof(to_look_for)/sizeof(to_look_for[0]),
                to_replace, to_replace + sizeof(to_replace)/sizeof(to_replace[0])
            );
        }

        return other_vars_fixes;
    }

};

//____________________________________________________________________________//


BOOST_AUTO_TEST_CASE( test_result_reports )
{
    guard G;
    ut_detail::ignore_unused_variable_warning( G );

#define PATTERN_FILE_NAME "log-formatter-test.pattern"

    std::string pattern_file_name(
        framework::master_test_suite().argc == 1
            ? (runtime_config::save_pattern() ? PATTERN_FILE_NAME : "./baseline-outputs/" PATTERN_FILE_NAME )
            : framework::master_test_suite().argv[1] );

    output_test_stream_for_loggers test_output( pattern_file_name, !runtime_config::save_pattern() );

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
        test_case* tc2 = BOOST_TEST_CASE( bad_foo );
        ts_3->add( tc2 );
        tc2->depends_on( tc1 );

    test_suite* ts_4 = BOOST_TEST_SUITE( "4 test cases inside" );
        ts_4->add( BOOST_TEST_CASE( bad_foo ) );
        ts_4->add( BOOST_TEST_CASE( very_bad_foo ) );
        ts_4->add( BOOST_TEST_CASE( very_bad_exception ) );
        ts_4->add( BOOST_TEST_CASE( bad_foo ) );

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

// EOF
