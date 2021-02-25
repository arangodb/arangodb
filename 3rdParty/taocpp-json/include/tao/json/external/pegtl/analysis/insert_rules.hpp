// Copyright (c) 2014-2020 Dr. Colin Hirsch and Daniel Frey
// Please see LICENSE for license or visit https://github.com/taocpp/PEGTL/

#ifndef TAO_JSON_PEGTL_ANALYSIS_INSERT_RULES_HPP
#define TAO_JSON_PEGTL_ANALYSIS_INSERT_RULES_HPP

#include "../config.hpp"

#include "grammar_info.hpp"
#include "rule_info.hpp"

namespace TAO_JSON_PEGTL_NAMESPACE::analysis
{
   template< typename... Rules >
   struct insert_rules
   {
      static void insert( grammar_info& g, rule_info& r )
      {
         ( r.rules.emplace_back( Rules::analyze_t::template insert< Rules >( g ) ), ... );
      }
   };

}  // namespace TAO_JSON_PEGTL_NAMESPACE::analysis

#endif
