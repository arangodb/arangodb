// Copyright (c) 2014-2020 Dr. Colin Hirsch and Daniel Frey
// Please see LICENSE for license or visit https://github.com/taocpp/PEGTL/

#ifndef TAO_JSON_PEGTL_CONTRIB_JSON_HPP
#define TAO_JSON_PEGTL_CONTRIB_JSON_HPP

#include "../ascii.hpp"
#include "../config.hpp"
#include "../rules.hpp"
#include "../utf8.hpp"

namespace TAO_JSON_PEGTL_NAMESPACE::json
{
   // JSON grammar according to RFC 8259

   // clang-format off
   struct ws : one< ' ', '\t', '\n', '\r' > {};

   template< typename R, typename P = ws >
   struct padr : internal::seq< R, internal::star< P > > {};

   struct begin_array : padr< one< '[' > > {};
   struct begin_object : padr< one< '{' > > {};
   struct end_array : one< ']' > {};
   struct end_object : one< '}' > {};
   struct name_separator : pad< one< ':' >, ws > {};
   struct value_separator : padr< one< ',' > > {};

   struct false_ : string< 'f', 'a', 'l', 's', 'e' > {};  // NOLINT(readability-identifier-naming)
   struct null : string< 'n', 'u', 'l', 'l' > {};
   struct true_ : string< 't', 'r', 'u', 'e' > {};  // NOLINT(readability-identifier-naming)

   struct digits : plus< digit > {};
   struct exp : seq< one< 'e', 'E' >, opt< one< '-', '+'> >, must< digits > > {};
   struct frac : if_must< one< '.' >, digits > {};
   struct int_ : sor< one< '0' >, digits > {};  // NOLINT(readability-identifier-naming)
   struct number : seq< opt< one< '-' > >, int_, opt< frac >, opt< exp > > {};

   struct xdigit : pegtl::xdigit {};
   struct unicode : list< seq< one< 'u' >, rep< 4, must< xdigit > > >, one< '\\' > > {};
   struct escaped_char : one< '"', '\\', '/', 'b', 'f', 'n', 'r', 't' > {};
   struct escaped : sor< escaped_char, unicode > {};
   struct unescaped : utf8::range< 0x20, 0x10FFFF > {};
   struct char_ : if_then_else< one< '\\' >, must< escaped >, unescaped > {};  // NOLINT(readability-identifier-naming)

   struct string_content : until< at< one< '"' > >, must< char_ > > {};
   struct string : seq< one< '"' >, must< string_content >, any >
   {
      using content = string_content;
   };

   struct key_content : until< at< one< '"' > >, must< char_ > > {};
   struct key : seq< one< '"' >, must< key_content >, any >
   {
      using content = key_content;
   };

   struct value;

   struct array_element;
   struct array_content : opt< list_must< array_element, value_separator > > {};
   struct array : seq< begin_array, array_content, must< end_array > >
   {
      using begin = begin_array;
      using end = end_array;
      using element = array_element;
      using content = array_content;
   };

   struct member : if_must< key, name_separator, value > {};
   struct object_content : opt< list_must< member, value_separator > > {};
   struct object : seq< begin_object, object_content, must< end_object > >
   {
      using begin = begin_object;
      using end = end_object;
      using element = member;
      using content = object_content;
   };

   struct value : padr< sor< string, number, object, array, false_, true_, null > > {};
   struct array_element : seq< value > {};

   struct text : seq< star< ws >, value > {};
   // clang-format on

}  // namespace TAO_JSON_PEGTL_NAMESPACE::json

#endif
