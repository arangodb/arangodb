// Copyright (c) 2018-2020 Dr. Colin Hirsch and Daniel Frey
// Please see LICENSE for license or visit https://github.com/taocpp/json/

#include <limits>

#include "test.hpp"
#include "test_events.hpp"

#include <tao/json.hpp>

#include <tao/json/contrib/traits.hpp>

namespace tao::json
{
   struct pair_ctor_test
   {
      pair_ctor_test() = delete;

      explicit pair_ctor_test( const std::int64_t i )
         : v( i )
      {}

      std::int64_t v;
   };

   template<>
   struct traits< pair_ctor_test >
   {
      template< template< typename... > class Traits >
      [[nodiscard]] static pair_ctor_test as( const basic_value< Traits >& v )
      {
         return pair_ctor_test( v.get_signed() );
      }
   };

   void test_pair()
   {
      {
         using t = std::pair< std::string, int >;
         const t p( "foo", 42 );
         const value v = p;
         TEST_ASSERT( v == p );
         TEST_ASSERT( v.is_array() );
         const auto q = v.as< t >();
         TEST_ASSERT( p == q );
         t r;
         v.to( r );
         TEST_ASSERT( p == r );
         parts_parser s( "[\"foo\",42]", __FUNCTION__ );
         const auto u = consume< t >( s );
         TEST_ASSERT( u == p );
         const value w = produce::to_value( u );
         TEST_ASSERT( w == v );
      }
      {
         using t = std::pair< pair_ctor_test, pair_ctor_test >;
         const value v = value::array( { -1, -2 } );
         const auto p = v.as< t >();
         TEST_ASSERT( p.first.v == -1 );
         TEST_ASSERT( p.second.v == -2 );
      }
   }

   void test_shared()
   {
      using namespace test;
      {
         const auto f = std::make_shared< std::uint64_t >( 42 );
         TEST_ASSERT( !traits< std::shared_ptr< uint64_t > >::is_nothing< traits >( f ) );
         value v = f;
         TEST_ASSERT( v == f );
         TEST_ASSERT( v.is_number() );
         const auto g = v.as< std::shared_ptr< std::uint64_t > >();
         TEST_ASSERT( g != f );
         TEST_ASSERT( *g == *f );
         std::shared_ptr< std::uint64_t > h;
         v.to( h );
         TEST_ASSERT( h != f );
         TEST_ASSERT( g != h );
         TEST_ASSERT( *g == *h );
         consumer c = { uint64( 42 ) };
         events::produce( c, f );
      }
   }

   void test_unique()
   {
      const std::unique_ptr< std::uint64_t > f( new std::uint64_t( 42 ) );
      value v = f;
      TEST_ASSERT( v == f );
      TEST_ASSERT( v.is_number() );
      const auto g = v.as< std::unique_ptr< std::uint64_t > >();
      TEST_ASSERT( g != f );
      TEST_ASSERT( *g == *f );
   }

   void test_deque()
   {
      const std::deque< std::uint64_t > f = { 1, 2, 3, 4 };
      value v = f;
      TEST_ASSERT( v == f );
      TEST_ASSERT( v.is_array() );
      TEST_ASSERT( v.get_array().size() == 4 );
      TEST_ASSERT( v[ 0 ] == 1 );
      TEST_ASSERT( v[ 1 ] == 2 );
      TEST_ASSERT( v[ 2 ] == 3 );
      TEST_ASSERT( v[ 3 ] == 4 );
      const auto g = v.as< std::deque< std::uint64_t > >();
      TEST_ASSERT( g == f );
   }

   void test_list()
   {
      const std::list< std::uint64_t > f = { 1, 2, 3, 4 };
      value v = f;
      TEST_ASSERT( v == f );
      TEST_ASSERT( v.is_array() );
      TEST_ASSERT( v.get_array().size() == 4 );
      TEST_ASSERT( v[ 0 ] == 1 );
      TEST_ASSERT( v[ 1 ] == 2 );
      TEST_ASSERT( v[ 2 ] == 3 );
      TEST_ASSERT( v[ 3 ] == 4 );
      const auto g = v.as< std::list< std::uint64_t > >();
      TEST_ASSERT( g == f );
   }

   void test_set()
   {
      const std::set< std::uint64_t > f = { 1, 2, 3, 4 };
      value v = f;
      TEST_ASSERT( v == f );
      TEST_ASSERT( v.is_array() );
      TEST_ASSERT( v.get_array().size() == 4 );
      TEST_ASSERT( v[ 0 ] == 1 );
      TEST_ASSERT( v[ 1 ] == 2 );
      TEST_ASSERT( v[ 2 ] == 3 );
      TEST_ASSERT( v[ 3 ] == 4 );
      const auto g = v.as< std::set< std::uint64_t > >();
      TEST_ASSERT( g == f );
   }

   void test_vector()
   {
      const std::vector< std::uint64_t > f = { 1, 2, 3, 4 };
      value v = f;
      TEST_ASSERT( v == f );
      TEST_ASSERT( v >= f );
      TEST_ASSERT( v <= f );
      TEST_ASSERT( f == v );
      TEST_ASSERT( f >= v );
      TEST_ASSERT( f <= v );
      TEST_ASSERT( v.is_array() );
      TEST_ASSERT( v.get_array().size() == 4 );
      TEST_ASSERT( v[ 0 ] == 1 );
      TEST_ASSERT( v[ 1 ] == 2 );
      TEST_ASSERT( v[ 2 ] == 3 );
      TEST_ASSERT( v[ 3 ] == 4 );
      const auto g = v.as< std::vector< std::uint64_t > >();
      TEST_ASSERT( g == f );

      const std::vector< std::uint64_t > h = { 2, 3, 4 };
      TEST_ASSERT( h != v );
      TEST_ASSERT( h > v );
      TEST_ASSERT( h >= v );
      TEST_ASSERT( !( h < v ) );
      TEST_ASSERT( !( h <= v ) );
      TEST_ASSERT( !( h == v ) );
      TEST_ASSERT( v != h );
      TEST_ASSERT( v < h );
      TEST_ASSERT( v <= h );
      TEST_ASSERT( !( v > h ) );
      TEST_ASSERT( !( v >= h ) );
      TEST_ASSERT( !( v == h ) );
   }

   void test_map()
   {
      const std::map< std::string, std::uint64_t, std::less<> > f = { { "a", 1 }, { "b", 2 }, { "c", 3 } };
      value v = f;
      TEST_ASSERT( v == f );
      TEST_ASSERT( v.is_object() );
      TEST_ASSERT( v.get_object().size() == 3 );
      TEST_ASSERT( v[ "a" ] == 1 );
      TEST_ASSERT( v[ "b" ] == 2 );
      TEST_ASSERT( v[ "c" ] == 3 );
      const auto g = v.as< std::map< std::string, std::uint64_t, std::less<> > >();
      TEST_ASSERT( g == f );

      const std::map< std::string, std::uint64_t, std::less<> > h = { { "a", 1 }, { "b", 3 }, { "c", 1 } };
      TEST_ASSERT( h != v );
      TEST_ASSERT( h > v );
      TEST_ASSERT( h >= v );
      TEST_ASSERT( !( h < v ) );
      TEST_ASSERT( !( h <= v ) );
      TEST_ASSERT( !( h == v ) );
      TEST_ASSERT( v != h );
      TEST_ASSERT( v < h );
      TEST_ASSERT( v <= h );
      TEST_ASSERT( !( v > h ) );
      TEST_ASSERT( !( v >= h ) );
      TEST_ASSERT( !( v == h ) );

      const std::map< std::string, std::uint64_t, std::less<> > i = { { "a", 1 }, { "c", 2 }, { "d", 0 } };
      TEST_ASSERT( i != v );
      TEST_ASSERT( i > v );
      TEST_ASSERT( i >= v );
      TEST_ASSERT( !( i < v ) );
      TEST_ASSERT( !( i <= v ) );
      TEST_ASSERT( !( i == v ) );
      TEST_ASSERT( v != i );
      TEST_ASSERT( v < i );
      TEST_ASSERT( v <= i );
      TEST_ASSERT( !( v > i ) );
      TEST_ASSERT( !( v >= i ) );
      TEST_ASSERT( !( v == i ) );
   }

   void test_pointer()
   {
      const pointer p( "/foo/1/bar/-" );
      value v = p;
      TEST_ASSERT( v == value::array( { "foo", 1, "bar", "-" } ) );
      events::to_value t;
      events::produce( t, p );
      TEST_ASSERT( t.value == v );
   }

   void test_vector_bool()
   {
      std::vector< bool > g;
      g.push_back( true );
      g.push_back( true );
      g.push_back( false );
      value v = g;
      TEST_ASSERT( v.is_array() );
   }

   void unit_test()
   {
      test_pair();

      test_shared();
      test_unique();

      test_deque();
      test_list();
      test_set();
      test_vector();
      test_map();

      test_pointer();

      test_vector_bool();
   }

}  // namespace tao::json

#include "main.hpp"
