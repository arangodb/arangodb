// Copyright (c) 2014-2020 Dr. Colin Hirsch and Daniel Frey
// Please see LICENSE for license or visit https://github.com/taocpp/PEGTL/

#ifndef TAO_JSON_PEGTL_INTERNAL_SKIP_CONTROL_HPP
#define TAO_JSON_PEGTL_INTERNAL_SKIP_CONTROL_HPP

#include <type_traits>

#include "../config.hpp"

namespace TAO_JSON_PEGTL_NAMESPACE::internal
{
   // This class is a simple tagging mechanism.
   // By default, skip_control< Rule > is  'false'.
   // Each internal (!) rule that should be hidden
   // from the control and action class' callbacks
   // simply specializes skip_control<> to return
   // 'true' for the above expression.

   template< typename Rule >
   inline constexpr bool skip_control = false;

}  // namespace TAO_JSON_PEGTL_NAMESPACE::internal

#endif
