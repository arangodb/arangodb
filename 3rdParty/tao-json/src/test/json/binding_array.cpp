// Copyright (c) 2018-2020 Dr. Colin Hirsch and Daniel Frey
// Please see LICENSE for license or visit https://github.com/taocpp/json/

#include "test.hpp"

#include <tao/json.hpp>

#include <tao/json/contrib/traits.hpp>

namespace tao::json
{
   using type_1 = std::pair< int, std::string >;         // traits< std::pair< ... > > is implemented with binding::array and binding::element.
   using type_2 = std::tuple< int, std::string, bool >;  // traits< std::tuple< ... > > is implemented with binding::array and binding::element2.

   class type_3
   {
   public:
      [[nodiscard]] int get_int() const noexcept
      {
         return 7;
      }

      [[nodiscard]] const std::string& get_string() const noexcept
      {
         return m_string;
      }

   private:
      std::string m_string = "dangerous";
   };

   template<>
   struct traits< type_3 >
      : binding::array< TAO_JSON_BIND_ELEMENT( &type_3::get_int ),
                        TAO_JSON_BIND_ELEMENT( &type_3::get_string ) >
   {};

   struct type_4
   {
      int i = 0;
   };

   template<>
   struct traits< type_4 >
      : binding::array< TAO_JSON_BIND_ELEMENT( &type_4::i ),
                        TAO_JSON_BIND_ELEMENT_BOOL( true ),
                        TAO_JSON_BIND_ELEMENT_UNSIGNED( 90 ),
                        TAO_JSON_BIND_ELEMENT_SIGNED( -5 ),
                        TAO_JSON_BIND_ELEMENT_STRING( "abc" ) >
   {};

   void unit_test_1()
   {
      const value v = value::array( { 142, "hallo" } );
      const auto a = v.as< type_1 >();
      TEST_ASSERT( a.first == 142 );
      TEST_ASSERT( a.second == "hallo" );
      TEST_THROWS( v.as< type_2 >() );
      TEST_ASSERT( a == v );
      TEST_ASSERT( v == a );
      const value x = null;
      const value y = value::array( { 142, "world" } );
      const value z = value::array( { 143, "hallo" } );
      TEST_ASSERT( a != x );
      TEST_ASSERT( a != y );
      TEST_ASSERT( a != z );
      TEST_ASSERT( x != a );
      TEST_ASSERT( y != a );
      TEST_ASSERT( z != a );
   }

   void unit_test_2()
   {
      const value v = value::array( { 142, "hallo", 143 } );
      TEST_THROWS( v.as< type_1 >() );
      TEST_THROWS( v.as< type_2 >() );
   }

   void unit_test_3()
   {
      const value v = value::array( { 142 } );
      TEST_THROWS( v.as< type_1 >() );
      TEST_THROWS( v.as< type_2 >() );
   }

   void unit_test_4()
   {
      const value v = value::array( { "ciao", "hallo" } );
      TEST_THROWS( v.as< type_1 >() );
   }

   void unit_test_5()
   {
      const value v = value::array( { 142, 143 } );
      TEST_THROWS( v.as< type_1 >() );
   }

   void unit_test_6()
   {
      const value v = value::array( { 142, "hallo", true } );
      const auto a = v.as< type_2 >();
      TEST_ASSERT( std::get< 0 >( a ) == 142 );
      TEST_ASSERT( std::get< 1 >( a ) == "hallo" );
      TEST_ASSERT( std::get< 2 >( a ) == true );
      TEST_THROWS( v.as< type_1 >() );
   }

   void unit_test_7()
   {
      const value v = value::array( { "ciao", "hallo", true } );
      TEST_THROWS( v.as< type_1 >() );
      TEST_THROWS( v.as< type_2 >() );
   }

   void unit_test_8()
   {
      const value v = value::array( { 142, "hallo" } );
      type_1 a;
      TEST_ASSERT( a.first != 142 );
      TEST_ASSERT( a.second != "hallo" );
      v.to( a );
      TEST_ASSERT( a.first == 142 );
      TEST_ASSERT( a.second == "hallo" );
      type_2 b;
      TEST_THROWS( v.to( b ) );
   }

   void unit_test_0()
   {
      const value v = value::array( { 142, "hallo", true } );
      type_2 a;
      TEST_ASSERT( std::get< 0 >( a ) != 142 );
      TEST_ASSERT( std::get< 1 >( a ) != "hallo" );
      TEST_ASSERT( std::get< 2 >( a ) != true );
      v.to( a );
      TEST_ASSERT( std::get< 0 >( a ) == 142 );
      TEST_ASSERT( std::get< 1 >( a ) == "hallo" );
      TEST_ASSERT( std::get< 2 >( a ) == true );
      type_1 b;
      TEST_THROWS( v.to( b ) );
   }

   void unit_test_10()
   {
      const value v = value::array( { 142, 143 } );
      type_1 a;
      TEST_THROWS( v.to( a ) );
      type_2 b;
      TEST_THROWS( v.to( b ) );
   }

   void unit_test_11()
   {
      const value v = value::array( { 142, "hallo", 144 } );
      type_2 a;
      TEST_THROWS( v.to( a ) );
      type_1 b;
      TEST_THROWS( v.to( b ) );
   }

   void unit_test_12()
   {
      const type_1 a = { 19, "ciao" };
      const value v = a;
      TEST_ASSERT( v.is_array() );
      TEST_ASSERT( v.get_array().size() == 2 );
      TEST_ASSERT( v.get_array()[ 0 ].is_integer() );
      TEST_ASSERT( v.get_array()[ 0 ].as< int >() == 19 );
      TEST_ASSERT( v.get_array()[ 1 ].is_string_type() );
      TEST_ASSERT( v.get_array()[ 1 ].as< std::string >() == "ciao" );

      TEST_ASSERT( a == v );
      TEST_ASSERT( v == a );
      TEST_ASSERT( !( a != v ) );
      TEST_ASSERT( !( v != a ) );
      const type_1 b = { 20, "ciao" };
      const type_1 c = { 19, "hola" };
      TEST_ASSERT( b != c );
      TEST_ASSERT( c != b );
      TEST_ASSERT( a != b );
      TEST_ASSERT( b != a );
      TEST_ASSERT( a != c );
      TEST_ASSERT( c != a );
      TEST_ASSERT( !( b == c ) );
      TEST_ASSERT( !( c == b ) );
      TEST_ASSERT( !( a == b ) );
      TEST_ASSERT( !( b == a ) );
      TEST_ASSERT( !( a == c ) );
      TEST_ASSERT( !( c == a ) );

      const value w = produce::to_value( a );
      TEST_ASSERT( w == v );
      TEST_ASSERT( w == a );
      TEST_ASSERT( a == v );
   }

   void unit_test_13()
   {
      const type_2 a( 20, "hola", false );
      const value v = a;
      TEST_ASSERT( v.is_array() );
      TEST_ASSERT( v.get_array().size() == 3 );
      TEST_ASSERT( v.get_array()[ 0 ].is_integer() );
      TEST_ASSERT( v.get_array()[ 0 ].as< int >() == 20 );
      TEST_ASSERT( v.get_array()[ 1 ].is_string_type() );
      TEST_ASSERT( v.get_array()[ 1 ].as< std::string >() == "hola" );
      TEST_ASSERT( v.get_array()[ 2 ].is_boolean() );
      TEST_ASSERT( v.get_array()[ 2 ].as< bool >() == false );

      TEST_ASSERT( a == v );
      TEST_ASSERT( v == a );
      TEST_ASSERT( !( a != v ) );
      TEST_ASSERT( !( v != a ) );
      const type_2 b( 19, "hola", false );
      const type_2 c( 20, "hola", true );
      TEST_ASSERT( b != c );
      TEST_ASSERT( c != b );
      TEST_ASSERT( a != b );
      TEST_ASSERT( b != a );
      TEST_ASSERT( a != c );
      TEST_ASSERT( c != a );
      TEST_ASSERT( !( b == c ) );
      TEST_ASSERT( !( c == b ) );
      TEST_ASSERT( !( a == b ) );
      TEST_ASSERT( !( b == a ) );
      TEST_ASSERT( !( a == c ) );
      TEST_ASSERT( !( c == a ) );

      const value w = produce::to_value( a );
      TEST_ASSERT( w == v );
      TEST_ASSERT( w == a );
      TEST_ASSERT( a == v );
   }

   void unit_test_14()
   {
      parts_parser p( "[1,\"foo\"]", __FUNCTION__ );
      const auto a = consume< type_1 >( p );
      TEST_ASSERT( a.first == 1 );
      TEST_ASSERT( a.second == "foo" );
   }

   void unit_test_15()
   {
      parts_parser p( "1", __FUNCTION__ );
      TEST_THROWS( consume< type_1 >( p ) );
   }

   void unit_test_16()
   {
      parts_parser p( "[1]", __FUNCTION__ );
      TEST_THROWS( consume< type_1 >( p ) );
   }

   void unit_test_17()
   {
      parts_parser p( "[1,\"foo\",2]", __FUNCTION__ );
      TEST_THROWS( consume< type_1 >( p ) );
   }

   void unit_test_18()
   {
      parts_parser p( "\"foo\"", __FUNCTION__ );
      TEST_THROWS( consume< type_1 >( p ) );
   }

   void unit_test_19()
   {
      parts_parser p( "{}", __FUNCTION__ );
      TEST_THROWS( consume< type_1 >( p ) );
   }

   void unit_test_20()
   {
      type_3 a;
      const value v = a;
      TEST_ASSERT( v.is_array() );
      TEST_ASSERT( v.get_array().size() == 2 );
      TEST_ASSERT( v.get_array()[ 0 ].is_integer() );
      TEST_ASSERT( v.get_array()[ 0 ].as< int >() == 7 );
      TEST_ASSERT( v.get_array()[ 1 ].is_string_type() );
      TEST_ASSERT( v.get_array()[ 1 ].as< std::string >() == "dangerous" );
      const auto w = produce::to_value( a );
      TEST_ASSERT( v == w );
      TEST_ASSERT( a == v );
      TEST_ASSERT( w == a );
   }

   void unit_test_21()
   {
      const auto v = value::array( { 1, true, 90, -5, "abc" } );
      const auto a = v.as< type_4 >();
      TEST_ASSERT( a.i == 1 );
      type_4 b;
      TEST_ASSERT( b.i == 0 );
      v.to( b );
      TEST_ASSERT( b.i == 1 );
      TEST_ASSERT( a == v );
      TEST_ASSERT( v == b );
      TEST_ASSERT( !( a != v ) );
      TEST_ASSERT( !( v != b ) );
   }

   void unit_test_22()
   {
      const auto v = value::array( { 200, true, 90, -5, "abc" } );
      const auto a = v.as< type_4 >();
      TEST_ASSERT( a.i == 200 );
      type_4 b;
      TEST_ASSERT( b.i == 0 );
      v.to( b );
      TEST_ASSERT( b.i == 200 );
      TEST_ASSERT( a == v );
      TEST_ASSERT( v == b );
      TEST_ASSERT( !( a != v ) );
      TEST_ASSERT( !( v != b ) );
   }

   void unit_test_23()
   {
      const auto v = value::array( { 1, false, 90, -5, "abc" } );
      TEST_THROWS( v.as< type_4 >() );
      type_4 a;
      TEST_THROWS( v.to( a ) );
   }

   void unit_test_24()
   {
      const auto v = value::array( { 1, true, 91, -5, "abc" } );
      TEST_THROWS( v.as< type_4 >() );
      type_4 a;
      TEST_THROWS( v.to( a ) );
   }

   void unit_test_25()
   {
      const auto v = value::array( { 1, true, 90, -6, "abc" } );
      TEST_THROWS( v.as< type_4 >() );
      type_4 a;
      TEST_THROWS( v.to( a ) );
   }

   void unit_test_26()
   {
      const auto v = value::array( { 1, true, 90, -5, "abcd" } );
      TEST_THROWS( v.as< type_4 >() );
      type_4 a;
      TEST_THROWS( v.to( a ) );
   }

   void unit_test_27()
   {
      const auto v = value::array( { 1, 1, 90, -5, "abc" } );
      TEST_THROWS( v.as< type_4 >() );
      type_4 a;
      TEST_THROWS( v.to( a ) );
   }

   void unit_test_28()
   {
      const auto v = value::array( { 1, true, 90, -5, "abc", 2 } );
      TEST_THROWS( v.as< type_4 >() );
      type_4 a;
      TEST_THROWS( v.to( a ) );
   }

   void unit_test_29()
   {
      type_4 a;
      a.i = 80;
      const value v = a;
      TEST_ASSERT( a == v );
      TEST_ASSERT( v.is_array() );
      TEST_ASSERT( v.get_array().size() == 5 );
      TEST_ASSERT( v.get_array()[ 0 ].is_integer() );
      TEST_ASSERT( v.get_array()[ 0 ].as< int >() == 80 );
      TEST_ASSERT( v.get_array()[ 1 ].is_boolean() );
      TEST_ASSERT( v.get_array()[ 1 ].as< bool >() == true );
      TEST_ASSERT( v.get_array()[ 2 ].is_integer() );
      TEST_ASSERT( v.get_array()[ 2 ].as< int >() == 90 );
      TEST_ASSERT( v.get_array()[ 3 ].is_integer() );
      TEST_ASSERT( v.get_array()[ 3 ].as< int >() == -5 );
      TEST_ASSERT( v.get_array()[ 4 ].is_string_type() );
      TEST_ASSERT( v.get_array()[ 4 ].as< std::string >() == "abc" );
      const value w = produce::to_value( a );
      TEST_ASSERT( w == a );
      TEST_ASSERT( v == w );
   }

   void unit_test_30()
   {
      parts_parser p( "[7,true,90,-5,\"abc\"]", __FUNCTION__ );
      const auto a = consume< type_4 >( p );
      TEST_ASSERT( a.i == 7 );
   }

   void unit_test_31()
   {
      parts_parser p( "[7,true,90,-5,\"abc\",null]", __FUNCTION__ );
      TEST_THROWS( consume< type_4 >( p ) );
   }

   void unit_test_32()
   {
      parts_parser p( "[7,true,90,-5]", __FUNCTION__ );
      TEST_THROWS( consume< type_4 >( p ) );
   }

   void unit_test_33()
   {
      parts_parser p( "[7,false,90,-5,\"abc\"]", __FUNCTION__ );
      TEST_THROWS( consume< type_4 >( p ) );
   }

   void unit_test_34()
   {
      parts_parser p( "[7,true,91,-5,\"abc\"]", __FUNCTION__ );
      TEST_THROWS( consume< type_4 >( p ) );
   }

   void unit_test_35()
   {
      parts_parser p( "[7,true,90,-6,\"abc\"]", __FUNCTION__ );
      TEST_THROWS( consume< type_4 >( p ) );
   }

   void unit_test_36()
   {
      parts_parser p( "[7,true,90,-5,\"abcd\"]", __FUNCTION__ );
      TEST_THROWS( consume< type_4 >( p ) );
   }

   void unit_test_37()
   {
      parts_parser p( "[7,true,90,-5,90]", __FUNCTION__ );
      TEST_THROWS( consume< type_4 >( p ) );
   }

   void unit_test_38()
   {
      parts_parser p( "[]", __FUNCTION__ );
      TEST_THROWS( consume< type_4 >( p ) );
   }

   void unit_test_39()
   {
      parts_parser p( "null", __FUNCTION__ );
      TEST_THROWS( consume< type_4 >( p ) );
   }

   struct type_a
   {
      int i = 3;
   };

   struct type_b
      : public type_a
   {
      int j = 4;
   };

   template<>
   struct traits< type_a >
      : public binding::array< TAO_JSON_BIND_ELEMENT( &type_a::i ) >
   {};

   template<>
   struct traits< type_b >
      : public binding::array< binding::inherit< traits< type_a > >,
                               TAO_JSON_BIND_ELEMENT( &type_b::j ) >
   {};

   void unit_test_40()
   {
      const value v = from_string( "[ 5, 6 ]" );
      const auto a = v.as< type_b >();
      TEST_ASSERT( a.i == 5 );
      TEST_ASSERT( a.j == 6 );
   }

   struct type_c
      : public type_b
   {
      int k = 5;
   };

   template<>
   struct traits< type_c >
      : public binding::array< TAO_JSON_BIND_ELEMENT( &type_c::k ),
                               binding::inherit< traits< type_b > > >
   {};

   void unit_test_41()
   {
      const value v = from_string( "[ 7, 8, 9 ]" );
      const auto a = v.as< type_c >();
      TEST_ASSERT( a.k == 7 );
      TEST_ASSERT( a.i == 8 );
      TEST_ASSERT( a.j == 9 );
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
      unit_test_0();
      unit_test_10();
      unit_test_11();
      unit_test_12();
      unit_test_13();
      unit_test_14();
      unit_test_15();
      unit_test_16();
      unit_test_17();
      unit_test_18();
      unit_test_19();
      unit_test_20();
      unit_test_21();
      unit_test_22();
      unit_test_23();
      unit_test_24();
      unit_test_25();
      unit_test_26();
      unit_test_27();
      unit_test_28();
      unit_test_29();
      unit_test_30();
      unit_test_31();
      unit_test_32();
      unit_test_33();
      unit_test_34();
      unit_test_35();
      unit_test_36();
      unit_test_37();
      unit_test_38();
      unit_test_39();
      unit_test_40();
      unit_test_41();
   }

}  // namespace tao::json

#include "../../test/json/main.hpp"
