// Copyright (c) 2018-2020 Dr. Colin Hirsch and Daniel Frey
// Please see LICENSE for license or visit https://github.com/taocpp/json/

#include <limits>

#include "test.hpp"

#include <tao/json.hpp>

namespace tao::json
{
   struct type_1
   {};

   template<>
   struct traits< type_1 >
   {
      TAO_JSON_DEFAULT_KEY( "type_1" );

      static void assign( value& v, const type_1 /*unused*/ )
      {
         v = 42;
      }
   };

   void unit_test()
   {
      {
         const std::optional< std::string > f;
         value v = f;
         TEST_ASSERT( v == f );
         TEST_ASSERT( v.is_null() );
         const auto g = v.as< std::optional< std::string > >();
         TEST_ASSERT( g == f );
      }
      {
         const std::optional< std::string > f( "hallo" );
         value v = f;
         TEST_ASSERT( v == f );
         TEST_ASSERT( v.is_string() );
         TEST_ASSERT( v == "hallo" );
         TEST_ASSERT( v.get_string() == "hallo" );
         const auto g = v.as< std::optional< std::string > >();
         TEST_ASSERT( *g == *f );
      }
      {
         const std::optional< int > i;
         const value v = null;
         TEST_ASSERT( i == v );
      }
      {
         const type_1 a;
         const value v = { a };
         TEST_ASSERT( v.is_object() );
         TEST_ASSERT( v.get_object().size() == 1 );
         TEST_ASSERT( v.get_object().begin()->first == "type_1" );
         TEST_ASSERT( v.get_object().begin()->second.as< int >() == 42 );
      }
      {
         const std::optional< type_1 > a = type_1();
         const value v = { a };
         TEST_ASSERT( v.is_object() );
         TEST_ASSERT( v.get_object().size() == 1 );
         TEST_ASSERT( v.get_object().begin()->first == "type_1" );
         TEST_ASSERT( v.get_object().begin()->second.as< int >() == 42 );
      }
      {
         const std::optional< std::optional< type_1 > > a = std::optional< type_1 >( type_1() );
         const value v = { a };
         TEST_ASSERT( v.is_object() );
         TEST_ASSERT( v.get_object().size() == 1 );
         TEST_ASSERT( v.get_object().begin()->first == "type_1" );
         TEST_ASSERT( v.get_object().begin()->second.as< int >() == 42 );
      }
   }

}  // namespace tao::json

#include "main.hpp"
