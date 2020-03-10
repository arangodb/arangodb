// Copyright (c) 2019-2020 Dr. Colin Hirsch and Daniel Frey
// Please see LICENSE for license or visit https://github.com/taocpp/PEGTL/

#ifndef TAO_JSON_PEGTL_CONTRIB_INTEGER_HPP
#define TAO_JSON_PEGTL_CONTRIB_INTEGER_HPP

#include <cstdint>
#include <cstdlib>

#include <type_traits>

#include "../ascii.hpp"
#include "../parse.hpp"
#include "../parse_error.hpp"
#include "../rules.hpp"

namespace TAO_JSON_PEGTL_NAMESPACE::integer
{
   namespace internal
   {
      struct unsigned_rule_old
         : plus< digit >
      {
         // Pre-3.0 version of this rule.
      };

      struct unsigned_rule_new
         : if_then_else< one< '0' >, not_at< digit >, plus< digit > >
      {
         // New version that does not allow leading zeros.
      };

      struct signed_rule_old
         : seq< opt< one< '-', '+' > >, plus< digit > >
      {
         // Pre-3.0 version of this rule.
      };

      struct signed_rule_new
         : seq< opt< one< '-', '+' > >, if_then_else< one< '0' >, not_at< digit >, plus< digit > > >
      {
         // New version that does not allow leading zeros.
      };

      struct signed_rule_bis
         : seq< opt< one< '-' > >, if_then_else< one< '0' >, not_at< digit >, plus< digit > > >
      {
      };

      struct signed_rule_ter
         : seq< one< '-', '+' >, if_then_else< one< '0' >, not_at< digit >, plus< digit > > >
      {
      };

      [[nodiscard]] constexpr bool is_digit( const char c ) noexcept
      {
         // We don't use std::isdigit() because it might
         // return true for other values on MS platforms.

         return ( '0' <= c ) && ( c <= '9' );
      }

      template< typename Integer, Integer Maximum = ( std::numeric_limits< Integer >::max )() >
      [[nodiscard]] constexpr bool accumulate_digit( Integer& result, const char digit ) noexcept
      {
         // Assumes that digit is a digit as per is_digit(); returns false on overflow.

         static_assert( std::is_integral_v< Integer > );

         constexpr Integer cutoff = Maximum / 10;
         constexpr Integer cutlim = Maximum % 10;

         const Integer c = digit - '0';

         if( ( result > cutoff ) || ( ( result == cutoff ) && ( c > cutlim ) ) ) {
            return false;
         }
         result *= 10;
         result += c;
         return true;
      }

      template< typename Integer, Integer Maximum = ( std::numeric_limits< Integer >::max )() >
      [[nodiscard]] constexpr bool accumulate_digits( Integer& result, const std::string_view input ) noexcept
      {
         // Assumes input is a non-empty sequence of digits; returns false on overflow.

         for( char c : input ) {
            if( !accumulate_digit< Integer, Maximum >( result, c ) ) {
               return false;
            }
         }
         return true;
      }

      template< typename Integer, Integer Maximum = ( std::numeric_limits< Integer >::max )() >
      [[nodiscard]] constexpr bool convert_positive( Integer& result, const std::string_view input ) noexcept
      {
         // Assumes result == 0 and that input is a non-empty sequence of digits; returns false on overflow.

         static_assert( std::is_integral_v< Integer > );
         return accumulate_digits< Integer, Maximum >( result, input );
      }

      template< typename Signed >
      [[nodiscard]] constexpr bool convert_negative( Signed& result, const std::string_view input ) noexcept
      {
         // Assumes result == 0 and that input is a non-empty sequence of digits; returns false on overflow.

         static_assert( std::is_signed_v< Signed > );
         using Unsigned = std::make_unsigned_t< Signed >;
         constexpr Unsigned maximum = static_cast< Unsigned >( ( std::numeric_limits< Signed >::max )() ) + 1;
         Unsigned temporary = 0;
         if( accumulate_digits< Unsigned, maximum >( temporary, input ) ) {
            result = static_cast< Signed >( ~temporary ) + 1;
            return true;
         }
         return false;
      }

      template< typename Unsigned, Unsigned Maximum = ( std::numeric_limits< Unsigned >::max )() >
      [[nodiscard]] constexpr bool convert_unsigned( Unsigned& result, const std::string_view input ) noexcept
      {
         // Assumes result == 0 and that input is a non-empty sequence of digits; returns false on overflow.

         static_assert( std::is_unsigned_v< Unsigned > );
         return accumulate_digits< Unsigned, Maximum >( result, input );
      }

      template< typename Signed >
      [[nodiscard]] constexpr bool convert_signed( Signed& result, const std::string_view input ) noexcept
      {
         // Assumes result == 0 and that input is an optional sign followed by a non-empty sequence of digits; returns false on overflow.

         static_assert( std::is_signed_v< Signed > );
         if( input[ 0 ] == '-' ) {
            return convert_negative< Signed >( result, std::string_view( input.data() + 1, input.size() - 1 ) );
         }
         const auto offset = unsigned( input[ 0 ] == '+' );
         return convert_positive< Signed >( result, std::string_view( input.data() + offset, input.size() - offset ) );
      }

      template< typename Input >
      [[nodiscard]] bool match_unsigned( Input& in ) noexcept( noexcept( in.empty() ) )
      {
         if( !in.empty() ) {
            const char c = in.peek_char();
            if( is_digit( c ) ) {
               in.bump_in_this_line();
               if( c == '0' ) {
                  return in.empty() || ( !is_digit( in.peek_char() ) );  // TODO: Throw exception on digit?
               }
               while( ( !in.empty() ) && is_digit( in.peek_char() ) ) {
                  in.bump_in_this_line();
               }
               return true;
            }
         }
         return false;
      }

      template< typename Input,
                typename Unsigned,
                Unsigned Maximum = ( std::numeric_limits< Unsigned >::max )() >
      [[nodiscard]] bool match_and_convert_unsigned_with_maximum( Input& in, Unsigned& st )
      {
         // Assumes st == 0.

         if( !in.empty() ) {
            char c = in.peek_char();
            if( is_digit( c ) ) {
               if( c == '0' ) {
                  in.bump_in_this_line();
                  return in.empty() || ( !is_digit( in.peek_char() ) );  // TODO: Throw exception on digit?
               }
               do {
                  if( !accumulate_digit< Unsigned, Maximum >( st, c ) ) {
                     throw parse_error( "integer overflow", in );  // Consistent with "as if" an action was doing the conversion.
                  }
                  in.bump_in_this_line();
               } while( ( !in.empty() ) && is_digit( c = in.peek_char() ) );
               return true;
            }
         }
         return false;
      }

   }  // namespace internal

   struct unsigned_action
   {
      // Assumes that 'in' contains a non-empty sequence of ASCII digits.

      template< typename Input, typename Unsigned >
      static auto apply( const Input& in, Unsigned& st ) -> std::enable_if_t< std::is_unsigned_v< Unsigned >, void >
      {
         // This function "only" offers basic exception safety.
         st = 0;
         if( !internal::convert_unsigned( st, in.string_view() ) ) {
            throw parse_error( "unsigned integer overflow", in );
         }
      }

      template< typename Input, typename State >
      static auto apply( const Input& in, State& st ) -> std::enable_if_t< std::is_class_v< State >, void >
      {
         apply( in, st.converted );  // Compatibility for pre-3.0 behaviour.
      }

      template< typename Input, typename Unsigned, typename... Ts >
      static auto apply( const Input& in, std::vector< Unsigned, Ts... >& st ) -> std::enable_if_t< std::is_unsigned_v< Unsigned >, void >
      {
         Unsigned u = 0;
         apply( in, u );
         st.emplace_back( u );
      }
   };

   struct unsigned_rule
   {
      using analyze_t = internal::unsigned_rule_new::analyze_t;

      template< typename Input >
      [[nodiscard]] static bool match( Input& in ) noexcept( noexcept( in.empty() ) )
      {
         return internal::match_unsigned( in );  // Does not check for any overflow.
      }
   };

   struct unsigned_rule_with_action
   {
      using analyze_t = internal::unsigned_rule_new::analyze_t;

      template< apply_mode A,
                rewind_mode M,
                template< typename... >
                class Action,
                template< typename... >
                class Control,
                typename Input,
                typename... States >
      [[nodiscard]] static auto match( Input& in, States&&... /*unused*/ ) noexcept( noexcept( in.empty() ) ) -> std::enable_if_t< A == apply_mode::nothing, bool >
      {
         return internal::match_unsigned( in );  // Does not check for any overflow.
      }

      template< apply_mode A,
                rewind_mode M,
                template< typename... >
                class Action,
                template< typename... >
                class Control,
                typename Input,
                typename Unsigned >
      [[nodiscard]] static auto match( Input& in, Unsigned& st ) -> std::enable_if_t< ( A == apply_mode::action ) && std::is_unsigned_v< Unsigned >, bool >
      {
         // This function "only" offers basic exception safety.
         st = 0;
         return internal::match_and_convert_unsigned_with_maximum( in, st );  // Throws on overflow.
      }

      // TODO: Overload for st.converted?
      // TODO: Overload for std::vector< Unsigned >?
   };

   template< typename Unsigned, Unsigned Maximum >
   struct maximum_action
   {
      // Assumes that 'in' contains a non-empty sequence of ASCII digits.

      static_assert( std::is_unsigned_v< Unsigned > );

      template< typename Input, typename Unsigned2 >
      static auto apply( const Input& in, Unsigned2& st ) -> std::enable_if_t< std::is_same_v< Unsigned, Unsigned2 >, void >
      {
         // This function "only" offers basic exception safety.
         st = 0;
         if( !internal::convert_unsigned< Unsigned, Maximum >( st, in.string_view() ) ) {
            throw parse_error( "unsigned integer overflow", in );
         }
      }

      template< typename Input, typename State >
      static auto apply( const Input& in, State& st ) -> std::enable_if_t< std::is_class_v< State >, void >
      {
         apply( in, st.converted );  // Compatibility for pre-3.0 behaviour.
      }

      template< typename Input, typename Unsigned2, typename... Ts >
      static auto apply( const Input& in, std::vector< Unsigned2, Ts... >& st ) -> std::enable_if_t< std::is_same_v< Unsigned, Unsigned2 >, void >
      {
         Unsigned u = 0;
         apply( in, u );
         st.emplace_back( u );
      }
   };

   template< typename Unsigned, Unsigned Maximum = ( std::numeric_limits< Unsigned >::max )() >
   struct maximum_rule
   {
      static_assert( std::is_unsigned_v< Unsigned > );

      using analyze_t = internal::unsigned_rule_new::analyze_t;

      template< typename Input >
      [[nodiscard]] static bool match( Input& in )
      {
         Unsigned st = 0;
         return internal::match_and_convert_unsigned_with_maximum< Input, Unsigned, Maximum >( in, st );  // Throws on overflow.
      }
   };

   template< typename Unsigned, Unsigned Maximum = ( std::numeric_limits< Unsigned >::max )() >
   struct maximum_rule_with_action
   {
      static_assert( std::is_unsigned_v< Unsigned > );

      using analyze_t = internal::unsigned_rule_new::analyze_t;

      template< apply_mode A,
                rewind_mode M,
                template< typename... >
                class Action,
                template< typename... >
                class Control,
                typename Input,
                typename... States >
      [[nodiscard]] static auto match( Input& in, States&&... /*unused*/ ) -> std::enable_if_t< A == apply_mode::nothing, bool >
      {
         Unsigned st = 0;
         return internal::match_and_convert_unsigned_with_maximum< Input, Unsigned, Maximum >( in, st );  // Throws on overflow.
      }

      template< apply_mode A,
                rewind_mode M,
                template< typename... >
                class Action,
                template< typename... >
                class Control,
                typename Input,
                typename Unsigned2 >
      [[nodiscard]] static auto match( Input& in, Unsigned2& st ) -> std::enable_if_t< ( A == apply_mode::action ) && std::is_same_v< Unsigned, Unsigned2 >, bool >
      {
         // This function "only" offers basic exception safety.
         st = 0;
         return internal::match_and_convert_unsigned_with_maximum< Input, Unsigned, Maximum >( in, st );  // Throws on overflow.
      }

      // TODO: Overload for st.converted?
      // TODO: Overload for std::vector< Unsigned >?
   };

   struct signed_action
   {
      // Assumes that 'in' contains a non-empty sequence of ASCII digits,
      // with optional leading sign; with sign, in.size() must be >= 2.

      template< typename Input, typename Signed >
      static auto apply( const Input& in, Signed& st ) -> std::enable_if_t< std::is_signed_v< Signed >, void >
      {
         // This function "only" offers basic exception safety.
         st = 0;
         if( !internal::convert_signed( st, in.string_view() ) ) {
            throw parse_error( "signed integer overflow", in );
         }
      }

      template< typename Input, typename State >
      static auto apply( const Input& in, State& st ) -> std::enable_if_t< std::is_class_v< State >, void >
      {
         apply( in, st.converted );  // Compatibility for pre-3.0 behaviour.
      }

      template< typename Input, typename Signed, typename... Ts >
      static auto apply( const Input& in, std::vector< Signed, Ts... >& st ) -> std::enable_if_t< std::is_signed_v< Signed >, void >
      {
         Signed s = 0;
         apply( in, s );
         st.emplace_back( s );
      }
   };

   struct signed_rule
   {
      using analyze_t = internal::signed_rule_new::analyze_t;

      template< typename Input >
      [[nodiscard]] static bool match( Input& in ) noexcept( noexcept( in.empty() ) )
      {
         return TAO_JSON_PEGTL_NAMESPACE::parse< internal::signed_rule_new >( in );  // Does not check for any overflow.
      }
   };

   namespace internal
   {
      template< typename Rule >
      struct signed_action_action
         : nothing< Rule >
      {
      };

      template<>
      struct signed_action_action< signed_rule_new >
         : signed_action
      {
      };

   }  // namespace internal

   struct signed_rule_with_action
   {
      using analyze_t = internal::signed_rule_new::analyze_t;

      template< apply_mode A,
                rewind_mode M,
                template< typename... >
                class Action,
                template< typename... >
                class Control,
                typename Input,
                typename... States >
      [[nodiscard]] static auto match( Input& in, States&&... /*unused*/ ) noexcept( noexcept( in.empty() ) ) -> std::enable_if_t< A == apply_mode::nothing, bool >
      {
         return TAO_JSON_PEGTL_NAMESPACE::parse< internal::signed_rule_new >( in );  // Does not check for any overflow.
      }

      template< apply_mode A,
                rewind_mode M,
                template< typename... >
                class Action,
                template< typename... >
                class Control,
                typename Input,
                typename Signed >
      [[nodiscard]] static auto match( Input& in, Signed& st ) -> std::enable_if_t< ( A == apply_mode::action ) && std::is_signed_v< Signed >, bool >
      {
         return TAO_JSON_PEGTL_NAMESPACE::parse< internal::signed_rule_new, internal::signed_action_action >( in, st );  // Throws on overflow.
      }

      // TODO: Overload for st.converted?
      // TODO: Overload for std::vector< Signed >?
   };

}  // namespace TAO_JSON_PEGTL_NAMESPACE::integer

#endif
