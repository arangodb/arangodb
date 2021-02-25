// Copyright (c) 2014-2020 Dr. Colin Hirsch and Daniel Frey
// Please see LICENSE for license or visit https://github.com/taocpp/PEGTL/

#ifndef TAO_JSON_PEGTL_ANALYSIS_GENERIC_HPP
#define TAO_JSON_PEGTL_ANALYSIS_GENERIC_HPP

#include "../config.hpp"

#include "grammar_info.hpp"
#include "insert_rules.hpp"
#include "rule_type.hpp"

namespace TAO_JSON_PEGTL_NAMESPACE::analysis
{
   template< rule_type Type, typename... Rules >
   struct generic
   {
      template< typename Name >
      static std::string_view insert( grammar_info& g )
      {
         const auto [ it, success ] = g.insert< Name >( Type );
         if( success ) {
            insert_rules< Rules... >::insert( g, it->second );
         }
         return it->first;
      }
   };

}  // namespace TAO_JSON_PEGTL_NAMESPACE::analysis

#endif
