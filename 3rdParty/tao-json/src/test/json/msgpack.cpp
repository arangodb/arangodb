// Copyright (c) 2017-2020 Dr. Colin Hirsch and Daniel Frey
// Please see LICENSE for license or visit https://github.com/taocpp/json/

#include "test.hpp"
#include "test_unhex.hpp"

#include <tao/json/from_string.hpp>
#include <tao/json/to_string.hpp>

#include <tao/json/msgpack.hpp>

namespace tao::json
{
   void msgpack_encode( const std::string& text, const std::string& data )
   {
      TEST_ASSERT( msgpack::to_string( from_string( text ) ) == test_unhex( data ) );
   }

   void msgpack_decode( const std::string& data, const std::string& text )
   {
      TEST_ASSERT( to_string( msgpack::from_string( test_unhex( data ) ) ) == to_string( from_string( text ) ) );
   }

   void unit_test()
   {
      msgpack_encode( "null", "c0" );
      msgpack_encode( "true", "c3" );
      msgpack_encode( "false", "c2" );
      msgpack_encode( "\"a\"", "a161" );
      msgpack_encode( "\"ab\"", "a26162" );
      msgpack_encode( "[]", "90" );
      msgpack_encode( "{}", "80" );
      msgpack_encode( "[null]", "91c0" );
      msgpack_encode( "{\"a\":true,\"b\":false}", "82a161c3a162c2" );
      msgpack_encode( "\"000\"", "a3303030" );
      msgpack_encode( "\"01234567890123456789\"", "b43031323334353637383930313233343536373839" );
      msgpack_encode( "\"0123456789012345678901234567890123456789\"", "d92830313233343536373839303132333435363738393031323334353637383930313233343536373839" );
      msgpack_encode( "\"" + std::string( std::size_t( 500 ), 'f' ) + "\"", "da01f4" + std::string( std::size_t( 1000 ), '6' ) );
      msgpack_encode( "\"" + std::string( std::size_t( 70000 ), 'f' ) + "\"", "db00011170" + std::string( std::size_t( 140000 ), '6' ) );
      msgpack_encode( "0", "00" );
      msgpack_encode( "1", "01" );
      msgpack_encode( "127", "7f" );
      msgpack_encode( "128", "cc80" );
      msgpack_encode( "255", "ccff" );
      msgpack_encode( "256", "cd0100" );
      msgpack_encode( "32767", "cd7fff" );
      msgpack_encode( "32768", "cd8000" );
      msgpack_encode( "65535", "cdffff" );
      msgpack_encode( "2147483647", "ce7fffffff" );
      msgpack_encode( "2147483648", "ce80000000" );
      msgpack_encode( "4294967295", "ceffffffff" );
      msgpack_encode( "9223372036854775807", "cf7fffffffffffffff" );
      msgpack_encode( "9223372036854775808", "cf8000000000000000" );
      msgpack_encode( "-1", "ff" );
      msgpack_encode( "-32", "e0" );
      msgpack_encode( "-128", "d080" );
      msgpack_encode( "-129", "d1ff7f" );
      msgpack_encode( "-32768", "d18000" );
      msgpack_encode( "-32769", "d2ffff7fff" );
      msgpack_encode( "-2147483648", "d280000000" );
      msgpack_encode( "-2147483649", "d3ffffffff7fffffff" );
      msgpack_encode( "-9223372036854775808", "d38000000000000000" );
      msgpack_encode( "3.14159", "cb400921f9f01b866e" );
      msgpack_encode( "-4.2777e-12", "cbbd92d043147f15ff" );

      msgpack_decode( "00", "0" );
      msgpack_decode( "01", "1" );
      msgpack_decode( "40", "64" );
      msgpack_decode( "7f", "127" );
      msgpack_decode( "ff", "-1" );
      msgpack_decode( "f0", "-16" );

      msgpack_decode( "c0", "null" );
      msgpack_decode( "c2", "false" );
      msgpack_decode( "c3", "true" );

      msgpack_decode( "cc00", "0" );
      msgpack_decode( "ccff", "255" );

      msgpack_decode( "cd0000", "0" );
      msgpack_decode( "cd00ff", "255" );
      msgpack_decode( "cdfffe", "65534" );

      msgpack_decode( "ce00000000", "0" );
      msgpack_decode( "ce000000ff", "255" );
      msgpack_decode( "cefffffffe", "4294967294" );

      msgpack_decode( "cf0000000000000000", "0" );
      msgpack_decode( "cf00000000000000ff", "255" );
      msgpack_decode( "cffffffffffffffffe", "18446744073709551614" );

      msgpack_decode( "d000", "0" );
      msgpack_decode( "d0ff", "-1" );
      msgpack_decode( "d080", "-128" );
      msgpack_decode( "d07f", "127" );

      msgpack_decode( "d10000", "0" );
      msgpack_decode( "d1ffff", "-1" );
      msgpack_decode( "d18000", "-32768" );
      msgpack_decode( "d17fff", "32767" );

      msgpack_decode( "d200000000", "0" );
      msgpack_decode( "d2ffffffff", "-1" );
      msgpack_decode( "d280000000", "-2147483648" );
      msgpack_decode( "d27fffffff", "2147483647" );

      msgpack_decode( "d30000000000000000", "0" );
      msgpack_decode( "d3ffffffffffffffff", "-1" );
      msgpack_decode( "d38000000000000000", "-9223372036854775808" );
      msgpack_decode( "d37fffffffffffffff", "9223372036854775807" );

      msgpack_decode( "ca402df84d", to_string( value( 2.71828F ) ) );
      msgpack_decode( "ca9607d3a8", to_string( value( -1.0972e-25F ) ) );

      msgpack_decode( "cb400921f9f01b866e", "3.14159" );
      msgpack_decode( "cbbd92d043147f15ff", "-4.2777e-12" );

      msgpack_decode( "d900", "\"\"" );
      msgpack_decode( "d90130", "\"0\"" );
      msgpack_decode( "d90430313233", "\"0123\"" );
      msgpack_decode( "da0000", "\"\"" );
      msgpack_decode( "da000139", "\"9\"" );
      msgpack_decode( "db00000000", "\"\"" );
      msgpack_decode( "db0000000138", "\"8\"" );

      msgpack_decode( "da01f4" + std::string( std::size_t( 1000 ), '6' ), "\"" + std::string( std::size_t( 500 ), 'f' ) + "\"" );
      msgpack_decode( "db00011170" + std::string( std::size_t( 140000 ), '6' ), "\"" + std::string( std::size_t( 70000 ), 'f' ) + "\"" );

      msgpack_decode( "90", "[]" );
      msgpack_decode( "80", "{}" );
      msgpack_decode( "82a161c3a162c2", "{\"a\":true,\"b\":false}" );
   }

}  // namespace tao::json

#include "main.hpp"
