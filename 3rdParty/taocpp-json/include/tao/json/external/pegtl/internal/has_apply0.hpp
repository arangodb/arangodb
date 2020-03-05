// Copyright (c) 2017-2020 Dr. Colin Hirsch and Daniel Frey
// Please see LICENSE for license or visit https://github.com/taocpp/PEGTL/

#ifndef TAO_JSON_PEGTL_INTERNAL_HAS_APPLY0_HPP
#define TAO_JSON_PEGTL_INTERNAL_HAS_APPLY0_HPP

#include <type_traits>

#include "../config.hpp"

namespace TAO_JSON_PEGTL_NAMESPACE::internal
{
   template< typename, typename, template< typename... > class, typename... >
   struct has_apply0
      : std::false_type
   {};

   template< typename C, template< typename... > class Action, typename... S >
   struct has_apply0< C, decltype( C::template apply0< Action >( std::declval< S >()... ) ), Action, S... >
      : std::true_type
   {};

}  // namespace TAO_JSON_PEGTL_NAMESPACE::internal

#endif
