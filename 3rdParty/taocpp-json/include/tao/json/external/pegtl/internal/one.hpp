// Copyright (c) 2014-2020 Dr. Colin Hirsch and Daniel Frey
// Please see LICENSE for license or visit https://github.com/taocpp/PEGTL/

#ifndef TAO_JSON_PEGTL_INTERNAL_ONE_HPP
#define TAO_JSON_PEGTL_INTERNAL_ONE_HPP

#include <cstddef>

#include "../config.hpp"

#include "bump_help.hpp"
#include "result_on_found.hpp"
#include "skip_control.hpp"

#include "../analysis/generic.hpp"

namespace TAO_JSON_PEGTL_NAMESPACE::internal
{
   template< result_on_found R, typename Peek, typename Peek::data_t... Cs >
   struct one
   {
      using analyze_t = analysis::generic< analysis::rule_type::any >;

      template< typename Input >
      [[nodiscard]] static bool match( Input& in ) noexcept( noexcept( in.size( Peek::max_input_size ) ) )
      {
         if( const std::size_t s = in.size( Peek::max_input_size ); s >= Peek::min_input_size ) {
            if( const auto t = Peek::peek( in, s ) ) {
               if( ( ( t.data == Cs ) || ... ) == bool( R ) ) {
                  bump_help< R, Input, typename Peek::data_t, Cs... >( in, t.size );
                  return true;
               }
            }
         }
         return false;
      }
   };

   template< result_on_found R, typename Peek, typename Peek::data_t... Cs >
   inline constexpr bool skip_control< one< R, Peek, Cs... > > = true;

}  // namespace TAO_JSON_PEGTL_NAMESPACE::internal

#endif
