// Copyright (c) 2016-2020 Dr. Colin Hirsch and Daniel Frey
// Please see LICENSE for license or visit https://github.com/taocpp/json/

#include "test.hpp"

#include <tao/json/stream.hpp>
#include <tao/json/to_string.hpp>
#include <tao/json/value.hpp>

#include <sstream>

namespace tao::json
{
   void test_simple( const value& v, const std::string& s )
   {
      TEST_ASSERT( to_string( v ) == s );
      std::ostringstream oss;
      oss << v;
      TEST_ASSERT( oss.str() == s );
   }

   void test_pretty( const value& v, const std::string& s )
   {
      std::ostringstream oss;
      oss << std::setw( 2 ) << v;
      TEST_ASSERT( oss.str() == s );
   }

   void test_pretty_crlf( const value& v, const std::string& s )
   {
      std::ostringstream oss;
      to_stream( oss, v, 2, "\r\n" );
      TEST_ASSERT( oss.str() == s );
   }

   void unit_test()
   {
      value e;
      value d = 42;

      const value v = std::move( d );

      TEST_THROWS( to_string( e ) );

      test_simple( null, "null" );
      test_simple( true, "true" );
      test_simple( false, "false" );
      test_simple( 42, "42" );
      test_simple( 42U, "42" );
      test_simple( 42.1, "42.1" );
      test_simple( "foo", "\"foo\"" );
      test_simple( empty_array, "[]" );
      test_simple( value::array( {} ), "[]" );
      test_simple( value::array( { 1 } ), "[1]" );
      test_simple( value::array( { 1, 2, 3 } ), "[1,2,3]" );
      test_simple( empty_object, "{}" );
      test_simple( { { "foo", 42 } }, "{\"foo\":42}" );
      test_simple( { { "foo", 42U } }, "{\"foo\":42}" );
      test_simple( { { "foo", 42 }, { "bar", 43 } }, "{\"bar\":43,\"foo\":42}" );
      test_simple( { { "foo", 42 }, { "bar", 43U } }, "{\"bar\":43,\"foo\":42}" );
      test_simple( { { "foo", v }, { "bar", 43U } }, "{\"bar\":43,\"foo\":42}" );
      test_simple( { { "foo", &v }, { "bar", 43U } }, "{\"bar\":43,\"foo\":42}" );
      test_simple( { { "foo", value::array( { 1, { { "bar", 42 }, { "baz", value::array( { empty_object, 43 } ) } }, empty_array } ) } }, "{\"foo\":[1,{\"bar\":42,\"baz\":[{},43]},[]]}" );

      test_pretty( null, "null" );
      test_pretty( true, "true" );
      test_pretty( false, "false" );
      test_pretty( 42, "42" );
      test_pretty( 42.1, "42.1" );
      test_pretty( "foo", "\"foo\"" );
      test_pretty( empty_array, "[]" );
      test_pretty( value::array( {} ), "[]" );
      test_pretty( value::array( { 1 } ), "[\n  1\n]" );
      test_pretty( value::array( { 1, 2U, 3 } ), "[\n  1,\n  2,\n  3\n]" );
      test_pretty( empty_object, "{}" );
      test_pretty( { { "foo", 42 } }, "{\n  \"foo\": 42\n}" );
      test_pretty( { { "foo", 42 }, { "bar", 43U } }, "{\n  \"bar\": 43,\n  \"foo\": 42\n}" );
      test_pretty( { { "foo", v }, { "bar", 43U } }, "{\n  \"bar\": 43,\n  \"foo\": 42\n}" );
      test_pretty( { { "foo", &v }, { "bar", 43U } }, "{\n  \"bar\": 43,\n  \"foo\": 42\n}" );
      test_pretty( { { "foo", value::array( { 1, { { "bar", 42 }, { "baz", value::array( { empty_object, 43 } ) } }, empty_array } ) } }, "{\n  \"foo\": [\n    1,\n    {\n      \"bar\": 42,\n      \"baz\": [\n        {},\n        43\n      ]\n    },\n    []\n  ]\n}" );

      test_pretty_crlf( null, "null" );
      test_pretty_crlf( true, "true" );
      test_pretty_crlf( false, "false" );
      test_pretty_crlf( 42, "42" );
      test_pretty_crlf( 42.1, "42.1" );
      test_pretty_crlf( "foo", "\"foo\"" );
      test_pretty_crlf( empty_array, "[]" );
      test_pretty_crlf( value::array( {} ), "[]" );
      test_pretty_crlf( value::array( { 1 } ), "[\r\n  1\r\n]" );
      test_pretty_crlf( value::array( { 1, 2U, 3 } ), "[\r\n  1,\r\n  2,\r\n  3\r\n]" );
      test_pretty_crlf( empty_object, "{}" );
      test_pretty_crlf( { { "foo", 42 } }, "{\r\n  \"foo\": 42\r\n}" );
      test_pretty_crlf( { { "foo", 42 }, { "bar", 43U } }, "{\r\n  \"bar\": 43,\r\n  \"foo\": 42\r\n}" );
      test_pretty_crlf( { { "foo", v }, { "bar", 43U } }, "{\r\n  \"bar\": 43,\r\n  \"foo\": 42\r\n}" );
      test_pretty_crlf( { { "foo", &v }, { "bar", 43U } }, "{\r\n  \"bar\": 43,\r\n  \"foo\": 42\r\n}" );
      test_pretty_crlf( { { "foo", value::array( { 1, { { "bar", 42 }, { "baz", value::array( { empty_object, 43 } ) } }, empty_array } ) } }, "{\r\n  \"foo\": [\r\n    1,\r\n    {\r\n      \"bar\": 42,\r\n      \"baz\": [\r\n        {},\r\n        43\r\n      ]\r\n    },\r\n    []\r\n  ]\r\n}" );
   }

}  // namespace tao::json

#include "main.hpp"
