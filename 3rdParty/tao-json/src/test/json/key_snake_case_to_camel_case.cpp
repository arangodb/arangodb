// Copyright (c) 2017-2020 Dr. Colin Hirsch and Daniel Frey
// Please see LICENSE for license or visit https://github.com/taocpp/json/

#include "test.hpp"

#include <tao/json/to_string.hpp>
#include <tao/json/value.hpp>

#include <tao/json/events/key_snake_case_to_camel_case.hpp>

namespace tao::json
{
   void test( const value& v, const std::string& s )
   {
      TEST_ASSERT( to_string< events::key_snake_case_to_camel_case >( v ) == s );
   }

   void unit_test()
   {
      test( { { "foo_bar", 42 } }, "{\"fooBar\":42}" );

      test( { { "foo_bar", "foo_bar" } }, "{\"fooBar\":\"foo_bar\"}" );

      test( { { "foo_", 42 } }, "{\"foo_\":42}" );
      test( { { "foo__bar", 42 } }, "{\"foo_Bar\":42}" );
      test( { { "_foo", 42 } }, "{\"Foo\":42}" );
      test( { { "_Foo", 42 } }, "{\"_Foo\":42}" );
      test( { { "foo_Bar", 42 } }, "{\"foo_Bar\":42}" );
   }

}  // namespace tao::json

#include "main.hpp"
