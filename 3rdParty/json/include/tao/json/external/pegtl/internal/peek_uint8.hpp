// Copyright (c) 2018-2020 Dr. Colin Hirsch and Daniel Frey
// Please see LICENSE for license or visit https://github.com/taocpp/PEGTL/

#ifndef TAO_JSON_PEGTL_INTERNAL_PEEK_UINT8_HPP
#define TAO_JSON_PEGTL_INTERNAL_PEEK_UINT8_HPP

#include <cstddef>
#include <cstdint>

#include "../config.hpp"

#include "input_pair.hpp"

namespace TAO_JSON_PEGTL_NAMESPACE::internal
{
   struct peek_uint8
   {
      using data_t = std::uint8_t;
      using pair_t = input_pair< std::uint8_t >;

      static constexpr std::size_t min_input_size = 1;
      static constexpr std::size_t max_input_size = 1;

      template< typename Input >
      [[nodiscard]] static pair_t peek( const Input& in, const std::size_t /*unused*/ = 1 ) noexcept
      {
         return { in.peek_uint8(), 1 };
      }
   };

}  // namespace TAO_JSON_PEGTL_NAMESPACE::internal

#endif
