// Copyright (c) 2017-2020 Dr. Colin Hirsch and Daniel Frey
// Please see LICENSE for license or visit https://github.com/taocpp/PEGTL/

#ifndef TAO_JSON_PEGTL_CONTRIB_REP_ONE_MIN_MAX_HPP
#define TAO_JSON_PEGTL_CONTRIB_REP_ONE_MIN_MAX_HPP

#include <algorithm>

#include "../config.hpp"

#include "../analysis/counted.hpp"

#include "../internal/bump_help.hpp"
#include "../internal/skip_control.hpp"

namespace TAO_JSON_PEGTL_NAMESPACE
{
   namespace internal
   {
      template< unsigned Min, unsigned Max, char C >
      struct rep_one_min_max
      {
         using analyze_t = analysis::counted< analysis::rule_type::any, Min >;

         static_assert( Min <= Max );

         template< typename Input >
         [[nodiscard]] static bool match( Input& in )
         {
            const auto size = in.size( Max + 1 );
            if( size < Min ) {
               return false;
            }
            std::size_t i = 0;
            while( ( i < size ) && ( in.peek_char( i ) == C ) ) {
               ++i;
            }
            if( ( Min <= i ) && ( i <= Max ) ) {
               bump_help< result_on_found::success, Input, char, C >( in, i );
               return true;
            }
            return false;
         }
      };

      template< unsigned Min, unsigned Max, char C >
      inline constexpr bool skip_control< rep_one_min_max< Min, Max, C > > = true;

   }  // namespace internal

   inline namespace ascii
   {
      template< unsigned Min, unsigned Max, char C >
      struct rep_one_min_max : internal::rep_one_min_max< Min, Max, C >
      {
      };

   }  // namespace ascii

}  // namespace TAO_JSON_PEGTL_NAMESPACE

#endif
