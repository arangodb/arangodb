// Copyright (c) 2016-2020 Dr. Colin Hirsch and Daniel Frey
// Please see LICENSE for license or visit https://github.com/taocpp/json/

#ifndef TAO_JSON_EVENTS_TO_VALUE_HPP
#define TAO_JSON_EVENTS_TO_VALUE_HPP

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "../binary_view.hpp"
#include "../value.hpp"

namespace tao::json::events
{
   // Events consumer to build a JSON Value.

   template< template< typename... > class Traits >
   struct to_basic_value
   {
      std::vector< basic_value< Traits > > stack_;
      std::vector< std::string > keys_;
      basic_value< Traits > value;

      void null()
      {
         value.set_null();
      }

      void boolean( const bool v )
      {
         value.set_boolean( v );
      }

      void number( const std::int64_t v )
      {
         value.set_signed( v );
      }

      void number( const std::uint64_t v )
      {
         value.set_unsigned( v );
      }

      void number( const double v )
      {
         value.set_double( v );
      }

      void string( const std::string_view v )
      {
         value.emplace_string( v );
      }

      void string( const char* v )
      {
         value.emplace_string( v );
      }

      void string( std::string&& v )
      {
         value.emplace_string( std::move( v ) );
      }

      void binary( const tao::binary_view v )
      {
         value.emplace_binary( v.begin(), v.end() );
      }

      void binary( std::vector< std::byte >&& v )
      {
         value.emplace_binary( std::move( v ) );
      }

      void begin_array()
      {
         stack_.emplace_back( empty_array );
      }

      void begin_array( const std::size_t size )
      {
         begin_array();
         stack_.back().get_array().reserve( size );
      }

      void element()
      {
         stack_.back().emplace_back( std::move( value ) );
      }

      void end_array( const std::size_t /*unused*/ = 0 )
      {
         value = std::move( stack_.back() );
         stack_.pop_back();
      }

      void begin_object( const std::size_t /*unused*/ = 0 )
      {
         stack_.emplace_back( empty_object );
      }

      void key( const std::string_view v )
      {
         keys_.emplace_back( v );
      }

      void key( const char* v )
      {
         keys_.emplace_back( v );
      }

      void key( std::string&& v )
      {
         keys_.emplace_back( std::move( v ) );
      }

      void member()
      {
         stack_.back().try_emplace( std::move( keys_.back() ), std::move( value ) );
         keys_.pop_back();
      }

      void end_object( const std::size_t /*unused*/ = 0 )
      {
         value = std::move( stack_.back() );
         stack_.pop_back();
      }
   };

   using to_value = to_basic_value< traits >;

}  // namespace tao::json::events

#endif
