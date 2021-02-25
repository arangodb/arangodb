// Copyright (c) 2014-2020 Dr. Colin Hirsch and Daniel Frey
// Please see LICENSE for license or visit https://github.com/taocpp/PEGTL/

#ifndef TAO_JSON_PEGTL_INTERNAL_RANGES_HPP
#define TAO_JSON_PEGTL_INTERNAL_RANGES_HPP

#include "../config.hpp"

#include "range.hpp"
#include "skip_control.hpp"

#include "../analysis/generic.hpp"

namespace TAO_JSON_PEGTL_NAMESPACE::internal
{
   template< int Eol, typename Char, Char... Cs >
   struct ranges_impl;

   template< int Eol, typename Char >
   struct ranges_impl< Eol, Char >
   {
      static constexpr bool can_match_eol = false;

      [[nodiscard]] static bool match( const Char /*unused*/ ) noexcept
      {
         return false;
      }
   };

   template< int Eol, typename Char, Char Eq >
   struct ranges_impl< Eol, Char, Eq >
   {
      static constexpr bool can_match_eol = ( Eq == Eol );

      [[nodiscard]] static bool match( const Char c ) noexcept
      {
         return c == Eq;
      }
   };

   template< int Eol, typename Char, Char Lo, Char Hi, Char... Cs >
   struct ranges_impl< Eol, Char, Lo, Hi, Cs... >
   {
      static_assert( Lo <= Hi, "invalid range detected" );

      static constexpr bool can_match_eol = ( ( ( Lo <= Eol ) && ( Eol <= Hi ) ) || ranges_impl< Eol, Char, Cs... >::can_match_eol );

      [[nodiscard]] static bool match( const Char c ) noexcept
      {
         return ( ( Lo <= c ) && ( c <= Hi ) ) || ranges_impl< Eol, Char, Cs... >::match( c );
      }
   };

   template< typename Peek, typename Peek::data_t... Cs >
   struct ranges
   {
      using analyze_t = analysis::generic< analysis::rule_type::any >;

      template< int Eol >
      static constexpr bool can_match_eol = ranges_impl< Eol, typename Peek::data_t, Cs... >::can_match_eol;

      template< typename Input >
      [[nodiscard]] static bool match( Input& in ) noexcept( noexcept( in.size( Peek::max_input_size ) ) )
      {
         if( const std::size_t s = in.size( Peek::max_input_size ); s >= Peek::min_input_size ) {
            if( const auto t = Peek::peek( in, s ) ) {
               if( ranges_impl< Input::eol_t::ch, typename Peek::data_t, Cs... >::match( t.data ) ) {
                  if constexpr( can_match_eol< Input::eol_t::ch > ) {
                     in.bump( t.size );
                  }
                  else {
                     in.bump_in_this_line( t.size );
                  }
                  return true;
               }
            }
         }
         return false;
      }
   };

   template< typename Peek, typename Peek::data_t Lo, typename Peek::data_t Hi >
   struct ranges< Peek, Lo, Hi >
      : range< result_on_found::success, Peek, Lo, Hi >
   {
   };

   template< typename Peek, typename Peek::data_t... Cs >
   inline constexpr bool skip_control< ranges< Peek, Cs... > > = true;

}  // namespace TAO_JSON_PEGTL_NAMESPACE::internal

#endif
