// Copyright (c) 2019-2020 Dr. Colin Hirsch and Daniel Frey
// Please see LICENSE for license or visit https://github.com/taocpp/PEGTL/

#ifndef TAO_JSON_PEGTL_MATCH_HPP
#define TAO_JSON_PEGTL_MATCH_HPP

#include <type_traits>

#include "apply_mode.hpp"
#include "config.hpp"
#include "nothing.hpp"
#include "require_apply.hpp"
#include "require_apply0.hpp"
#include "rewind_mode.hpp"

#include "internal/dusel_mode.hpp"
#include "internal/duseltronik.hpp"
#include "internal/has_apply.hpp"
#include "internal/has_apply0.hpp"
#include "internal/missing_apply.hpp"
#include "internal/missing_apply0.hpp"
#include "internal/skip_control.hpp"

namespace TAO_JSON_PEGTL_NAMESPACE
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
   [[nodiscard]] bool match( Input& in, States&&... st )
   {
      constexpr bool enable_control = !internal::skip_control< Rule >;
      constexpr bool enable_action = enable_control && ( A == apply_mode::action );

      using iterator_t = typename Input::iterator_t;
      constexpr bool has_apply_void = enable_action && internal::has_apply< Control< Rule >, void, Action, const iterator_t&, const Input&, States... >::value;
      constexpr bool has_apply_bool = enable_action && internal::has_apply< Control< Rule >, bool, Action, const iterator_t&, const Input&, States... >::value;
      constexpr bool has_apply = has_apply_void || has_apply_bool;

      constexpr bool has_apply0_void = enable_action && internal::has_apply0< Control< Rule >, void, Action, const Input&, States... >::value;
      constexpr bool has_apply0_bool = enable_action && internal::has_apply0< Control< Rule >, bool, Action, const Input&, States... >::value;
      constexpr bool has_apply0 = has_apply0_void || has_apply0_bool;

      static_assert( !( has_apply && has_apply0 ), "both apply() and apply0() defined" );

      constexpr bool is_nothing = std::is_base_of_v< nothing< Rule >, Action< Rule > >;
      static_assert( !( has_apply && is_nothing ), "unexpected apply() defined" );
      static_assert( !( has_apply0 && is_nothing ), "unexpected apply0() defined" );

      if constexpr( !has_apply && std::is_base_of_v< require_apply, Action< Rule > > ) {
         internal::missing_apply< Control< Rule >, Action >( in, st... );
      }

      if constexpr( !has_apply0 && std::is_base_of_v< require_apply0, Action< Rule > > ) {
         internal::missing_apply0< Control< Rule >, Action >( in, st... );
      }

      constexpr bool validate_nothing = std::is_base_of_v< maybe_nothing, Action< void > >;
      constexpr bool is_maybe_nothing = std::is_base_of_v< maybe_nothing, Action< Rule > >;
      static_assert( !enable_action || !validate_nothing || is_nothing || is_maybe_nothing || has_apply || has_apply0, "either apply() or apply0() must be defined" );

      constexpr auto mode = static_cast< internal::dusel_mode >( enable_control + has_apply_void + 2 * has_apply_bool + 3 * has_apply0_void + 4 * has_apply0_bool );
      return internal::duseltronik< Rule, A, M, Action, Control, mode >::match( in, st... );
   }

}  // namespace TAO_JSON_PEGTL_NAMESPACE

#endif
