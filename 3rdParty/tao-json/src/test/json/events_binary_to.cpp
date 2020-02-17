// Copyright (c) 2019-2020 Dr. Colin Hirsch and Daniel Frey
// Please see LICENSE for license or visit https://github.com/taocpp/json/

#include "test.hpp"
#include "test_types.hpp"

#include <tao/json.hpp>

namespace tao::json
{
   void test_base64( const value& v, const std::string& j )
   {
      TEST_ASSERT( to_string< events::binary_to_base64 >( v ) == j );
   }

   void test_base64url( const value& v, const std::string& j )
   {
      TEST_ASSERT( to_string< events::binary_to_base64url >( v ) == j );
   }

   void test_exception( const value& v )
   {
      TEST_THROWS( to_string< events::binary_to_exception >( v ) );
   }

   void test_hex( const value& v, const std::string& j )
   {
      TEST_ASSERT( to_string< events::binary_to_hex >( v ) == j );
   }

   void unit_test()
   {
      value v = null;

      test_base64( v, "null" );
      test_base64url( v, "null" );
      test_hex( v, "null" );

      v = empty_binary;

      test_base64( v, "\"\"" );
      test_base64url( v, "\"\"" );
      test_exception( v );
      test_hex( v, "\"\"" );

      v = binary{ std::byte( 0 ), std::byte( 255 ), std::byte( 42 ), std::byte( 99 ) };

      test_base64( v, "\"AP8qYw==\"" );
      test_base64url( v, "\"AP8qYw\"" );
      test_exception( v );
      test_hex( v, "\"00FF2A63\"" );
   }

}  // namespace tao::json

#include "main.hpp"
