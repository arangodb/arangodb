// Copyright (c) 2014-2020 Dr. Colin Hirsch and Daniel Frey
// Please see LICENSE for license or visit https://github.com/taocpp/PEGTL/

#ifndef TAO_JSON_PEGTL_NOTHING_HPP
#define TAO_JSON_PEGTL_NOTHING_HPP

#include "config.hpp"

namespace TAO_JSON_PEGTL_NAMESPACE
{
   template< typename Rule >
   struct nothing
   {
   };

   using maybe_nothing = nothing< void >;

}  // namespace TAO_JSON_PEGTL_NAMESPACE

#endif
