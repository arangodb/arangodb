// Copyright (c) 2018-2020 Dr. Colin Hirsch and Daniel Frey
// Please see LICENSE for license or visit https://github.com/taocpp/json/

#include "test.hpp"

#include <tao/json.hpp>

namespace tao::json
{
   using type_1 = std::pair< int, std::string >;

   struct version_a
      : binding::array< TAO_JSON_BIND_ELEMENT( &type_1::first ),
                        TAO_JSON_BIND_ELEMENT( &type_1::second ) >
   {};

   struct version_b
      : binding::array< TAO_JSON_BIND_ELEMENT( &type_1::first ),
                        TAO_JSON_BIND_ELEMENT( &type_1::second ),
                        binding::element_b< true > >
   {};

   template<>
   struct traits< type_1 >
      : binding::versions< version_b, version_a >
   {
      TAO_JSON_DEFAULT_KEY( "t" );
   };

   void unit_test_1()
   {
      const value v = value::array( { 42, "florida arklab" } );
      const auto a = v.as< type_1 >();
      TEST_ASSERT( a.first == 42 );
      TEST_ASSERT( a.second == "florida arklab" );
   }

   void unit_test_2()
   {
      const value v = value::array( { 42, "florida arklab", true } );
      const auto a = v.as< type_1 >();
      TEST_ASSERT( a.first == 42 );
      TEST_ASSERT( a.second == "florida arklab" );
      const value w = a;
      TEST_ASSERT( w == v );
      const value x = produce::to_value( a );
      TEST_ASSERT( x == w );
      TEST_ASSERT( v == x );
   }

   void unit_test_3()
   {
      const value v = value::array( { 42, "florida arklab", false } );
      TEST_THROWS( v.as< type_1 >() );
      const value w = value::array( { true, "florida arklab", true } );
      TEST_THROWS( w.as< type_1 >() );
      const value x = value::array( { 42, "florida arklab", true, null } );
      TEST_THROWS( x.as< type_1 >() );
   }

   void unit_test_4()
   {
      parts_parser p( "[ 17, \"valley forge\" ]", __FUNCTION__ );
      const auto a = consume< type_1 >( p );
      TEST_ASSERT( a.first == 17 );
      TEST_ASSERT( a.second == "valley forge" );
   }

   void unit_test_5()
   {
      parts_parser p( "[ 17, \"valley forge\", true ]", __FUNCTION__ );
      const auto a = consume< type_1 >( p );
      TEST_ASSERT( a.first == 17 );
      TEST_ASSERT( a.second == "valley forge" );
   }

   void unit_test_6()
   {
      parts_parser p( "[ 17, \"valley forge\", true, false ]", __FUNCTION__ );
      TEST_THROWS( consume< type_1 >( p ) );
   }

   void unit_test()
   {
      unit_test_1();
      unit_test_2();
      unit_test_3();
      unit_test_4();
      unit_test_5();
      unit_test_6();
   }

}  // namespace tao::json

#include "../../test/json/main.hpp"
