// Copyright (c) 2017-2020 Dr. Colin Hirsch and Daniel Frey
// Please see LICENSE for license or visit https://github.com/taocpp/json/

#ifndef TAO_JSON_JAXN_FROM_FILE_HPP
#define TAO_JSON_JAXN_FROM_FILE_HPP

#include <string>
#include <utility>

#include "../events/to_value.hpp"
#include "../events/transformer.hpp"

#include "events/from_file.hpp"

namespace tao::json::jaxn
{
   template< template< typename... > class Traits, template< typename... > class... Transformers >
   [[nodiscard]] basic_value< Traits > basic_from_file( const std::string& filename )
   {
      json::events::transformer< json::events::to_basic_value< Traits >, Transformers... > consumer;
      events::from_file( consumer, filename );
      return std::move( consumer.value );
   }

   template< template< typename... > class... Transformers >
   [[nodiscard]] value from_file( const std::string& filename )
   {
      return basic_from_file< traits, Transformers... >( filename );
   }

}  // namespace tao::json::jaxn

#endif
