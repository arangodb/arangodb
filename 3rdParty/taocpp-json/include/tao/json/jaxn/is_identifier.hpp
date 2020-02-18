// Copyright (c) 2017-2020 Dr. Colin Hirsch and Daniel Frey
// Please see LICENSE for license or visit https://github.com/taocpp/json/

#ifndef TAO_JSON_JAXN_IS_IDENTIFIER_HPP
#define TAO_JSON_JAXN_IS_IDENTIFIER_HPP

#include <cctype>
#include <string_view>

namespace tao::json::jaxn
{
   [[nodiscard]] inline bool is_identifier( const std::string_view v ) noexcept
   {
      if( v.empty() || std::isdigit( v[ 0 ] ) ) {
         return false;
      }
      for( const auto c : v ) {
         if( !std::isalnum( c ) && c != '_' ) {
            return false;
         }
      }
      return true;
   }

}  // namespace tao::json::jaxn

#endif
