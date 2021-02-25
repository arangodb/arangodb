// Copyright (c) 2017-2020 Dr. Colin Hirsch and Daniel Frey
// Please see LICENSE for license or visit https://github.com/taocpp/PEGTL/

#ifndef TAO_JSON_PEGTL_INTERNAL_APPLY_HPP
#define TAO_JSON_PEGTL_INTERNAL_APPLY_HPP

#include "../config.hpp"

#include "apply_single.hpp"
#include "skip_control.hpp"

#include "../analysis/counted.hpp"
#include "../apply_mode.hpp"
#include "../rewind_mode.hpp"

namespace TAO_JSON_PEGTL_NAMESPACE::internal
{
   template< typename... Actions >
   struct apply
   {
      using analyze_t = analysis::counted< analysis::rule_type::any, 0 >;

      template< apply_mode A,
                rewind_mode M,
                template< typename... >
                class Action,
                template< typename... >
                class Control,
                typename Input,
                typename... States >
      [[nodiscard]] static bool match( Input& in, States&&... st )
      {
         if constexpr( ( A == apply_mode::action ) && ( sizeof...( Actions ) > 0 ) ) {
            using action_t = typename Input::action_t;
            const action_t i2( in.iterator(), in );  // No data -- range is from begin to begin.
            return ( apply_single< Actions >::match( i2, st... ) && ... );
         }
         else {
#if defined( _MSC_VER )
            (void)in;
            (void)( (void)st, ... );
#endif
            return true;
         }
      }
   };

   template< typename... Actions >
   inline constexpr bool skip_control< apply< Actions... > > = true;

}  // namespace TAO_JSON_PEGTL_NAMESPACE::internal

#endif
