// Copyright (c) 2014-2020 Dr. Colin Hirsch and Daniel Frey
// Please see LICENSE for license or visit https://github.com/taocpp/PEGTL/

#ifndef TAO_JSON_PEGTL_INTERNAL_STAR_HPP
#define TAO_JSON_PEGTL_INTERNAL_STAR_HPP

#include <type_traits>

#include "../config.hpp"

#include "seq.hpp"
#include "skip_control.hpp"

#include "../apply_mode.hpp"
#include "../rewind_mode.hpp"

#include "../analysis/generic.hpp"

namespace TAO_JSON_PEGTL_NAMESPACE::internal
{
   template< typename Rule, typename... Rules >
   struct star
   {
      using analyze_t = analysis::generic< analysis::rule_type::opt, Rule, Rules..., star >;

      template< apply_mode A,
                rewind_mode,
                template< typename... >
                class Action,
                template< typename... >
                class Control,
                typename Input,
                typename... States >
      [[nodiscard]] static bool match( Input& in, States&&... st )
      {
         while( seq< Rule, Rules... >::template match< A, rewind_mode::required, Action, Control >( in, st... ) ) {
         }
         return true;
      }
   };

   template< typename Rule, typename... Rules >
   inline constexpr bool skip_control< star< Rule, Rules... > > = true;

}  // namespace TAO_JSON_PEGTL_NAMESPACE::internal

#endif
