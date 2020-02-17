// Copyright (c) 2016-2018 Dr. Colin Hirsch and Daniel Frey
// Please see LICENSE for license or visit https://github.com/taocpp/json/

#ifndef TAO_JSON_INCLUDE_CONTRIB_NLOHMANN_TO_VALUE_HPP
#define TAO_JSON_INCLUDE_CONTRIB_NLOHMANN_TO_VALUE_HPP

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace tao::json::nlohmann
{
   // Events consumer to build an nlohmann/json value.

   template< typename Value >
   class to_value
   {
   private:
      std::vector< Value > stack_;
      std::vector< std::string > keys_;

   public:
      Value value;

      void null()
      {
         value = nullptr;
      }

      void boolean( const bool v )
      {
         value = v;
      }

      void number( const std::int64_t v )
      {
         value = v;
      }

      void number( const std::uint64_t v )
      {
         value = v;
      }

      void number( const double v )
      {
         value = v;
      }

      void string( const std::string& v )
      {
         value = v;
      }

      void string( std::string&& v )
      {
         value = std::move( v );
      }

      // array
      void begin_array()
      {
         stack_.push_back( Value::array() );
      }

      void element()
      {
         stack_.back().push_back( std::move( value ) );
      }

      void end_array()
      {
         value = std::move( stack_.back() );
         stack_.pop_back();
      }

      // object
      void begin_object()
      {
         stack_.push_back( Value::object() );
      }

      void key( const std::string& v )
      {
         keys_.push_back( v );
      }

      void key( std::string&& v )
      {
         keys_.push_back( std::move( v ) );
      }

      void member()
      {
         stack_.back().push_back( typename Value::object_t::value_type( std::move( keys_.back() ), std::move( value ) ) );
         keys_.pop_back();
      }

      void end_object()
      {
         value = std::move( stack_.back() );
         stack_.pop_back();
      }
   };

}  // namespace tao::json::nlohmann

#endif
