// Copyright (c) 2017-2020 Dr. Colin Hirsch and Daniel Frey
// Please see LICENSE for license or visit https://github.com/taocpp/json/

#ifndef TAO_JSON_BINARY_VIEW_HPP
#define TAO_JSON_BINARY_VIEW_HPP

#include "span.hpp"

#include <algorithm>

namespace tao
{
   using binary_view = span< const std::byte >;

   namespace internal
   {
      [[nodiscard]] inline bool binary_equal( const binary_view lhs, const binary_view rhs ) noexcept
      {
         return std::equal( lhs.begin(), lhs.end(), rhs.begin(), rhs.end() );
      }

      [[nodiscard]] inline bool binary_less( const binary_view lhs, const binary_view rhs ) noexcept
      {
         return std::lexicographical_compare( lhs.begin(), lhs.end(), rhs.begin(), rhs.end() );
      }

   }  // namespace internal

}  // namespace tao

#endif
