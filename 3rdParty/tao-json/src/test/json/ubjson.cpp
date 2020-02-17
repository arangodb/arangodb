// Copyright (c) 2017-2020 Dr. Colin Hirsch and Daniel Frey
// Please see LICENSE for license or visit https://github.com/taocpp/json/

#include "test.hpp"
#include "test_unhex.hpp"

#include <tao/json/from_string.hpp>
#include <tao/json/to_string.hpp>

#include <tao/json/ubjson.hpp>

namespace tao::json
{
   void ubjson_encode( const std::string& text, const std::string& data )
   {
      TEST_ASSERT( ubjson::to_string( from_string( text ) ) == test_unhex( data ) );
   }

   void ubjson_decode( const std::string& data )
   {
      const auto this_should_not_throw = test_unhex( data );
      TEST_THROWS( ubjson::from_string( this_should_not_throw ) );
   }

   void ubjson_decode( const std::string& data, const std::string& text )
   {
      for( std::size_t i = 2; i + 2 < data.size(); i += 2 ) {
         ubjson_decode( data.substr( 0, i ) );
      }
      const auto u = test_unhex( data );
      TEST_ASSERT( to_string( ubjson::from_string( u ) ) == to_string( from_string( text ) ) );
      TEST_ASSERT( to_string( ubjson::from_string( u + "NNN" ) ) == to_string( from_string( text ) ) );
      TEST_ASSERT( to_string( ubjson::from_string( "NNN" + u ) ) == to_string( from_string( text ) ) );
      TEST_ASSERT( to_string( ubjson::from_string( "NNNNN" + u + "NNNNN" ) ) == to_string( from_string( text ) ) );
   }

   void unit_test()
   {
      ubjson_decode( "4e" );

      ubjson_decode( "5a", "null" );
      ubjson_decode( "54", "true" );
      ubjson_decode( "46", "false" );

      ubjson_decode( "4380" );
      ubjson_decode( "43ff" );

      ubjson_decode( "4300", "\"\\u0000\"" );
      ubjson_decode( "4344", "\"D\"" );
      ubjson_decode( "437f", "\"\\u007f\"" );

      ubjson_decode( "5355023d3d", "\"==\"" );
      ubjson_decode( "5369023d3d", "\"==\"" );

      ubjson_decode( "5b244e235510" );
      ubjson_decode( "5b2454234c00ffffffffffffff" );

      ubjson_decode( "5b5d", "[]" );
      ubjson_decode( "5b235500", "[]" );
      ubjson_decode( "5b236900", "[]" );
      ubjson_decode( "5b5a5d", "[null]" );
      ubjson_decode( "5b2355015a", "[null]" );
      ubjson_decode( "5b2369015a", "[null]" );
      ubjson_decode( "5b245a235502", "[null,null]" );
      ubjson_decode( "5b2454236902", "[true,true]" );
      ubjson_decode( "5b244623490002", "[false,false]" );
      ubjson_decode( "5b245a236c00000002", "[null,null]" );
      ubjson_decode( "5b245a234c0000000000000002", "[null,null]" );
      ubjson_decode( "5b2469234c00000000000000020102", "[1,2]" );
      ubjson_decode( "5b245b2355025d5d", "[[],[]]" );
      ubjson_decode( "5b245b2355025d235500", "[[],[]]" );
      ubjson_decode( "5b245b2355025d2355015b5d", "[[],[[]]]" );
      ubjson_decode( "5b247b2355017d", "[{}]" );
      ubjson_decode( "5b247b235501235500", "[{}]" );
      ubjson_decode( "5b247b23550123550169013d46", "[{\"=\":false}]" );

      ubjson_decode( "7b7d", "{}" );
      ubjson_decode( "7b235500", "{}" );
      ubjson_decode( "7b236900", "{}" );
      ubjson_decode( "7b69013d55037d", "{\"=\":3}" );
      ubjson_decode( "7b23550169013d5503", "{\"=\":3}" );
      ubjson_decode( "7b245423550269013d69017b", "{\"{\":true,\"=\":true}" );
      ubjson_decode( "7b245b23550269013d5d69017b5d", "{\"{\":[],\"=\":[]}" );
      ubjson_decode( "7b245b23550269013d5d69017b235500", "{\"{\":[],\"=\":[]}" );
      ubjson_decode( "7b247b23550169013d7d", "{\"=\":{}}" );

      ubjson_encode( "null", "5a" );
      ubjson_encode( "true", "54" );
      ubjson_encode( "false", "46" );
      ubjson_encode( "\"a\"", "4361" );
      ubjson_encode( "\"aa\"", "5355026161" );
      ubjson_encode( "[]", "5b235500" );
      ubjson_encode( "{}", "7b235500" );
      ubjson_encode( "[null]", "5b2355015a" );
      ubjson_encode( "{\"a\":true,\"b\":false}", "7b2355025501615455016246" );
      ubjson_encode( "\"000\"", "535503303030" );
      ubjson_encode( "0", "5500" );
      ubjson_encode( "1", "5501" );
      ubjson_encode( "255", "55ff" );
      ubjson_encode( "256", "490100" );
      ubjson_encode( "32767", "497fff" );
      ubjson_encode( "32768", "6c00008000" );
      ubjson_encode( "2147483647", "6c7fffffff" );
      ubjson_encode( "2147483648", "4c0000000080000000" );
      ubjson_encode( "9223372036854775807", "4c7fffffffffffffff" );
      ubjson_encode( "9223372036854775808", "48551339323233333732303336383534373735383038" );
      ubjson_encode( "-1", "69ff" );
      ubjson_encode( "-128", "6980" );
      ubjson_encode( "-129", "49ff7f" );
      ubjson_encode( "-32768", "498000" );
      ubjson_encode( "-32769", "6cffff7fff" );
      ubjson_encode( "-2147483648", "6c80000000" );
      ubjson_encode( "-2147483649", "4cffffffff7fffffff" );
      ubjson_encode( "-9223372036854775808", "4c8000000000000000" );
   }

}  // namespace tao::json

#include "main.hpp"
