// Copyright (c) 2018-2020 Dr. Colin Hirsch and Daniel Frey
// Please see LICENSE for license or visit https://github.com/taocpp/json/

#ifndef TAO_JSON_JAXN_CONSUME_FILE_HPP
#define TAO_JSON_JAXN_CONSUME_FILE_HPP

#include <utility>

#include "../external/pegtl/file_input.hpp"

#include "../consume.hpp"
#include "../forward.hpp"

#include "parts_parser.hpp"

namespace tao::json::jaxn
{
   template< typename T, template< typename... > class Traits = traits, typename F >
   [[nodiscard]] T consume_file( F&& filename )
   {
      jaxn::basic_parts_parser< pegtl::file_input< pegtl::tracking_mode::lazy > > pp( std::forward< F >( filename ) );
      return json::consume< T, Traits >( pp );
   }

   template< template< typename... > class Traits = traits, typename F, typename T >
   void consume_file( F&& filename, T& t )
   {
      jaxn::basic_parts_parser< pegtl::file_input< pegtl::tracking_mode::lazy > > pp( std::forward< F >( filename ) );
      json::consume< Traits >( pp, t );
   }

}  // namespace tao::json::jaxn

#endif
