// Copyright (c) 2016-2020 Dr. Colin Hirsch and Daniel Frey
// Please see LICENSE for license or visit https://github.com/taocpp/PEGTL/

#ifndef TAO_JSON_PEGTL_INTERNAL_REQUIRE_HPP
#define TAO_JSON_PEGTL_INTERNAL_REQUIRE_HPP

#include "../config.hpp"

#include "skip_control.hpp"
#include "trivial.hpp"

#include "../analysis/generic.hpp"

namespace TAO_JSON_PEGTL_NAMESPACE::internal
{
   template< unsigned Amount >
   struct require;

   template<>
   struct require< 0 >
      : trivial< true >
   {
   };

   template< unsigned Amount >
   struct require
   {
      using analyze_t = analysis::generic< analysis::rule_type::opt >;

      template< typename Input >
      [[nodiscard]] static bool match( Input& in ) noexcept( noexcept( in.size( 0 ) ) )
      {
         return in.size( Amount ) >= Amount;
      }
   };

   template< unsigned Amount >
   inline constexpr bool skip_control< require< Amount > > = true;

}  // namespace TAO_JSON_PEGTL_NAMESPACE::internal

#endif
