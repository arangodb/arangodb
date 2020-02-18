// Copyright (c) 2014-2020 Dr. Colin Hirsch and Daniel Frey
// Please see LICENSE for license or visit https://github.com/taocpp/PEGTL/

#ifndef TAO_JSON_PEGTL_INTERNAL_STRING_HPP
#define TAO_JSON_PEGTL_INTERNAL_STRING_HPP

#include <cstring>
#include <utility>

#include "../config.hpp"

#include "bump_help.hpp"
#include "result_on_found.hpp"
#include "skip_control.hpp"
#include "trivial.hpp"

#include "../analysis/counted.hpp"

namespace TAO_JSON_PEGTL_NAMESPACE::internal
{
   [[nodiscard]] inline bool unsafe_equals( const char* s, const std::initializer_list< char >& l ) noexcept
   {
      return std::memcmp( s, &*l.begin(), l.size() ) == 0;
   }

   template< char... Cs >
   struct string;

   template<>
   struct string<>
      : trivial< true >
   {
   };

   template< char... Cs >
   struct string
   {
      using analyze_t = analysis::counted< analysis::rule_type::any, sizeof...( Cs ) >;

      template< typename Input >
      [[nodiscard]] static bool match( Input& in ) noexcept( noexcept( in.size( 0 ) ) )
      {
         if( in.size( sizeof...( Cs ) ) >= sizeof...( Cs ) ) {
            if( unsafe_equals( in.current(), { Cs... } ) ) {
               bump_help< result_on_found::success, Input, char, Cs... >( in, sizeof...( Cs ) );
               return true;
            }
         }
         return false;
      }
   };

   template< char... Cs >
   inline constexpr bool skip_control< string< Cs... > > = true;

}  // namespace TAO_JSON_PEGTL_NAMESPACE::internal

#endif
