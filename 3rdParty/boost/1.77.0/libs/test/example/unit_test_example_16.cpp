//  (C) Copyright Raffi Enficiaud 2019.
//  Distributed under the Boost Software License, Version 1.0.
//  (See accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)

//  See http://www.boost.org/libs/test for the library home page.
//
// ***************************************************************************

// Boost.Test
#include <boost/test/unit_test.hpp>
#include <boost/test/unit_test_parameters.hpp>

bool init_unit_test()
{
  using namespace boost::unit_test;

// Having some problems on AppleClang 10.10 / Xcode 6/7
#if !defined(BOOST_TEST_DYN_LINK) || (!defined(BOOST_CLANG) || (BOOST_CLANG != 1) || (__clang_major__ >= 8))
  log_level logLevel = runtime_config::get<log_level>(runtime_config::btrt_log_level);
  std::cout << "Current log level: " << static_cast<int>(logLevel) << std::endl;
#endif
  return true;
}

BOOST_AUTO_TEST_CASE( my_test1 )
{
    BOOST_CHECK( true );
}

int main(int argc, char* argv[])
{
  int retCode = boost::unit_test::unit_test_main( &init_unit_test, argc, argv );
  return retCode;
}
