// Copyright (c) 2014-2020 Dr. Colin Hirsch and Daniel Frey
// Please see LICENSE for license or visit https://github.com/taocpp/PEGTL/

#ifndef TAO_JSON_PEGTL_INTERNAL_PEEK_UTF8_HPP
#define TAO_JSON_PEGTL_INTERNAL_PEEK_UTF8_HPP

#include "../config.hpp"

#include "input_pair.hpp"

namespace TAO_JSON_PEGTL_NAMESPACE::internal
{
   struct peek_utf8
   {
      using data_t = char32_t;
      using pair_t = input_pair< char32_t >;

      static constexpr std::size_t min_input_size = 1;
      static constexpr std::size_t max_input_size = 4;

      template< typename Input >
      [[nodiscard]] static pair_t peek( const Input& in, const std::size_t s ) noexcept
      {
         char32_t c0 = in.peek_uint8();

         if( ( c0 & 0x80 ) == 0 ) {
            return { c0, 1 };
         }
         return peek_impl( in, c0, s );
      }

   private:
      template< typename Input >
      [[nodiscard]] static pair_t peek_impl( const Input& in, char32_t c0, const std::size_t s ) noexcept
      {
         if( ( c0 & 0xE0 ) == 0xC0 ) {
            if( s >= 2 ) {
               const char32_t c1 = in.peek_uint8( 1 );
               if( ( c1 & 0xC0 ) == 0x80 ) {
                  c0 &= 0x1F;
                  c0 <<= 6;
                  c0 |= ( c1 & 0x3F );
                  if( c0 >= 0x80 ) {
                     return { c0, 2 };
                  }
               }
            }
         }
         else if( ( c0 & 0xF0 ) == 0xE0 ) {
            if( s >= 3 ) {
               const char32_t c1 = in.peek_uint8( 1 );
               const char32_t c2 = in.peek_uint8( 2 );
               if( ( ( c1 & 0xC0 ) == 0x80 ) && ( ( c2 & 0xC0 ) == 0x80 ) ) {
                  c0 &= 0x0F;
                  c0 <<= 6;
                  c0 |= ( c1 & 0x3F );
                  c0 <<= 6;
                  c0 |= ( c2 & 0x3F );
                  if( c0 >= 0x800 && !( c0 >= 0xD800 && c0 <= 0xDFFF ) ) {
                     return { c0, 3 };
                  }
               }
            }
         }
         else if( ( c0 & 0xF8 ) == 0xF0 ) {
            if( s >= 4 ) {
               const char32_t c1 = in.peek_uint8( 1 );
               const char32_t c2 = in.peek_uint8( 2 );
               const char32_t c3 = in.peek_uint8( 3 );
               if( ( ( c1 & 0xC0 ) == 0x80 ) && ( ( c2 & 0xC0 ) == 0x80 ) && ( ( c3 & 0xC0 ) == 0x80 ) ) {
                  c0 &= 0x07;
                  c0 <<= 6;
                  c0 |= ( c1 & 0x3F );
                  c0 <<= 6;
                  c0 |= ( c2 & 0x3F );
                  c0 <<= 6;
                  c0 |= ( c3 & 0x3F );
                  if( c0 >= 0x10000 && c0 <= 0x10FFFF ) {
                     return { c0, 4 };
                  }
               }
            }
         }
         return { 0, 0 };
      }
   };

}  // namespace TAO_JSON_PEGTL_NAMESPACE::internal

#endif
