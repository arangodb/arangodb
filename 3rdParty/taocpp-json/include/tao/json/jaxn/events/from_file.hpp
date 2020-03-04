// Copyright (c) 2017-2020 Dr. Colin Hirsch and Daniel Frey
// Please see LICENSE for license or visit https://github.com/taocpp/json/

#ifndef TAO_JSON_JAXN_EVENTS_FROM_FILE_HPP
#define TAO_JSON_JAXN_EVENTS_FROM_FILE_HPP

#include <utility>

#include "../internal/action.hpp"
#include "../internal/errors.hpp"
#include "../internal/grammar.hpp"

#include "../../external/pegtl/file_input.hpp"

namespace tao::json::jaxn::events
{
   // Events producer to parse a file containing a JAXN string representation.

   template< typename T, typename Consumer >
   void from_file( Consumer& consumer, T&& filename )
   {
      pegtl::file_input< pegtl::tracking_mode::lazy > in( std::forward< T >( filename ) );
      pegtl::parse< internal::grammar, internal::action, internal::errors >( in, consumer );
   }

}  // namespace tao::json::jaxn::events

#endif
