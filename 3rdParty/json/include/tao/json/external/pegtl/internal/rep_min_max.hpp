// Copyright (c) 2014-2020 Dr. Colin Hirsch and Daniel Frey
// Please see LICENSE for license or visit https://github.com/taocpp/PEGTL/

#ifndef TAO_JSON_PEGTL_INTERNAL_REP_MIN_MAX_HPP
#define TAO_JSON_PEGTL_INTERNAL_REP_MIN_MAX_HPP

#include <type_traits>

#include "../config.hpp"

#include "duseltronik.hpp"
#include "not_at.hpp"
#include "seq.hpp"
#include "skip_control.hpp"
#include "trivial.hpp"

#include "../apply_mode.hpp"
#include "../rewind_mode.hpp"

#include "../analysis/counted.hpp"

namespace TAO_JSON_PEGTL_NAMESPACE::internal
{
   template< unsigned Min, unsigned Max, typename... Rules >
   struct rep_min_max;

   template< unsigned Min, unsigned Max >
   struct rep_min_max< Min, Max >
      : trivial< false >
   {
      static_assert( Min <= Max );
   };

   template< typename Rule, typename... Rules >
   struct rep_min_max< 0, 0, Rule, Rules... >
      : not_at< Rule, Rules... >
   {
   };

   template< unsigned Min, unsigned Max, typename... Rules >
   struct rep_min_max
   {
      using analyze_t = analysis::counted< analysis::rule_type::seq, Min, Rules... >;

      static_assert( Min <= Max );

      template< apply_mode A,
                rewind_mode M,
                template< typename... >
                class Action,
                template< typename... >
                class Control,
                typename Input,
                typename... States >
      [[nodiscard]] static bool match( Input& in, States&&... st )
      {
         auto m = in.template mark< M >();
         using m_t = decltype( m );

         for( unsigned i = 0; i != Min; ++i ) {
            if( !( Control< Rules >::template match< A, m_t::next_rewind_mode, Action, Control >( in, st... ) && ... ) ) {
               return false;
            }
         }
         for( unsigned i = Min; i != Max; ++i ) {
            if( !duseltronik< seq< Rules... >, A, rewind_mode::required, Action, Control >::match( in, st... ) ) {
               return m( true );
            }
         }
         return m( duseltronik< not_at< Rules... >, A, m_t::next_rewind_mode, Action, Control >::match( in, st... ) );  // NOTE that not_at<> will always rewind.
      }
   };

   template< unsigned Min, unsigned Max, typename... Rules >
   inline constexpr bool skip_control< rep_min_max< Min, Max, Rules... > > = true;

}  // namespace TAO_JSON_PEGTL_NAMESPACE::internal

#endif
