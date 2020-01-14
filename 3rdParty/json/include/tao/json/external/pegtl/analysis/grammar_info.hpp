// Copyright (c) 2014-2020 Dr. Colin Hirsch and Daniel Frey
// Please see LICENSE for license or visit https://github.com/taocpp/PEGTL/

#ifndef TAO_JSON_PEGTL_ANALYSIS_GRAMMAR_INFO_HPP
#define TAO_JSON_PEGTL_ANALYSIS_GRAMMAR_INFO_HPP

#include <map>
#include <string>
#include <utility>

#include "../config.hpp"
#include "../internal/demangle.hpp"

#include "rule_info.hpp"

namespace TAO_JSON_PEGTL_NAMESPACE::analysis
{
   struct grammar_info
   {
      using map_t = std::map< std::string_view, rule_info >;
      map_t map;

      template< typename Name >
      auto insert( const rule_type type )
      {
         return map.try_emplace( internal::demangle< Name >(), rule_info( type ) );
      }
   };

}  // namespace TAO_JSON_PEGTL_NAMESPACE::analysis

#endif
