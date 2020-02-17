// Copyright (c) 2018-2020 Dr. Colin Hirsch and Daniel Frey
// Please see LICENSE for license or visit https://github.com/taocpp/json/

#include "test.hpp"

#include <tao/json.hpp>

namespace tao::json
{
   void unit_test_1()
   {
      {
         jaxn::parts_parser p( "", __FUNCTION__ );
         TEST_ASSERT( p.empty() );
      }
      {
         jaxn::parts_parser p( "   \t", __FUNCTION__ );
         TEST_ASSERT( p.empty() );
      }
      {
         jaxn::parts_parser p( " [ true, false, null ] ", __FUNCTION__ );
         auto a = p.begin_array();
         p.element( a );
         TEST_ASSERT( p.null() == false );
         TEST_ASSERT( p.boolean() == true );
         p.element( a );
         TEST_ASSERT( p.null() == false );
         TEST_ASSERT( p.boolean() == false );
         p.element( a );
         TEST_ASSERT( p.null() == true );
         p.end_array( a );
         TEST_ASSERT( p.empty() );
      }
      {
         jaxn::parts_parser p( "42", __FUNCTION__ );
         TEST_ASSERT( p.number_unsigned() == 42 );
         TEST_ASSERT( p.empty() );
      }
      {
         jaxn::parts_parser p( "42", __FUNCTION__ );
         TEST_ASSERT( p.number_signed() == 42 );
         TEST_ASSERT( p.empty() );
      }
      {
         jaxn::parts_parser p( "+42", __FUNCTION__ );
         TEST_ASSERT( p.number_signed() == 42 );
         TEST_ASSERT( p.empty() );
      }
      {
         jaxn::parts_parser p( "-42", __FUNCTION__ );
         TEST_ASSERT( p.number_signed() == -42 );
         TEST_ASSERT( p.empty() );
      }
      {
         jaxn::parts_parser p( "0x1234", __FUNCTION__ );
         TEST_ASSERT( p.number_unsigned() == 0x1234 );
         TEST_ASSERT( p.empty() );
      }
      {
         jaxn::parts_parser p( "0x1234", __FUNCTION__ );
         TEST_ASSERT( p.number_signed() == 0x1234 );
         TEST_ASSERT( p.empty() );
      }
      {
         jaxn::parts_parser p( "+0x1234", __FUNCTION__ );
         TEST_ASSERT( p.number_signed() == 0x1234 );
         TEST_ASSERT( p.empty() );
      }
      {
         jaxn::parts_parser p( "-0x1234", __FUNCTION__ );
         TEST_ASSERT( p.number_signed() == -0x1234 );
         TEST_ASSERT( p.empty() );
      }
      {
         jaxn::parts_parser p( "0.0", __FUNCTION__ );
         TEST_ASSERT( p.number_double() == 0.0 );
         TEST_ASSERT( p.empty() );
      }
      {
         jaxn::parts_parser p( "+0.0", __FUNCTION__ );
         TEST_ASSERT( p.number_double() == 0.0 );
         TEST_ASSERT( p.empty() );
      }
      {
         jaxn::parts_parser p( "-0.0", __FUNCTION__ );
         TEST_ASSERT( p.number_double() == -0.0 );
         TEST_ASSERT( p.empty() );
      }
      {
         jaxn::parts_parser p( "100.45", __FUNCTION__ );
         TEST_ASSERT( p.number_double() == 100.45 );
         TEST_ASSERT( p.empty() );
      }
      {
         jaxn::parts_parser p( "+100.45", __FUNCTION__ );
         TEST_ASSERT( p.number_double() == 100.45 );
         TEST_ASSERT( p.empty() );
      }
      {
         jaxn::parts_parser p( "-100.45", __FUNCTION__ );
         TEST_ASSERT( p.number_double() == -100.45 );
         TEST_ASSERT( p.empty() );
      }
      {
         jaxn::parts_parser p( "12345", __FUNCTION__ );
         TEST_ASSERT( p.number_double() == 12345.0 );
         TEST_ASSERT( p.empty() );
      }
      {
         jaxn::parts_parser p( "+12345", __FUNCTION__ );
         TEST_ASSERT( p.number_double() == 12345.0 );
         TEST_ASSERT( p.empty() );
      }
      {
         jaxn::parts_parser p( "-12345", __FUNCTION__ );
         TEST_ASSERT( p.number_double() == -12345.0 );
         TEST_ASSERT( p.empty() );
      }
      {
         jaxn::parts_parser p( "NaN", __FUNCTION__ );
         TEST_ASSERT( std::isnan( p.number_double() ) );
         TEST_ASSERT( p.empty() );
      }
      {
         jaxn::parts_parser p( "Infinity", __FUNCTION__ );
         TEST_ASSERT( p.number_double() == INFINITY );
         TEST_ASSERT( p.empty() );
      }
      {
         jaxn::parts_parser p( "+Infinity", __FUNCTION__ );
         TEST_ASSERT( p.number_double() == INFINITY );
         TEST_ASSERT( p.empty() );
      }
      {
         jaxn::parts_parser p( "-Infinity", __FUNCTION__ );
         TEST_ASSERT( p.number_double() == -INFINITY );
         TEST_ASSERT( p.empty() );
      }
      {
         jaxn::parts_parser p( "$34.00.32", __FUNCTION__ );
         const std::vector< std::byte > z = { std::byte( 0x34 ), std::byte( 0x00 ), std::byte( 0x32 ) };
         TEST_ASSERT( p.binary() == z );
         TEST_ASSERT( p.empty() );
      }
   }

   void unit_test_2()
   {
      {
         jaxn::parts_parser p( " [ true , -42.7 , false , null ] ", __FUNCTION__ );
         TEST_ASSERT( p.null() == false );
         auto a = p.begin_array();
         p.element( a );
         TEST_ASSERT( p.boolean() == true );
         TEST_ASSERT( p.null() == false );
         p.element( a );
         TEST_ASSERT( p.number_double() == -42.7 );
         TEST_ASSERT( p.null() == false );
         p.element( a );
         TEST_ASSERT( p.null() == false );
         TEST_ASSERT( p.boolean() == false );
         p.element( a );
         TEST_ASSERT( p.null() == true );
         p.end_array( a );
         TEST_ASSERT( p.empty() );
      }
      {
         std::vector< std::uint64_t > r;
         jaxn::parts_parser p( "[1,2,3,4] ", __FUNCTION__ );
         TEST_ASSERT( p.null() == false );
         auto a = p.begin_array();
         while( p.element_or_end_array( a ) ) {
            r.emplace_back( p.number_unsigned() );
         }
         TEST_ASSERT( p.empty() );
         const std::vector< std::uint64_t > z = { 1, 2, 3, 4 };
         TEST_ASSERT( r == z );
      }
      {
         jaxn::parts_parser p( " { \"a\" : 1, \"b\" : 2, \"c\" : null } ", __FUNCTION__ );
         TEST_ASSERT( p.null() == false );
         auto a = p.begin_object();
         p.member( a );
         TEST_ASSERT( p.key() == "a" );
         TEST_ASSERT( p.null() == false );
         TEST_ASSERT( p.number_signed() == 1 );
         p.member( a );
         TEST_ASSERT( p.key() == "b" );
         TEST_ASSERT( p.null() == false );
         TEST_ASSERT( p.number_unsigned() == 2 );
         p.member( a );
         TEST_ASSERT( p.key() == "c" );
         TEST_ASSERT( p.null() == true );
         p.end_object( a );
         TEST_ASSERT( p.empty() );
      }
      {
         std::map< std::string, std::uint64_t > r;
         jaxn::parts_parser p( "{\"h\":1,\"c\":4,\"f\":2,\"d\":3} ", __FUNCTION__ );
         TEST_ASSERT( p.null() == false );
         auto a = p.begin_object();
         while( p.member_or_end_object( a ) ) {
            auto k = p.key();
            r.try_emplace( std::move( k ), p.number_unsigned() );
         }
         TEST_ASSERT( p.empty() );
         const std::map< std::string, std::uint64_t > z = { { "c", 4 }, { "d", 3 }, { "f", 2 }, { "h", 1 } };
         TEST_ASSERT( r == z );
      }
   }

   void unit_test()
   {
      unit_test_1();
      unit_test_2();
   }

}  // namespace tao::json

#include "main.hpp"
