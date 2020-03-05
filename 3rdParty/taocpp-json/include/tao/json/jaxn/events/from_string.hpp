// Copyright (c) 2017-2020 Dr. Colin Hirsch and Daniel Frey
// Please see LICENSE for license or visit https://github.com/taocpp/json/

#ifndef TAO_JSON_JAXN_EVENTS_FROM_STRING_HPP
#define TAO_JSON_JAXN_EVENTS_FROM_STRING_HPP

#include <string>
#include <string_view>

#include "../../external/pegtl/memory_input.hpp"

#include "from_input.hpp"

namespace tao::json::jaxn::events
{
   // Events producer to parse a JAXN string representation.

   template< typename Consumer >
   void from_string( Consumer& consumer, const char* data, const std::size_t size, const char* source = nullptr, const std::size_t byte = 0, const std::size_t line = 1, const std::size_t column = 0 )
   {
      pegtl::memory_input< pegtl::tracking_mode::lazy, pegtl::eol::lf_crlf, const char* > in( data, data + size, source ? source : "tao::json::jaxn::events::from_string", byte, line, column );
      jaxn::events::from_input( consumer, std::move( in ) );
   }

   template< typename Consumer >
   void from_string( Consumer& consumer, const char* data, const std::size_t size, const std::string& source, const std::size_t byte = 0, const std::size_t line = 1, const std::size_t column = 0 )
   {
      jaxn::events::from_string( consumer, data, size, source.c_str(), byte, line, column );
   }

   template< typename Consumer, typename... Ts >
   void from_string( Consumer& consumer, const std::string_view data, Ts&&... ts )
   {
      jaxn::events::from_string( consumer, data.data(), data.size(), std::forward< Ts >( ts )... );
   }

}  // namespace tao::json::jaxn::events

#endif
