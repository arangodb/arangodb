// Copyright (c) 2016-2020 Dr. Colin Hirsch and Daniel Frey
// Please see LICENSE for license or visit https://github.com/taocpp/json/

#ifndef TAO_JSON_SELF_CONTAINED_HPP
#define TAO_JSON_SELF_CONTAINED_HPP

#include "events/to_value.hpp"
#include "events/virtual_ref.hpp"

#include "value.hpp"

namespace tao::json
{
   // recursively checks for the existence
   // of VALUE_PTR or OPAQUE_PTR nodes, STRING_VIEW or BINARY_VIEW,
   // returns true is no such nodes were found.

   template< template< typename... > class Traits >
   [[nodiscard]] bool is_self_contained( const basic_value< Traits >& v ) noexcept
   {
      switch( v.type() ) {
         case type::UNINITIALIZED:
            return true;

         case type::NULL_:
         case type::BOOLEAN:
         case type::SIGNED:
         case type::UNSIGNED:
         case type::DOUBLE:
         case type::STRING:
         case type::BINARY:
            return true;

         case type::STRING_VIEW:
         case type::BINARY_VIEW:
            return false;

         case type::ARRAY:
            for( auto& e : v.get_array() ) {
               if( !is_self_contained( e ) ) {
                  return false;
               }
            }
            return true;

         case type::OBJECT:
            for( auto& e : v.get_object() ) {
               if( !is_self_contained( e.second ) ) {
                  return false;
               }
            }
            return true;

         case type::VALUE_PTR:
            return false;

         case type::OPAQUE_PTR:
            return false;

         case type::VALUELESS_BY_EXCEPTION:
            return true;
      }
      // LCOV_EXCL_START
      assert( false );
      return false;
      // LCOV_EXCL_STOP
   }

   // Removes all VALUE_PTR and OPAQUE_PTR nodes,
   // recursively, by copying/generating their content;
   // replaces STRING_VIEW and BINARY_VIEW with STRING
   // and BINARY, respectively, with a copy of the data.

   template< template< typename... > class Traits >
   void make_self_contained( basic_value< Traits >& v )
   {
      switch( v.type() ) {
         case type::UNINITIALIZED:
            return;

         case type::NULL_:
         case type::BOOLEAN:
         case type::SIGNED:
         case type::UNSIGNED:
         case type::DOUBLE:
         case type::STRING:
         case type::BINARY:
            return;

         case type::STRING_VIEW:
            v.emplace_string( v.get_string_view() );
            return;

         case type::BINARY_VIEW: {
            const auto xv = v.get_binary_view();
            v.emplace_binary( xv.begin(), xv.end() );
            return;
         }

         case type::ARRAY:
            for( auto& e : v.get_array() ) {
               make_self_contained( e );
            }
            return;

         case type::OBJECT:
            for( auto& e : v.get_object() ) {
               make_self_contained( e.second );
            }
            return;

         case type::VALUE_PTR:
            v = *v.get_value_ptr();
            make_self_contained( v );
            return;

         case type::OPAQUE_PTR: {
            const auto& q = v.get_opaque_ptr();
            events::to_basic_value< Traits > consumer;
            events::virtual_ref< events::to_basic_value< Traits > > ref( consumer );
            q.producer( ref, q.data );
            consumer.value.public_base() = std::move( v.public_base() );
            v = std::move( consumer.value );
            return;
         }

         case type::VALUELESS_BY_EXCEPTION:
            return;
      }
      throw std::logic_error( "invalid value for tao::json::type" );  // LCOV_EXCL_LINE
   }

   template< template< typename... > class Traits >
   [[nodiscard]] basic_value< Traits > self_contained_copy( const basic_value< Traits >& v )
   {
      basic_value< Traits > r( &v );
      make_self_contained( r );
      return r;
   }

}  // namespace tao::json

#endif
