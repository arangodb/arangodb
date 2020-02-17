// Copyright (c) 2015-2020 Dr. Colin Hirsch and Daniel Frey
// Please see LICENSE for license or visit https://github.com/taocpp/json/

#include <limits>

#include <tao/json/from_string.hpp>
#include <tao/json/value.hpp>

#include "test.hpp"
#include "test_types.hpp"

namespace tao::json
{
   void test_uninitialized()
   {
      const value v{};

      assert_uninitialized( v );

      TEST_THROWS( v.at( 0 ) );
      TEST_THROWS( v.at( "foo" ) );

      value u = v;

      assert_uninitialized( u );

      TEST_ASSERT( u == v );
      TEST_ASSERT( u <= v );
      TEST_ASSERT( u >= v );
      TEST_ASSERT( !( u != v ) );
      TEST_ASSERT( !( u < v ) );
      TEST_ASSERT( !( u > v ) );

      const value w = std::move( u );

      assert_uninitialized( w );

      TEST_ASSERT( w == v );
      TEST_ASSERT( w <= v );
      TEST_ASSERT( w >= v );
      TEST_ASSERT( !( w != v ) );
      TEST_ASSERT( !( w < v ) );
      TEST_ASSERT( !( w > v ) );

      u = uninitialized;

      assert_uninitialized( u );

      TEST_ASSERT( u == v );
   }

   void test_null()
   {
      const value v = null;
      const value v2( v );

      assert_null( v );
      assert_null( v2 );

      const value u( null );

      assert_null( u );

      TEST_ASSERT( u == v );

      TEST_THROWS( v.at( 0 ) );
      TEST_THROWS( v.at( "foo" ) );
   }

   void test_boolean( const bool b )
   {
      const value v( b );
      const value v2( v );

      assert_boolean( v, b );
      assert_boolean( v2, b );

      TEST_ASSERT( v == v );
      TEST_ASSERT( value( b ) == v );
      TEST_ASSERT( value( !b ) != v );

      TEST_THROWS( v.at( 0 ) );
      TEST_THROWS( v.at( "foo" ) );
   }

   template< typename T >
   void test_signed( const T t )
   {
      const value v( t );
      const value v2( v );

      assert_signed( v, t );
      assert_signed( v2, t );

      TEST_THROWS( v.at( 0 ) );
      TEST_THROWS( v.at( "foo" ) );
   }

   template< typename T >
   void test_signed()
   {
      test_signed< T >( 0 );
      test_signed< T >( 1 );
      test_signed< T >( -1 );
      test_signed< T >( 42 );
      test_signed< T >( -42 );
      test_signed< T >( ( std::numeric_limits< T >::min )() );
      test_signed< T >( ( std::numeric_limits< T >::max )() );
   }

   template< typename T >
   void test_unsigned( const T t )
   {
      const value v( t );
      const value v2( v );

      assert_unsigned( v, t );
      assert_unsigned( v2, t );

      TEST_THROWS( v.at( 0 ) );
      TEST_THROWS( v.at( "foo" ) );
   }

   template< typename T >
   void test_unsigned()
   {
      test_unsigned< T >( 0 );
      test_unsigned< T >( 1 );
      test_unsigned< T >( 42 );
      test_unsigned< T >( ( std::numeric_limits< T >::min )() );
      test_unsigned< T >( ( std::numeric_limits< T >::max )() );
   }

   void test_double( const double d )
   {
      const value v( d );
      const value v2( v );

      assert_double( v, d );
      assert_double( v2, d );

      TEST_THROWS( v.at( 0 ) );
      TEST_THROWS( v.at( "foo" ) );
   }

   template< unsigned N >
   void test_string( const char ( &s )[ N ] )
   {
      const std::string t = s;

      const value v( s );
      const value v2( v );

      assert_string( v, t );
      assert_string( v2, t );

      TEST_ASSERT( v.get_string() == value( t ).get_string() );
      TEST_ASSERT( v.get_string() == value( std::string( s ) ).get_string() );
      TEST_ASSERT( v.get_string() == value( std::string( s, N - 1 ) ).get_string() );
      TEST_ASSERT( v.get_string() == value( std::string( s + 0 ) ).get_string() );

      TEST_THROWS( v.at( 0 ) );
      TEST_THROWS( v.at( "foo" ) );
   }

   void test_empty_array( const value& v )
   {
      const value v2( v );

      assert_array( v, 0 );
      assert_array( v2, 0 );

      TEST_ASSERT( v == v2 );

      TEST_THROWS( v.at( 0 ) );
      TEST_THROWS( v.at( "foo" ) );
   }

   void test_empty_object( const value& v )
   {
      const value v2( v );

      assert_object( v, 0 );
      assert_object( v2, 0 );

      TEST_ASSERT( v == v2 );

      TEST_THROWS( v.at( 0 ) );
      TEST_THROWS( v.at( "foo" ) );
   }

   void test_array_1234()
   {
      const value v = value::array( { 1, 2, 3, 4 } );
      const value v2( v );

      assert_array( v, 4 );

      const std::vector< value > r = { value( 1 ), value( 2 ), value( 3 ), value( 4 ) };

      TEST_ASSERT( v == value( r ) );
      TEST_ASSERT( v.get_array() == r );

      TEST_ASSERT( v.at( 0 ).get_signed() == 1 );
      TEST_ASSERT( v.at( 1 ).get_signed() == 2 );
      TEST_ASSERT( v.at( 2 ).get_signed() == 3 );
      TEST_ASSERT( v.at( 3 ).get_signed() == 4 );

      TEST_ASSERT( v.at( 0 ).get_signed() == 1 );
      TEST_ASSERT( v.at( 1 ).get_signed() == 2 );
      TEST_ASSERT( v.at( 2 ).get_signed() == 3 );
      TEST_ASSERT( v.at( 3 ).get_signed() == 4 );

      TEST_THROWS( v.at( 4 ) );
      TEST_THROWS( v.at( "foo" ) );
   }

   void test_object_1234()
   {
      const value v{ { "foo", "bar" }, { "bar", 42 }, { "baz", { { "baz", value::array( { true, false, 0 } ) } } } };
      const value v2( v );

      assert_object( v, 3 );

      // TODO: Add more tests

      TEST_THROWS( value{ { "foo", 1 }, { "foo", 2 } } );
   }

   void unit_test()
   {
      test_uninitialized();

      test_null();

      test_boolean( true );
      test_boolean( false );

      test_signed< signed char >();
      test_signed< signed short >();
      test_signed< signed int >();
      test_signed< signed long >();
      test_signed< signed long long >();

      test_unsigned< unsigned char >();
      test_unsigned< unsigned short >();
      test_unsigned< unsigned int >();
      test_unsigned< unsigned long >();
      test_unsigned< unsigned long long >();

      test_double( 0.0 );
      test_double( 42.0 );

      value v;
      {
         const double a = std::numeric_limits< double >::infinity();
         const double b = std::numeric_limits< double >::quiet_NaN();
         const double c = std::numeric_limits< double >::signaling_NaN();

         v = null;

         assert_null( v );

         v = true;

         assert_boolean( v, true );

         v = 1;

         assert_signed( v, 1 );

         v = 1U;

         assert_unsigned( v, 1 );

         v = 2.0;

         assert_double( v, 2.0 );

         v = "hallo";

         assert_string( v, "hallo" );

         v = a;

         assert_double( v, a );

         v = std::vector< value >();

         assert_array( v, 0 );

         v = b;

         assert_double( v, std::nullopt );
         TEST_ASSERT( std::isnan( v.get_double() ) );

         v = std::map< std::string, value, std::less<> >();

         assert_object( v, 0 );

         v = c;

         assert_double( v, std::nullopt );
         TEST_ASSERT( std::isnan( v.get_double() ) );
      }

      test_string( "" );
      test_string( "foo" );
      test_string( "abcdefghijklmnpqrstuvwxyz" );

      TEST_ASSERT( value( "\0" ).get_string().empty() );  // TODO?

      TEST_ASSERT( value( "baz" ).get_string().size() == 3 );

      test_empty_array( empty_array );
      test_empty_array( std::vector< value >() );

      test_empty_object( empty_object );
      test_empty_object( std::map< std::string, value, std::less<> >() );

      test_array_1234();
      test_object_1234();

      TEST_THROWS( value( json::from_string( "1" ) ).emplace_back( 2 ) );
      TEST_THROWS( value( json::from_string( "1" ) ).try_emplace( "foo", 3 ) );
      {
         value a;
         a.emplace_back( 4 );
         TEST_ASSERT( a.type() == type::ARRAY );
         TEST_ASSERT( a.at( 0 ) == 4 );
         value b( a );
         TEST_ASSERT( a.get_array() == b.get_array() );
      }
      {
         value a;
         a.try_emplace( "foo", 5 );
         TEST_ASSERT( a.type() == type::OBJECT );
         TEST_ASSERT( a.at( "foo" ) == 5 );
         value b( a );
         TEST_ASSERT( a == b );
         TEST_ASSERT( a.get_object() == b.get_object() );
      }
      {
         const value a( "foo" );
         const value b( a );
         TEST_ASSERT( a.get_string() == b.get_string() );
      }
      {
         const value a( 4 );
         const value b( 4.0 );
         TEST_ASSERT( a == b );
         TEST_ASSERT( a == 4 );
         TEST_ASSERT( a == 4.0 );
         TEST_ASSERT( b == 4 );
         TEST_ASSERT( b == 4.0 );
      }
      {
         value v2 = { { "foo", { { "bar", { { "baz", 42 } } } } } };
         TEST_ASSERT( v2.at( "foo" ).at( "bar" ).at( "baz" ).is_signed() );
         TEST_ASSERT( v2.at( "foo" ).at( "bar" ).at( "baz" ).get_signed() == 42 );
         v2 = v2.at( "foo" ).at( "bar" );
         TEST_ASSERT( v2.at( "baz" ).is_signed() );
         TEST_ASSERT( v2.at( "baz" ).get_signed() == 42 );
      }

      std::optional< value > ov = std::nullopt;
      ov = std::nullopt;
   }

}  // namespace tao::json

#include "main.hpp"
