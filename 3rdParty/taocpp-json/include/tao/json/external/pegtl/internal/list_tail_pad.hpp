// Copyright (c) 2014-2020 Dr. Colin Hirsch and Daniel Frey
// Please see LICENSE for license or visit https://github.com/taocpp/PEGTL/

#ifndef TAO_JSON_PEGTL_INTERNAL_LIST_TAIL_PAD_HPP
#define TAO_JSON_PEGTL_INTERNAL_LIST_TAIL_PAD_HPP

#include "../config.hpp"

#include "list.hpp"
#include "opt.hpp"
#include "pad.hpp"
#include "seq.hpp"
#include "star.hpp"

namespace TAO_JSON_PEGTL_NAMESPACE::internal
{
   template< typename Rule, typename Sep, typename Pad >
   using list_tail_pad = seq< list< Rule, pad< Sep, Pad > >, opt< star< Pad >, Sep > >;

}  // namespace TAO_JSON_PEGTL_NAMESPACE::internal

#endif
