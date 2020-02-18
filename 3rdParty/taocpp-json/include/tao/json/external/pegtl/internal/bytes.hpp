// Copyright (c) 2014-2020 Dr. Colin Hirsch and Daniel Frey
// Please see LICENSE for license or visit https://github.com/taocpp/PEGTL/

#ifndef TAO_JSON_PEGTL_INTERNAL_BYTES_HPP
#define TAO_JSON_PEGTL_INTERNAL_BYTES_HPP

#include "../config.hpp"

#include "skip_control.hpp"

#include "../analysis/counted.hpp"

namespace TAO_JSON_PEGTL_NAMESPACE::internal
{
   template< unsigned Num >
   struct bytes
   {
      using analyze_t = analysis::counted< analysis::rule_type::any, Num >;

      template< typename Input >
      [[nodiscard]] static bool match( Input& in ) noexcept( noexcept( in.size( 0 ) ) )
      {
         if( in.size( Num ) >= Num ) {
            in.bump( Num );
            return true;
         }
         return false;
      }
   };

   template< unsigned Num >
   inline constexpr bool skip_control< bytes< Num > > = true;

}  // namespace TAO_JSON_PEGTL_NAMESPACE::internal

#endif
