// Copyright (c) 2016-2020 Dr. Colin Hirsch and Daniel Frey
// Please see LICENSE for license or visit https://github.com/taocpp/json/

#include "test.hpp"

#include <tao/json/value.hpp>

#include <tao/json/events/compare.hpp>
#include <tao/json/events/from_string.hpp>
#include <tao/json/events/from_value.hpp>

namespace tao::json
{
   [[nodiscard]] bool test_value( const value& v, events::compare& c )
   {
      c.reset();
      events::from_value( c, v );
      return c.match();
   }

   [[nodiscard]] bool test_parse( const std::string& v, events::compare& c )
   {
      c.reset();
      events::from_string( c, v );
      return c.match();
   }

   void test_null()
   {
      value v_null = null;
      events::compare c_null( v_null );

      TEST_ASSERT( test_value( null, c_null ) );
      TEST_ASSERT( !test_value( true, c_null ) );
      TEST_ASSERT( !test_value( false, c_null ) );
      TEST_ASSERT( !test_value( 0, c_null ) );
      TEST_ASSERT( !test_value( 0U, c_null ) );
      TEST_ASSERT( !test_value( 0.0, c_null ) );
      TEST_ASSERT( !test_value( -42, c_null ) );
      TEST_ASSERT( !test_value( -42.0, c_null ) );
      TEST_ASSERT( !test_value( 42, c_null ) );
      TEST_ASSERT( !test_value( 42U, c_null ) );
      TEST_ASSERT( !test_value( 42.0, c_null ) );
      TEST_ASSERT( !test_value( "", c_null ) );
      TEST_ASSERT( !test_value( "Hello, world!", c_null ) );
      TEST_ASSERT( !test_value( empty_array, c_null ) );
      TEST_ASSERT( !test_value( empty_object, c_null ) );
      TEST_ASSERT( !test_value( value::array( { null } ), c_null ) );
      TEST_ASSERT( !test_value( { { "", null } }, c_null ) );

      TEST_ASSERT( test_parse( "null", c_null ) );
      TEST_ASSERT( !test_parse( "true", c_null ) );
      TEST_ASSERT( !test_parse( "false", c_null ) );
      TEST_ASSERT( !test_parse( "-0", c_null ) );
      TEST_ASSERT( !test_parse( "0", c_null ) );
      TEST_ASSERT( !test_parse( "0.0", c_null ) );
      TEST_ASSERT( !test_parse( "-42", c_null ) );
      TEST_ASSERT( !test_parse( "-42.0", c_null ) );
      TEST_ASSERT( !test_parse( "42", c_null ) );
      TEST_ASSERT( !test_parse( "42.0", c_null ) );
      TEST_ASSERT( !test_parse( "\"\"", c_null ) );
      TEST_ASSERT( !test_parse( "\"Hello, world!\"", c_null ) );
      TEST_ASSERT( !test_parse( "[]", c_null ) );
      TEST_ASSERT( !test_parse( "{}", c_null ) );
      TEST_ASSERT( !test_parse( "[null]", c_null ) );
      TEST_ASSERT( !test_parse( "{\"\":null}", c_null ) );
   }

   void test_boolean()
   {
      events::compare c_true( value( true ) );

      TEST_ASSERT( !test_value( null, c_true ) );
      TEST_ASSERT( test_value( true, c_true ) );
      TEST_ASSERT( !test_value( false, c_true ) );
      TEST_ASSERT( !test_value( 0, c_true ) );
      TEST_ASSERT( !test_value( 0U, c_true ) );
      TEST_ASSERT( !test_value( 0.0, c_true ) );
      TEST_ASSERT( !test_value( 42, c_true ) );
      TEST_ASSERT( !test_value( 42U, c_true ) );
      TEST_ASSERT( !test_value( 42.0, c_true ) );
      TEST_ASSERT( !test_value( "Hello, world!", c_true ) );
      TEST_ASSERT( !test_value( empty_array, c_true ) );
      TEST_ASSERT( !test_value( empty_object, c_true ) );

      TEST_ASSERT( !test_parse( "null", c_true ) );
      TEST_ASSERT( test_parse( "true", c_true ) );
      TEST_ASSERT( !test_parse( "false", c_true ) );
      TEST_ASSERT( !test_parse( "-0", c_true ) );
      TEST_ASSERT( !test_parse( "0", c_true ) );
      TEST_ASSERT( !test_parse( "0.0", c_true ) );
      TEST_ASSERT( !test_parse( "-42", c_true ) );
      TEST_ASSERT( !test_parse( "-42.0", c_true ) );
      TEST_ASSERT( !test_parse( "42", c_true ) );
      TEST_ASSERT( !test_parse( "42.0", c_true ) );
      TEST_ASSERT( !test_parse( "\"\"", c_true ) );
      TEST_ASSERT( !test_parse( "\"Hello, world!\"", c_true ) );
      TEST_ASSERT( !test_parse( "[]", c_true ) );
      TEST_ASSERT( !test_parse( "{}", c_true ) );
      TEST_ASSERT( !test_parse( "[null]", c_true ) );
      TEST_ASSERT( !test_parse( "{\"\":null}", c_true ) );
   }

   void test_number()
   {
      events::compare c_0( value( 0 ) );

      TEST_ASSERT( !test_value( null, c_0 ) );
      TEST_ASSERT( !test_value( true, c_0 ) );
      TEST_ASSERT( !test_value( false, c_0 ) );
      TEST_ASSERT( test_value( 0, c_0 ) );
      TEST_ASSERT( test_value( 0U, c_0 ) );
      TEST_ASSERT( test_value( 0.0, c_0 ) );
      TEST_ASSERT( !test_value( 42, c_0 ) );
      TEST_ASSERT( !test_value( 42U, c_0 ) );
      TEST_ASSERT( !test_value( 42.0, c_0 ) );
      TEST_ASSERT( !test_value( "Hello, world!", c_0 ) );
      TEST_ASSERT( !test_value( empty_array, c_0 ) );
      TEST_ASSERT( !test_value( empty_object, c_0 ) );

      TEST_ASSERT( !test_parse( "null", c_0 ) );
      TEST_ASSERT( !test_parse( "true", c_0 ) );
      TEST_ASSERT( !test_parse( "false", c_0 ) );
      TEST_ASSERT( test_parse( "-0", c_0 ) );
      TEST_ASSERT( test_parse( "0", c_0 ) );
      TEST_ASSERT( test_parse( "0.0", c_0 ) );
      TEST_ASSERT( !test_parse( "-42", c_0 ) );
      TEST_ASSERT( !test_parse( "-42.0", c_0 ) );
      TEST_ASSERT( !test_parse( "42", c_0 ) );
      TEST_ASSERT( !test_parse( "42.0", c_0 ) );
      TEST_ASSERT( !test_parse( "\"\"", c_0 ) );
      TEST_ASSERT( !test_parse( "\"Hello, world!\"", c_0 ) );
      TEST_ASSERT( !test_parse( "[]", c_0 ) );
      TEST_ASSERT( !test_parse( "{}", c_0 ) );
      TEST_ASSERT( !test_parse( "[null]", c_0 ) );
      TEST_ASSERT( !test_parse( "{\"\":null}", c_0 ) );

      events::compare c_42( value( 42 ) );

      TEST_ASSERT( !test_value( null, c_42 ) );
      TEST_ASSERT( !test_value( true, c_42 ) );
      TEST_ASSERT( !test_value( false, c_42 ) );
      TEST_ASSERT( !test_value( 0, c_42 ) );
      TEST_ASSERT( !test_value( 0U, c_42 ) );
      TEST_ASSERT( !test_value( 0.0, c_42 ) );
      TEST_ASSERT( test_value( 42, c_42 ) );
      TEST_ASSERT( test_value( 42U, c_42 ) );
      TEST_ASSERT( test_value( 42.0, c_42 ) );
      TEST_ASSERT( !test_value( "Hello, world!", c_42 ) );
      TEST_ASSERT( !test_value( empty_array, c_42 ) );
      TEST_ASSERT( !test_value( empty_object, c_42 ) );
   }

   void test_string()
   {
      events::compare c_hello_world( value( "Hello, world!" ) );

      TEST_ASSERT( !test_value( null, c_hello_world ) );
      TEST_ASSERT( !test_value( true, c_hello_world ) );
      TEST_ASSERT( !test_value( false, c_hello_world ) );
      TEST_ASSERT( !test_value( 0, c_hello_world ) );
      TEST_ASSERT( !test_value( 0U, c_hello_world ) );
      TEST_ASSERT( !test_value( 0.0, c_hello_world ) );
      TEST_ASSERT( !test_value( 42, c_hello_world ) );
      TEST_ASSERT( !test_value( 42U, c_hello_world ) );
      TEST_ASSERT( !test_value( 42.0, c_hello_world ) );
      TEST_ASSERT( test_value( "Hello, world!", c_hello_world ) );
      TEST_ASSERT( !test_value( "foo", c_hello_world ) );
      TEST_ASSERT( !test_value( empty_array, c_hello_world ) );
      TEST_ASSERT( !test_value( empty_object, c_hello_world ) );

      TEST_ASSERT( !test_parse( "null", c_hello_world ) );
      TEST_ASSERT( !test_parse( "true", c_hello_world ) );
      TEST_ASSERT( !test_parse( "false", c_hello_world ) );
      TEST_ASSERT( !test_parse( "-0", c_hello_world ) );
      TEST_ASSERT( !test_parse( "0", c_hello_world ) );
      TEST_ASSERT( !test_parse( "0.0", c_hello_world ) );
      TEST_ASSERT( !test_parse( "-42", c_hello_world ) );
      TEST_ASSERT( !test_parse( "-42.0", c_hello_world ) );
      TEST_ASSERT( !test_parse( "42", c_hello_world ) );
      TEST_ASSERT( !test_parse( "42.0", c_hello_world ) );
      TEST_ASSERT( !test_parse( "\"\"", c_hello_world ) );
      TEST_ASSERT( test_parse( "\"Hello, world!\"", c_hello_world ) );
      TEST_ASSERT( !test_parse( "[]", c_hello_world ) );
      TEST_ASSERT( !test_parse( "{}", c_hello_world ) );
      TEST_ASSERT( !test_parse( "[null]", c_hello_world ) );
      TEST_ASSERT( !test_parse( "{\"\":null}", c_hello_world ) );
   }

   void test_array()
   {
      events::compare c_empty_array( empty_array );

      TEST_ASSERT( !test_value( null, c_empty_array ) );
      TEST_ASSERT( !test_value( true, c_empty_array ) );
      TEST_ASSERT( !test_value( false, c_empty_array ) );
      TEST_ASSERT( !test_value( 0, c_empty_array ) );
      TEST_ASSERT( !test_value( 0U, c_empty_array ) );
      TEST_ASSERT( !test_value( 0.0, c_empty_array ) );
      TEST_ASSERT( !test_value( 42, c_empty_array ) );
      TEST_ASSERT( !test_value( 42U, c_empty_array ) );
      TEST_ASSERT( !test_value( 42.0, c_empty_array ) );
      TEST_ASSERT( !test_value( "Hello, world!", c_empty_array ) );
      TEST_ASSERT( !test_value( "foo", c_empty_array ) );
      TEST_ASSERT( test_value( empty_array, c_empty_array ) );
      TEST_ASSERT( !test_value( value::array( { empty_array } ), c_empty_array ) );
      TEST_ASSERT( !test_value( value::array( { 1, 2, 3 } ), c_empty_array ) );
      TEST_ASSERT( !test_value( empty_object, c_empty_array ) );

      TEST_ASSERT( !test_parse( "null", c_empty_array ) );
      TEST_ASSERT( !test_parse( "true", c_empty_array ) );
      TEST_ASSERT( !test_parse( "false", c_empty_array ) );
      TEST_ASSERT( !test_parse( "-0", c_empty_array ) );
      TEST_ASSERT( !test_parse( "0", c_empty_array ) );
      TEST_ASSERT( !test_parse( "0.0", c_empty_array ) );
      TEST_ASSERT( !test_parse( "-42", c_empty_array ) );
      TEST_ASSERT( !test_parse( "-42.0", c_empty_array ) );
      TEST_ASSERT( !test_parse( "42", c_empty_array ) );
      TEST_ASSERT( !test_parse( "42.0", c_empty_array ) );
      TEST_ASSERT( !test_parse( "\"\"", c_empty_array ) );
      TEST_ASSERT( !test_parse( "\"Hello, world!\"", c_empty_array ) );
      TEST_ASSERT( test_parse( "[]", c_empty_array ) );
      TEST_ASSERT( !test_parse( "[[]]", c_empty_array ) );
      TEST_ASSERT( !test_parse( "[{}]", c_empty_array ) );
      TEST_ASSERT( !test_parse( "{}", c_empty_array ) );
      TEST_ASSERT( !test_parse( "[null]", c_empty_array ) );
      TEST_ASSERT( !test_parse( "{\"\":null}", c_empty_array ) );

      events::compare c_array( value::array( { 1, 2, 3 } ) );

      TEST_ASSERT( !test_value( null, c_array ) );
      TEST_ASSERT( !test_value( true, c_array ) );
      TEST_ASSERT( !test_value( false, c_array ) );
      TEST_ASSERT( !test_value( 0, c_array ) );
      TEST_ASSERT( !test_value( 0U, c_array ) );
      TEST_ASSERT( !test_value( 0.0, c_array ) );
      TEST_ASSERT( !test_value( 42, c_array ) );
      TEST_ASSERT( !test_value( 42U, c_array ) );
      TEST_ASSERT( !test_value( 42.0, c_array ) );
      TEST_ASSERT( !test_value( "Hello, world!", c_array ) );
      TEST_ASSERT( !test_value( "foo", c_array ) );
      TEST_ASSERT( !test_value( empty_array, c_array ) );
      TEST_ASSERT( !test_value( value::array( { empty_array } ), c_empty_array ) );
      TEST_ASSERT( test_value( value::array( { 1, 2, 3 } ), c_array ) );
      TEST_ASSERT( test_value( value::array( { 1, 2U, 3.0 } ), c_array ) );
      TEST_ASSERT( !test_value( value::array( { 1, 2 } ), c_array ) );
      TEST_ASSERT( !test_value( value::array( { 1, 2, 4 } ), c_array ) );
      TEST_ASSERT( !test_value( value::array( { 1, 2, 4 } ), c_array ) );
      TEST_ASSERT( !test_value( value::array( { 1, 3, 2 } ), c_array ) );
      TEST_ASSERT( !test_value( value::array( { 1, 2, 3, 4 } ), c_array ) );
      TEST_ASSERT( !test_value( value::array( { 1, 2, value::array( { 3 } ) } ), c_array ) );
      TEST_ASSERT( !test_value( empty_object, c_array ) );

      TEST_ASSERT( !test_parse( "null", c_array ) );
      TEST_ASSERT( !test_parse( "true", c_array ) );
      TEST_ASSERT( !test_parse( "false", c_array ) );
      TEST_ASSERT( !test_parse( "-0", c_array ) );
      TEST_ASSERT( !test_parse( "0", c_array ) );
      TEST_ASSERT( !test_parse( "0.0", c_array ) );
      TEST_ASSERT( !test_parse( "-42", c_array ) );
      TEST_ASSERT( !test_parse( "-42.0", c_array ) );
      TEST_ASSERT( !test_parse( "42", c_array ) );
      TEST_ASSERT( !test_parse( "42.0", c_array ) );
      TEST_ASSERT( !test_parse( "\"\"", c_array ) );
      TEST_ASSERT( !test_parse( "\"Hello, world!\"", c_array ) );
      TEST_ASSERT( !test_parse( "[]", c_array ) );
      TEST_ASSERT( test_parse( "[1,2,3]", c_array ) );
      TEST_ASSERT( test_parse( "[1,2,3.0]", c_array ) );
      TEST_ASSERT( !test_parse( "[1,2]", c_array ) );
      TEST_ASSERT( !test_parse( "[1,2,3,4]", c_array ) );
      TEST_ASSERT( !test_parse( "[1,2,4]", c_array ) );
      TEST_ASSERT( !test_parse( "[1,3,2]", c_array ) );
      TEST_ASSERT( !test_parse( "[[1,2,3]]", c_array ) );
      TEST_ASSERT( !test_parse( "[[1],2,3]", c_array ) );
      TEST_ASSERT( !test_parse( "[1,[2],3]", c_array ) );
      TEST_ASSERT( !test_parse( "[1,2,[3]]", c_array ) );
      TEST_ASSERT( !test_parse( "{}", c_array ) );
      TEST_ASSERT( !test_parse( "[null]", c_array ) );
      TEST_ASSERT( !test_parse( "{\"\":null}", c_array ) );

      events::compare c_nested_array( value::array( { 1, 2, value::array( { 3, 4, value::array( { 5 } ) } ) } ) );

      TEST_ASSERT( !test_value( value::array( { 1, 2, value::array( { 3 } ) } ), c_nested_array ) );
      TEST_ASSERT( !test_value( value::array( { 1, 2, value::array( { 3, 4 } ) } ), c_nested_array ) );
      TEST_ASSERT( !test_value( value::array( { 1, 2, value::array( { 3, 4, value::array( {} ) } ) } ), c_nested_array ) );
      TEST_ASSERT( test_value( value::array( { 1, 2, value::array( { 3, 4, value::array( { 5 } ) } ) } ), c_nested_array ) );
      TEST_ASSERT( !test_value( value::array( { 1, 2, value::array( { 3, 4, value::array( { 5, 6 } ) } ) } ), c_nested_array ) );
      TEST_ASSERT( !test_value( value::array( { 1, value::array( { 3, 4, value::array( { 5 } ) } ) } ), c_nested_array ) );
      TEST_ASSERT( !test_value( value::array( { 2, value::array( { 3, 4, value::array( { 5 } ) } ) } ), c_nested_array ) );
      TEST_ASSERT( !test_value( value::array( { 1, 2, value::array( { 3, value::array( { 5 } ) } ) } ), c_nested_array ) );
      TEST_ASSERT( !test_value( value::array( { 1, 2, value::array( { 3, 4, value::array( { 5 } ), 6 } ) } ), c_nested_array ) );
      TEST_ASSERT( !test_value( value::array( { 1, 2, value::array( { 3, 4, value::array( { 5 } ) } ), 6 } ), c_nested_array ) );
   }

   void test_object()
   {
      events::compare c_empty_object( empty_object );

      TEST_ASSERT( !test_value( null, c_empty_object ) );
      TEST_ASSERT( !test_value( true, c_empty_object ) );
      TEST_ASSERT( !test_value( false, c_empty_object ) );
      TEST_ASSERT( !test_value( 0, c_empty_object ) );
      TEST_ASSERT( !test_value( 0U, c_empty_object ) );
      TEST_ASSERT( !test_value( 0.0, c_empty_object ) );
      TEST_ASSERT( !test_value( 42, c_empty_object ) );
      TEST_ASSERT( !test_value( 42U, c_empty_object ) );
      TEST_ASSERT( !test_value( 42.0, c_empty_object ) );
      TEST_ASSERT( !test_value( "Hello, world!", c_empty_object ) );
      TEST_ASSERT( !test_value( "foo", c_empty_object ) );
      TEST_ASSERT( !test_value( empty_array, c_empty_object ) );
      TEST_ASSERT( !test_value( value::array( { empty_array } ), c_empty_object ) );
      TEST_ASSERT( !test_value( value::array( { 1, 2, 3 } ), c_empty_object ) );
      TEST_ASSERT( test_value( empty_object, c_empty_object ) );

      TEST_ASSERT( !test_parse( "null", c_empty_object ) );
      TEST_ASSERT( !test_parse( "true", c_empty_object ) );
      TEST_ASSERT( !test_parse( "false", c_empty_object ) );
      TEST_ASSERT( !test_parse( "-0", c_empty_object ) );
      TEST_ASSERT( !test_parse( "0", c_empty_object ) );
      TEST_ASSERT( !test_parse( "0.0", c_empty_object ) );
      TEST_ASSERT( !test_parse( "-42", c_empty_object ) );
      TEST_ASSERT( !test_parse( "-42.0", c_empty_object ) );
      TEST_ASSERT( !test_parse( "42", c_empty_object ) );
      TEST_ASSERT( !test_parse( "42.0", c_empty_object ) );
      TEST_ASSERT( !test_parse( "\"\"", c_empty_object ) );
      TEST_ASSERT( !test_parse( "\"Hello, world!\"", c_empty_object ) );
      TEST_ASSERT( !test_parse( "[]", c_empty_object ) );
      TEST_ASSERT( !test_parse( "[{}]", c_empty_object ) );
      TEST_ASSERT( test_parse( "{}", c_empty_object ) );
      TEST_ASSERT( !test_parse( "[null]", c_empty_object ) );
      TEST_ASSERT( !test_parse( "{\"\":null}", c_empty_object ) );

      events::compare c_object( value( { { "a", 1 }, { "b", 2 }, { "c", 3 } } ) );

      TEST_ASSERT( !test_value( null, c_object ) );
      TEST_ASSERT( !test_value( true, c_object ) );
      TEST_ASSERT( !test_value( false, c_object ) );
      TEST_ASSERT( !test_value( 0, c_object ) );
      TEST_ASSERT( !test_value( 0U, c_object ) );
      TEST_ASSERT( !test_value( 0.0, c_object ) );
      TEST_ASSERT( !test_value( 42, c_object ) );
      TEST_ASSERT( !test_value( 42U, c_object ) );
      TEST_ASSERT( !test_value( 42.0, c_object ) );
      TEST_ASSERT( !test_value( "Hello, world!", c_object ) );
      TEST_ASSERT( !test_value( "foo", c_object ) );
      TEST_ASSERT( !test_value( empty_array, c_object ) );
      TEST_ASSERT( !test_value( value::array( { empty_array } ), c_empty_object ) );
      TEST_ASSERT( !test_value( value::array( { 1, 2, 3 } ), c_object ) );
      TEST_ASSERT( !test_value( empty_object, c_object ) );
      TEST_ASSERT( !test_value( { { "a", 1 }, { "b", 2 } }, c_object ) );
      TEST_ASSERT( test_value( { { "a", 1 }, { "b", 2 }, { "c", 3 } }, c_object ) );
      TEST_ASSERT( test_value( { { "a", 1 }, { "b", 2U }, { "c", 3.0 } }, c_object ) );
      TEST_ASSERT( !test_value( { { "a", 1 }, { "b", 2 }, { "c", 3 }, { "d", 4 } }, c_object ) );
      TEST_ASSERT( !test_value( { { "a", 1 }, { "b", 2 }, { "c", 2 } }, c_object ) );
      TEST_ASSERT( !test_value( { { "a", 1 }, { "b", 1 }, { "c", 3 } }, c_object ) );
      TEST_ASSERT( !test_value( { { "a", 1 }, { "c", 3 } }, c_object ) );
      TEST_ASSERT( !test_value( { { "b", 2 }, { "c", 3 } }, c_object ) );

      TEST_ASSERT( !test_parse( "null", c_object ) );
      TEST_ASSERT( !test_parse( "true", c_object ) );
      TEST_ASSERT( !test_parse( "false", c_object ) );
      TEST_ASSERT( !test_parse( "-0", c_object ) );
      TEST_ASSERT( !test_parse( "0", c_object ) );
      TEST_ASSERT( !test_parse( "0.0", c_object ) );
      TEST_ASSERT( !test_parse( "-42", c_object ) );
      TEST_ASSERT( !test_parse( "-42.0", c_object ) );
      TEST_ASSERT( !test_parse( "42", c_object ) );
      TEST_ASSERT( !test_parse( "42.0", c_object ) );
      TEST_ASSERT( !test_parse( "\"\"", c_object ) );
      TEST_ASSERT( !test_parse( "\"Hello, world!\"", c_object ) );
      TEST_ASSERT( !test_parse( "[]", c_object ) );
      TEST_ASSERT( !test_parse( "[{}]", c_object ) );
      TEST_ASSERT( !test_parse( "{}", c_object ) );
      TEST_ASSERT( test_parse( "{\"a\":1,\"b\":2,\"c\":3}", c_object ) );
      TEST_ASSERT( test_parse( "{\"a\":1,\"c\":3,\"b\":2}", c_object ) );
      TEST_ASSERT( !test_parse( "{\"a\":1,\"b\":2}", c_object ) );
      TEST_ASSERT( !test_parse( "{\"a\":1,\"b\":2,\"c\":3,\"c\":3}", c_object ) );  // duplicate key -> nope!
      TEST_ASSERT( !test_parse( "{\"a\":1,\"b\":2,\"c\":3,\"d\":4}", c_object ) );
      TEST_ASSERT( !test_parse( "{\"a\":1,\"c\":3}", c_object ) );
      TEST_ASSERT( !test_parse( "[null]", c_object ) );
      TEST_ASSERT( !test_parse( "{\"\":null}", c_object ) );
   }

   void test_mixed()
   {
      const value s = "hallo";
      const value b = true;
      value v = { { "foo", 0 }, { "bar", &b }, { "baz", value::array( { null, &b, false, 0, 1, &s } ) } };

      events::compare c( v );
      events::from_value( c, v );
      TEST_ASSERT( c.match() );

      c.reset();
      v.at( "foo" ) = 42;
      events::from_value( c, v );
      TEST_ASSERT( !c.match() );

      c.reset();
      v.at( "foo" ) = 0;
      events::from_value( c, v );
      TEST_ASSERT( c.match() );

      c.reset();
      v.at( "baz" ).at( 2 ) = 42;
      events::from_value( c, v );
      TEST_ASSERT( !c.match() );

      c.reset();
      v.at( "baz" ).at( 2 ) = true;
      events::from_value( c, v );
      TEST_ASSERT( !c.match() );

      c.reset();
      v.at( "baz" ).at( 2 ) = false;
      events::from_value( c, v );
      TEST_ASSERT( c.match() );
   }

   void unit_test()
   {
      test_null();
      test_boolean();
      test_number();
      test_string();
      test_array();
      test_object();
      test_mixed();
   }

}  // namespace tao::json

#include "main.hpp"
