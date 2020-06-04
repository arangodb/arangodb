// Copyright (c) 2015-2020 Dr. Colin Hirsch and Daniel Frey
// Please see LICENSE for license or visit https://github.com/taocpp/json/

#ifndef TAO_JSON_TO_STRING_HPP
#define TAO_JSON_TO_STRING_HPP

#include <sstream>

#include "to_stream.hpp"

namespace tao::json
{
   template< template< typename... > class... Transformers, typename... Ts >
   [[nodiscard]] std::string to_string( Ts&&... ts )
   {
      std::ostringstream o;
      json::to_stream< Transformers... >( o, std::forward< Ts >( ts )... );
      return o.str();
   }

}  // namespace tao::json

#endif
