// Copyright (c) 2014-2020 Dr. Colin Hirsch and Daniel Frey
// Please see LICENSE for license or visit https://github.com/taocpp/PEGTL/

#ifndef TAO_JSON_PEGTL_INTERNAL_PEEK_UTF16_HPP
#define TAO_JSON_PEGTL_INTERNAL_PEEK_UTF16_HPP

#include <type_traits>

#include "../config.hpp"

#include "input_pair.hpp"
#include "read_uint.hpp"

namespace TAO_JSON_PEGTL_NAMESPACE::internal
{
   template< typename R >
   struct peek_utf16_impl
   {
      using data_t = char32_t;
      using pair_t = input_pair< char32_t >;

      using short_t = std::make_unsigned< char16_t >::type;

      static_assert( sizeof( short_t ) == 2 );
      static_assert( sizeof( char16_t ) == 2 );

      static constexpr std::size_t min_input_size = 2;
      static constexpr std::size_t max_input_size = 4;

      template< typename Input >
      [[nodiscard]] static pair_t peek( const Input& in, const std::size_t s ) noexcept
      {
         const char32_t t = R::read( in.current() );
         if( ( t < 0xd800 ) || ( t > 0xdfff ) ) {
            return { t, 2 };
         }
         if( ( t >= 0xdc00 ) || ( s < 4 ) ) {
            return { 0, 0 };
         }
         const char32_t u = R::read( in.current() + 2 );
         if( ( u >= 0xdc00 ) && ( u <= 0xdfff ) ) {
            const auto cp = ( ( ( t & 0x03ff ) << 10 ) | ( u & 0x03ff ) ) + 0x10000;
            return { cp, 4 };
         }
         return { 0, 0 };
      }
   };

   using peek_utf16_be = peek_utf16_impl< read_uint16_be >;
   using peek_utf16_le = peek_utf16_impl< read_uint16_le >;

}  // namespace TAO_JSON_PEGTL_NAMESPACE::internal

#endif
