// Copyright (c) 2014-2020 Dr. Colin Hirsch and Daniel Frey
// Please see LICENSE for license or visit https://github.com/taocpp/PEGTL/

#ifndef TAO_JSON_PEGTL_ANALYSIS_COUNTED_HPP
#define TAO_JSON_PEGTL_ANALYSIS_COUNTED_HPP

#include "../config.hpp"

#include <cstddef>

#include "generic.hpp"

namespace TAO_JSON_PEGTL_NAMESPACE::analysis
{
   template< rule_type Type, std::size_t Count, typename... Rules >
   struct counted
      : generic< ( Count != 0 ) ? Type : rule_type::opt, Rules... >
   {
   };

}  // namespace TAO_JSON_PEGTL_NAMESPACE::analysis

#endif
