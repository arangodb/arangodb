// Copyright (c) 2014-2020 Dr. Colin Hirsch and Daniel Frey
// Please see LICENSE for license or visit https://github.com/taocpp/PEGTL/

#ifndef TAO_JSON_PEGTL_INTERNAL_PEEK_CHAR_HPP
#define TAO_JSON_PEGTL_INTERNAL_PEEK_CHAR_HPP

#include <cstddef>

#include "../config.hpp"

#include "input_pair.hpp"

namespace TAO_JSON_PEGTL_NAMESPACE::internal
{
   struct peek_char
   {
      using data_t = char;
      using pair_t = input_pair< char >;

      static constexpr std::size_t min_input_size = 1;
      static constexpr std::size_t max_input_size = 1;

      template< typename Input >
      [[nodiscard]] static pair_t peek( const Input& in, const std::size_t /*unused*/ = 1 ) noexcept
      {
         return { in.peek_char(), 1 };
      }
   };

}  // namespace TAO_JSON_PEGTL_NAMESPACE::internal

#endif
