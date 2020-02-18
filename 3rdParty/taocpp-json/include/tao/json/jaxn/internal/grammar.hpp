// Copyright (c) 2017-2020 Dr. Colin Hirsch and Daniel Frey
// Please see LICENSE for license or visit https://github.com/taocpp/json/

#ifndef TAO_JSON_JAXN_INTERNAL_GRAMMAR_HPP
#define TAO_JSON_JAXN_INTERNAL_GRAMMAR_HPP

#include <cctype>

#include "../../external/pegtl.hpp"
#include "../../external/pegtl/contrib/abnf.hpp"

namespace tao::json::jaxn::internal
{
   // clang-format off
   namespace rules
   {
      using namespace pegtl;

      struct line_comment_char : sor< one< '\t' >, utf8::ranges< 0x20, 0x7E, 0x80, 0x10FFFF > > {};
      struct line_comment : seq< one< '/' >, until< eolf, must< line_comment_char > > > {};

      struct block_comment_char : sor< one< '\t' >, utf8::ranges< 0x20, 0x7E, 0x80, 0x10FFFF >, eol > {};
      struct end_block_comment : until< pegtl::string< '*', '/' >, must< block_comment_char > > {};
      struct block_comment : if_must< one< '*' >, end_block_comment > {};

      struct comment : sor< line_comment, block_comment > {};

      struct ws : sor< one< ' ', '\t', '\n', '\r' >, seq< one< '#' >, until< eolf > >, if_must< one< '/' >, comment > > {};

      template< typename R, typename P = ws >
      using padr = pegtl::internal::seq< R, pegtl::internal::star< P > >;

      struct begin_array : padr< one< '[' > > {};
      struct begin_object : padr< one< '{' > > {};
      struct end_array : one< ']' > {};
      struct end_object : one< '}' > {};
      struct name_separator : pad< one< ':' >, ws > {};
      struct value_separator : padr< one< ',' > > {};
      struct element_separator : padr< one< ',' > > {};
      struct value_concat : pad< one< '+' >, ws > {};

      struct false_ : pegtl::string< 'f', 'a', 'l', 's', 'e' > {};
      struct null : pegtl::string< 'n', 'u', 'l', 'l' > {};
      struct true_ : pegtl::string< 't', 'r', 'u', 'e' > {};

      struct nan : pegtl::string< 'N', 'a', 'N' > {};

      template< bool NEG >
      struct infinity : pegtl::string< 'I', 'n', 'f', 'i', 'n', 'i', 't', 'y' > {};

      template< bool NEG >
      struct hexnum : plus< abnf::HEXDIG > {};

      struct digits : plus< abnf::DIGIT > {};

      struct esign : one< '-', '+' > {};

      struct edigits : digits {};
      struct fdigits : digits {};
      struct idigits : digits {};

      struct exp : seq< one< 'e', 'E' >, opt< esign >, must< edigits > > {};
      struct frac0 : seq< one< '.' >, opt< fdigits > > {};
      struct frac1 : if_must< one< '.' >, fdigits > {};

      template< bool NEG >
      struct number : seq< sor< seq< idigits, opt< frac0 > >, frac1 >, opt< exp > > {};

      struct xdigit : abnf::HEXDIG {};
      struct escaped_unicode_code_point_content : seq< one< '{' >, plus< xdigit > > {};
      struct escaped_unicode_code_point : seq< one< 'u' >, escaped_unicode_code_point_content, one< '}' > > {};
      struct escaped_unicode : list< seq< one< 'u' >, rep< 4, xdigit > >, one< '\\' > > {};

      struct escaped_char : one< '"', '\'', '\\', '/', 'b', 'f', 'n', 'r', 't', 'v', '0' > {};
      struct escaped : sor< escaped_char, escaped_unicode_code_point, escaped_unicode > {};

      template< char D >
      struct unescaped
      {
         using analyze_t = pegtl::analysis::generic< pegtl::analysis::rule_type::any >;

         template< typename Input >
         [[nodiscard]] static bool match( Input& in )
         {
            bool result = false;

            while( const std::size_t s = in.size( 4 ) ) {
               if( const auto t = pegtl::internal::peek_utf8::peek( in, s ) ) {
                  if( ( 0x20 <= t.data ) && ( t.data <= 0x10FFFF ) && ( t.data != '\\' ) && ( t.data != D ) && ( t.data != 0x7F ) ) {
                     in.bump_in_this_line( t.size );
                     result = true;
                     continue;
                  }
               }
               return result;
            }
            throw pegtl::parse_error( "invalid character in string", in );
         }
      };

      template< char D >
      struct chars : if_then_else< one< '\\' >, must< escaped >, unescaped< D > > {};

      template< char D >
      struct qstring_content : until< at< one< D > >, must< chars< D > > > {};

      template< char D >
      struct qstring : seq< one< D >, must< qstring_content< D > >, any > {};

      template< char D >
      struct mchars_non_eol
      {
         using analyze_t = pegtl::analysis::generic< pegtl::analysis::rule_type::any >;

         template< typename Input >
         [[nodiscard]] static bool match( Input& in )
         {
            bool result = false;

            while( const std::size_t s = in.size( 4 ) ) {
               if( const auto t = pegtl::internal::peek_utf8::peek( in, s ) ) {
                  if( ( ( 0x20 <= t.data ) && ( t.data <= 0x10FFFF ) && ( t.data != D ) && ( t.data != 0x7F ) ) || ( t.data == '\t' ) ) {
                     in.bump_in_this_line( t.size );
                     result = true;
                     continue;
                  }
               }
               return result;
            }
            throw pegtl::parse_error( "invalid character in string", in );
         }
      };

      template< char D >
      struct mchars : sor< mchars_non_eol< D >, one< D >, eol > {};

      template< char D >
      struct mqstring_content : until< at< three< D > >, must< mchars< D > > > {};

      template< char D >
      struct mqstring : seq< three< D >, opt< eol >, must< mqstring_content< D > >, any, any, any > {};

      struct string_fragment : sor< mqstring< '"' >, mqstring< '\'' >, qstring< '"' >, qstring< '\'' > > {};

      struct string : list_must< string_fragment, value_concat > {};

      struct binary_prefix : one< '$' > {};

      struct bescaped_hexcode : seq< one< 'x' >, rep< 2, must< xdigit > > > {};

      struct bescaped_char : one< '"', '\'', '\\', '/', 'b', 'f', 'n', 'r', 't', 'v', '0' > {};
      struct bescaped : sor< bescaped_char, bescaped_hexcode > {};

      template< char D >
      struct bunescaped
      {
         using analyze_t = pegtl::analysis::generic< pegtl::analysis::rule_type::any >;

         template< typename Input >
         [[nodiscard]] static bool match( Input& in )
         {
            bool result = false;

            while( !in.empty() ) {
               const auto t = in.peek_char();
               if( ( 0x20 <= t ) && ( t <= 0x7E ) && ( t != '\\' ) && ( t != D ) ) {
                  in.bump_in_this_line( 1 );
                  result = true;
                  continue;
               }
               return result;
            }
            throw pegtl::parse_error( "invalid character in binary string", in );
         }
      };

      template< char D >
      struct bchars : if_then_else< one< '\\' >, must< bescaped >, bunescaped< D > > {};

      template< char D >
      struct bqstring_content : until< at< one< D > >, must< bchars< D > > > {};

      template< char D >
      struct bqstring : seq< one< D >, must< bqstring_content< D > >, any > {};

      struct bstring : sor< bqstring< '"' >, bqstring< '\'' > > {};

      struct bbyte : rep< 2, abnf::HEXDIG > {};

      struct bpart : plus< bbyte > {};

      struct bdirect : list_must< bpart, one< '.' > > {};

      struct bvalue : seq< binary_prefix, opt< sor< bstring, bdirect > > > {};

      struct binary : list_must< bvalue, value_concat > {};

      struct value;

      struct array_element;
      struct array_content : opt< list_tail< array_element, element_separator > > {};
      struct array : seq< begin_array, array_content, must< end_array > >
      {
         using begin = begin_array;
         using end = end_array;
         using element = array_element;
         using content = array_content;
      };

      struct key : string {};

      struct mkey : sor< key, identifier > {};
      struct member : if_must< mkey, name_separator, value > {};
      struct object_content : opt< list_tail< member, value_separator > > {};
      struct object : seq< begin_object, object_content, must< end_object > >
      {
         using begin = begin_object;
         using end = end_object;
         using element = member;
         using content = object_content;
      };

      template< bool NEG >
      struct zero : one< '0' > {};

      struct sor_value
      {
         using analyze_t = pegtl::analysis::generic< pegtl::analysis::rule_type::sor, string, number< false >, object, array, false_, true_, null >;

         template< typename Rule,
                   apply_mode A,
                   template< typename... > class Action,
                   template< typename... > class Control,
                   typename Input,
                   typename... States >
         [[nodiscard]] static bool match_must( Input& in, States&&... st )
         {
            return Control< must< Rule > >::template match< A, rewind_mode::dontcare, Action, Control >( in, st... );
         }

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

               case 'x':
               case 'X':
                  in.bump_in_this_line( 2 );
                  return Control< must< hexnum< NEG > > >::template match< A, M, Action, Control >( in, st... );

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
            return Control< zero< NEG > >::template match< A, M, Action, Control >( in, st... );
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
            switch( in.peek_char() ) {
            case 'N':
               return Control< must< nan > >::template match< A, M, Action, Control >( in, st... );

            case 'I':
               return Control< must< infinity< NEG > > >::template match< A, M, Action, Control >( in, st... );

            case '0':
               if( !match_zero< NEG, A, rewind_mode::dontcare, Action, Control >( in, st... ) ) {
                  throw pegtl::parse_error( "incomplete number", in );
               }
               return true;

            default:
               return Control< number< NEG > >::template match< A, M, Action, Control >( in, st... );
            }
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
            case '{': return Control< object >::template match< A, M, Action, Control >( in, st... );
            case '[': return Control< array >::template match< A, M, Action, Control >( in, st... );
            case 'n': return Control< null >::template match< A, M, Action, Control >( in, st... );
            case 't': return Control< true_ >::template match< A, M, Action, Control >( in, st... );
            case 'f': return Control< false_ >::template match< A, M, Action, Control >( in, st... );

            case '"':
            case '\'':
               return Control< string >::template match< A, M, Action, Control >( in, st... );

            case '$':
               return Control< binary >::template match< A, M, Action, Control >( in, st... );

            case '+':
               in.bump_in_this_line();
               if( in.empty() || !match_number< false, A, rewind_mode::dontcare, Action, Control >( in, st... ) ) {
                  throw pegtl::parse_error( "incomplete number", in );
               }
               return true;

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

}  // namespace tao::json::jaxn::internal

#endif
