// Copyright (c) 2018-2020 Dr. Colin Hirsch and Daniel Frey
// Please see LICENSE for license or visit https://github.com/taocpp/json/

#ifndef TAO_JSON_CBOR_CONSUME_FILE_HPP
#define TAO_JSON_CBOR_CONSUME_FILE_HPP

#include <utility>

#include "../external/pegtl/file_input.hpp"

#include "../consume.hpp"
#include "../forward.hpp"

#include "parts_parser.hpp"

namespace tao::json::cbor
{
   template< typename T, template< typename... > class Traits = traits, typename F >
   [[nodiscard]] T consume_file( F&& filename )
   {
      cbor::basic_parts_parser< utf8_mode::check, pegtl::file_input< pegtl::tracking_mode::lazy > > pp( std::forward< F >( filename ) );
      return json::consume< T, Traits >( pp );
   }

   template< template< typename... > class Traits = traits, typename F, typename T >
   void consume_file( F&& filename, T& t )
   {
      cbor::basic_parts_parser< utf8_mode::check, pegtl::file_input< pegtl::tracking_mode::lazy > > pp( std::forward< F >( filename ) );
      json::consume< Traits >( pp, t );
   }

}  // namespace tao::json::cbor

#endif
