// Copyright (c) 2014-2020 Dr. Colin Hirsch and Daniel Frey
// Please see LICENSE for license or visit https://github.com/taocpp/PEGTL/

#ifndef TAO_JSON_PEGTL_APPLY_MODE_HPP
#define TAO_JSON_PEGTL_APPLY_MODE_HPP

#include "config.hpp"

namespace TAO_JSON_PEGTL_NAMESPACE
{
   enum class apply_mode : bool
   {
      action = true,
      nothing = false
   };

}  // namespace TAO_JSON_PEGTL_NAMESPACE

#endif
