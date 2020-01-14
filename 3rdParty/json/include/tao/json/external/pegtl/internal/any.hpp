// Copyright (c) 2014-2020 Dr. Colin Hirsch and Daniel Frey
// Please see LICENSE for license or visit https://github.com/taocpp/PEGTL/

#ifndef TAO_JSON_PEGTL_INTERNAL_ANY_HPP
#define TAO_JSON_PEGTL_INTERNAL_ANY_HPP

#include "../config.hpp"

#include "peek_char.hpp"
#include "skip_control.hpp"

#include "../analysis/generic.hpp"

namespace TAO_JSON_PEGTL_NAMESPACE::internal
{
   template< typename Peek >
   struct any;

   template<>
   struct any< peek_char >
   {
      using analyze_t = analysis::generic< analysis::rule_type::any >;

      template< typename Input >
      [[nodiscard]] static bool match( Input& in ) noexcept( noexcept( in.empty() ) )
      {
         if( !in.empty() ) {
            in.bump();
            return true;
         }
         return false;
      }
   };

   template< typename Peek >
   struct any
   {
      using analyze_t = analysis::generic< analysis::rule_type::any >;

      template< typename Input >
      [[nodiscard]] static bool match( Input& in ) noexcept( noexcept( in.size( Peek::max_input_size ) ) )
      {
         if( const std::size_t s = in.size( Peek::max_input_size ); s >= Peek::min_input_size ) {
            if( const auto t = Peek::peek( in, s ) ) {
               in.bump( t.size );
               return true;
            }
         }
         return false;
      }
   };

   template< typename Peek >
   inline constexpr bool skip_control< any< Peek > > = true;

}  // namespace TAO_JSON_PEGTL_NAMESPACE::internal

#endif
