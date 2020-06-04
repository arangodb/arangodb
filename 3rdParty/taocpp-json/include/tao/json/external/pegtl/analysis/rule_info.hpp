// Copyright (c) 2014-2020 Dr. Colin Hirsch and Daniel Frey
// Please see LICENSE for license or visit https://github.com/taocpp/PEGTL/

#ifndef TAO_JSON_PEGTL_ANALYSIS_RULE_INFO_HPP
#define TAO_JSON_PEGTL_ANALYSIS_RULE_INFO_HPP

#include <string>
#include <vector>

#include "../config.hpp"

#include "rule_type.hpp"

namespace TAO_JSON_PEGTL_NAMESPACE::analysis
{
   struct rule_info
   {
      explicit rule_info( const rule_type in_type ) noexcept
         : type( in_type )
      {
      }

      rule_type type;
      std::vector< std::string > rules;
   };

}  // namespace TAO_JSON_PEGTL_NAMESPACE::analysis

#endif
