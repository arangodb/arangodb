// Copyright (c) 2019-2020 Dr. Colin Hirsch and Daniel Frey
// Please see LICENSE for license or visit https://github.com/taocpp/json/

#ifndef TAO_JSON_SRC_TEST_JSON_TEST_TYPES_HPP
#define TAO_JSON_SRC_TEST_JSON_TEST_TYPES_HPP

#include <optional>

#include <tao/json/value.hpp>

namespace tao::json
{
   template< template< typename... > class Traits >
   void assert_uninitialized( const basic_value< Traits >& v )
   {
      TEST_ASSERT( !bool( v ) );
      TEST_ASSERT( v.type() == type::UNINITIALIZED );

      TEST_ASSERT( v.is_uninitialized() );
      TEST_ASSERT( !v.is_null() );
      TEST_ASSERT( !v.is_boolean() );
      TEST_ASSERT( !v.is_signed() );
      TEST_ASSERT( !v.is_unsigned() );
      TEST_ASSERT( !v.is_integer() );
      TEST_ASSERT( !v.is_double() );
      TEST_ASSERT( !v.is_number() );
      TEST_ASSERT( !v.is_string() );
      TEST_ASSERT( !v.is_string_view() );
      TEST_ASSERT( !v.is_string_type() );
      TEST_ASSERT( !v.is_binary() );
      TEST_ASSERT( !v.is_binary_view() );
      TEST_ASSERT( !v.is_binary_type() );
      TEST_ASSERT( !v.is_array() );
      TEST_ASSERT( !v.is_object() );
      TEST_ASSERT( !v.is_value_ptr() );
      TEST_ASSERT( !v.is_opaque_ptr() );

      TEST_THROWS( v.get_boolean() );
      TEST_THROWS( v.get_signed() );
      TEST_THROWS( v.get_unsigned() );
      TEST_THROWS( v.get_double() );
      TEST_THROWS( v.get_string() );
      TEST_THROWS( v.get_string_view() );
      TEST_THROWS( v.get_string_type() );
      TEST_THROWS( v.get_binary() );
      TEST_THROWS( v.get_binary_view() );
      TEST_THROWS( v.get_binary_type() );
      TEST_THROWS( v.get_array() );
      TEST_THROWS( v.get_object() );
      TEST_THROWS( v.get_value_ptr() );
      TEST_THROWS( v.get_opaque_ptr() );
   }

   template< template< typename... > class Traits >
   void assert_null( const basic_value< Traits >& v )
   {
      TEST_ASSERT( bool( v ) );
      TEST_ASSERT( v.type() == type::NULL_ );

      TEST_ASSERT( !v.is_uninitialized() );
      TEST_ASSERT( v.is_null() );
      TEST_ASSERT( !v.is_boolean() );
      TEST_ASSERT( !v.is_signed() );
      TEST_ASSERT( !v.is_unsigned() );
      TEST_ASSERT( !v.is_integer() );
      TEST_ASSERT( !v.is_double() );
      TEST_ASSERT( !v.is_number() );
      TEST_ASSERT( !v.is_string() );
      TEST_ASSERT( !v.is_string_view() );
      TEST_ASSERT( !v.is_string_type() );
      TEST_ASSERT( !v.is_binary() );
      TEST_ASSERT( !v.is_binary_view() );
      TEST_ASSERT( !v.is_binary_type() );
      TEST_ASSERT( !v.is_array() );
      TEST_ASSERT( !v.is_object() );
      TEST_ASSERT( !v.is_value_ptr() );
      TEST_ASSERT( !v.is_opaque_ptr() );

      TEST_THROWS( v.get_boolean() );
      TEST_THROWS( v.get_signed() );
      TEST_THROWS( v.get_unsigned() );
      TEST_THROWS( v.get_double() );
      TEST_THROWS( v.get_string() );
      TEST_THROWS( v.get_string_view() );
      TEST_THROWS( v.get_string_type() );
      TEST_THROWS( v.get_binary() );
      TEST_THROWS( v.get_binary_view() );
      TEST_THROWS( v.get_binary_type() );
      TEST_THROWS( v.get_array() );
      TEST_THROWS( v.get_object() );
      TEST_THROWS( v.get_value_ptr() );
      TEST_THROWS( v.get_opaque_ptr() );
   }

   template< template< typename... > class Traits >
   void assert_boolean( const basic_value< Traits >& v, const std::optional< bool >& t )
   {
      TEST_ASSERT( bool( v ) );
      TEST_ASSERT( v.type() == type::BOOLEAN );

      TEST_ASSERT( !v.is_uninitialized() );
      TEST_ASSERT( !v.is_null() );
      TEST_ASSERT( v.is_boolean() );
      TEST_ASSERT( !v.is_signed() );
      TEST_ASSERT( !v.is_unsigned() );
      TEST_ASSERT( !v.is_integer() );
      TEST_ASSERT( !v.is_double() );
      TEST_ASSERT( !v.is_number() );
      TEST_ASSERT( !v.is_string() );
      TEST_ASSERT( !v.is_string_view() );
      TEST_ASSERT( !v.is_string_type() );
      TEST_ASSERT( !v.is_binary() );
      TEST_ASSERT( !v.is_binary_view() );
      TEST_ASSERT( !v.is_binary_type() );
      TEST_ASSERT( !v.is_array() );
      TEST_ASSERT( !v.is_object() );
      TEST_ASSERT( !v.is_value_ptr() );
      TEST_ASSERT( !v.is_opaque_ptr() );

      (void)v.get_boolean();
      TEST_THROWS( v.get_signed() );
      TEST_THROWS( v.get_unsigned() );
      TEST_THROWS( v.get_double() );
      TEST_THROWS( v.get_string() );
      TEST_THROWS( v.get_string_view() );
      TEST_THROWS( v.get_string_type() );
      TEST_THROWS( v.get_binary() );
      TEST_THROWS( v.get_binary_view() );
      TEST_THROWS( v.get_binary_type() );
      TEST_THROWS( v.get_array() );
      TEST_THROWS( v.get_object() );
      TEST_THROWS( v.get_value_ptr() );
      TEST_THROWS( v.get_opaque_ptr() );

      if( t ) {
         TEST_ASSERT( v.get_boolean() == t );
         TEST_ASSERT( v.template as< bool >() == t );
      }
   }

   template< template< typename... > class Traits >
   void assert_signed( const basic_value< Traits >& v, const std::optional< std::int64_t >& t )
   {
      TEST_ASSERT( bool( v ) );
      TEST_ASSERT( v.type() == type::SIGNED );

      TEST_ASSERT( !v.is_uninitialized() );
      TEST_ASSERT( !v.is_null() );
      TEST_ASSERT( !v.is_boolean() );
      TEST_ASSERT( v.is_signed() );
      TEST_ASSERT( v.is_integer() );
      TEST_ASSERT( !v.is_unsigned() );
      TEST_ASSERT( !v.is_double() );
      TEST_ASSERT( v.is_number() );
      TEST_ASSERT( !v.is_string() );
      TEST_ASSERT( !v.is_string_view() );
      TEST_ASSERT( !v.is_string_type() );
      TEST_ASSERT( !v.is_binary() );
      TEST_ASSERT( !v.is_binary_view() );
      TEST_ASSERT( !v.is_binary_type() );
      TEST_ASSERT( !v.is_array() );
      TEST_ASSERT( !v.is_object() );
      TEST_ASSERT( !v.is_value_ptr() );
      TEST_ASSERT( !v.is_opaque_ptr() );

      TEST_THROWS( v.get_boolean() );
      (void)v.get_signed();
      TEST_THROWS( v.get_unsigned() );
      TEST_THROWS( v.get_double() );
      TEST_THROWS( v.get_string() );
      TEST_THROWS( v.get_string_view() );
      TEST_THROWS( v.get_string_type() );
      TEST_THROWS( v.get_binary() );
      TEST_THROWS( v.get_binary_view() );
      TEST_THROWS( v.get_binary_type() );
      TEST_THROWS( v.get_array() );
      TEST_THROWS( v.get_object() );
      TEST_THROWS( v.get_value_ptr() );
      TEST_THROWS( v.get_opaque_ptr() );

      if( t ) {
         TEST_ASSERT( v.get_signed() == t );
         TEST_ASSERT( v.template as< std::int64_t >() == t );
      }
   }

   template< template< typename... > class Traits >
   void assert_unsigned( const basic_value< Traits >& v, const std::optional< std::uint64_t >& t )
   {
      TEST_ASSERT( bool( v ) );
      TEST_ASSERT( v.type() == type::UNSIGNED );

      TEST_ASSERT( !v.is_uninitialized() );
      TEST_ASSERT( !v.is_null() );
      TEST_ASSERT( !v.is_boolean() );
      TEST_ASSERT( !v.is_signed() );
      TEST_ASSERT( v.is_unsigned() );
      TEST_ASSERT( v.is_integer() );
      TEST_ASSERT( !v.is_double() );
      TEST_ASSERT( v.is_number() );
      TEST_ASSERT( !v.is_string() );
      TEST_ASSERT( !v.is_string_view() );
      TEST_ASSERT( !v.is_string_type() );
      TEST_ASSERT( !v.is_binary() );
      TEST_ASSERT( !v.is_binary_view() );
      TEST_ASSERT( !v.is_binary_type() );
      TEST_ASSERT( !v.is_array() );
      TEST_ASSERT( !v.is_object() );
      TEST_ASSERT( !v.is_value_ptr() );
      TEST_ASSERT( !v.is_opaque_ptr() );

      TEST_THROWS( v.get_boolean() );
      TEST_THROWS( v.get_signed() );
      (void)v.get_unsigned();
      TEST_THROWS( v.get_double() );
      TEST_THROWS( v.get_string() );
      TEST_THROWS( v.get_string_view() );
      TEST_THROWS( v.get_string_type() );
      TEST_THROWS( v.get_binary() );
      TEST_THROWS( v.get_binary_view() );
      TEST_THROWS( v.get_binary_type() );
      TEST_THROWS( v.get_array() );
      TEST_THROWS( v.get_object() );
      TEST_THROWS( v.get_value_ptr() );
      TEST_THROWS( v.get_opaque_ptr() );

      if( t ) {
         TEST_ASSERT( v.get_unsigned() == t );
         TEST_ASSERT( v.template as< std::uint64_t >() == t );
      }
   }

   template< template< typename... > class Traits >
   void assert_double( const basic_value< Traits >& v, const std::optional< double >& t )
   {
      TEST_ASSERT( bool( v ) );
      TEST_ASSERT( v.type() == type::DOUBLE );

      TEST_ASSERT( !v.is_uninitialized() );
      TEST_ASSERT( !v.is_null() );
      TEST_ASSERT( !v.is_boolean() );
      TEST_ASSERT( !v.is_signed() );
      TEST_ASSERT( !v.is_unsigned() );
      TEST_ASSERT( !v.is_integer() );
      TEST_ASSERT( v.is_double() );
      TEST_ASSERT( v.is_number() );
      TEST_ASSERT( !v.is_string() );
      TEST_ASSERT( !v.is_string_view() );
      TEST_ASSERT( !v.is_string_type() );
      TEST_ASSERT( !v.is_binary() );
      TEST_ASSERT( !v.is_binary_view() );
      TEST_ASSERT( !v.is_binary_type() );
      TEST_ASSERT( !v.is_array() );
      TEST_ASSERT( !v.is_object() );
      TEST_ASSERT( !v.is_value_ptr() );
      TEST_ASSERT( !v.is_opaque_ptr() );

      TEST_THROWS( v.get_boolean() );
      TEST_THROWS( v.get_signed() );
      TEST_THROWS( v.get_unsigned() );
      (void)v.get_double();
      TEST_THROWS( v.get_string() );
      TEST_THROWS( v.get_string_view() );
      TEST_THROWS( v.get_string_type() );
      TEST_THROWS( v.get_binary() );
      TEST_THROWS( v.get_binary_view() );
      TEST_THROWS( v.get_binary_type() );
      TEST_THROWS( v.get_array() );
      TEST_THROWS( v.get_object() );
      TEST_THROWS( v.get_value_ptr() );
      TEST_THROWS( v.get_opaque_ptr() );

      if( t ) {
         TEST_ASSERT( v.get_double() == t );
         TEST_ASSERT( v.template as< double >() == t );
      }
   }

   template< template< typename... > class Traits >
   void assert_string( const basic_value< Traits >& v, const std::optional< std::string >& t )
   {
      TEST_ASSERT( bool( v ) );
      TEST_ASSERT( v.type() == type::STRING );

      TEST_ASSERT( !v.is_uninitialized() );
      TEST_ASSERT( !v.is_null() );
      TEST_ASSERT( !v.is_boolean() );
      TEST_ASSERT( !v.is_signed() );
      TEST_ASSERT( !v.is_unsigned() );
      TEST_ASSERT( !v.is_integer() );
      TEST_ASSERT( !v.is_double() );
      TEST_ASSERT( !v.is_number() );
      TEST_ASSERT( v.is_string() );
      TEST_ASSERT( !v.is_string_view() );
      TEST_ASSERT( v.is_string_type() );
      TEST_ASSERT( !v.is_binary() );
      TEST_ASSERT( !v.is_binary_view() );
      TEST_ASSERT( !v.is_binary_type() );
      TEST_ASSERT( !v.is_array() );
      TEST_ASSERT( !v.is_object() );
      TEST_ASSERT( !v.is_value_ptr() );
      TEST_ASSERT( !v.is_opaque_ptr() );

      TEST_THROWS( v.get_boolean() );
      TEST_THROWS( v.get_signed() );
      TEST_THROWS( v.get_unsigned() );
      TEST_THROWS( v.get_double() );
      (void)v.get_string();
      TEST_THROWS( v.get_string_view() );
      (void)v.get_string_type();
      TEST_THROWS( v.get_binary() );
      TEST_THROWS( v.get_binary_view() );
      TEST_THROWS( v.get_binary_type() );
      TEST_THROWS( v.get_array() );
      TEST_THROWS( v.get_object() );
      TEST_THROWS( v.get_value_ptr() );
      TEST_THROWS( v.get_opaque_ptr() );

      if( t ) {
         TEST_ASSERT( v.get_string() == t );
         TEST_ASSERT( v.get_string_type() == t );
         TEST_ASSERT( v.template as< std::string >() == t );
         TEST_ASSERT( v.template as< std::string_view >() == t );
      }
   }

   template< template< typename... > class Traits >
   void assert_string_view( const basic_value< Traits >& v, const std::optional< std::string >& t )
   {
      TEST_ASSERT( bool( v ) );
      TEST_ASSERT( v.type() == type::STRING_VIEW );

      TEST_ASSERT( !v.is_uninitialized() );
      TEST_ASSERT( !v.is_null() );
      TEST_ASSERT( !v.is_boolean() );
      TEST_ASSERT( !v.is_signed() );
      TEST_ASSERT( !v.is_unsigned() );
      TEST_ASSERT( !v.is_integer() );
      TEST_ASSERT( !v.is_double() );
      TEST_ASSERT( !v.is_number() );
      TEST_ASSERT( !v.is_string() );
      TEST_ASSERT( v.is_string_view() );
      TEST_ASSERT( v.is_string_type() );
      TEST_ASSERT( !v.is_binary() );
      TEST_ASSERT( !v.is_binary_view() );
      TEST_ASSERT( !v.is_binary_type() );
      TEST_ASSERT( !v.is_array() );
      TEST_ASSERT( !v.is_object() );
      TEST_ASSERT( !v.is_value_ptr() );
      TEST_ASSERT( !v.is_opaque_ptr() );

      TEST_THROWS( v.get_boolean() );
      TEST_THROWS( v.get_signed() );
      TEST_THROWS( v.get_unsigned() );
      TEST_THROWS( v.get_double() );
      TEST_THROWS( v.get_string() );
      (void)v.get_string_view();
      (void)v.get_string_type();
      TEST_THROWS( v.get_binary() );
      TEST_THROWS( v.get_binary_view() );
      TEST_THROWS( v.get_binary_type() );
      TEST_THROWS( v.get_array() );
      TEST_THROWS( v.get_object() );
      TEST_THROWS( v.get_value_ptr() );
      TEST_THROWS( v.get_opaque_ptr() );

      if( t ) {
         TEST_ASSERT( v.get_string_view() == t );
         TEST_ASSERT( v.get_string_type() == t );
         TEST_ASSERT( v.template as< std::string >() == t );
         TEST_ASSERT( v.template as< std::string_view >() == t );
      }
   }

   template< template< typename... > class Traits >
   void assert_binary( const basic_value< Traits >& v, const std::optional< binary >& t )
   {
      TEST_ASSERT( bool( v ) );
      TEST_ASSERT( v.type() == type::BINARY );

      TEST_ASSERT( !v.is_uninitialized() );
      TEST_ASSERT( !v.is_null() );
      TEST_ASSERT( !v.is_boolean() );
      TEST_ASSERT( !v.is_signed() );
      TEST_ASSERT( !v.is_unsigned() );
      TEST_ASSERT( !v.is_integer() );
      TEST_ASSERT( !v.is_double() );
      TEST_ASSERT( !v.is_number() );
      TEST_ASSERT( !v.is_string() );
      TEST_ASSERT( !v.is_string_view() );
      TEST_ASSERT( !v.is_string_type() );
      TEST_ASSERT( v.is_binary() );
      TEST_ASSERT( !v.is_binary_view() );
      TEST_ASSERT( v.is_binary_type() );
      TEST_ASSERT( !v.is_array() );
      TEST_ASSERT( !v.is_object() );
      TEST_ASSERT( !v.is_value_ptr() );
      TEST_ASSERT( !v.is_opaque_ptr() );

      TEST_THROWS( v.get_boolean() );
      TEST_THROWS( v.get_signed() );
      TEST_THROWS( v.get_unsigned() );
      TEST_THROWS( v.get_double() );
      TEST_THROWS( v.get_string() );
      TEST_THROWS( v.get_string_view() );
      TEST_THROWS( v.get_string_type() );
      (void)v.get_binary();
      TEST_THROWS( v.get_binary_view() );
      (void)v.get_binary_type();
      TEST_THROWS( v.get_array() );
      TEST_THROWS( v.get_object() );
      TEST_THROWS( v.get_value_ptr() );
      TEST_THROWS( v.get_opaque_ptr() );

      if( t ) {
         // TODO!
      }
   }

   template< template< typename... > class Traits >
   void assert_binary_view( const basic_value< Traits >& v, const std::optional< binary >& t )
   {
      TEST_ASSERT( bool( v ) );
      TEST_ASSERT( v.type() == type::BINARY_VIEW );

      TEST_ASSERT( !v.is_uninitialized() );
      TEST_ASSERT( !v.is_null() );
      TEST_ASSERT( !v.is_boolean() );
      TEST_ASSERT( !v.is_signed() );
      TEST_ASSERT( !v.is_unsigned() );
      TEST_ASSERT( !v.is_integer() );
      TEST_ASSERT( !v.is_double() );
      TEST_ASSERT( !v.is_number() );
      TEST_ASSERT( !v.is_string() );
      TEST_ASSERT( !v.is_string_view() );
      TEST_ASSERT( !v.is_string_type() );
      TEST_ASSERT( !v.is_binary() );
      TEST_ASSERT( v.is_binary_view() );
      TEST_ASSERT( v.is_binary_type() );
      TEST_ASSERT( !v.is_array() );
      TEST_ASSERT( !v.is_object() );
      TEST_ASSERT( !v.is_value_ptr() );
      TEST_ASSERT( !v.is_opaque_ptr() );

      TEST_THROWS( v.get_boolean() );
      TEST_THROWS( v.get_signed() );
      TEST_THROWS( v.get_unsigned() );
      TEST_THROWS( v.get_double() );
      TEST_THROWS( v.get_string() );
      TEST_THROWS( v.get_string_view() );
      TEST_THROWS( v.get_string_type() );
      TEST_THROWS( v.get_binary() );
      (void)v.get_binary_view();
      (void)v.get_binary_type();
      TEST_THROWS( v.get_array() );
      TEST_THROWS( v.get_object() );
      TEST_THROWS( v.get_value_ptr() );
      TEST_THROWS( v.get_opaque_ptr() );

      if( t ) {
         // TODO!
      }
   }

   template< template< typename... > class Traits >
   void assert_array( const basic_value< Traits >& v, const std::optional< std::size_t >& t )
   {
      TEST_ASSERT( bool( v ) );
      TEST_ASSERT( v.type() == type::ARRAY );

      TEST_ASSERT( !v.is_uninitialized() );
      TEST_ASSERT( !v.is_null() );
      TEST_ASSERT( !v.is_boolean() );
      TEST_ASSERT( !v.is_signed() );
      TEST_ASSERT( !v.is_unsigned() );
      TEST_ASSERT( !v.is_integer() );
      TEST_ASSERT( !v.is_double() );
      TEST_ASSERT( !v.is_number() );
      TEST_ASSERT( !v.is_string() );
      TEST_ASSERT( !v.is_string_view() );
      TEST_ASSERT( !v.is_string_type() );
      TEST_ASSERT( !v.is_binary() );
      TEST_ASSERT( !v.is_binary_view() );
      TEST_ASSERT( !v.is_binary_type() );
      TEST_ASSERT( v.is_array() );
      TEST_ASSERT( !v.is_object() );
      TEST_ASSERT( !v.is_value_ptr() );
      TEST_ASSERT( !v.is_opaque_ptr() );

      TEST_THROWS( v.get_boolean() );
      TEST_THROWS( v.get_signed() );
      TEST_THROWS( v.get_unsigned() );
      TEST_THROWS( v.get_double() );
      TEST_THROWS( v.get_string() );
      TEST_THROWS( v.get_string_view() );
      TEST_THROWS( v.get_string_type() );
      TEST_THROWS( v.get_binary() );
      TEST_THROWS( v.get_binary_view() );
      TEST_THROWS( v.get_binary_type() );
      (void)v.get_array();
      TEST_THROWS( v.get_object() );
      TEST_THROWS( v.get_value_ptr() );
      TEST_THROWS( v.get_opaque_ptr() );

      if( t ) {
         TEST_ASSERT( v.get_array().size() == t );
      }
   }

   template< template< typename... > class Traits >
   void assert_object( const basic_value< Traits >& v, const std::optional< std::size_t >& t )
   {
      TEST_ASSERT( bool( v ) );
      TEST_ASSERT( v.type() == type::OBJECT );

      TEST_ASSERT( !v.is_uninitialized() );
      TEST_ASSERT( !v.is_null() );
      TEST_ASSERT( !v.is_boolean() );
      TEST_ASSERT( !v.is_signed() );
      TEST_ASSERT( !v.is_unsigned() );
      TEST_ASSERT( !v.is_integer() );
      TEST_ASSERT( !v.is_double() );
      TEST_ASSERT( !v.is_number() );
      TEST_ASSERT( !v.is_string() );
      TEST_ASSERT( !v.is_string_view() );
      TEST_ASSERT( !v.is_string_type() );
      TEST_ASSERT( !v.is_binary() );
      TEST_ASSERT( !v.is_binary_view() );
      TEST_ASSERT( !v.is_binary_type() );
      TEST_ASSERT( !v.is_array() );
      TEST_ASSERT( v.is_object() );
      TEST_ASSERT( !v.is_value_ptr() );
      TEST_ASSERT( !v.is_opaque_ptr() );

      TEST_THROWS( v.get_boolean() );
      TEST_THROWS( v.get_signed() );
      TEST_THROWS( v.get_unsigned() );
      TEST_THROWS( v.get_double() );
      TEST_THROWS( v.get_string() );
      TEST_THROWS( v.get_string_view() );
      TEST_THROWS( v.get_string_type() );
      TEST_THROWS( v.get_binary() );
      TEST_THROWS( v.get_binary_view() );
      TEST_THROWS( v.get_binary_type() );
      TEST_THROWS( v.get_array() );
      (void)v.get_object();
      TEST_THROWS( v.get_value_ptr() );
      TEST_THROWS( v.get_opaque_ptr() );

      if( t ) {
         TEST_ASSERT( v.get_object().size() == t );
      }
   }

}  // namespace tao::json

#endif
