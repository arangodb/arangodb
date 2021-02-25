// Copyright (c) 2015-2020 Dr. Colin Hirsch and Daniel Frey
// Please see LICENSE for license or visit https://github.com/taocpp/json/

#ifndef TAO_JSON_INTERNAL_GRAMMAR_HPP
#define TAO_JSON_INTERNAL_GRAMMAR_HPP

#include "../external/pegtl.hpp"
#include "../external/pegtl/contrib/abnf.hpp"

namespace tao::json::internal
{
   // clang-format off
   namespace rules
   {
      using namespace pegtl;

      struct ws : one< ' ', '\t', '\n', '\r' > {};

      template< typename R, typename P = ws >
      using padr = pegtl::internal::seq< R, pegtl::internal::star< P > >;

      struct begin_array : padr< one< '[' > > {};
      struct begin_object : padr< one< '{' > > {};
      struct end_array : one< ']' > {};
      struct end_object : one< '}' > {};
      struct name_separator : pad< one< ':' >, ws > {};
      struct value_separator : padr< one< ',' > > {};
      struct element_separator : padr< one< ',' > > {};

      struct false_ : pegtl::string< 'f', 'a', 'l', 's', 'e' > {};
      struct null : pegtl::string< 'n', 'u', 'l', 'l' > {};
      struct true_ : pegtl::string< 't', 'r', 'u', 'e' > {};

      struct digits : plus< abnf::DIGIT > {};

      struct esign : one< '-', '+' > {};

      struct edigits : digits {};
      struct fdigits : digits {};
      struct idigits : digits {};

      struct exp : seq< one< 'e', 'E' >, opt< esign >, must< edigits > > {};
      struct frac : if_must< one< '.' >, fdigits > {};

      template< bool NEG >
      struct number : seq< idigits, opt< frac >, opt< exp > > {};

      struct xdigit : abnf::HEXDIG {};
      struct escaped_unicode : list< seq< one< 'u' >, rep< 4, must< xdigit > > >, one< '\\' > > {};

      struct escaped_char : one< '"', '\\', '/', 'b', 'f', 'n', 'r', 't' > {};
      struct escaped : sor< escaped_char, escaped_unicode > {};

      struct unescaped
      {
         using analyze_t = pegtl::analysis::generic< pegtl::analysis::rule_type::any >;

         template< typename Input >
         [[nodiscard]] static bool match( Input& in )
         {
            bool result = false;

            while( const std::size_t s = in.size( 4 ) ) {
               if( const auto t = pegtl::internal::peek_utf8::peek( in, s ) ) {
                  if( ( 0x20 <= t.data ) && ( t.data != '\\' ) && ( t.data != '"' ) ) {
                     in.bump_in_this_line( t.size );
                     result = true;
                     continue;
                  }
               }
               break;
            }
            return result;
         }
      };

      struct chars : if_must_else< one< '\\' >, escaped, unescaped > {};

      struct string_content : until< at< one< '"' > >, chars > {};
      struct string : seq< one< '"' >, must< string_content >, any >
      {
         using content = string_content;
      };

      struct key_content : string_content {};
      struct key : seq< one< '"' >, must< key_content >, any >
      {
         using content = key_content;
      };

      struct value;

      struct array_element;
      struct array_content : opt< list_must< array_element, element_separator > > {};
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

      template< bool NEG >
      struct zero {};

      struct sor_value
      {
         using analyze_t = pegtl::analysis::generic< pegtl::analysis::rule_type::sor, string, number< false >, object, array, false_, true_, null >;

         template< bool NEG,
                   apply_mode A,
                   rewind_mode M,
                   template< typename... > class Action,
                   template< typename... > class Control,
                   typename Input,
                   typename... States >
         [[nodiscard]] static bool match_zero( Input& in, States&&... st )
         {
            if( in.size( 2 ) > 1 ) {
               switch( in.peek_char( 1 ) ) {
               case '.':
               case 'e':
               case 'E':
                  return Control< number< NEG > >::template match< A, M, Action, Control >( in, st... );

               case '0':
               case '1':
               case '2':
               case '3':
               case '4':
               case '5':
               case '6':
               case '7':
               case '8':
               case '9':
                  throw pegtl::parse_error( "invalid leading zero", in );
               }
            }
            in.bump_in_this_line();
            Control< zero< NEG > >::template apply0< Action >( in, st... );
            return true;
         }

         template< bool NEG,
                   apply_mode A,
                   rewind_mode M,
                   template< typename... > class Action,
                   template< typename... > class Control,
                   typename Input,
                   typename... States >
         [[nodiscard]] static bool match_number( Input& in, States&&... st )
         {
            if( in.peek_char() == '0' ) {
               if( !match_zero< NEG, A, rewind_mode::dontcare, Action, Control >( in, st... ) ) {
                  throw pegtl::parse_error( "incomplete number", in );
               }
               return true;
            }
            return Control< number< NEG > >::template match< A, M, Action, Control >( in, st... );
         }

         template< apply_mode A,
                   rewind_mode M,
                   template< typename... > class Action,
                   template< typename... > class Control,
                   typename Input,
                   typename... States >
         [[nodiscard]] static bool match_impl( Input& in, States&&... st )
         {
            switch( in.peek_char() ) {
            case '"': return Control< string >::template match< A, M, Action, Control >( in, st... );
            case '{': return Control< object >::template match< A, M, Action, Control >( in, st... );
            case '[': return Control< array >::template match< A, M, Action, Control >( in, st... );
            case 'n': return Control< null >::template match< A, M, Action, Control >( in, st... );
            case 't': return Control< true_ >::template match< A, M, Action, Control >( in, st... );
            case 'f': return Control< false_ >::template match< A, M, Action, Control >( in, st... );

            case '-':
               in.bump_in_this_line();
               if( in.empty() || !match_number< true, A, rewind_mode::dontcare, Action, Control >( in, st... ) ) {
                  throw pegtl::parse_error( "incomplete number", in );
               }
               return true;

            default:
               return match_number< false, A, M, Action, Control >( in, st... );
            }
         }

         template< apply_mode A,
                   rewind_mode M,
                   template< typename... > class Action,
                   template< typename... > class Control,
                   typename Input,
                   typename... States >
         [[nodiscard]] static bool match( Input& in, States&&... st )
         {
            if( in.size( 2 ) && match_impl< A, M, Action, Control >( in, st... ) ) {
               in.discard();
               return true;
            }
            return false;
         }
      };

      struct value : padr< sor_value > {};
      struct array_element : value {};

      struct text : seq< star< ws >, value > {};

   }  // namespace rules

   struct grammar : pegtl::must< rules::text, pegtl::eof > {};
   struct embedded : pegtl::must< rules::sor_value > {};
   // clang-format on

}  // namespace tao::json::internal

#endif
