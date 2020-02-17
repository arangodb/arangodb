// Copyright (c) 2017-2020 Dr. Colin Hirsch and Daniel Frey
// Please see LICENSE for license or visit https://github.com/taocpp/json/

#include "test.hpp"

#include <tao/json/jaxn/to_string.hpp>
#include <tao/json/value.hpp>

#include <cmath>
#include <sstream>

namespace tao::json
{
   void test_simple( const value& v, const std::string& s )
   {
      TEST_ASSERT( jaxn::to_string( v ) == s );
   }

   void test_pretty( const value& v, const std::string& s )
   {
      TEST_ASSERT( jaxn::to_string( v, 2 ) == s );
   }

   void unit_test()
   {
      value e;
      const value v = 42;

      TEST_THROWS( jaxn::to_string( e ) );

      test_simple( null, "null" );
      test_simple( true, "true" );
      test_simple( false, "false" );
      test_simple( 42, "42" );
      test_simple( 42U, "42" );
      test_simple( 42.1, "42.1" );
      test_simple( NAN, "NaN" );
      test_simple( INFINITY, "Infinity" );
      test_simple( -INFINITY, "-Infinity" );
      test_simple( "foo", "\"foo\"" );
      test_simple( std::vector< std::byte >( { std::byte( 1 ), std::byte( 2 ), std::byte( 3 ) } ), "$010203" );
      test_simple( empty_array, "[]" );
      test_simple( value::array( {} ), "[]" );
      test_simple( value::array( { 1 } ), "[1]" );
      test_simple( value::array( { 1, 2, 3 } ), "[1,2,3]" );
      test_simple( empty_object, "{}" );
      test_simple( { { "foo", 42 } }, "{foo:42}" );
      test_simple( { { "foo", 42U } }, "{foo:42}" );
      test_simple( { { "foo", 42 }, { "bar", 43 } }, "{bar:43,foo:42}" );
      test_simple( { { "foo", 42 }, { "bar", 43U } }, "{bar:43,foo:42}" );
      test_simple( { { "foo", v }, { "bar", 43U } }, "{bar:43,foo:42}" );
      test_simple( { { "foo", &v }, { "bar", 43U } }, "{bar:43,foo:42}" );
      test_simple( { { "foo", value::array( { 1, { { "bar", 42 }, { "baz", value::array( { empty_object, 43 } ) } }, empty_array } ) } }, "{foo:[1,{bar:42,baz:[{},43]},[]]}" );

      test_pretty( null, "null" );
      test_pretty( true, "true" );
      test_pretty( false, "false" );
      test_pretty( 42, "42" );
      test_pretty( 42.1, "42.1" );
      test_pretty( "foo", "\"foo\"" );
      test_pretty( std::vector< std::byte >( { std::byte( 1 ), std::byte( 2 ), std::byte( 3 ) } ), "$010203" );
      test_pretty( empty_array, "[]" );
      test_pretty( value::array( {} ), "[]" );
      test_pretty( value::array( { 1 } ), "[\n  1\n]" );
      test_pretty( value::array( { 1, 2U, 3 } ), "[\n  1,\n  2,\n  3\n]" );
      test_pretty( empty_object, "{}" );
      test_pretty( { { "foo", 42 } }, "{\n  foo: 42\n}" );
      test_pretty( { { "foo", 42 }, { "bar", 43U } }, "{\n  bar: 43,\n  foo: 42\n}" );
      test_pretty( { { "foo", v }, { "bar", 43U } }, "{\n  bar: 43,\n  foo: 42\n}" );
      test_pretty( { { "foo", &v }, { "bar", 43U } }, "{\n  bar: 43,\n  foo: 42\n}" );
      test_pretty( { { "foo", value::array( { 1, { { "bar", 42 }, { "baz", value::array( { empty_object, 43 } ) } }, empty_array } ) } }, "{\n  foo: [\n    1,\n    {\n      bar: 42,\n      baz: [\n        {},\n        43\n      ]\n    },\n    []\n  ]\n}" );
   }

}  // namespace tao::json

#include "main.hpp"
