// Copyright (c) 2015-2020 Dr. Colin Hirsch and Daniel Frey
// Please see LICENSE for license or visit https://github.com/taocpp/json/

#include "test.hpp"

#include <tao/json/from_string.hpp>
#include <tao/json/value.hpp>

namespace tao::json
{
   [[nodiscard]] value custom_from_string( const char* v )
   {
      return json::from_string( v );
   }

   [[nodiscard]] value custom_from_string( const std::string& v )
   {
      return json::from_string( v );
   }

   void test_string()
   {
      TEST_ASSERT( custom_from_string( "\"\"" ) == value( "" ) );

      TEST_THROWS( custom_from_string( "\"" ) );
      TEST_THROWS( custom_from_string( "\"\r\n\"" ) );

      // TODO...
   }

   void test_array()
   {
      const auto v = custom_from_string( "[ null, true, false, 42, 43.0, \"foo\", [ 1, 2, 3 ], { \"a\" : \"b\", \"c\" : \"d\" } ]" );

      TEST_ASSERT( v.type() == type::ARRAY );

      const auto& a = v.get_array();

      TEST_ASSERT( a.size() == 8 );
      TEST_ASSERT( a[ 0 ] == value( null ) );
      TEST_ASSERT( a[ 1 ] == value( true ) );
      TEST_ASSERT( a[ 2 ] == value( false ) );
      TEST_ASSERT( a[ 3 ] == value( 42 ) );
      TEST_ASSERT( a[ 4 ] == value( 43.0 ) );
      TEST_ASSERT( a[ 5 ] == value( "foo" ) );
      TEST_ASSERT( a[ 6 ].type() == type::ARRAY );
      TEST_ASSERT( a[ 6 ].get_array().size() == 3 );
      TEST_ASSERT( a[ 6 ].get_array()[ 0 ] == value( 1 ) );
      TEST_ASSERT( a[ 6 ].get_array()[ 1 ] == value( 2 ) );
      TEST_ASSERT( a[ 6 ].get_array()[ 2 ] == value( 3 ) );
      TEST_ASSERT( a[ 7 ].type() == type::OBJECT );
      TEST_ASSERT( a[ 7 ].get_object().size() == 2 );
      TEST_ASSERT( a[ 7 ].get_object().at( "a" ).get_string() == "b" );
      TEST_ASSERT( a[ 7 ].get_object().at( "c" ).get_string() == "d" );
   }

   void test_object()
   {
      const auto v = custom_from_string( "{ \"a\": null, \"b\" : true, \"c\"  : false , \"d\" : 42, \"e\" : 43.0   , \"f\" : \"foo\", \"g\" : [ 1, 2, 3 ], \"h\" : { \"a\" : \"b\", \"c\" : \"d\" } }" );

      TEST_ASSERT( v.type() == type::OBJECT );

      const auto& o = v.get_object();

      TEST_ASSERT( o.size() == 8 );
      TEST_ASSERT( o.at( "a" ) == value( null ) );
      TEST_ASSERT( o.at( "b" ) == value( true ) );
      TEST_ASSERT( o.at( "c" ) == value( false ) );
      TEST_ASSERT( o.at( "d" ) == value( 42 ) );
      TEST_ASSERT( o.at( "e" ) == value( 43.0 ) );
      TEST_ASSERT( o.at( "f" ) == value( "foo" ) );
      TEST_ASSERT( o.at( "g" ).type() == type::ARRAY );
      TEST_ASSERT( o.at( "g" ).get_array().size() == 3 );
      TEST_ASSERT( o.at( "g" ).get_array()[ 0 ] == value( 1 ) );
      TEST_ASSERT( o.at( "g" ).get_array()[ 1 ] == value( 2 ) );
      TEST_ASSERT( o.at( "g" ).get_array()[ 2 ] == value( 3 ) );
      TEST_ASSERT( o.at( "h" ).type() == type::OBJECT );
      TEST_ASSERT( o.at( "h" ).get_object().size() == 2 );
      TEST_ASSERT( o.at( "h" ).get_object().at( "a" ).get_string() == "b" );
      TEST_ASSERT( o.at( "h" ).get_object().at( "c" ).get_string() == "d" );
   }

   void unit_test()
   {
      TEST_ASSERT( custom_from_string( "null" ) == value( null ) );
      TEST_ASSERT( custom_from_string( "42" ) == value( 42 ) );
      TEST_ASSERT( custom_from_string( "42.0" ) == value( 42.0 ) );
      TEST_ASSERT( custom_from_string( "true" ) == value( true ) );
      TEST_ASSERT( custom_from_string( "false" ) == value( false ) );

      TEST_ASSERT( custom_from_string( "0" ).get_unsigned() == 0 );
      TEST_ASSERT( custom_from_string( "1" ).get_unsigned() == 1 );
      TEST_ASSERT( custom_from_string( "-1" ).get_signed() == -1 );

      // full signed and unsigned 64 bit integer range
      TEST_ASSERT( custom_from_string( "9223372036854775807" ).get_unsigned() == 9223372036854775807 );
      TEST_ASSERT( custom_from_string( "-9223372036854775808" ).get_signed() == -9223372036854775807LL - 1 );
      TEST_ASSERT( custom_from_string( "18446744073709551615" ).get_unsigned() == 18446744073709551615ULL );

      // anything beyond is double
      TEST_ASSERT( custom_from_string( "-9223372036854775809" ).is_double() );
      TEST_ASSERT( custom_from_string( "18446744073709551616" ).is_double() );

      TEST_ASSERT( custom_from_string( "0" ) == 0 );
      TEST_ASSERT( custom_from_string( "1" ) == 1 );
      TEST_ASSERT( custom_from_string( "-1" ) == -1 );

      TEST_ASSERT( custom_from_string( "0" ).get_unsigned() == 0 );
      TEST_ASSERT( custom_from_string( "-0" ).get_signed() == 0 );

      TEST_ASSERT( custom_from_string( "9223372036854775807" ) == 9223372036854775807 );
      TEST_ASSERT( custom_from_string( "-9223372036854775808" ) == -9223372036854775807LL - 1 );
      TEST_ASSERT( custom_from_string( "18446744073709551615" ) == 18446744073709551615ULL );

      TEST_ASSERT( custom_from_string( "\"foo\"" ) == value( "foo" ) );
      TEST_ASSERT( custom_from_string( "\"f\177o\"" ) == value( "f\177o" ) );

      // TODO: This is sometimes allowed to allow embedded null-bytes within a null-terminated string. (Modified UTF-8)
      TEST_THROWS( custom_from_string( "\"f\300\200o\"" ) );  // Codepoint 0x00 as 2 byte UTF-8 - overlong encoding.

      TEST_THROWS( custom_from_string( "\"f\300\201o\"" ) );          // Codepoint 0x01 as 2 byte UTF-8 - overlong encoding.
      TEST_THROWS( custom_from_string( "\"f\340\200\201o\"" ) );      // Codepoint 0x01 as 3 byte UTF-8 - overlong encoding.
      TEST_THROWS( custom_from_string( "\"f\360\200\200\201o\"" ) );  // Codepoint 0x01 as 4 byte UTF-8 - overlong encoding.

      TEST_THROWS( custom_from_string( "\"f\355\240\200o\"" ) );      // Codepoint 0xD800 as UTF-8 - UTF-16 surrogate.
      TEST_THROWS( custom_from_string( "\"f\364\220\200\200o\"" ) );  // Codepoint 0x110000 as UTF-8 - codepoint too large.

      test_array();
      test_object();

      // TODO: Other integer tests missing from integer.cpp?

      TEST_THROWS( custom_from_string( "1" + std::string( internal::max_mantissa_digits, '0' ) ) );  // Throws due to overflow.
      TEST_THROWS( custom_from_string( "2" + std::string( internal::max_mantissa_digits, '1' ) ) );  // Throws due to overflow.

      TEST_THROWS( custom_from_string( "" ) );
      TEST_THROWS( custom_from_string( "[" ) );
      TEST_THROWS( custom_from_string( "{" ) );
      TEST_THROWS( custom_from_string( "]" ) );
      TEST_THROWS( custom_from_string( "}" ) );
      TEST_THROWS( custom_from_string( "'" ) );
      TEST_THROWS( custom_from_string( "\"" ) );
      TEST_THROWS( custom_from_string( "..." ) );
      TEST_THROWS( custom_from_string( "\"\xfd\xbf\xbf\xbf\xbf" ) );
      TEST_THROWS( custom_from_string( "\"\xfd\xbf\xbf\xbf\xbf\"" ) );
      TEST_THROWS( custom_from_string( "\"\xfd\xbf\xbf\xbf\xbf\xbf" ) );
      TEST_THROWS( custom_from_string( "\"\xfd\xbf\xbf\xbf\xbf\xbf\"" ) );
      TEST_THROWS( custom_from_string( "\"f\0o\"" ) );
   }

}  // namespace tao::json

#include "main.hpp"
