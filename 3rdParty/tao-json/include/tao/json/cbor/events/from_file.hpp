// Copyright (c) 2018-2020 Dr. Colin Hirsch and Daniel Frey
// Please see LICENSE for license or visit https://github.com/taocpp/json/

#ifndef TAO_JSON_CBOR_EVENTS_FROM_FILE_HPP
#define TAO_JSON_CBOR_EVENTS_FROM_FILE_HPP

#include <utility>

#include "../../external/pegtl/file_input.hpp"
#include "../../external/pegtl/parse.hpp"

#include "../internal/grammar.hpp"

namespace tao::json::cbor::events
{
   // Events producer to parse a file containing a CBOR string representation.

   template< typename T, typename Consumer >
   void from_file( Consumer& consumer, T&& filename )
   {
      pegtl::file_input< pegtl::tracking_mode::lazy > in( std::forward< T >( filename ) );
      pegtl::parse< internal::grammar >( in, consumer );
   }

}  // namespace tao::json::cbor::events

#endif
