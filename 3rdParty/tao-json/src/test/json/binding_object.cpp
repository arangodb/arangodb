// Copyright (c) 2018-2020 Dr. Colin Hirsch and Daniel Frey
// Please see LICENSE for license or visit https://github.com/taocpp/json/

#include "test.hpp"

#include <tao/json.hpp>

namespace tao::json
{
   using type_0 = std::pair< int, int >;

   template<>
   struct traits< type_0 >
      : binding::array< TAO_JSON_BIND_ELEMENT( &type_0::first ),
                        TAO_JSON_BIND_ELEMENT( &type_0::second ) >
   {
      TAO_JSON_DEFAULT_KEY( "z" );
   };

   struct type_1
   {
      int i = 8000;
      std::string s;
      std::optional< bool > b;
      std::optional< double > d;
      type_0 z;
   };

   template<>
   struct traits< type_1 >
      : binding::object< TAO_JSON_BIND_REQUIRED( "i", &type_1::i ),
                         TAO_JSON_BIND_OPTIONAL( "s", &type_1::s ),
                         TAO_JSON_BIND_REQUIRED( "b", &type_1::b ),
                         TAO_JSON_BIND_OPTIONAL( "d", &type_1::d ),
                         TAO_JSON_BIND_REQUIRED1( &type_1::z ) >
   {};

   void unit_test_1()
   {
      const value v = {
         { "i", 42 },
         { "s", "foo" },
         { "b", true },
         { "d", 43.1 },
         { "z", value::array( { 5, 6 } ) }
      };
      const auto a = v.as< type_1 >();
      TEST_ASSERT( a.i == 42 );
      TEST_ASSERT( a.s == "foo" );
      TEST_ASSERT( a.b && ( *a.b == true ) );
      TEST_ASSERT( a.d && ( *a.d == 43.1 ) );
      TEST_ASSERT( a.z.first == 5 );
      TEST_ASSERT( a.z.second == 6 );
      TEST_ASSERT( a == v );
      TEST_ASSERT( value( a ) == v );
      TEST_ASSERT( v == a );
      TEST_ASSERT( v == value( a ) );
      TEST_ASSERT( !( a != v ) );
      TEST_ASSERT( !( v != a ) );
      type_1 b;
      v.to( b );
      TEST_ASSERT( b.i == 42 );
      TEST_ASSERT( b.s == "foo" );
      TEST_ASSERT( b.b && ( *a.b == true ) );
      TEST_ASSERT( b.d && ( *a.d == 43.1 ) );
      TEST_ASSERT( b.z.first == 5 );
      TEST_ASSERT( b.z.second == 6 );
      auto w = v;
      w[ "b" ] = false;
      TEST_ASSERT( w != a );
      TEST_ASSERT( w != v );
      TEST_ASSERT( value( null ) != a );
      TEST_ASSERT( value( null ) != v );
      TEST_ASSERT( value( null ) != w );
      b = consume_string< type_1 >( R"( { "i" : 42, "s" : "foo", "b" : true, "d" : 43.1, "z" : [ 5, 6 ] } )" );
      TEST_ASSERT( v == b );
      TEST_THROWS( consume_string< type_1 >( R"( { "s" : "foo", "b" : true, "d" : 43.1, "z" : [ 5, 6 ] } )" ) );
      TEST_THROWS( consume_string< type_1 >( R"( { "i" : 42, "s" : "foo", "b" : true, "e" : 43.1, "z" : [ 5, 6 ] } )" ) );
      TEST_THROWS( consume_string< type_1 >( R"( { "i" : 42, "s" : "foo", "b" : true, "d" : 43.1, "z" : [ 5, 6 ], "i" : 43 } )" ) );
   }

   void unit_test_2()
   {
      const value v = {
         { "i", 42 },
         { "b", true },
         { "z", value::array( { 5, 6 } ) }
      };
      const auto a = v.as< type_1 >();
      TEST_ASSERT( a.i == 42 );
      TEST_ASSERT( a.s.empty() );
      TEST_ASSERT( a.b && ( *a.b == true ) );
      TEST_ASSERT( !a.d );
      TEST_ASSERT( a.z.first == 5 );
      TEST_ASSERT( a.z.second == 6 );
      value w = v;
      w[ "s" ] = "";
      w[ "d" ] = null;  // Must compare equal without this entry when using for_nothing_value::suppress.
      TEST_ASSERT( a == w );
      TEST_ASSERT( w == a );
      TEST_ASSERT( !( a != w ) );
      TEST_ASSERT( !( w != a ) );
      type_1 b;
      b.s = "hallo";
      v.to( b );
      TEST_ASSERT( b.i == 42 );
      TEST_ASSERT( b.s == "hallo" );
      TEST_ASSERT( b.b && ( *b.b == true ) );
      TEST_ASSERT( !b.d );
      TEST_ASSERT( b.z.first == 5 );
      TEST_ASSERT( b.z.second == 6 );
      w.get_object().erase( "i" );
      TEST_THROWS( w.as< type_1 >() );
      w[ "i" ] = 42;
      b = w.as< type_1 >();
      w[ "j" ] = 43;
      TEST_THROWS( w.as< type_1 >() );
   }

   void unit_test_3()
   {
      type_1 a;
      a.i = 90;
      a.s = "bar";
      a.b = false;
      a.d = 44.2;
      a.z.first = 8;
      a.z.second = 9;
      const value v = a;
      TEST_ASSERT( v.is_object() );
      TEST_ASSERT( v.at( "i" ).is_integer() );
      TEST_ASSERT( v.as< int >( "i" ) == 90 );
      TEST_ASSERT( v.at( "s" ).is_string_type() );
      TEST_ASSERT( v.as< std::string >( "s" ) == "bar" );
      TEST_ASSERT( v.at( "b" ).is_boolean() );
      TEST_ASSERT( v.as< bool >( "b" ) == false );
      TEST_ASSERT( v.at( "d" ).is_double() );
      TEST_ASSERT( v.as< double >( "d" ) == 44.2 );
      TEST_ASSERT( v.at( "z" ).is_array() );
      TEST_ASSERT( v.at( "z" ).get_array().size() == 2 );
      TEST_ASSERT( v.at( "z" ).get_array()[ 0 ].as< int >() == 8 );
      TEST_ASSERT( v.at( "z" ).get_array()[ 1 ].as< int >() == 9 );
      const value w = produce::to_value( a );
      TEST_ASSERT( v == w );
   }

   struct type_3
   {
      int i = 3;
   };

   struct type_4
      : public type_3
   {
      int j = 4;
   };

   template<>
   struct traits< type_3 >
      : public binding::object< TAO_JSON_BIND_REQUIRED( "i", &type_3::i ) >
   {};

   template<>
   struct traits< type_4 >
      : public binding::object< binding::inherit< traits< type_3 > >,
                                TAO_JSON_BIND_REQUIRED( "j", &type_4::j ) >
   {};

   void unit_test_4()
   {
      const value v = from_string( "{ \"i\": 5, \"j\": 6 }" );
      const auto a = v.as< type_4 >();
      TEST_ASSERT( a.i == 5 );
      TEST_ASSERT( a.j == 6 );
   }

   struct type_5
   {};

   template<>
   struct traits< type_5 >
      : public binding::object< TAO_JSON_BIND_REQUIRED_BOOL( "bool", true ),
                                TAO_JSON_BIND_REQUIRED_SIGNED( "signed", -4 ),
                                TAO_JSON_BIND_REQUIRED_UNSIGNED( "unsigned", 32 ),
                                TAO_JSON_BIND_REQUIRED_STRING( "string", "servus" ) >
   {};

   void unit_test_5()
   {
      const value v = from_string( R"({ "bool" : true, "signed" : -4, "unsigned" : 32, "string" : "servus" })" );
      (void)v.as< type_5 >();
   }

   // TODO: Test with different for_unknown_key.
   // TODO: Test with different for_nothing_value (incl. consistency of size to consumer).

   void unit_test()
   {
      unit_test_1();
      unit_test_2();
      unit_test_3();
      unit_test_4();
      unit_test_5();
   }

}  // namespace tao::json

#include "../../test/json/main.hpp"
