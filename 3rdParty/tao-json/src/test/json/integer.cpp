// Copyright (c) 2016-2020 Dr. Colin Hirsch and Daniel Frey
// Please see LICENSE for license or visit https://github.com/taocpp/json/

#include "test.hpp"

#include <tao/json/from_string.hpp>
#include <tao/json/value.hpp>

namespace tao::json
{
   template< typename T >
   void test_integer_s( const std::string& input, const T v )
   {
      const value j = json::from_string( input );
      TEST_ASSERT( j.as< T >() == v );
   }

   template< typename T, typename U >
   void test_integer( const U input, const U v )
   {
      const value j = input;
      TEST_ASSERT( j.as< T >() == static_cast< T >( v ) );
   }

   template< typename T >
   void test_integer_unsigned( const T v )
   {
      test_integer< signed char >( v, v );
      test_integer< unsigned char >( v, v );
      test_integer< signed short >( v, v );
      test_integer< unsigned short >( v, v );
      test_integer< signed int >( v, v );
      test_integer< unsigned int >( v, v );
      test_integer< signed long >( v, v );
      test_integer< unsigned long >( v, v );
      test_integer< signed long long >( v, v );
      test_integer< unsigned long long >( v, v );
   }

   template< typename T >
   void test_integer_signed( const T v )
   {
      test_integer< signed char >( v, v );
      test_integer< signed short >( v, v );
      test_integer< signed int >( v, v );
      test_integer< signed long >( v, v );
      test_integer< signed long long >( v, v );
   }

   void unit_test()
   {
      test_integer_s< signed char >( "0", 0 );
      test_integer_s< unsigned char >( "0", 0 );
      test_integer_s< signed short >( "0", 0 );
      test_integer_s< unsigned short >( "0", 0 );
      test_integer_s< signed int >( "0", 0 );
      test_integer_s< unsigned int >( "0", 0 );
      test_integer_s< signed long >( "0", 0 );
      test_integer_s< unsigned long >( "0", 0 );
      test_integer_s< signed long long >( "0", 0 );
      test_integer_s< unsigned long long >( "0", 0 );

      test_integer_s< signed char >( "1", 1 );
      test_integer_s< unsigned char >( "1", 1 );
      test_integer_s< signed short >( "1", 1 );
      test_integer_s< unsigned short >( "1", 1 );
      test_integer_s< signed int >( "1", 1 );
      test_integer_s< unsigned int >( "1", 1 );
      test_integer_s< signed long >( "1", 1 );
      test_integer_s< unsigned long >( "1", 1 );
      test_integer_s< signed long long >( "1", 1 );
      test_integer_s< unsigned long long >( "1", 1 );

      test_integer_s< signed char >( "-1", -1 );
      test_integer_s< signed short >( "-1", -1 );
      test_integer_s< signed int >( "-1", -1 );
      test_integer_s< signed long >( "-1", -1 );
      test_integer_s< signed long long >( "-1", -1 );

      test_integer_unsigned< signed char >( 0 );
      test_integer_unsigned< signed char >( 1 );
      test_integer_signed< signed char >( -1 );

      test_integer_unsigned< unsigned char >( 0 );
      test_integer_unsigned< unsigned char >( 1 );

      test_integer_unsigned< signed short >( 0 );
      test_integer_unsigned< signed short >( 1 );
      test_integer_signed< signed short >( -1 );

      test_integer_unsigned< unsigned short >( 0 );
      test_integer_unsigned< unsigned short >( 1 );

      test_integer_unsigned< signed int >( 0 );
      test_integer_unsigned< signed int >( 1 );
      test_integer_signed< signed int >( -1 );

      test_integer_unsigned< unsigned int >( 0 );
      test_integer_unsigned< unsigned int >( 1 );

      test_integer_unsigned< signed long >( 0 );
      test_integer_unsigned< signed long >( 1 );
      test_integer_signed< signed long >( -1 );

      test_integer_unsigned< unsigned long >( 0 );
      test_integer_unsigned< unsigned long >( 1 );

      test_integer_unsigned< signed long long >( 0 );
      test_integer_unsigned< signed long long >( 1 );
      test_integer_signed< signed long long >( -1 );

      test_integer_unsigned< unsigned long long >( 0 );
      test_integer_unsigned< unsigned long long >( 1 );
   }

}  // namespace tao::json

#include "main.hpp"
