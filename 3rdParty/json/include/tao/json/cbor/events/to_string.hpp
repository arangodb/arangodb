// Copyright (c) 2017-2020 Dr. Colin Hirsch and Daniel Frey
// Please see LICENSE for license or visit https://github.com/taocpp/json/

#ifndef TAO_JSON_CBOR_EVENTS_TO_STRING_HPP
#define TAO_JSON_CBOR_EVENTS_TO_STRING_HPP

#include <sstream>

#include "to_stream.hpp"

namespace tao::json::cbor::events
{
   struct to_string
      : to_stream
   {
      std::ostringstream oss;

      to_string()
         : to_stream( oss )
      {
      }

      [[nodiscard]] std::string value() const
      {
         return oss.str();
      }
   };

}  // namespace tao::json::cbor::events

#endif
