// Copyright (c) 2017-2020 Dr. Colin Hirsch and Daniel Frey
// Please see LICENSE for license or visit https://github.com/taocpp/json/

#include "test.hpp"

#include <tao/json/binary.hpp>
#include <tao/json/jaxn/from_string.hpp>
#include <tao/json/value.hpp>

namespace tao::json
{
   [[nodiscard]] value custom_from_string( const char* v )
   {
      return jaxn::from_string( v );
   }

   [[nodiscard]] value custom_from_string( const std::string& v )
   {
      return jaxn::from_string( v );
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
      const auto v = custom_from_string( "[ null, true, false, //dummy\n42 //*\n, 43.0 ///*\n, \"foo\" + 'bar', [ 1, +2, 3, ], { \"a\"/**/ : \"b\"/*foo/b\nar*b\naz/*b\nla*/, \"c\" : \"d\", } ]" );

      TEST_ASSERT( v.type() == type::ARRAY );

      const auto& a = v.get_array();

      TEST_ASSERT( a.size() == 8 );
      TEST_ASSERT( a[ 0 ] == value( null ) );
      TEST_ASSERT( a[ 1 ] == value( true ) );
      TEST_ASSERT( a[ 2 ] == value( false ) );
      TEST_ASSERT( a[ 3 ] == value( 42 ) );
      TEST_ASSERT( a[ 4 ] == value( 43.0 ) );
      TEST_ASSERT( a[ 5 ] == value( "foobar" ) );
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
      const auto v = custom_from_string( "{ \"a\" : null, b: true, \"c\" : false, d : 42, \"e\" : +43.0, \"f\" : \"foo\", \"g\" : [ 1, 2, +3 ], h : { \"a\" : \"b\", c : \"d\" } }" );

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
      TEST_ASSERT( custom_from_string( "true" ) == value( true ) );
      TEST_ASSERT( custom_from_string( "false" ) == value( false ) );

      TEST_ASSERT( custom_from_string( "42" ) == value( 42 ) );
      TEST_ASSERT( custom_from_string( "42." ) == value( 42.0 ) );
      TEST_ASSERT( custom_from_string( "42.0" ) == value( 42.0 ) );

      TEST_ASSERT( custom_from_string( "0.5" ) == value( 0.5 ) );
      TEST_ASSERT( custom_from_string( ".5" ) == value( 0.5 ) );
      TEST_THROWS( custom_from_string( "." ) );
      TEST_THROWS( custom_from_string( "00." ) );

      TEST_ASSERT( custom_from_string( "0" ).get_unsigned() == 0 );
      TEST_ASSERT( custom_from_string( "1" ).get_unsigned() == 1 );
      TEST_ASSERT( custom_from_string( "+1" ).get_unsigned() == 1 );
      TEST_ASSERT( custom_from_string( "-1" ).get_signed() == -1 );

      // full signed and unsigned 64 bit integer range
      TEST_ASSERT( custom_from_string( "9223372036854775807" ).get_unsigned() == 9223372036854775807 );
      TEST_ASSERT( custom_from_string( "+9223372036854775807" ).get_unsigned() == 9223372036854775807 );
      TEST_ASSERT( custom_from_string( "-9223372036854775808" ).get_signed() == -9223372036854775807LL - 1 );
      TEST_ASSERT( custom_from_string( "18446744073709551615" ).get_unsigned() == 18446744073709551615ULL );
      TEST_ASSERT( custom_from_string( "+18446744073709551615" ).get_unsigned() == 18446744073709551615ULL );

      // anything beyond is double
      TEST_ASSERT( custom_from_string( "-9223372036854775809" ).is_double() );
      TEST_ASSERT( custom_from_string( "18446744073709551616" ).is_double() );
      TEST_ASSERT( custom_from_string( "+18446744073709551616" ).is_double() );

      TEST_ASSERT( custom_from_string( "0" ) == 0 );
      TEST_ASSERT( custom_from_string( "1" ) == 1 );
      TEST_ASSERT( custom_from_string( "+1" ) == 1 );
      TEST_ASSERT( custom_from_string( "-1" ) == -1 );

      TEST_ASSERT( custom_from_string( "0x0" ).get_unsigned() == 0 );
      TEST_ASSERT( custom_from_string( "0xff" ).get_unsigned() == 0xff );
      TEST_ASSERT( custom_from_string( "0xFF" ).get_unsigned() == 0xff );
      TEST_ASSERT( custom_from_string( "0xabde1234EFAB9876" ).get_unsigned() == 0xabde1234EFAB9876 );

      TEST_ASSERT( custom_from_string( "-0x0" ).get_signed() == 0 );
      TEST_ASSERT( custom_from_string( "-0xff" ).get_signed() == -0xff );
      TEST_ASSERT( custom_from_string( "-0xFF" ).get_signed() == -0xff );

      TEST_ASSERT( std::isnan( custom_from_string( "NaN" ).get_double() ) );
      TEST_ASSERT( std::isnan( custom_from_string( "+NaN" ).get_double() ) );
      TEST_ASSERT( std::isnan( custom_from_string( "-NaN" ).get_double() ) );

      TEST_ASSERT( custom_from_string( "Infinity" ).get_double() == INFINITY );
      TEST_ASSERT( custom_from_string( "+Infinity" ).get_double() == INFINITY );
      TEST_ASSERT( custom_from_string( "-Infinity" ).get_double() == -INFINITY );

      TEST_ASSERT( custom_from_string( "9223372036854775807" ) == 9223372036854775807 );
      TEST_ASSERT( custom_from_string( "+9223372036854775807" ) == 9223372036854775807 );
      TEST_ASSERT( custom_from_string( "-9223372036854775808" ) == -9223372036854775807LL - 1 );
      TEST_ASSERT( custom_from_string( "18446744073709551615" ) == 18446744073709551615ULL );
      TEST_ASSERT( custom_from_string( "+18446744073709551615" ) == 18446744073709551615ULL );

      TEST_ASSERT( custom_from_string( "\"\"" ) == "" );
      TEST_ASSERT( custom_from_string( "\"'\"" ) == "'" );

      TEST_ASSERT( custom_from_string( "''" ) == "" );
      TEST_ASSERT( custom_from_string( "'\"'" ) == "\"" );
      TEST_ASSERT( custom_from_string( "'foo'" ) == "foo" );
      TEST_ASSERT( custom_from_string( "'''foo'''" ) == "foo" );
      TEST_ASSERT( custom_from_string( "'''\nfoo'''" ) == "foo" );
      TEST_ASSERT( custom_from_string( "'''\n\nfoo'''" ) == "\nfoo" );
      TEST_ASSERT( custom_from_string( "\"\"\"foo\"\"\"" ) == "foo" );
      TEST_ASSERT( custom_from_string( "'''foo\nbar\\n'''" ) == "foo\nbar\\n" );
      TEST_ASSERT( custom_from_string( "'fo\\\"o'" ) == "fo\"o" );
      TEST_ASSERT( custom_from_string( "'fo\\'o'" ) == "fo'o" );
      TEST_ASSERT( custom_from_string( "'fo\\\\o'" ) == "fo\\o" );
      TEST_ASSERT( custom_from_string( "'fo\\/o'" ) == "fo/o" );
      TEST_ASSERT( custom_from_string( "'f\\0o'" ) == std::string( "f\0o", 3 ) );
      TEST_ASSERT( custom_from_string( "'\\uD834\\uDF06'" ) == "𝌆" );
      TEST_ASSERT( custom_from_string( "'\\u{1D306}'" ) == "𝌆" );
      TEST_ASSERT( custom_from_string( "'x\\uD834\\uDF06\\u{000000000000001D306}y'" ) == "x𝌆𝌆y" );
      TEST_ASSERT( custom_from_string( "\"foo\"" ) == value( "foo" ) );
      TEST_ASSERT( custom_from_string( "\"f\\u007Fo\"" ) == value( "f\177o" ) );
      TEST_THROWS( custom_from_string( "\"f\177o\"" ) );

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

      TEST_THROWS( custom_from_string( "'fo\\o'" ) );
      TEST_THROWS( custom_from_string( "'f\\1o'" ) );
      TEST_THROWS( custom_from_string( "'f\0o'" ) );
      TEST_THROWS( custom_from_string( "\n\n'123456789\0'" ) );

      TEST_ASSERT( custom_from_string( "//\n42" ) == 42 );
      TEST_ASSERT( custom_from_string( "//comment\n42" ) == 42 );
      TEST_ASSERT( custom_from_string( "/**/42" ) == 42 );
      TEST_ASSERT( custom_from_string( "/***/42" ) == 42 );
      TEST_ASSERT( custom_from_string( "/****/42" ) == 42 );
      TEST_ASSERT( custom_from_string( "/*****/42" ) == 42 );
      TEST_ASSERT( custom_from_string( "/*comment*/42" ) );
      TEST_ASSERT( custom_from_string( "42//" ) == 42 );
      TEST_ASSERT( custom_from_string( "42//\n" ) == 42 );
      TEST_ASSERT( custom_from_string( "42//comment" ) == 42 );
      TEST_ASSERT( custom_from_string( "42//comment\n" ) == 42 );
      TEST_ASSERT( custom_from_string( "42/**/" ) == 42 );
      TEST_ASSERT( custom_from_string( "42/**/\n" ) == 42 );
      TEST_ASSERT( custom_from_string( "42/*comment*/" ) );
      TEST_ASSERT( custom_from_string( "42/*comment*/\n" ) );
      TEST_ASSERT( custom_from_string( "42/**/" ) == 42 );
      TEST_ASSERT( custom_from_string( "42/*\n*/" ) == 42 );
      TEST_ASSERT( custom_from_string( "42/*comment*/" ) );
      TEST_ASSERT( custom_from_string( "42/*comment\n*/" ) );

      TEST_THROWS( custom_from_string( "//\v\n42" ) );
      TEST_THROWS( custom_from_string( "/*\v*/42" ) );
      TEST_THROWS( custom_from_string( "4/*comment*/2" ) );

      TEST_ASSERT( custom_from_string( "$" ) == empty_binary );
      TEST_ASSERT( custom_from_string( "$''" ) == empty_binary );
      TEST_ASSERT( custom_from_string( "$+$''" ) == empty_binary );
      TEST_ASSERT( custom_from_string( "$''+$" ) == empty_binary );

      TEST_ASSERT( custom_from_string( "$'a'" ) == 0x61_binary );
      TEST_ASSERT( custom_from_string( "$+$'a'" ) == 0x61_binary );
      TEST_ASSERT( custom_from_string( "$'a'+$" ) == 0x61_binary );

      TEST_ASSERT( custom_from_string( "$62" ) == 0x62_binary );
      TEST_ASSERT( custom_from_string( "$62+$'a'" ) == 0x6261_binary );
      TEST_ASSERT( custom_from_string( "$'a'+$62" ) == 0x6162_binary );

      TEST_ASSERT( custom_from_string( "$6162" ) == 0x6162_binary );
      TEST_ASSERT( custom_from_string( "$61.62" ) == 0x6162_binary );
      TEST_ASSERT( custom_from_string( "$61+$62" ) == 0x6162_binary );

      TEST_ASSERT( custom_from_string( "$'Hello, world!'" ) == 0x48656c6c6f2c20776f726c6421_binary );
   }

}  // namespace tao::json

#include "main.hpp"
