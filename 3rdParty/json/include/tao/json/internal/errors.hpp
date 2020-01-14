// Copyright (c) 2015-2020 Dr. Colin Hirsch and Daniel Frey
// Please see LICENSE for license or visit https://github.com/taocpp/json/

#ifndef TAO_JSON_INTERNAL_ERRORS_HPP
#define TAO_JSON_INTERNAL_ERRORS_HPP

#include <string>

#include "../external/pegtl/normal.hpp"
#include "../external/pegtl/parse_error.hpp"

#include "grammar.hpp"

namespace tao::json::internal
{
   template< typename Rule >
   struct errors
      : pegtl::normal< Rule >
   {
      static const std::string error_message;

      template< typename Input, typename... States >
      static void raise( const Input& in, States&&... /*unused*/ )
      {
         throw pegtl::parse_error( error_message, in );
      }

      template< template< typename... > class Action, typename Iterator, typename Input, typename... States >
      static auto apply( const Iterator& begin, const Input& in, States&&... st )
         -> decltype( Action< Rule >::apply( std::declval< const typename Input::action_t& >(), st... ) )
      {
         try {
            using action_t = typename Input::action_t;
            const action_t action_input( begin, in );
            return Action< Rule >::apply( action_input, st... );
         }
         catch( const pegtl::parse_error& ) {
            throw;
         }
         catch( const std::exception& e ) {
            throw pegtl::parse_error( e.what(), in );
         }
      }

      template< template< typename... > class Action, typename Input, typename... States >
      static auto apply0( const Input& in, States&&... st )
         -> decltype( Action< Rule >::apply0( st... ) )
      {
         try {
            return Action< Rule >::apply0( st... );
         }
         catch( const pegtl::parse_error& ) {
            throw;
         }
         catch( const std::exception& e ) {
            throw pegtl::parse_error( e.what(), in );
         }
      }
   };

   // clang-format off
   template<> inline const std::string errors< rules::text >::error_message = "no valid JSON";

   template<> inline const std::string errors< rules::end_array >::error_message = "incomplete array, expected ']'";
   template<> inline const std::string errors< rules::end_object >::error_message = "incomplete object, expected '}'";
   template<> inline const std::string errors< rules::member >::error_message = "expected member";
   template<> inline const std::string errors< rules::name_separator >::error_message = "expected ':'";
   template<> inline const std::string errors< rules::array_element >::error_message = "expected value";
   template<> inline const std::string errors< rules::value >::error_message = "expected value";
   template<> inline const std::string errors< rules::sor_value >::error_message = "expected value";

   template<> inline const std::string errors< rules::edigits >::error_message = "expected at least one exponent digit";
   template<> inline const std::string errors< rules::fdigits >::error_message = "expected at least one fraction digit";
   template<> inline const std::string errors< rules::xdigit >::error_message = "incomplete universal character name";
   template<> inline const std::string errors< rules::escaped >::error_message = "unknown escape sequence";
   template<> inline const std::string errors< rules::unescaped >::error_message = "invalid unescaped character";
   template<> inline const std::string errors< rules::string::content >::error_message = "unterminated string";
   template<> inline const std::string errors< rules::key::content >::error_message = "unterminated key";

   template<> inline const std::string errors< pegtl::eof >::error_message = "unexpected character after JSON value";
   // clang-format on

}  // namespace tao::json::internal

#endif
