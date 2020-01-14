// Copyright (c) 2014-2020 Dr. Colin Hirsch and Daniel Frey
// Please see LICENSE for license or visit https://github.com/taocpp/PEGTL/

#ifndef TAO_JSON_PEGTL_INTERNAL_RESULT_ON_FOUND_HPP
#define TAO_JSON_PEGTL_INTERNAL_RESULT_ON_FOUND_HPP

#include "../config.hpp"

namespace TAO_JSON_PEGTL_NAMESPACE::internal
{
   enum class result_on_found : bool
   {
      success = true,
      failure = false
   };

}  // namespace TAO_JSON_PEGTL_NAMESPACE::internal

#endif
