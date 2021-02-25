// Copyright (c) 2014-2020 Dr. Colin Hirsch and Daniel Frey
// Please see LICENSE for license or visit https://github.com/taocpp/PEGTL/

#ifndef TAO_JSON_PEGTL_PARSE_HPP
#define TAO_JSON_PEGTL_PARSE_HPP

#include <cassert>

#include "apply_mode.hpp"
#include "config.hpp"
#include "normal.hpp"
#include "nothing.hpp"
#include "parse_error.hpp"
#include "rewind_mode.hpp"

#include "internal/action_input.hpp"

namespace TAO_JSON_PEGTL_NAMESPACE
{
   template< typename Rule,
             template< typename... > class Action = nothing,
             template< typename... > class Control = normal,
             apply_mode A = apply_mode::action,
             rewind_mode M = rewind_mode::required,
             typename Input,
             typename... States >
   bool parse( Input&& in, States&&... st )
   {
      return Control< Rule >::template match< A, M, Action, Control >( in, st... );
   }

   template< typename Rule,
             template< typename... > class Action = nothing,
             template< typename... > class Control = normal,
             apply_mode A = apply_mode::action,
             rewind_mode M = rewind_mode::required,
             typename Outer,
             typename Input,
             typename... States >
   bool parse_nested( const Outer& oi, Input&& in, States&&... st )
   {
      try {
         return parse< Rule, Action, Control, A, M >( in, st... );
      }
      catch( parse_error& e ) {
         e.positions.push_back( oi.position() );
         throw;
      }
   }

}  // namespace TAO_JSON_PEGTL_NAMESPACE

#endif
