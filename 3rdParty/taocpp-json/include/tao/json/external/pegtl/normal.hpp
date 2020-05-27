// Copyright (c) 2014-2020 Dr. Colin Hirsch and Daniel Frey
// Please see LICENSE for license or visit https://github.com/taocpp/PEGTL/

#ifndef TAO_JSON_PEGTL_NORMAL_HPP
#define TAO_JSON_PEGTL_NORMAL_HPP

#include <string>
#include <type_traits>
#include <utility>

#include "apply_mode.hpp"
#include "config.hpp"
#include "match.hpp"
#include "parse_error.hpp"
#include "rewind_mode.hpp"

#include "internal/demangle.hpp"
#include "internal/has_match.hpp"

namespace TAO_JSON_PEGTL_NAMESPACE
{
   template< typename Rule >
   struct normal
   {
      template< typename Input, typename... States >
      static void start( const Input& /*unused*/, States&&... /*unused*/ ) noexcept
      {
      }

      template< typename Input, typename... States >
      static void success( const Input& /*unused*/, States&&... /*unused*/ ) noexcept
      {
      }

      template< typename Input, typename... States >
      static void failure( const Input& /*unused*/, States&&... /*unused*/ ) noexcept
      {
      }

      template< typename Input, typename... States >
      static void raise( const Input& in, States&&... /*unused*/ )
      {
         throw parse_error( "parse error matching " + std::string( internal::demangle< Rule >() ), in );
      }

      template< template< typename... > class Action,
                typename Iterator,
                typename Input,
                typename... States >
      static auto apply( const Iterator& begin, const Input& in, States&&... st ) noexcept( noexcept( Action< Rule >::apply( std::declval< const typename Input::action_t& >(), st... ) ) )
         -> decltype( Action< Rule >::apply( std::declval< const typename Input::action_t& >(), st... ) )
      {
         const typename Input::action_t action_input( begin, in );
         return Action< Rule >::apply( action_input, st... );
      }

      template< template< typename... > class Action,
                typename Input,
                typename... States >
      static auto apply0( const Input& /*unused*/, States&&... st ) noexcept( noexcept( Action< Rule >::apply0( st... ) ) )
         -> decltype( Action< Rule >::apply0( st... ) )
      {
         return Action< Rule >::apply0( st... );
      }

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
         if constexpr( internal::has_match_v< Rule, A, M, Action, Control, Input, States... > ) {
            return Action< Rule >::template match< Rule, A, M, Action, Control >( in, st... );
         }
         else {
            return TAO_JSON_PEGTL_NAMESPACE::match< Rule, A, M, Action, Control >( in, st... );
         }
      }
   };

}  // namespace TAO_JSON_PEGTL_NAMESPACE

#endif
