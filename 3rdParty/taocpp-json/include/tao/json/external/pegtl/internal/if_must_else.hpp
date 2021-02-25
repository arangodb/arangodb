// Copyright (c) 2014-2020 Dr. Colin Hirsch and Daniel Frey
// Please see LICENSE for license or visit https://github.com/taocpp/PEGTL/

#ifndef TAO_JSON_PEGTL_INTERNAL_IF_MUST_ELSE_HPP
#define TAO_JSON_PEGTL_INTERNAL_IF_MUST_ELSE_HPP

#include "../config.hpp"

#include "if_then_else.hpp"
#include "must.hpp"

namespace TAO_JSON_PEGTL_NAMESPACE::internal
{
   template< typename Cond, typename Then, typename Else >
   using if_must_else = if_then_else< Cond, must< Then >, must< Else > >;

}  // namespace TAO_JSON_PEGTL_NAMESPACE::internal

#endif
