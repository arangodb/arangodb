// Copyright (c) 2014-2020 Dr. Colin Hirsch and Daniel Frey
// Please see LICENSE for license or visit https://github.com/taocpp/PEGTL/

#ifndef TAO_JSON_PEGTL_INTERNAL_IF_THEN_ELSE_HPP
#define TAO_JSON_PEGTL_INTERNAL_IF_THEN_ELSE_HPP

#include "../config.hpp"

#include "not_at.hpp"
#include "seq.hpp"
#include "skip_control.hpp"
#include "sor.hpp"

#include "../apply_mode.hpp"
#include "../rewind_mode.hpp"

#include "../analysis/generic.hpp"

namespace TAO_JSON_PEGTL_NAMESPACE::internal
{
   template< typename Cond, typename Then, typename Else >
   struct if_then_else
   {
      using analyze_t = analysis::generic< analysis::rule_type::sor, seq< Cond, Then >, seq< not_at< Cond >, Else > >;

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
         auto m = in.template mark< M >();
         using m_t = decltype( m );

         if( Control< Cond >::template match< A, rewind_mode::required, Action, Control >( in, st... ) ) {
            return m( Control< Then >::template match< A, m_t::next_rewind_mode, Action, Control >( in, st... ) );
         }
         return m( Control< Else >::template match< A, m_t::next_rewind_mode, Action, Control >( in, st... ) );
      }
   };

   template< typename Cond, typename Then, typename Else >
   inline constexpr bool skip_control< if_then_else< Cond, Then, Else > > = true;

}  // namespace TAO_JSON_PEGTL_NAMESPACE::internal

#endif
