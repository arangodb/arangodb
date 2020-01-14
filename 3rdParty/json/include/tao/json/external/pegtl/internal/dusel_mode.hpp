// Copyright (c) 2017-2020 Dr. Colin Hirsch and Daniel Frey
// Please see LICENSE for license or visit https://github.com/taocpp/PEGTL/

#ifndef TAO_JSON_PEGTL_INTERNAL_DUSEL_MODE_HPP
#define TAO_JSON_PEGTL_INTERNAL_DUSEL_MODE_HPP

#include "../config.hpp"

namespace TAO_JSON_PEGTL_NAMESPACE::internal
{
   enum class dusel_mode : char
   {
      nothing = 0,
      control = 1,
      control_and_apply_void = 2,
      control_and_apply_bool = 3,
      control_and_apply0_void = 4,
      control_and_apply0_bool = 5
   };

}  // namespace TAO_JSON_PEGTL_NAMESPACE::internal

#endif
