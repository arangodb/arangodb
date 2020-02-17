// Copyright (c) 2018-2020 Dr. Colin Hirsch and Daniel Frey
// Please see LICENSE for license or visit https://github.com/taocpp/json/

#include <bitset>
#include <limits>
#include <list>
#include <map>
#include <type_traits>
#include <vector>

#include "test.hpp"
#include "test_unhex.hpp"

#include <tao/json.hpp>
#include <tao/json/binding.hpp>
#include <tao/json/consume.hpp>

#include <tao/json/cbor/parts_parser.hpp>
#include <tao/json/parts_parser.hpp>

#include <tao/json/contrib/traits.hpp>

namespace tao::json
{
   template< typename T, typename = void >
   struct my_traits
      : public traits< T >
   {};

   class base_class
   {
   public:
      virtual ~base_class() = default;

      base_class( const base_class& ) = delete;
      base_class( base_class&& ) = delete;
      void operator=( const base_class& ) = delete;
      void operator=( base_class&& ) = delete;

   protected:
      base_class() = default;
   };

   class derived_one
      : public base_class
   {
   public:
      std::string s;
   };

   class derived_two
      : public base_class
   {
   public:
      int i = 0;
   };

   template<>
   struct my_traits< derived_one >
      : binding::object< TAO_JSON_BIND_REQUIRED( "s", &derived_one::s ) >
   {};

   template<>
   struct my_traits< derived_two >
      : binding::object< TAO_JSON_BIND_REQUIRED( "i", &derived_two::i ) >
   {};

   template<>
   struct my_traits< std::shared_ptr< base_class > >
      : binding::factory< TAO_JSON_FACTORY_BIND( "one", derived_one ),
                          TAO_JSON_FACTORY_BIND( "two", derived_two ) >
   {};

   template<>
   struct my_traits< std::unique_ptr< base_class > >
      : my_traits< std::shared_ptr< base_class > >
   {};

   struct foo
   {
      std::string a = "hello";
      std::string b = "world";
   };

   struct foo_version_two
      : public binding::array< TAO_JSON_BIND_ELEMENT( &foo::a ),
                               TAO_JSON_BIND_ELEMENT( &foo::b ),
                               binding::element_u< 2 > >
   {};

   struct foo_version_one
      : public binding::array< TAO_JSON_BIND_ELEMENT( &foo::a ),
                               TAO_JSON_BIND_ELEMENT( &foo::b ) >
   {};

   template<>
   struct my_traits< foo >
      : binding::versions< foo_version_two,
                           foo_version_one >
   {};

   struct foo2
      : public foo
   {
      int plus = 0;
   };

   template<>
   struct my_traits< foo2 >
      : binding::array< binding::inherit< foo_version_one >,
                        TAO_JSON_BIND_ELEMENT( &foo2::plus ) >
   {};

   struct bar
   {
      int i = -1;
      std::string c = "c";
   };

   template<>
   struct my_traits< bar >
      : binding::object< TAO_JSON_BIND_REQUIRED( "i", &bar::i ),
                         TAO_JSON_BIND_OPTIONAL( "c", &bar::c ) >
   {};

   struct baz
   {
      [[nodiscard]] int get_i() const
      {
         return i;
      }

      int i = -1;
      int j = -2;
   };

   [[nodiscard]] int get_j( const baz& b )
   {
      return b.j;
   }

   template<>
   struct my_traits< baz >
      : binding::array< TAO_JSON_BIND_ELEMENT( &baz::get_i ),
                        TAO_JSON_BIND_ELEMENT( &get_j ) >
   {};

   struct test_empty
   {
      std::list< int > list;
   };

   template<>
   struct my_traits< test_empty >
      : binding::object< TAO_JSON_BIND_OPTIONAL( "list", &test_empty::list ) >
   {};

   struct test_suppress
   {
      std::list< int > list;
   };

   template<>
   struct my_traits< test_suppress >
      : binding::basic_object< binding::for_unknown_key::skip,
                               binding::for_nothing_value::suppress,
                               TAO_JSON_BIND_OPTIONAL( "list", &test_suppress::list ) >
   {};

   using my_value = basic_value< my_traits >;

   void unit_test()
   {
      {
         test_suppress w;
         basic_value< my_traits > v = w;
         TEST_ASSERT( v.is_object() );
         TEST_ASSERT( v.get_object().empty() );
      }
      {
         test_empty w;
         basic_value< my_traits > v = w;
         TEST_ASSERT( v.is_object() );
         TEST_ASSERT( v.get_object().size() == 1 );
         TEST_ASSERT( v.get_object().at( "list" ).is_array() );
         TEST_ASSERT( v.get_object().at( "list" ).get_array().empty() );
         w.list.push_back( 3 );
         basic_value< my_traits > x = w;
         TEST_ASSERT( x.is_object() );
         TEST_ASSERT( x.get_object().size() == 1 );
         TEST_ASSERT( x.get_object().at( "list" ).is_array() );
         TEST_ASSERT( x.get_object().at( "list" ).get_array().size() == 1 );
         TEST_ASSERT( x.get_object().at( "list" ).get_array()[ 0 ] == 3 );
      }
      {
         const basic_value< my_traits > v = basic_value< my_traits >::array( { "a", "b", 3 } );
         const auto f = v.as< foo2 >();
         TEST_ASSERT( f.a == "a" );
         TEST_ASSERT( f.b == "b" );
         TEST_ASSERT( f.plus == 3 );
      }
      {
         const std::tuple< int, std::string, double > b{ 42, "hallo", 3.0 };
         basic_value< my_traits > v = b;
         TEST_ASSERT( v.is_array() );
         TEST_ASSERT( v.get_array().size() == 3 );
         TEST_ASSERT( v[ 0 ] == 42 );
         TEST_ASSERT( v[ 1 ] == std::string( "hallo" ) );
         TEST_ASSERT( v[ 2 ] == 3.0 );
         const auto c = v.as< std::tuple< int, std::string, double > >();
         TEST_ASSERT( b == c );
      }
      {
         baz b;
         basic_value< my_traits > v = b;
         TEST_ASSERT( v.is_array() );
         TEST_ASSERT( v.get_array().size() == 2 );
         TEST_ASSERT( v[ 0 ] == -1 );
         TEST_ASSERT( v[ 1 ] == -2 );
      }
      {
         parts_parser pp( "  [  \"kasimir\"  ,  \"fridolin\", 2  ]  ", __FUNCTION__ );
         const auto f = consume< foo, my_traits >( pp );
         TEST_ASSERT( f.a == "kasimir" );
         TEST_ASSERT( f.b == "fridolin" );
      }
      {
         const auto f = consume_string< foo, my_traits >( "  [  \"kasimir\"  ,  \"fridolin\", 2  ]  " );
         TEST_ASSERT( f.a == "kasimir" );
         TEST_ASSERT( f.b == "fridolin" );
      }
      {
         parts_parser pp( "  [  ]  ", __FUNCTION__ );
         const auto v = consume< std::shared_ptr< std::vector< int > >, my_traits >( pp );
         TEST_ASSERT( v->empty() );
      }
      {
         parts_parser pp( "  [  1  , 2, 3  ]  ", __FUNCTION__ );
         const auto v = consume< std::vector< int >, my_traits >( pp );
         TEST_ASSERT( v.size() == 3 );
         TEST_ASSERT( v[ 0 ] == 1 );
         TEST_ASSERT( v[ 1 ] == 2 );
         TEST_ASSERT( v[ 2 ] == 3 );
      }
      {
         cbor::parts_parser pp( test_unhex( "80" ), __FUNCTION__ );
         const auto v = consume< std::vector< unsigned >, my_traits >( pp );
         TEST_ASSERT( v.empty() );
      }
      {
         cbor::parts_parser pp( test_unhex( "8a00010203040506070809" ), __FUNCTION__ );
         const auto v = consume< std::vector< unsigned >, my_traits >( pp );
         TEST_ASSERT( v.size() == 10 );
         for( std::size_t i = 0; i < 10; ++i ) {
            TEST_ASSERT( v[ i ] == i );
         }
      }
      {
         const auto v = cbor::consume_string< std::vector< unsigned >, my_traits >( test_unhex( "8a00010203040506070809" ) );
         TEST_ASSERT( v.size() == 10 );
         for( std::size_t i = 0; i < 10; ++i ) {
            TEST_ASSERT( v[ i ] == i );
         }
      }
      {
         cbor::parts_parser pp( test_unhex( "9f00010203040506070809ff" ), __FUNCTION__ );
         const auto v = consume< std::vector< unsigned >, my_traits >( pp );
         TEST_ASSERT( v.size() == 10 );
         for( std::size_t i = 0; i < 10; ++i ) {
            TEST_ASSERT( v[ i ] == i );
         }
      }
      {
         cbor::parts_parser pp( test_unhex( "8261616162" ), __FUNCTION__ );
         const auto v = consume< foo, my_traits >( pp );
         TEST_ASSERT( v.a == "a" );
         TEST_ASSERT( v.b == "b" );
      }
      {
         parts_parser pp( " { \"a\" : 4, \"b\" : 5 } ", __FUNCTION__ );
         const auto v = consume< std::map< std::string, int, std::less<> >, my_traits >( pp );
         TEST_ASSERT( v.size() == 2 );
         TEST_ASSERT( v.at( "a" ) == 4 );
         TEST_ASSERT( v.at( "b" ) == 5 );
      }
      {
         parts_parser pp( " { \"c\" : \"yeah\" , \"i\" : 42 } ", __FUNCTION__ );
         const auto v = consume< bar, my_traits >( pp );
         TEST_ASSERT( v.c == "yeah" );
         TEST_ASSERT( v.i == 42 );
      }
      {
         parts_parser pp( " { \"i\" : 42 } ", __FUNCTION__ );
         const auto v = consume< bar, my_traits >( pp );
         TEST_ASSERT( v.c == "c" );
         TEST_ASSERT( v.i == 42 );
      }
      {
         cbor::parts_parser pp( test_unhex( "bf616364796561686169182aff" ), __FUNCTION__ );
         const auto v = consume< bar, my_traits >( pp );
         TEST_ASSERT( v.c == "yeah" );
         TEST_ASSERT( v.i == 42 );
      }
      {
         const basic_value< my_traits > v = {
            { "c", "x" },
            { "i", 2 }
         };
         const auto x = v.as< bar >();
         TEST_ASSERT( x.c == "x" );
         TEST_ASSERT( x.i == 2 );
      }
      {
         parts_parser pp( "{ \"two\":{ \"i\" : 17 }}", __FUNCTION__ );
         const auto v = consume< std::shared_ptr< base_class >, my_traits >( pp );
         const auto c = std::dynamic_pointer_cast< derived_two >( v );
         TEST_ASSERT( c );
         TEST_ASSERT( c->i == 17 );
      }
      {
         parts_parser pp( "{ \"two\":{ \"i\" : 17 }}", __FUNCTION__ );
         const auto v = consume< std::unique_ptr< base_class >, my_traits >( pp );
         const auto* c = dynamic_cast< derived_two* >( v.get() );
         TEST_ASSERT( c );
         TEST_ASSERT( c->i == 17 );
      }
      {
         basic_value< my_traits > v;
         v[ "two" ][ "i" ] = 17;
         const auto x = v.as< std::shared_ptr< base_class > >();
         const auto c = std::dynamic_pointer_cast< derived_two >( x );
         TEST_ASSERT( c );
         TEST_ASSERT( c->i == 17 );
      }
   }

}  // namespace tao::json

#include "main.hpp"
