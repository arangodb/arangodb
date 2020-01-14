// Copyright (c) 2019-2020 Dr. Colin Hirsch and Daniel Frey
// Please see LICENSE for license or visit https://github.com/taocpp/PEGTL/

#ifndef TAO_JSON_PEGTL_DISCARD_INPUT_HPP
#define TAO_JSON_PEGTL_DISCARD_INPUT_HPP

#include "apply_mode.hpp"
#include "config.hpp"
#include "match.hpp"
#include "nothing.hpp"
#include "rewind_mode.hpp"

namespace TAO_JSON_PEGTL_NAMESPACE
{
   struct discard_input
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
         const bool result = TAO_JSON_PEGTL_NAMESPACE::match< Rule, A, M, Action, Control >( in, st... );
         in.discard();
         return result;
      }
   };

}  // namespace TAO_JSON_PEGTL_NAMESPACE

#endif
