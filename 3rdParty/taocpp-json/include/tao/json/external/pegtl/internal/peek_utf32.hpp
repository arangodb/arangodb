// Copyright (c) 2014-2020 Dr. Colin Hirsch and Daniel Frey
// Please see LICENSE for license or visit https://github.com/taocpp/PEGTL/

#ifndef TAO_JSON_PEGTL_INTERNAL_PEEK_UTF32_HPP
#define TAO_JSON_PEGTL_INTERNAL_PEEK_UTF32_HPP

#include <cstddef>

#include "../config.hpp"

#include "input_pair.hpp"
#include "read_uint.hpp"

namespace TAO_JSON_PEGTL_NAMESPACE::internal
{
   template< typename R >
   struct peek_utf32_impl
   {
      using data_t = char32_t;
      using pair_t = input_pair< char32_t >;

      static_assert( sizeof( char32_t ) == 4 );

      static constexpr std::size_t min_input_size = 4;
      static constexpr std::size_t max_input_size = 4;

      template< typename Input >
      [[nodiscard]] static pair_t peek( const Input& in, const std::size_t /*unused*/ ) noexcept
      {
         const char32_t t = R::read( in.current() );
         if( ( t <= 0x10ffff ) && !( t >= 0xd800 && t <= 0xdfff ) ) {
            return { t, 4 };
         }
         return { 0, 0 };
      }
   };

   using peek_utf32_be = peek_utf32_impl< read_uint32_be >;
   using peek_utf32_le = peek_utf32_impl< read_uint32_le >;

}  // namespace TAO_JSON_PEGTL_NAMESPACE::internal

#endif
