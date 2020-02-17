// Copyright (c) 2018-2020 Dr. Colin Hirsch and Daniel Frey
// Please see LICENSE for license or visit https://github.com/taocpp/json/

#include "test.hpp"
#include "test_types.hpp"

#include <tao/json/value.hpp>

#include <tao/json/contrib/traits.hpp>

namespace tao::json
{
   template< typename T >
   struct my_traits
      : public traits< T >
   {};

   template<>
   struct my_traits< void >
   {
      template< typename >
      using public_base = internal::empty_base;

      static constexpr const bool enable_implicit_constructor = false;
   };

   [[nodiscard]] auto force( const basic_value< my_traits >& v )
   {
      return v;
   }

   void unit_test()
   {
      assert_null( force( null ) );
      assert_string( force( empty_string ), "" );
      assert_binary( force( empty_binary ), binary() );
      assert_array( force( empty_array ), 0 );
      assert_object( force( empty_object ), 0 );

      basic_value< my_traits > v = null;
      assert_null( v );
      v = empty_string;
      assert_string( v, "" );
      v = empty_binary;
      assert_binary( v, binary() );
      v = empty_array;
      assert_array( v, 0 );
      v = empty_object;
      assert_object( v, 0 );
   }

}  // namespace tao::json

#include "main.hpp"
