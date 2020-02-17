// Copyright (c) 2016-2020 Dr. Colin Hirsch and Daniel Frey
// Please see LICENSE for license or visit https://github.com/taocpp/json/

#include "test.hpp"

#include <tao/json/contrib/reference.hpp>
#include <tao/json/from_string.hpp>
#include <tao/json/value.hpp>

namespace tao::json
{
   void test( const std::string& s1, const std::string& s2 )
   {
      auto v1 = from_string( s1 );
      const auto v2 = from_string( s2 );
      resolve_references( v1 );
      TEST_ASSERT( v1 == v2 );
   }

   void test_throws( const value& v )
   {
      value v2( v );
      TEST_THROWS( resolve_references( v2 ) );
   }

   void unit_test()
   {
      {
         value e;
         resolve_references( e );
      }

      test( "{\"foo\":1,\"bar\":2}", "{\"foo\":1,\"bar\":2}" );
      test( "{\"foo\":1,\"bar\":{\"$ref\":\"#/foo\"}}", "{\"foo\":1,\"bar\":1}" );
      test( "{\"foo\":\"#/foo\",\"bar\":{\"$ref\":{\"$ref\":\"#/foo\"}}}", "{\"foo\":\"#/foo\",\"bar\":\"#/foo\"}" );

      test_throws( { { "$ref", "#" } } );
      test_throws( { { "$ref", "#/foo" } } );
      test_throws( { { "foo", 0 }, { "bar", { { "$ref", "#/foo/bar" } } } } );
   }

}  // namespace tao::json

#include "main.hpp"
