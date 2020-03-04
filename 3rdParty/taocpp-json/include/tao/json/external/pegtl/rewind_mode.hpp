// Copyright (c) 2016-2020 Dr. Colin Hirsch and Daniel Frey
// Please see LICENSE for license or visit https://github.com/taocpp/PEGTL/

#ifndef TAO_JSON_PEGTL_REWIND_MODE_HPP
#define TAO_JSON_PEGTL_REWIND_MODE_HPP

#include "config.hpp"

namespace TAO_JSON_PEGTL_NAMESPACE
{
   enum class rewind_mode : char
   {
      active,
      required,
      dontcare
   };

}  // namespace TAO_JSON_PEGTL_NAMESPACE

#endif
