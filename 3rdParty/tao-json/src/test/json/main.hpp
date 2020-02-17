// Copyright (c) 2015-2020 Dr. Colin Hirsch and Daniel Frey
// Please see LICENSE for license or visit https://github.com/taocpp/json/

#ifndef TAO_JSON_INCLUDE_SRC_TEST_MAIN_HPP
#define TAO_JSON_INCLUDE_SRC_TEST_MAIN_HPP

#include <cstdlib>
#include <iostream>

int main( int /*unused*/, char** argv )
{
   tao::json::unit_test();

   if( tao::json::failed != 0 ) {
      std::cerr << "json: unit test " << argv[ 0 ] << " failed " << tao::json::failed << std::endl;
   }
   return ( tao::json::failed == 0 ) ? EXIT_SUCCESS : EXIT_FAILURE;
}

#endif
