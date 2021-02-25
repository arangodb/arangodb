// Copyright (c) 2014-2020 Dr. Colin Hirsch and Daniel Frey
// Please see LICENSE for license or visit https://github.com/taocpp/PEGTL/

#ifndef TAO_JSON_PEGTL_INTERNAL_LIST_TAIL_HPP
#define TAO_JSON_PEGTL_INTERNAL_LIST_TAIL_HPP

#include "../config.hpp"

#include "list.hpp"
#include "opt.hpp"
#include "seq.hpp"

namespace TAO_JSON_PEGTL_NAMESPACE::internal
{
   template< typename Rule, typename Sep >
   using list_tail = seq< list< Rule, Sep >, opt< Sep > >;

}  // namespace TAO_JSON_PEGTL_NAMESPACE::internal

#endif
