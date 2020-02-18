// Copyright (c) 2014-2020 Dr. Colin Hirsch and Daniel Frey
// Please see LICENSE for license or visit https://github.com/taocpp/PEGTL/

#ifndef TAO_JSON_PEGTL_INTERNAL_STAR_MUST_HPP
#define TAO_JSON_PEGTL_INTERNAL_STAR_MUST_HPP

#include "../config.hpp"

#include "if_must.hpp"
#include "star.hpp"

namespace TAO_JSON_PEGTL_NAMESPACE::internal
{
   template< typename Cond, typename... Rules >
   using star_must = star< if_must< false, Cond, Rules... > >;

}  // namespace TAO_JSON_PEGTL_NAMESPACE::internal

#endif
