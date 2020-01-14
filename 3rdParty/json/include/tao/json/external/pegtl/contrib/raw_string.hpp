// Copyright (c) 2014-2020 Dr. Colin Hirsch and Daniel Frey
// Please see LICENSE for license or visit https://github.com/taocpp/PEGTL/

#ifndef TAO_JSON_PEGTL_CONTRIB_RAW_STRING_HPP
#define TAO_JSON_PEGTL_CONTRIB_RAW_STRING_HPP

#include <cstddef>
#include <type_traits>

#include "../apply_mode.hpp"
#include "../config.hpp"
#include "../rewind_mode.hpp"

#include "../internal/bytes.hpp"
#include "../internal/eof.hpp"
#include "../internal/eol.hpp"
#include "../internal/must.hpp"
#include "../internal/not_at.hpp"
#include "../internal/seq.hpp"
#include "../internal/skip_control.hpp"
#include "../internal/star.hpp"

#include "../analysis/generic.hpp"

namespace TAO_JSON_PEGTL_NAMESPACE
{
   namespace internal
   {
      template< char Open, char Marker >
      struct raw_string_open
      {
         using analyze_t = analysis::generic< analysis::rule_type::any >;

         template< apply_mode A,
                   rewind_mode,
                   template< typename... >
                   class Action,
                   template< typename... >
                   class Control,
                   typename Input,
                   typename... States >
         [[nodiscard]] static bool match( Input& in, std::size_t& marker_size, States&&... /*unused*/ ) noexcept( noexcept( in.size( 0 ) ) )
         {
            if( in.empty() || ( in.peek_char( 0 ) != Open ) ) {
               return false;
            }
            for( std::size_t i = 1; i < in.size( i + 1 ); ++i ) {
               switch( const auto c = in.peek_char( i ) ) {
                  case Open:
                     marker_size = i + 1;
                     in.bump_in_this_line( marker_size );
                     (void)eol::match( in );
                     return true;
                  case Marker:
                     break;
                  default:
                     return false;
               }
            }
            return false;
         }
      };

      template< char Open, char Marker >
      inline constexpr bool skip_control< raw_string_open< Open, Marker > > = true;

      template< char Marker, char Close >
      struct at_raw_string_close
      {
         using analyze_t = analysis::generic< analysis::rule_type::opt >;

         template< apply_mode A,
                   rewind_mode,
                   template< typename... >
                   class Action,
                   template< typename... >
                   class Control,
                   typename Input,
                   typename... States >
         [[nodiscard]] static bool match( Input& in, const std::size_t& marker_size, States&&... /*unused*/ ) noexcept( noexcept( in.size( 0 ) ) )
         {
            if( in.size( marker_size ) < marker_size ) {
               return false;
            }
            if( in.peek_char( 0 ) != Close ) {
               return false;
            }
            if( in.peek_char( marker_size - 1 ) != Close ) {
               return false;
            }
            for( std::size_t i = 0; i < ( marker_size - 2 ); ++i ) {
               if( in.peek_char( i + 1 ) != Marker ) {
                  return false;
               }
            }
            return true;
         }
      };

      template< char Marker, char Close >
      inline constexpr bool skip_control< at_raw_string_close< Marker, Close > > = true;

      template< typename Cond, typename... Rules >
      struct raw_string_until;

      template< typename Cond >
      struct raw_string_until< Cond >
      {
         using analyze_t = analysis::generic< analysis::rule_type::seq, star< not_at< Cond >, not_at< eof >, bytes< 1 > >, Cond >;

         template< apply_mode A,
                   rewind_mode M,
                   template< typename... >
                   class Action,
                   template< typename... >
                   class Control,
                   typename Input,
                   typename... States >
         [[nodiscard]] static bool match( Input& in, const std::size_t& marker_size, States&&... st )
         {
            auto m = in.template mark< M >();

            while( !Control< Cond >::template match< A, rewind_mode::required, Action, Control >( in, marker_size, st... ) ) {
               if( in.empty() ) {
                  return false;
               }
               in.bump();
            }
            return m( true );
         }
      };

      template< typename Cond, typename... Rules >
      struct raw_string_until
      {
         using analyze_t = analysis::generic< analysis::rule_type::seq, star< not_at< Cond >, not_at< eof >, Rules... >, Cond >;

         template< apply_mode A,
                   rewind_mode M,
                   template< typename... >
                   class Action,
                   template< typename... >
                   class Control,
                   typename Input,
                   typename... States >
         [[nodiscard]] static bool match( Input& in, const std::size_t& marker_size, States&&... st )
         {
            auto m = in.template mark< M >();
            using m_t = decltype( m );

            while( !Control< Cond >::template match< A, rewind_mode::required, Action, Control >( in, marker_size, st... ) ) {
               if( in.empty() || !( Control< Rules >::template match< A, m_t::next_rewind_mode, Action, Control >( in, st... ) && ... ) ) {
                  return false;
               }
            }
            return m( true );
         }
      };

      template< typename Cond, typename... Rules >
      inline constexpr bool skip_control< raw_string_until< Cond, Rules... > > = true;

   }  // namespace internal

   // raw_string matches Lua-style long literals.
   //
   // The following description was taken from the Lua documentation
   // (see http://www.lua.org/docs.html):
   //
   // - An "opening long bracket of level n" is defined as an opening square
   //   bracket followed by n equal signs followed by another opening square
   //   bracket. So, an opening long bracket of level 0 is written as `[[`,
   //   an opening long bracket of level 1 is written as `[=[`, and so on.
   // - A "closing long bracket" is defined similarly; for instance, a closing
   //   long bracket of level 4 is written as `]====]`.
   // - A "long literal" starts with an opening long bracket of any level and
   //   ends at the first closing long bracket of the same level. It can
   //   contain any text except a closing bracket of the same level.
   // - Literals in this bracketed form can run for several lines, do not
   //   interpret any escape sequences, and ignore long brackets of any other
   //   level.
   // - For convenience, when the opening long bracket is eagerly followed
   //   by a newline, the newline is not included in the string.
   //
   // Note that unlike Lua's long literal, a raw_string is customizable to use
   // other characters than `[`, `=` and `]` for matching. Also note that Lua
   // introduced newline-specific replacements in Lua 5.2, which we do not
   // support on the grammar level.

   template< char Open, char Marker, char Close, typename... Contents >
   struct raw_string
   {
      // This is used for binding the apply()-method and for error-reporting
      // when a raw string is not closed properly or has invalid content.
      struct content
         : internal::raw_string_until< internal::at_raw_string_close< Marker, Close >, Contents... >
      {
      };

      using analyze_t = typename internal::seq< internal::bytes< 1 >, content, internal::bytes< 1 > >::analyze_t;

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
         std::size_t marker_size;
         if( internal::raw_string_open< Open, Marker >::template match< A, M, Action, Control >( in, marker_size, st... ) ) {
            // TODO: Do not rely on must<>
            (void)internal::must< content >::template match< A, M, Action, Control >( in, marker_size, st... );
            in.bump_in_this_line( marker_size );
            return true;
         }
         return false;
      }
   };

}  // namespace TAO_JSON_PEGTL_NAMESPACE

#endif
