// Copyright (c) 2015-2020 Dr. Colin Hirsch and Daniel Frey
// Please see LICENSE for license or visit https://github.com/taocpp/json/

#include "test.hpp"

#include <tao/json/from_string.hpp>
#include <tao/json/value.hpp>

namespace tao::json
{
   void unit_test()
   {
      TEST_ASSERT( "[42]"_json.at( 0 ).get_unsigned() == 42 );
      TEST_ASSERT( "[[42]]"_json.at( 0 ).at( 0 ).get_unsigned() == 42 );
      TEST_ASSERT( "[[[[42]]]]"_json.at( 0 ).at( 0 ).at( 0 ).at( 0 ).get_unsigned() == 42 );

      TEST_ASSERT( "[42]"_json.at( 0 ) == 42 );
      TEST_ASSERT( "[[42]]"_json.at( 0 ).at( 0 ) == 42 );
      TEST_ASSERT( "[[[[42]]]]"_json.at( 0 ).at( 0 ).at( 0 ).at( 0 ) == 42 );

      TEST_ASSERT( "[1, 2, 3]"_json.at( 0 ).get_unsigned() == 1 );
      TEST_ASSERT( "[1, 2, 3]"_json.at( 2 ).get_unsigned() == 3 );
      TEST_ASSERT( "[1, 2, 3]"_json.at( 0 ).get_unsigned() == 1 );
      TEST_ASSERT( "[1, 2, 3]"_json.at( 2 ).get_unsigned() == 3 );

      TEST_THROWS( "42"_json.at( 0 ) );
      TEST_THROWS( "[]"_json.at( 0 ) );
      TEST_THROWS( "[42]"_json.at( 1 ) );
      TEST_THROWS( "{}"_json.at( 0 ) );
      TEST_THROWS( "{\"foo\":42}"_json.at( 1 ) );

      TEST_ASSERT( "[1, [2, [3, [[[4]], 5, 6]]]]"_json.at( 1 ).at( 1 ).at( 1 ).at( 1 ).get_unsigned() == 5 );

      TEST_ASSERT( "[1, [2, [3, [[[4]], 5, 6]]]]"_json.at( 1 ).at( 1 ).at( 1 ).at( 1 ) == 5 );

      TEST_ASSERT( "[42]"_json[ 0 ].get_unsigned() == 42 );
      TEST_ASSERT( "[[42]]"_json[ 0 ][ 0 ].get_unsigned() == 42 );
      TEST_ASSERT( "[[[[42]]]]"_json[ 0 ][ 0 ][ 0 ][ 0 ].get_unsigned() == 42 );

      TEST_ASSERT( "[42]"_json[ 0 ] == 42 );
      TEST_ASSERT( "[[42]]"_json[ 0 ][ 0 ] == 42 );
      TEST_ASSERT( "[[[[42]]]]"_json[ 0 ][ 0 ][ 0 ][ 0 ] == 42 );

      TEST_ASSERT( "[1, 2, 3]"_json[ 0 ].get_unsigned() == 1 );
      TEST_ASSERT( "[1, 2, 3]"_json[ 2 ].get_unsigned() == 3 );

      TEST_ASSERT( "[1, 2, 3]"_json[ 0 ] == 1 );
      TEST_ASSERT( "[1, 2, 3]"_json[ 2 ] == 3 );

      TEST_ASSERT( "[1, [2, [3, [[[4]], 5, 6]]]]"_json[ 1 ][ 1 ][ 1 ][ 1 ].get_unsigned() == 5 );

      TEST_ASSERT( "[1, [2, [3, [[[4]], 5, 6]]]]"_json[ 1 ][ 1 ][ 1 ][ 1 ] == 5 );

      value v = value::array( { 42 } );
      TEST_ASSERT( v[ 0 ] == 42 );
      v.at( 0 ) = 1;
      TEST_ASSERT( v[ 0 ] == 1 );
      v[ 0 ] = 2;
      TEST_ASSERT( v[ 0 ] == 2 );

      TEST_ASSERT( "{\"foo\":42}"_json.at( "foo" ) == 42 );
      TEST_THROWS( "{\"foo\":42}"_json.at( "bar" ) );
      TEST_ASSERT( "{\"foo\":1,\"bar\":2}"_json.at( "foo" ) == 1 );
      TEST_ASSERT( "{\"foo\":1,\"bar\":2}"_json.at( "bar" ) == 2 );

      TEST_ASSERT( "{\"foo\":42}"_json[ "foo" ] == 42 );
      TEST_ASSERT( !"{\"foo\":42}"_json[ "bar" ] );
      TEST_ASSERT( "{\"foo\":1,\"bar\":2}"_json[ "foo" ] == 1 );
      TEST_ASSERT( "{\"foo\":1,\"bar\":2}"_json[ "bar" ] == 2 );

      TEST_THROWS( "42"_json.at( "foo" ) );
      TEST_THROWS( "[]"_json.at( "foo" ) );
      TEST_THROWS( "{}"_json.at( "foo" ) );
      TEST_THROWS( "{\"foo\":42}"_json.at( "bar" ) );

      TEST_ASSERT( !"{}"_json[ "foo" ] );

      value v2 = empty_object;
      TEST_THROWS( v2.at( "foo" ) );
      v2[ "foo" ] = 1;
      TEST_ASSERT( v2.at( "foo" ) == 1 );
      std::string s = "foo";
      v2[ s ] = 2;
      TEST_ASSERT( v2.at( "foo" ) == 2 );
   }

}  // namespace tao::json

#include "main.hpp"
