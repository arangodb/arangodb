// Copyright (c) 2019-2020 Dr. Colin Hirsch and Daniel Frey
// Please see LICENSE for license or visit https://github.com/taocpp/PEGTL/

#ifndef TAO_JSON_PEGTL_CHANGE_ACTION_HPP
#define TAO_JSON_PEGTL_CHANGE_ACTION_HPP

#include <type_traits>

#include "apply_mode.hpp"
#include "config.hpp"
#include "nothing.hpp"
#include "rewind_mode.hpp"

namespace TAO_JSON_PEGTL_NAMESPACE
{
   template< template< typename... > class NewAction >
   struct change_action
      : maybe_nothing
   {
      template< typename Rule,
                apply_mode A,
                rewind_mode M,
                template< typename... >
                class Action,
                template< typename... >
                class Control,
                typename Input,
                typename... States >
      [[nodiscard]] static bool match( Input& in, States&&... st )
      {
         static_assert( !std::is_same_v< Action< void >, NewAction< void > >, "old and new action class templates are identical" );
         return Control< Rule >::template match< A, M, NewAction, Control >( in, st... );
      }
   };

}  // namespace TAO_JSON_PEGTL_NAMESPACE

#endif
