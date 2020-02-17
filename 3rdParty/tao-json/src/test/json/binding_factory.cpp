// Copyright (c) 2018-2020 Dr. Colin Hirsch and Daniel Frey
// Please see LICENSE for license or visit https://github.com/taocpp/json/

#include "test.hpp"

#include <tao/json.hpp>

#include <tao/json/contrib/traits.hpp>

namespace tao::json
{
   class base_1
   {
   public:
      virtual ~base_1() = default;

      base_1( const base_1& ) = delete;
      base_1( base_1&& ) = delete;
      void operator=( const base_1& ) = delete;
      void operator=( base_1&& ) = delete;

   protected:
      base_1() = default;
   };

   class derived_1
      : public base_1
   {
   public:
      std::string s;
   };

   class derived_2
      : public base_1
   {
   public:
      int i = 105;
      int j = 106;
   };

   class derived_3
      : public base_1
   {
   public:
      int k = 107;
   };

   template<>
   struct traits< derived_1 >
      : binding::object< TAO_JSON_BIND_REQUIRED( "s", &derived_1::s ) >
   {
   };

   template<>
   struct traits< derived_2 >
      : binding::object< TAO_JSON_BIND_REQUIRED( "i", &derived_2::i ),
                         TAO_JSON_BIND_REQUIRED( "j", &derived_2::j ) >
   {
      TAO_JSON_DEFAULT_KEY( "two" );
   };

   template<>
   struct traits< std::shared_ptr< base_1 > >
      : binding::factory< TAO_JSON_FACTORY_BIND( "one", derived_1 ),
                          TAO_JSON_FACTORY_BIND1( derived_2 ) >
   {};

   template<>
   struct traits< std::unique_ptr< base_1 > >
      : traits< std::shared_ptr< base_1 > >
   {};

   void unit_test_1()
   {
      const value v = {
         { "one", { { "s", "foo" } } }
      };
      const auto a = v.as< std::shared_ptr< base_1 > >();
      TEST_ASSERT( a );
      const auto b = std::dynamic_pointer_cast< derived_1 >( a );
      TEST_ASSERT( b );
      TEST_ASSERT( b->s == "foo" );
      const value w = a;
      TEST_ASSERT( w == v );
      const value x = produce::to_value( a );
      TEST_ASSERT( w == x );
      parts_parser p( to_string( v ), __FUNCTION__ );
      const auto c = consume< std::shared_ptr< base_1 > >( p );
      TEST_ASSERT( c );
      const auto d = std::dynamic_pointer_cast< derived_1 >( c );
      TEST_ASSERT( d );
      TEST_ASSERT( d->s == "foo" );
   }

   void unit_test_2()
   {
      const value v = {
         { "one", { { "s", "foo" } } }
      };
      const auto a = v.as< std::unique_ptr< base_1 > >();
      TEST_ASSERT( a );
      const auto* b = dynamic_cast< derived_1* >( a.get() );
      TEST_ASSERT( b );
      TEST_ASSERT( b->s == "foo" );
      const value w = a;
      TEST_ASSERT( w == v );
      const value x = produce::to_value( a );
      TEST_ASSERT( w == x );
      parts_parser p( to_string( v ), __FUNCTION__ );
      const auto c = consume< std::unique_ptr< base_1 > >( p );
      TEST_ASSERT( c );
      const auto d = dynamic_cast< derived_1* >( c.get() );
      TEST_ASSERT( d );
      TEST_ASSERT( d->s == "foo" );
   }

   void unit_test_3()
   {
      const value v = {
         { "two", { { "i", 42 }, { "j", 23 } } }
      };
      const auto a = v.as< std::shared_ptr< base_1 > >();
      TEST_ASSERT( a );
      const auto b = std::dynamic_pointer_cast< derived_2 >( a );
      TEST_ASSERT( b );
      TEST_ASSERT( b->i == 42 );
      TEST_ASSERT( b->j == 23 );
      const value w = a;
      TEST_ASSERT( w == v );
      const value x = produce::to_value( a );
      TEST_ASSERT( w == x );
      parts_parser p( to_string( v ), __FUNCTION__ );
      const auto c = consume< std::shared_ptr< base_1 > >( p );
      TEST_ASSERT( c );
      const auto d = std::dynamic_pointer_cast< derived_2 >( c );
      TEST_ASSERT( d );
      TEST_ASSERT( d->i == 42 );
      TEST_ASSERT( d->j == 23 );
   }

   void unit_test_4()
   {
      const value v = {
         { "two", { { "i", 42 }, { "j", 23 } } }
      };
      const auto a = v.as< std::unique_ptr< base_1 > >();
      TEST_ASSERT( a );
      const auto* b = dynamic_cast< derived_2* >( a.get() );
      TEST_ASSERT( b );
      TEST_ASSERT( b && ( b->i == 42 ) );
      TEST_ASSERT( b && ( b->j == 23 ) );
      const value w = a;
      TEST_ASSERT( w == v );
      const value x = produce::to_value( a );
      TEST_ASSERT( w == x );
      parts_parser p( to_string( v ), __FUNCTION__ );
      const auto c = consume< std::unique_ptr< base_1 > >( p );
      TEST_ASSERT( c );
      const auto* d = dynamic_cast< derived_2* >( c.get() );
      TEST_ASSERT( d );
      TEST_ASSERT( d && ( d->i == 42 ) );
      TEST_ASSERT( d && ( d->j == 23 ) );
   }

   void unit_test_5()
   {
      const value z = empty_object;
      TEST_THROWS( z.as< std::unique_ptr< base_1 > >() );
      const value v = {
         { "one", { { "s", "foo" } } },
         { "two", { { "i", 42 }, { "j", 23 } } }
      };
      TEST_THROWS( v.as< std::unique_ptr< base_1 > >() );
      const value w = {
         { "1", { { "s", "foo" } } }
      };
      TEST_THROWS( w.as< std::unique_ptr< base_1 > >() );
      TEST_THROWS( value( std::shared_ptr< base_1 >( new derived_3 ) ) );
      events::discard d;
      TEST_THROWS( events::produce( d, std::shared_ptr< base_1 >( new derived_3 ) ) );
      parts_parser p( "{ \"four\" : null }", __FUNCTION__ );
      TEST_THROWS( consume< std::shared_ptr< base_1 > >( p ) );
   }

   class base_2
   {
   public:
      base_2( const base_2& ) = delete;
      base_2( base_2&& ) = delete;
      void operator=( const base_2& ) = delete;
      void operator=( base_2&& ) = delete;

   protected:
      base_2() = default;
      virtual ~base_2() = default;
   };

   struct arg_dummy
   {};

   struct derived_8
      : public base_2
   {
      derived_8( const value& /*unused*/, const arg_dummy& /*unused*/ ) {}
   };

   struct derived_9
      : public base_2
   {
      derived_9( const value& /*unused*/, const arg_dummy& /*unused*/ ) {}
   };

   template<>
   struct traits< std::shared_ptr< base_2 > >
      : binding::factory< TAO_JSON_FACTORY_BIND( "eight", derived_8 ),
                          TAO_JSON_FACTORY_BIND( "nine", derived_9 ) >
   {};

   void unit_test_6()
   {
      arg_dummy d;

      const auto v = from_string( "{ \"nine\" : 1 }" );
      const auto a = v.as_with< std::shared_ptr< base_2 > >( d );
      TEST_ASSERT( a );
   }

   void unit_test_7()
   {
      arg_dummy d;

      const auto v = from_string( "[ { \"nine\" : 1 }, { \"eight\" : 2 } ]" );
      const auto a = v.as_with< std::vector< std::shared_ptr< base_2 > > >( d );
      TEST_ASSERT( a.size() == 2 );
      TEST_ASSERT( a[ 0 ] && a[ 1 ] );
      TEST_ASSERT( std::dynamic_pointer_cast< derived_9 >( a[ 0 ] ) );
      TEST_ASSERT( std::dynamic_pointer_cast< derived_8 >( a[ 1 ] ) );
   }

   void unit_test_8()
   {
      arg_dummy d;

      const auto v = from_string( "{ \"foo\" : { \"nine\" : 1 }, \"bar\" : { \"eight\" : 2 } }" );
      const auto a = v.as_with< std::map< std::string, std::shared_ptr< base_2 >, std::less<> > >( d );
      TEST_ASSERT( a.size() == 2 );
      TEST_ASSERT( std::dynamic_pointer_cast< derived_9 >( a.at( "foo" ) ) );
      TEST_ASSERT( std::dynamic_pointer_cast< derived_8 >( a.at( "bar" ) ) );
   }

   void unit_test()
   {
      unit_test_1();
      unit_test_2();
      unit_test_3();
      unit_test_4();
      unit_test_5();
      unit_test_6();
      unit_test_7();
      unit_test_8();
   }

}  // namespace tao::json

#include "main.hpp"
