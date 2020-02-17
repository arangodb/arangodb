// Copyright (c) 2018-2020 Dr. Colin Hirsch and Daniel Frey
// Please see LICENSE for license or visit https://github.com/taocpp/json/

#include "test.hpp"

#include <tao/json/value.hpp>

#include <tao/json/contrib/traits.hpp>

namespace tao::json
{
   template< typename T >
   struct my_traits
      : traits< T >
   {};

   template<>
   struct my_traits< void >
      : traits< void >
   {
      template< typename T >
      struct public_base
      {
         [[nodiscard]] const T& self() const noexcept
         {
            return static_cast< const T& >( *this );
         }

         [[nodiscard]] bool empty() const noexcept
         {
            switch( self().type() ) {
               case json::type::UNINITIALIZED:
               case json::type::VALUELESS_BY_EXCEPTION:
                  return true;

               case json::type::NULL_:
               case json::type::BOOLEAN:
               case json::type::SIGNED:
               case json::type::UNSIGNED:
               case json::type::DOUBLE:
               case json::type::VALUE_PTR:
               case json::type::OPAQUE_PTR:
                  return false;

               case json::type::STRING:
                  return self().get_string().empty();

               case json::type::STRING_VIEW:
                  return self().get_string_view().empty();

               case json::type::BINARY:
                  return self().get_binary().empty();

               case json::type::BINARY_VIEW:
                  return self().get_binary_view().empty();

               case json::type::ARRAY:
                  return self().get_array().empty();

               case json::type::OBJECT:
                  return self().get_object().empty();
            }
            // LCOV_EXCL_START
            assert( false );
            return false;
            // LCOV_EXCL_STOP
         }

         [[nodiscard]] std::size_t size() const noexcept
         {
            switch( self().type() ) {
               case json::type::UNINITIALIZED:
               case json::type::VALUELESS_BY_EXCEPTION:
                  return 0;

               case json::type::NULL_:
               case json::type::BOOLEAN:
               case json::type::SIGNED:
               case json::type::UNSIGNED:
               case json::type::DOUBLE:
               case json::type::VALUE_PTR:
               case json::type::OPAQUE_PTR:
                  return 1;

               case json::type::STRING:
                  return self().get_string().size();

               case json::type::STRING_VIEW:
                  return self().get_string_view().size();

               case json::type::BINARY:
                  return self().get_binary().size();

               case json::type::BINARY_VIEW:
                  return self().get_binary_view().size();

               case json::type::ARRAY:
                  return self().get_array().size();

               case json::type::OBJECT:
                  return self().get_object().size();
            }
            // LCOV_EXCL_START
            assert( false );
            return 0;
            // LCOV_EXCL_STOP
         }
      };
   };

   using my_value = basic_value< my_traits >;

   void unit_test()
   {
      my_value v1;
      assert( v1.empty() );
      assert( v1.size() == 0 );

      my_value v2 = "";
      assert( v2.empty() );
      assert( v2.size() == 0 );

      my_value v3 = empty_array;
      assert( v3.empty() );
      assert( v3.size() == 0 );

      my_value v4 = empty_object;
      assert( v4.empty() );
      assert( v4.size() == 0 );

      my_value f1 = 0;
      assert( !f1.empty() );
      assert( f1.size() == 1 );

      my_value f2 = my_value::array( { 42, "Hallo" } );
      assert( !f2.empty() );
      assert( f2.size() == 2 );
   }

}  // namespace tao::json

#include "main.hpp"
