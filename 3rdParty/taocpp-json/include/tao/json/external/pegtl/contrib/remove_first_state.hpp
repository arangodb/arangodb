// Copyright (c) 2019-2020 Dr. Colin Hirsch and Daniel Frey
// Please see LICENSE for license or visit https://github.com/taocpp/PEGTL/

#ifndef TAO_JSON_PEGTL_CONTRIB_REMOVE_FIRST_STATE_HPP
#define TAO_JSON_PEGTL_CONTRIB_REMOVE_FIRST_STATE_HPP

#include "../apply_mode.hpp"
#include "../config.hpp"
#include "../rewind_mode.hpp"

namespace TAO_JSON_PEGTL_NAMESPACE
{
   // NOTE: The naming of the following classes might still change.

   template< typename Rule, template< typename... > class Control >
   struct remove_first_state_after_match
      : Control< Rule >
   {
      template< typename Input, typename State, typename... States >
      static void start( const Input& in, State&& /*unused*/, States&&... st ) noexcept( noexcept( Control< Rule >::start( in, st... ) ) )
      {
         Control< Rule >::start( in, st... );
      }

      template< typename Input, typename State, typename... States >
      static void success( const Input& in, State&& /*unused*/, States&&... st ) noexcept( noexcept( Control< Rule >::success( in, st... ) ) )
      {
         Control< Rule >::success( in, st... );
      }

      template< typename Input, typename State, typename... States >
      static void failure( const Input& in, State&& /*unused*/, States&&... st ) noexcept( noexcept( Control< Rule >::failure( in, st... ) ) )
      {
         Control< Rule >::failure( in, st... );
      }

      template< typename Input, typename State, typename... States >
      static void raise( const Input& in, State&& /*unused*/, States&&... st )
      {
         Control< Rule >::raise( in, st... );
      }

      template< template< typename... > class Action,
                typename Iterator,
                typename Input,
                typename State,
                typename... States >
      static auto apply( const Iterator& begin, const Input& in, State&& /*unused*/, States&&... st ) noexcept( noexcept( Control< Rule >::template apply< Action >( begin, in, st... ) ) )
         -> decltype( Control< Rule >::template apply< Action >( begin, in, st... ) )
      {
         return Control< Rule >::template apply< Action >( begin, in, st... );
      }

      template< template< typename... > class Action,
                typename Input,
                typename State,
                typename... States >
      static auto apply0( const Input& in, State&& /*unused*/, States&&... st ) noexcept( noexcept( Control< Rule >::template apply0< Action >( in, st... ) ) )
         -> decltype( Control< Rule >::template apply0< Action >( in, st... ) )
      {
         return Control< Rule >::template apply0< Action >( in, st... );
      }
   };

   template< typename Rule, template< typename... > class Control >
   struct remove_self_and_first_state
      : Control< Rule >
   {
      template< apply_mode A,
                rewind_mode M,
                template< typename... >
                class Action,
                template< typename... >
                class,
                typename Input,
                typename State,
                typename... States >
      [[nodiscard]] static bool match( Input& in, State&& /*unused*/, States&&... st )
      {
         return Control< Rule >::template match< A, M, Action, Control >( in, st... );
      }
   };

}  // namespace TAO_JSON_PEGTL_NAMESPACE

#endif
