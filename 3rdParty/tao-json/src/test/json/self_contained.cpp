// Copyright (c) 2016-2020 Dr. Colin Hirsch and Daniel Frey
// Please see LICENSE for license or visit https://github.com/taocpp/json/

#include "test.hpp"

#include <tao/json/self_contained.hpp>
#include <tao/json/value.hpp>

namespace tao::json
{
   template< typename T >
   struct view_traits
      : traits< T >
   {
   };

   template<>
   struct view_traits< std::string_view >
      : traits< std::string_view >
   {
      template< template< typename... > class Traits >
      static void assign( basic_value< Traits >& v, const std::string_view sv ) noexcept
      {
         v.set_string_view( sv );
      }
   };

   template<>
   struct view_traits< tao::binary_view >
      : traits< tao::binary_view >
   {
      template< template< typename... > class Traits >
      static void assign( basic_value< Traits >& v, const tao::binary_view xv ) noexcept
      {
         v.set_binary_view( xv );
      }
   };

   void unit_test()
   {
      value v = { { "foo", 1 } };

      value v1 = { { "bar", v }, { "baz", value::array( { 2, v, 3 } ) } };
      value v2 = { { "bar", &v }, { "baz", value::array( { 2, &v, 3 } ) } };
      value v4 = { { "bar", { { "foo", 1 } } }, { "baz", value::array( { 2, { { "foo", 1 } }, 3 } ) } };

      TEST_ASSERT( v1 == v2 );
      TEST_ASSERT( v1 == v4 );
      TEST_ASSERT( v2 == v4 );

      TEST_ASSERT( v1.at( "bar" ).type() == type::OBJECT );
      TEST_ASSERT( v1.at( "baz" ).at( 1 ).type() == type::OBJECT );

      TEST_ASSERT( v2.at( "bar" ).type() == type::VALUE_PTR );
      TEST_ASSERT( v2.at( "baz" ).at( 1 ).type() == type::VALUE_PTR );

      TEST_ASSERT( is_self_contained( v1 ) );
      TEST_ASSERT( !is_self_contained( v2 ) );
      TEST_ASSERT( is_self_contained( v4 ) );

      make_self_contained( v1 );
      make_self_contained( v2 );

      TEST_ASSERT( is_self_contained( v1 ) );
      TEST_ASSERT( is_self_contained( v2 ) );
      TEST_ASSERT( v1 == v2 );
      TEST_ASSERT( v1 == v4 );
      TEST_ASSERT( v2 == v4 );

      TEST_ASSERT( v1.at( "bar" ).type() == type::OBJECT );
      TEST_ASSERT( v1.at( "baz" ).at( 1 ).type() == type::OBJECT );

      TEST_ASSERT( v2.at( "bar" ).type() == type::OBJECT );
      TEST_ASSERT( v2.at( "baz" ).at( 1 ).type() == type::OBJECT );

      std::byte data[ 4 ];

      TEST_ASSERT( is_self_contained( value() ) );
      TEST_ASSERT( is_self_contained( value( true ) ) );
      TEST_ASSERT( is_self_contained( value( std::string( "hallo" ) ) ) );
      TEST_ASSERT( is_self_contained( value( std::int64_t( -1 ) ) ) );
      TEST_ASSERT( is_self_contained( value( std::uint64_t( 1 ) ) ) );
      TEST_ASSERT( is_self_contained( value( double( 3.0 ) ) ) );
      TEST_ASSERT( is_self_contained( value( tao::binary( 2, std::byte( 42 ) ) ) ) );
      TEST_ASSERT( is_self_contained( value( null ) ) );
      TEST_ASSERT( is_self_contained( value( empty_array ) ) );
      TEST_ASSERT( is_self_contained( value( empty_object ) ) );
      TEST_ASSERT( is_self_contained( value( std::string_view( "hallo" ) ) ) );               // Default traits put a string in the value for safety.
      TEST_ASSERT( is_self_contained( value( tao::binary_view( data, sizeof( data ) ) ) ) );  // Default traits put a binary in the value for safety.

      basic_value< view_traits > sv( std::string_view( "hallo" ) );
      TEST_ASSERT( !is_self_contained( sv ) );
      make_self_contained( sv );
      TEST_ASSERT( is_self_contained( sv ) );
      TEST_ASSERT( basic_value< view_traits >( std::string_view( "hallo" ) ) == sv );

      basic_value< view_traits > bv( tao::binary_view( data, sizeof( data ) ) );
      TEST_ASSERT( !is_self_contained( bv ) );
      make_self_contained( bv );
      TEST_ASSERT( is_self_contained( bv ) );
      TEST_ASSERT( basic_value< view_traits >( tao::binary_view( data, sizeof( data ) ) ) == bv );
   }

}  // namespace tao::json

#include "main.hpp"
