// Copyright (c) 2014-2020 Dr. Colin Hirsch and Daniel Frey
// Please see LICENSE for license or visit https://github.com/taocpp/PEGTL/

#ifndef TAO_JSON_PEGTL_INTERNAL_LIST_MUST_HPP
#define TAO_JSON_PEGTL_INTERNAL_LIST_MUST_HPP

#include "../config.hpp"

#include "must.hpp"
#include "seq.hpp"
#include "star.hpp"

namespace TAO_JSON_PEGTL_NAMESPACE::internal
{
   template< typename Rule, typename Sep >
   using list_must = seq< Rule, star< Sep, must< Rule > > >;

}  // namespace TAO_JSON_PEGTL_NAMESPACE::internal

#endif
