// Copyright (c) 2017-2020 Dr. Colin Hirsch and Daniel Frey
// Please see LICENSE for license or visit https://github.com/taocpp/PEGTL/

#ifndef TAO_JSON_PEGTL_TRACKING_MODE_HPP
#define TAO_JSON_PEGTL_TRACKING_MODE_HPP

#include "config.hpp"

namespace TAO_JSON_PEGTL_NAMESPACE
{
   enum class tracking_mode : bool
   {
      eager,
      lazy
   };

}  // namespace TAO_JSON_PEGTL_NAMESPACE

#endif
