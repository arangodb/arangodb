// Copyright (c) 2014-2020 Dr. Colin Hirsch and Daniel Frey
// Please see LICENSE for license or visit https://github.com/taocpp/PEGTL/

#ifndef TAO_JSON_PEGTL_INTERNAL_TRY_CATCH_TYPE_HPP
#define TAO_JSON_PEGTL_INTERNAL_TRY_CATCH_TYPE_HPP

#include <type_traits>

#include "../config.hpp"

#include "duseltronik.hpp"
#include "seq.hpp"
#include "skip_control.hpp"
#include "trivial.hpp"

#include "../apply_mode.hpp"
#include "../rewind_mode.hpp"

#include "../analysis/generic.hpp"

namespace TAO_JSON_PEGTL_NAMESPACE::internal
{
   template< typename Exception, typename... Rules >
   struct try_catch_type;

   template< typename Exception >
   struct try_catch_type< Exception >
      : trivial< true >
   {
   };

   template< typename Exception, typename... Rules >
   struct try_catch_type
   {
      using analyze_t = analysis::generic< analysis::rule_type::seq, Rules... >;

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

         try {
            return m( duseltronik< seq< Rules... >, A, m_t::next_rewind_mode, Action, Control >::match( in, st... ) );
         }
         catch( const Exception& ) {
            return false;
         }
      }
   };

   template< typename Exception, typename... Rules >
   inline constexpr bool skip_control< try_catch_type< Exception, Rules... > > = true;

}  // namespace TAO_JSON_PEGTL_NAMESPACE::internal

#endif
