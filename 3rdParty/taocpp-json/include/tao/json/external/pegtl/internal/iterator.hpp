// Copyright (c) 2017-2020 Dr. Colin Hirsch and Daniel Frey
// Please see LICENSE for license or visit https://github.com/taocpp/PEGTL/

#ifndef TAO_JSON_PEGTL_INTERNAL_ITERATOR_HPP
#define TAO_JSON_PEGTL_INTERNAL_ITERATOR_HPP

#include <cstdlib>

#include "../config.hpp"

namespace TAO_JSON_PEGTL_NAMESPACE::internal
{
   struct iterator
   {
      iterator() = default;

      explicit iterator( const char* in_data ) noexcept
         : data( in_data )
      {
      }

      iterator( const char* in_data, const std::size_t in_byte, const std::size_t in_line, const std::size_t in_byte_in_line ) noexcept
         : data( in_data ),
           byte( in_byte ),
           line( in_line ),
           byte_in_line( in_byte_in_line )
      {
      }

      iterator( const iterator& ) = default;
      iterator( iterator&& ) = default;

      ~iterator() = default;

      iterator& operator=( const iterator& ) = default;
      iterator& operator=( iterator&& ) = default;

      void reset() noexcept
      {
         *this = iterator();
      }

      const char* data = nullptr;

      std::size_t byte = 0;
      std::size_t line = 1;
      std::size_t byte_in_line = 0;
   };

}  // namespace TAO_JSON_PEGTL_NAMESPACE::internal

#endif
