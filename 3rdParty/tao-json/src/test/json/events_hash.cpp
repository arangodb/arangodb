// Copyright (c) 2016-2020 Dr. Colin Hirsch and Daniel Frey
// Please see LICENSE for license or visit https://github.com/taocpp/json/

#include "test.hpp"

#include <tao/json/value.hpp>

#include <tao/json/events/from_string.hpp>
#include <tao/json/events/from_value.hpp>
#include <tao/json/events/hash.hpp>

namespace tao::json
{
   [[nodiscard]] std::string hash_value( const value& v )
   {
      events::hash h;
      events::from_value( h, v );
      return h.value();
   }

   [[nodiscard]] std::string hash_parse( const std::string& v )
   {
      events::hash h;
      events::from_string( h, v );
      return h.value();
   }

   [[nodiscard]] bool test( const value& v, const std::string& s )
   {
      return hash_value( v ) == hash_parse( s );
   }

   void unit_test()
   {
      TEST_ASSERT( test( null, "null" ) );
      TEST_ASSERT( test( 0, "0" ) );
      TEST_ASSERT( !test( 0, "null" ) );

      TEST_ASSERT( test( empty_array, "[]" ) );
      TEST_ASSERT( test( value::array( { 1, 2U, 3 } ), "[1,2,3.0]" ) );

      TEST_ASSERT( test( empty_object, "{}" ) );
      TEST_ASSERT( test( { { "a", 0 }, { "b", 1 } }, "{\"a\":0,\"b\":1}" ) );
      TEST_ASSERT( test( { { "a", 0 }, { "b", -1 } }, "{\"a\":-0,\"b\":-1}" ) );
      TEST_ASSERT( test( { { "a", 0.0 }, { "b", 1 } }, "{\"a\":0,\"b\":1.0}" ) );
      TEST_ASSERT( test( { { "a", 0 }, { "b", -1.0 } }, "{\"a\":-0.0,\"b\":-1}" ) );
      TEST_ASSERT( test( { { "a", 0 }, { "b", 1 } }, "{ \"a\": 0, \"b\": 1 }" ) );
      TEST_ASSERT( test( { { "a", 0 }, { "b", 1 } }, "{ \"b\": 1, \"a\": 0 }" ) );
      TEST_ASSERT( test( { { "a", true }, { "b", false } }, "{ \"a\": true, \"b\": false }" ) );
      TEST_ASSERT( test( { { "a", "Hello" }, { "b", "World" }, { "c", 1.25 } }, "{ \"a\": \"H\\u0065llo\", \"b\": \"World\", \"c\": 125e-2 }" ) );

      TEST_ASSERT( !test( { { "a", 0 }, { "b", 1 } }, "{ \"a\": 1, \"b\": 1 }" ) );
      TEST_ASSERT( !test( { { "a", 0 }, { "b", 1 } }, "{ \"a\": 0, \"b\": 0 }" ) );
      TEST_ASSERT( !test( { { "a", 0 }, { "b", 1 } }, "{ \"a\": 0 }" ) );
      TEST_ASSERT( !test( { { "a", 0 }, { "b", 1 } }, "{ \"b\": 1 }" ) );
      TEST_ASSERT( !test( { { "a", 0 }, { "b", 1 } }, "{ \"a\": 0, \"b\": 1, \"c\": 2 }" ) );
      TEST_ASSERT( !test( { { "a", 0 }, { "b", 1 } }, "{ \"a\": 0, \"c\": 1 }" ) );
      TEST_ASSERT( !test( { { "a", 0 }, { "b", 1 } }, "{ \"c\": 0, \"b\": 1 }" ) );

      TEST_THROWS( test( { { "a", 0 }, { "b", 1 } }, "{ \"a\": 0, \"a\": 0, \"c\": 1 }" ) );
   }

}  // namespace tao::json

#include "main.hpp"
