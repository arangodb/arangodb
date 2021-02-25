// Copyright (c) 2014-2020 Dr. Colin Hirsch and Daniel Frey
// Please see LICENSE for license or visit https://github.com/taocpp/PEGTL/

#ifndef TAO_JSON_PEGTL_INTERNAL_SEQ_HPP
#define TAO_JSON_PEGTL_INTERNAL_SEQ_HPP

#include "../config.hpp"

#include "skip_control.hpp"
#include "trivial.hpp"

#include "../apply_mode.hpp"
#include "../rewind_mode.hpp"

#include "../analysis/generic.hpp"

namespace TAO_JSON_PEGTL_NAMESPACE::internal
{
   template< typename... Rules >
   struct seq;

   template<>
   struct seq<>
      : trivial< true >
   {
   };

   template< typename Rule >
   struct seq< Rule >
   {
      using analyze_t = typename Rule::analyze_t;

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
         return Control< Rule >::template match< A, M, Action, Control >( in, st... );
      }
   };

   template< typename... Rules >
   struct seq
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
         auto m = in.template mark< M >();
         using m_t = decltype( m );
         return m( ( Control< Rules >::template match< A, m_t::next_rewind_mode, Action, Control >( in, st... ) && ... ) );
      }
   };

   template< typename... Rules >
   inline constexpr bool skip_control< seq< Rules... > > = true;

}  // namespace TAO_JSON_PEGTL_NAMESPACE::internal

#endif
