// Copyright (c) 2019-2020 Dr. Colin Hirsch and Daniel Frey
// Please see LICENSE for license or visit https://github.com/taocpp/PEGTL/

#ifndef TAO_JSON_PEGTL_INTERNAL_MISSING_APPLY_HPP
#define TAO_JSON_PEGTL_INTERNAL_MISSING_APPLY_HPP

#include "../config.hpp"
#include "../rewind_mode.hpp"

namespace TAO_JSON_PEGTL_NAMESPACE::internal
{
   template< typename Control,
             template< typename... >
             class Action,
             typename Input,
             typename... States >
   void missing_apply( Input& in, States&&... st )
   {
      auto m = in.template mark< rewind_mode::required >();
      (void)Control::template apply< Action >( m.iterator(), in, st... );
   }

}  // namespace TAO_JSON_PEGTL_NAMESPACE::internal

#endif
