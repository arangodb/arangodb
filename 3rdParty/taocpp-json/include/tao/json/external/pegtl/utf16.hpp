// Copyright (c) 2015-2020 Dr. Colin Hirsch and Daniel Frey
// Please see LICENSE for license or visit https://github.com/taocpp/PEGTL/

#ifndef TAO_JSON_PEGTL_UTF16_HPP
#define TAO_JSON_PEGTL_UTF16_HPP

#include "config.hpp"

#include "internal/peek_utf16.hpp"
#include "internal/result_on_found.hpp"
#include "internal/rules.hpp"

namespace TAO_JSON_PEGTL_NAMESPACE
{
   namespace utf16_be
   {
      // clang-format off
      struct any : internal::any< internal::peek_utf16_be > {};
      struct bom : internal::one< internal::result_on_found::success, internal::peek_utf16_be, 0xfeff > {};
      template< char32_t... Cs > struct not_one : internal::one< internal::result_on_found::failure, internal::peek_utf16_be, Cs... > {};
      template< char32_t Lo, char32_t Hi > struct not_range : internal::range< internal::result_on_found::failure, internal::peek_utf16_be, Lo, Hi > {};
      template< char32_t... Cs > struct one : internal::one< internal::result_on_found::success, internal::peek_utf16_be, Cs... > {};
      template< char32_t Lo, char32_t Hi > struct range : internal::range< internal::result_on_found::success, internal::peek_utf16_be, Lo, Hi > {};
      template< char32_t... Cs > struct ranges : internal::ranges< internal::peek_utf16_be, Cs... > {};
      template< char32_t... Cs > struct string : internal::seq< internal::one< internal::result_on_found::success, internal::peek_utf16_be, Cs >... > {};
      // clang-format on

   }  // namespace utf16_be

   namespace utf16_le
   {
      // clang-format off
      struct any : internal::any< internal::peek_utf16_le > {};
      struct bom : internal::one< internal::result_on_found::success, internal::peek_utf16_le, 0xfeff > {};
      template< char32_t... Cs > struct not_one : internal::one< internal::result_on_found::failure, internal::peek_utf16_le, Cs... > {};
      template< char32_t Lo, char32_t Hi > struct not_range : internal::range< internal::result_on_found::failure, internal::peek_utf16_le, Lo, Hi > {};
      template< char32_t... Cs > struct one : internal::one< internal::result_on_found::success, internal::peek_utf16_le, Cs... > {};
      template< char32_t Lo, char32_t Hi > struct range : internal::range< internal::result_on_found::success, internal::peek_utf16_le, Lo, Hi > {};
      template< char32_t... Cs > struct ranges : internal::ranges< internal::peek_utf16_le, Cs... > {};
      template< char32_t... Cs > struct string : internal::seq< internal::one< internal::result_on_found::success, internal::peek_utf16_le, Cs >... > {};
      // clang-format on

   }  // namespace utf16_le

   namespace utf16 = TAO_JSON_PEGTL_NATIVE_UTF16;

}  // namespace TAO_JSON_PEGTL_NAMESPACE

#endif
