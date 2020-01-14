// Copyright (c) 2014-2020 Dr. Colin Hirsch and Daniel Frey
// Please see LICENSE for license or visit https://github.com/taocpp/PEGTL/

#ifndef TAO_JSON_PEGTL_ANALYSIS_RULE_TYPE_HPP
#define TAO_JSON_PEGTL_ANALYSIS_RULE_TYPE_HPP

#include "../config.hpp"

namespace TAO_JSON_PEGTL_NAMESPACE::analysis
{
   enum class rule_type : char
   {
      any,  // Consumption-on-success is always true; assumes bounded repetition of conjunction of sub-rules.
      opt,  // Consumption-on-success not necessarily true; assumes bounded repetition of conjunction of sub-rules.
      seq,  // Consumption-on-success depends on consumption of (non-zero bounded repetition of) conjunction of sub-rules.
      sor   // Consumption-on-success depends on consumption of (non-zero bounded repetition of) disjunction of sub-rules.
   };

}  // namespace TAO_JSON_PEGTL_NAMESPACE::analysis

#endif
