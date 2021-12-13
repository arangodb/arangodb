//  (C) Copyright Raffi Enficiaud 2018.
//  Distributed under the Boost Software License, Version 1.0.
//  (See accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)

//  See http://www.boost.org/libs/test for the library home page.
//
//  Checks boost::exception, see https://github.com/boostorg/test/issues/147
// ***************************************************************************

// Boost.Test
#define BOOST_TEST_MAIN
#include <boost/test/unit_test.hpp>
#include <boost/test/tools/output_test_stream.hpp>

// BOOST
#include <boost/exception/exception.hpp>
#include <boost/exception/info.hpp>
#include <boost/cstdint.hpp>

#include <iostream>
#include <ios>

using boost::test_tools::output_test_stream;
using namespace boost::unit_test;

typedef boost::error_info<struct tag_error_code, boost::uint32_t> error_code;
typedef boost::error_info<struct tag_error_string, std::string> error_string;
struct myexception : std::exception, virtual boost::exception
{};

// this one should generate a message as it does not execute any assertion
void exception_raised() {
    BOOST_THROW_EXCEPTION( myexception() << error_code(123) << error_string("error%%string") );
}

struct output_test_stream2 : public output_test_stream {
  std::string get_stream_string_representation() const {
      (void)const_cast<output_test_stream2*>(this)->output_test_stream::check_length(0, false); // to sync only!!
      return output_test_stream::get_stream_string_representation();
  }
};

BOOST_AUTO_TEST_CASE( test_logs )
{
    test_suite* ts_main = BOOST_TEST_SUITE( "fake master" );
    ts_main->add( BOOST_TEST_CASE( exception_raised ) );

    output_test_stream2 test_output;

    ts_main->p_default_status.value = test_unit::RS_ENABLED;
    boost::unit_test::unit_test_log.set_stream(test_output);
    boost::unit_test::unit_test_log.set_threshold_level( log_successful_tests );

    framework::finalize_setup_phase( ts_main->p_id );
    framework::run( ts_main->p_id, false ); // do not continue the test tree to have the test_log_start/end

    boost::unit_test::unit_test_log.set_stream(std::cout);

    std::string error_string(test_output.get_stream_string_representation());
    // the message is "Dynamic exception type: boost::exception_detail::clone_impl<myexception>" on Unix
    // and "Dynamic exception type: boost::exception_detail::clone_impl<struct myexception>" on Windows.
    // Also contains "[tag_error_code*] = 123" on Unix and "[struct tag_error_code * __ptr64] = 123" On Windows
    // Also contains "[tag_error_string*] = error%%string" on Unix and "[struct tag_error_string * __ptr64] = error%%string" On Windows
    BOOST_TEST(error_string.find("tag_error_code") != std::string::npos);
    BOOST_TEST(error_string.find("= 123") != std::string::npos);
    BOOST_TEST(error_string.find("tag_error_string") != std::string::npos);
    BOOST_TEST(error_string.find("= error%%string") != std::string::npos);
    BOOST_TEST(error_string.find("Dynamic exception type") != std::string::npos);
    BOOST_TEST(error_string.find("boost::wrapexcept<") != std::string::npos);
    BOOST_TEST(error_string.find("myexception>") != std::string::npos);
}
