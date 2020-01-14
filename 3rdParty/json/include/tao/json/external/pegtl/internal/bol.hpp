// Copyright (c) 2017-2020 Dr. Colin Hirsch and Daniel Frey
// Please see LICENSE for license or visit https://github.com/taocpp/PEGTL/

#ifndef TAO_JSON_PEGTL_INTERNAL_BOL_HPP
#define TAO_JSON_PEGTL_INTERNAL_BOL_HPP

#include "../config.hpp"

#include "skip_control.hpp"

#include "../analysis/generic.hpp"

namespace TAO_JSON_PEGTL_NAMESPACE::internal
{
   struct bol
   {
      using analyze_t = analysis::generic< analysis::rule_type::opt >;

      template< typename Input >
      [[nodiscard]] static bool match( Input& in ) noexcept
      {
         return in.byte_in_line() == 0;
      }
   };

   template<>
   inline constexpr bool skip_control< bol > = true;

}  // namespace TAO_JSON_PEGTL_NAMESPACE::internal

#endif
