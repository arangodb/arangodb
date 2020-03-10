// Copyright (c) 2019-2020 Dr. Colin Hirsch and Daniel Frey
// Please see LICENSE for license or visit https://github.com/taocpp/PEGTL/

#ifndef TAO_JSON_PEGTL_INTERNAL_REMATCH_HPP
#define TAO_JSON_PEGTL_INTERNAL_REMATCH_HPP

#include "../config.hpp"

#include "skip_control.hpp"

#include "../apply_mode.hpp"
#include "../memory_input.hpp"
#include "../rewind_mode.hpp"

namespace TAO_JSON_PEGTL_NAMESPACE::internal
{
   template< typename Head, typename... Rules >
   struct rematch;

   template< typename Head >
   struct rematch< Head >
   {
      using analyze_t = typename Head::analyze_t;

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
         return Control< Head >::template match< A, M, Action, Control >( in, st... );
      }
   };

   template< typename Head, typename Rule, typename... Rules >
   struct rematch< Head, Rule, Rules... >
   {
      using analyze_t = typename Head::analyze_t;  // NOTE: Rule and Rules are ignored for analyze().

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
         auto m = in.template mark< rewind_mode::required >();

         if( Control< Head >::template match< A, rewind_mode::active, Action, Control >( in, st... ) ) {
            memory_input< Input::tracking_mode_v, typename Input::eol_t, typename Input::source_t > i2( m.iterator(), in.current(), in.source() );
            return m( ( Control< Rule >::template match< A, rewind_mode::active, Action, Control >( i2, st... ) && ... && ( i2.restart( m ), Control< Rules >::template match< A, rewind_mode::active, Action, Control >( i2, st... ) ) ) );
         }
         return false;
      }
   };

   template< typename Head, typename... Rules >
   inline constexpr bool skip_control< rematch< Head, Rules... > > = true;

}  // namespace TAO_JSON_PEGTL_NAMESPACE::internal

#endif
