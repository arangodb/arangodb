// Copyright (c) 2017-2020 Dr. Colin Hirsch and Daniel Frey
// Please see LICENSE for license or visit https://github.com/taocpp/json/

#ifndef TAO_JSON_SRC_TEST_JSON_UNHEX_HPP
#define TAO_JSON_SRC_TEST_JSON_UNHEX_HPP

#include <cassert>
#include <string>

namespace tao::json
{
   [[nodiscard]] inline char test_unhex( const char c )
   {
      if( ( '0' <= c ) && ( c <= '9' ) ) {
         return static_cast< char >( c - '0' );
      }
      if( ( 'a' <= c ) && ( c <= 'f' ) ) {
         return static_cast< char >( c - 'a' + 10 );
      }
      if( ( 'A' <= c ) && ( c <= 'F' ) ) {
         return static_cast< char >( c - 'A' + 10 );
      }
      // LCOV_EXCL_START
      assert( false );
      return 0;
      // LCOV_EXCL_STOP
   }

   [[nodiscard]] inline std::string test_unhex( const std::string& data )
   {
      assert( !( data.size() & 1 ) );
      std::string result;
      result.reserve( data.size() / 2 );
      for( std::string::size_type i = 0; i < data.size(); i += 2 ) {
         result += static_cast< char >( ( test_unhex( data[ i ] ) << 4 ) + test_unhex( data[ i + 1 ] ) );
      }
      return result;
   }

}  // namespace tao::json

#endif
