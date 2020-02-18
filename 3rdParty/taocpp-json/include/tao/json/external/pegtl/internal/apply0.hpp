// Copyright (c) 2017-2020 Dr. Colin Hirsch and Daniel Frey
// Please see LICENSE for license or visit https://github.com/taocpp/PEGTL/

#ifndef TAO_JSON_PEGTL_INTERNAL_APPLY0_HPP
#define TAO_JSON_PEGTL_INTERNAL_APPLY0_HPP

#include "../config.hpp"

#include "apply0_single.hpp"
#include "skip_control.hpp"

#include "../analysis/counted.hpp"
#include "../apply_mode.hpp"
#include "../rewind_mode.hpp"

namespace TAO_JSON_PEGTL_NAMESPACE::internal
{
   template< typename... Actions >
   struct apply0
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
      [[nodiscard]] static bool match( Input& /*unused*/, States&&... st )
      {
         if constexpr( A == apply_mode::action ) {
            return ( apply0_single< Actions >::match( st... ) && ... );
         }
         else {
#if defined( _MSC_VER )
            (void)( (void)st, ... );
#endif
            return true;
         }
      }
   };

   template< typename... Actions >
   inline constexpr bool skip_control< apply0< Actions... > > = true;

}  // namespace TAO_JSON_PEGTL_NAMESPACE::internal

#endif
