// Copyright (c) 2015-2020 Dr. Colin Hirsch and Daniel Frey
// Please see LICENSE for license or visit https://github.com/taocpp/PEGTL/

#ifndef TAO_JSON_PEGTL_INTERNAL_BUMP_HELP_HPP
#define TAO_JSON_PEGTL_INTERNAL_BUMP_HELP_HPP

#include <cstddef>
#include <type_traits>

#include "../config.hpp"

#include "result_on_found.hpp"

namespace TAO_JSON_PEGTL_NAMESPACE::internal
{
   template< result_on_found R, typename Input, typename Char, Char... Cs >
   void bump_help( Input& in, const std::size_t count ) noexcept
   {
      if constexpr( ( ( Cs != Input::eol_t::ch ) && ... ) != bool( R ) ) {
         in.bump( count );
      }
      else {
         in.bump_in_this_line( count );
      }
   }

}  // namespace TAO_JSON_PEGTL_NAMESPACE::internal

#endif
