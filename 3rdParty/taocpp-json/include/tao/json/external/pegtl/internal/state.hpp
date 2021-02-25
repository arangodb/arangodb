// Copyright (c) 2014-2020 Dr. Colin Hirsch and Daniel Frey
// Please see LICENSE for license or visit https://github.com/taocpp/PEGTL/

#ifndef TAO_JSON_PEGTL_INTERNAL_STATE_HPP
#define TAO_JSON_PEGTL_INTERNAL_STATE_HPP

#include "../config.hpp"

#include "duseltronik.hpp"
#include "seq.hpp"
#include "skip_control.hpp"

#include "../apply_mode.hpp"
#include "../rewind_mode.hpp"

#include "../analysis/generic.hpp"

namespace TAO_JSON_PEGTL_NAMESPACE::internal
{
   template< typename State, typename... Rules >
   struct state
   {
      using analyze_t = analysis::generic< analysis::rule_type::seq, Rules... >;

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
         State s( static_cast< const Input& >( in ), st... );
         if( duseltronik< seq< Rules... >, A, M, Action, Control >::match( in, s ) ) {
            s.success( static_cast< const Input& >( in ), st... );
            return true;
         }
         return false;
      }
   };

   template< typename State, typename... Rules >
   inline constexpr bool skip_control< state< State, Rules... > > = true;

}  // namespace TAO_JSON_PEGTL_NAMESPACE::internal

#endif
