// Copyright (c) 2018-2020 Dr. Colin Hirsch and Daniel Frey
// Please see LICENSE for license or visit https://github.com/taocpp/PEGTL/

#ifndef TAO_JSON_PEGTL_UINT32_HPP
#define TAO_JSON_PEGTL_UINT32_HPP

#include "config.hpp"

#include "internal/peek_mask_uint.hpp"
#include "internal/peek_uint.hpp"
#include "internal/result_on_found.hpp"
#include "internal/rules.hpp"

namespace TAO_JSON_PEGTL_NAMESPACE
{
   namespace uint32_be
   {
      // clang-format off
      struct any : internal::any< internal::peek_uint32_be > {};

      template< std::uint32_t... Cs > struct not_one : internal::one< internal::result_on_found::failure, internal::peek_uint32_be, Cs... > {};
      template< std::uint32_t Lo, std::uint32_t Hi > struct not_range : internal::range< internal::result_on_found::failure, internal::peek_uint32_be, Lo, Hi > {};
      template< std::uint32_t... Cs > struct one : internal::one< internal::result_on_found::success, internal::peek_uint32_be, Cs... > {};
      template< std::uint32_t Lo, std::uint32_t Hi > struct range : internal::range< internal::result_on_found::success, internal::peek_uint32_be, Lo, Hi > {};
      template< std::uint32_t... Cs > struct ranges : internal::ranges< internal::peek_uint32_be, Cs... > {};
      template< std::uint32_t... Cs > struct string : internal::seq< internal::one< internal::result_on_found::success, internal::peek_uint32_be, Cs >... > {};

      template< std::uint32_t M, std::uint32_t... Cs > struct mask_not_one : internal::one< internal::result_on_found::failure, internal::peek_mask_uint32_be< M >, Cs... > {};
      template< std::uint32_t M, std::uint32_t Lo, std::uint32_t Hi > struct mask_not_range : internal::range< internal::result_on_found::failure, internal::peek_mask_uint32_be< M >, Lo, Hi > {};
      template< std::uint32_t M, std::uint32_t... Cs > struct mask_one : internal::one< internal::result_on_found::success, internal::peek_mask_uint32_be< M >, Cs... > {};
      template< std::uint32_t M, std::uint32_t Lo, std::uint32_t Hi > struct mask_range : internal::range< internal::result_on_found::success, internal::peek_mask_uint32_be< M >, Lo, Hi > {};
      template< std::uint32_t M, std::uint32_t... Cs > struct mask_ranges : internal::ranges< internal::peek_mask_uint32_be< M >, Cs... > {};
      template< std::uint32_t M, std::uint32_t... Cs > struct mask_string : internal::seq< internal::one< internal::result_on_found::success, internal::peek_mask_uint32_be< M >, Cs >... > {};
      // clang-format on

   }  // namespace uint32_be

   namespace uint32_le
   {
      // clang-format off
      struct any : internal::any< internal::peek_uint32_le > {};

      template< std::uint32_t... Cs > struct not_one : internal::one< internal::result_on_found::failure, internal::peek_uint32_le, Cs... > {};
      template< std::uint32_t Lo, std::uint32_t Hi > struct not_range : internal::range< internal::result_on_found::failure, internal::peek_uint32_le, Lo, Hi > {};
      template< std::uint32_t... Cs > struct one : internal::one< internal::result_on_found::success, internal::peek_uint32_le, Cs... > {};
      template< std::uint32_t Lo, std::uint32_t Hi > struct range : internal::range< internal::result_on_found::success, internal::peek_uint32_le, Lo, Hi > {};
      template< std::uint32_t... Cs > struct ranges : internal::ranges< internal::peek_uint32_le, Cs... > {};
      template< std::uint32_t... Cs > struct string : internal::seq< internal::one< internal::result_on_found::success, internal::peek_uint32_le, Cs >... > {};

      template< std::uint32_t M, std::uint32_t... Cs > struct mask_not_one : internal::one< internal::result_on_found::failure, internal::peek_mask_uint32_le< M >, Cs... > {};
      template< std::uint32_t M, std::uint32_t Lo, std::uint32_t Hi > struct mask_not_range : internal::range< internal::result_on_found::failure, internal::peek_mask_uint32_le< M >, Lo, Hi > {};
      template< std::uint32_t M, std::uint32_t... Cs > struct mask_one : internal::one< internal::result_on_found::success, internal::peek_mask_uint32_le< M >, Cs... > {};
      template< std::uint32_t M, std::uint32_t Lo, std::uint32_t Hi > struct mask_range : internal::range< internal::result_on_found::success, internal::peek_mask_uint32_le< M >, Lo, Hi > {};
      template< std::uint32_t M, std::uint32_t... Cs > struct mask_ranges : internal::ranges< internal::peek_mask_uint32_le< M >, Cs... > {};
      template< std::uint32_t M, std::uint32_t... Cs > struct mask_string : internal::seq< internal::one< internal::result_on_found::success, internal::peek_mask_uint32_le< M >, Cs >... > {};
      // clang-format on

   }  // namespace uint32_le

}  // namespace TAO_JSON_PEGTL_NAMESPACE

#endif
