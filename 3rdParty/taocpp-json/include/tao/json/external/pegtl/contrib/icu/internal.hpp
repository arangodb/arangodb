// Copyright (c) 2018-2020 Dr. Colin Hirsch and Daniel Frey
// Please see LICENSE for license or visit https://github.com/taocpp/PEGTL/

#ifndef TAO_JSON_PEGTL_CONTRIB_ICU_INTERNAL_HPP
#define TAO_JSON_PEGTL_CONTRIB_ICU_INTERNAL_HPP

#include <unicode/uchar.h>

#include "../../config.hpp"

#include "../../analysis/generic.hpp"
#include "../../internal/skip_control.hpp"

namespace TAO_JSON_PEGTL_NAMESPACE::internal
{
   namespace icu
   {
      template< typename Peek, UProperty P, bool V = true >
      struct binary_property
      {
         using analyze_t = analysis::generic< analysis::rule_type::any >;

         template< typename Input >
         [[nodiscard]] static bool match( Input& in ) noexcept( noexcept( in.size( Peek::max_input_size ) ) )
         {
            if( const std::size_t s = in.size( Peek::max_input_size ); s >= Peek::min_input_size ) {
               if( const auto r = Peek::peek( in, s ) ) {
                  if( u_hasBinaryProperty( r.data, P ) == V ) {
                     in.bump( r.size );
                     return true;
                  }
               }
            }
            return false;
         }
      };

      template< typename Peek, UProperty P, int V >
      struct property_value
      {
         using analyze_t = analysis::generic< analysis::rule_type::any >;

         template< typename Input >
         [[nodiscard]] static bool match( Input& in ) noexcept( noexcept( in.size( Peek::max_input_size ) ) )
         {
            if( const std::size_t s = in.size( Peek::max_input_size ); s >= Peek::min_input_size ) {
               if( const auto r = Peek::peek( in, s ) ) {
                  if( u_getIntPropertyValue( r.data, P ) == V ) {
                     in.bump( r.size );
                     return true;
                  }
               }
            }
            return false;
         }
      };

   }  // namespace icu

   template< typename Peek, UProperty P, bool V >
   inline constexpr bool skip_control< icu::binary_property< Peek, P, V > > = true;

   template< typename Peek, UProperty P, int V >
   inline constexpr bool skip_control< icu::property_value< Peek, P, V > > = true;

}  // namespace TAO_JSON_PEGTL_NAMESPACE::internal

#endif
