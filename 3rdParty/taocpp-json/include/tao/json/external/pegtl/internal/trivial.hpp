// Copyright (c) 2014-2020 Dr. Colin Hirsch and Daniel Frey
// Please see LICENSE for license or visit https://github.com/taocpp/PEGTL/

#ifndef TAO_JSON_PEGTL_INTERNAL_TRIVIAL_HPP
#define TAO_JSON_PEGTL_INTERNAL_TRIVIAL_HPP

#include "../config.hpp"

#include "skip_control.hpp"

#include "../analysis/counted.hpp"

namespace TAO_JSON_PEGTL_NAMESPACE::internal
{
   template< bool Result >
   struct trivial
   {
      using analyze_t = analysis::counted< analysis::rule_type::any, unsigned( !Result ) >;

      template< typename Input >
      [[nodiscard]] static bool match( Input& /*unused*/ ) noexcept
      {
         return Result;
      }
   };

   template< bool Result >
   inline constexpr bool skip_control< trivial< Result > > = true;

}  // namespace TAO_JSON_PEGTL_NAMESPACE::internal

#endif
