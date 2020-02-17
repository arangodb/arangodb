// Copyright (c) 2018-2020 Dr. Colin Hirsch and Daniel Frey
// Please see LICENSE for license or visit https://github.com/taocpp/json/

#ifndef TAO_JSON_SRC_TEST_JSON_TEST_EVENTS_HPP
#define TAO_JSON_SRC_TEST_JSON_TEST_EVENTS_HPP

#include <tao/json.hpp>

namespace tao::json::test
{
   template< typename Consumer >
   void make_events( Consumer& c )
   {
      c.begin_array();
      {
         c.null();
         c.element();
      }
      {
         c.boolean( true );
         c.element();
      }
      {
         c.boolean( false );
         c.element();
      }
      {
         c.begin_array( 3 );
         {
            c.number( std::int64_t( 0 ) );
            c.element();
         }
         {
            c.number( std::uint64_t( 0 ) );
            c.element();
         }
         {
            c.number( double( 0.0 ) );
            c.element();
         }
         c.end_array( 3 );
      }
      {
         c.number( std::int64_t( 1 ) );
         c.element();
      }
      {
         c.number( std::int64_t( 31 ) );
         c.element();
      }
      {
         c.number( std::int64_t( 32 ) );
         c.element();
      }
      {
         c.number( std::int64_t( 42 ) );
         c.element();
      }
      {
         c.number( std::int64_t( 63 ) );
         c.element();
      }
      {
         c.number( std::int64_t( 64 ) );
         c.element();
      }
      {
         c.number( std::int64_t( 127 ) );
         c.element();
      }
      {
         c.number( std::int64_t( 128 ) );
         c.element();
      }
      {
         c.number( std::int64_t( 255 ) );
         c.element();
      }
      {
         c.number( std::int64_t( 256 ) );
         c.element();
      }
      {
         c.number( std::int64_t( 32767 ) );
         c.element();
      }
      {
         c.number( std::int64_t( 32768 ) );
         c.element();
      }
      {
         c.number( std::int64_t( 65535 ) );
         c.element();
      }
      {
         c.number( std::int64_t( 65536 ) );
         c.element();
      }
      {
         c.number( std::int64_t( 1677215 ) );
         c.element();
      }
      {
         c.number( std::int64_t( 1677216 ) );
         c.element();
      }
      {
         c.number( std::uint64_t( 1 ) );
         c.element();
      }
      {
         c.number( std::uint64_t( 31 ) );
         c.element();
      }
      {
         c.number( std::uint64_t( 32 ) );
         c.element();
      }
      {
         c.number( std::uint64_t( 42 ) );
         c.element();
      }
      {
         c.number( std::uint64_t( 63 ) );
         c.element();
      }
      {
         c.number( std::uint64_t( 64 ) );
         c.element();
      }
      {
         c.number( std::uint64_t( 127 ) );
         c.element();
      }
      {
         c.number( std::uint64_t( 128 ) );
         c.element();
      }
      {
         c.number( std::uint64_t( 255 ) );
         c.element();
      }
      {
         c.number( std::uint64_t( 256 ) );
         c.element();
      }
      {
         c.number( std::uint64_t( 32767 ) );
         c.element();
      }
      {
         c.number( std::uint64_t( 32768 ) );
         c.element();
      }
      {
         c.number( std::uint64_t( 65535 ) );
         c.element();
      }
      {
         c.number( std::uint64_t( 65536 ) );
         c.element();
      }
      {
         c.number( std::uint64_t( 1677215 ) );
         c.element();
      }
      {
         c.number( std::uint64_t( 1677216 ) );
         c.element();
      }
      {
         c.number( std::int64_t( -1 ) );
         c.element();
      }
      {
         c.number( std::int64_t( -31 ) );
         c.element();
      }
      {
         c.number( std::int64_t( -32 ) );
         c.element();
      }
      {
         c.number( std::int64_t( -42 ) );
         c.element();
      }
      {
         c.number( std::int64_t( -63 ) );
         c.element();
      }
      {
         c.number( std::int64_t( -64 ) );
         c.element();
      }
      {
         c.number( std::int64_t( -127 ) );
         c.element();
      }
      {
         c.number( std::int64_t( -128 ) );
         c.element();
      }
      {
         c.number( std::int64_t( -255 ) );
         c.element();
      }
      {
         c.number( std::int64_t( -256 ) );
         c.element();
      }
      {
         c.number( std::int64_t( -32767 ) );
         c.element();
      }
      {
         c.number( std::int64_t( -32768 ) );
         c.element();
      }
      {
         c.number( std::int64_t( -65535 ) );
         c.element();
      }
      {
         c.number( std::int64_t( -65536 ) );
         c.element();
      }
      {
         c.number( std::int64_t( -1677215 ) );
         c.element();
      }
      {
         c.number( std::int64_t( -1677216 ) );
         c.element();
      }
      {
         // TODO: More numbers...
      } {
         c.string( "" );
         c.element();
      }
      {
         c.string( "%" );
         c.element();
      }
      {
         c.string( "$$$" );
         c.element();
      }
      {
         const std::string s = "hallo";
         c.string( s );
         c.element();
      }
      {
         std::string s = "hello";
         c.string( std::move( s ) );
         c.element();
      }
      {
         const std::string s = "howdy";
         const std::string_view t = s;
         c.string( t );
         c.element();
      }
      {
         const std::vector< std::byte > v = { std::byte( 0 ), std::byte( 100 ), std::byte( 200 ) };
         c.binary( v );
         c.element();
      }
      {
         std::vector< std::byte > v = { std::byte( 0 ), std::byte( 100 ), std::byte( 200 ) };
         c.binary( std::move( v ) );
         c.element();
      }
      {
         const std::vector< std::byte > v = { std::byte( 0 ), std::byte( 100 ), std::byte( 200 ) };
         const tao::binary_view t = v;
         c.binary( t );
         c.element();
      }
      {
         c.binary( std::vector< std::byte >() );
         c.element();
      }
      {
         c.begin_array();
         c.end_array();
      }
      {
         c.begin_array( 0 );
         c.end_array( 0 );
      }
      {
         c.begin_object();
         c.end_object();
      }
      {
         c.begin_object( 0 );
         c.end_object( 0 );
      }
      {
         c.begin_object();
         {
            c.key( "foo1" );
            c.number( std::int64_t( 1 ) );
            c.element();
         }
         {
            const std::string s = "foo2";
            c.key( s );
            c.number( std::uint64_t( 2 ) );
            c.element();
         }
         {
            std::string s = "foo3";
            c.key( std::move( s ) );
            c.number( std::int64_t( 3 ) );
            c.element();
         }
         {
            const std::string s = "foo4";
            const std::string_view v = s;
            c.key( v );
            c.number( std::uint64_t( 4 ) );
            c.element();
         }
         c.end_object();
      }
      {
         c.begin_object( 4 );
         {
            c.key( "foo1" );
            c.number( std::int64_t( -1 ) );
            c.element();
         }
         {
            const std::string s = "foo2";
            c.key( s );
            c.number( std::int64_t( -2 ) );
            c.element();
         }
         {
            std::string s = "foo3";
            c.key( std::move( s ) );
            c.number( std::int64_t( -3 ) );
            c.element();
         }
         {
            const std::string s = "foo4";
            const std::string_view v = s;
            c.key( v );
            c.number( std::int64_t( -4 ) );
            c.element();
         }
         c.end_object( 4 );
      }
      c.end_array();
   }

}  // namespace tao::json::test

#endif
