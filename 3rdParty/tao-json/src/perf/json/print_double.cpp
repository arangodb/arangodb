// Copyright (c) 2018-2020 Dr. Colin Hirsch and Daniel Frey
// Please see LICENSE for license or visit https://github.com/taocpp/json/

#include <tao/json.hpp>

#include "bench_mark.hpp"

int main( int /*unused*/, char** /*unused*/ )
{
   char b[ 28 ];

   tao::bench::mark( "pd", "0.0", [&]() { tao::json::ryu::d2s_finite( 0.0, b ); } );

   tao::bench::mark( "pd", "1.0", [&]() { tao::json::ryu::d2s_finite( 3.0, b ); } );
   tao::bench::mark( "pd", "123456789.0", [&]() { tao::json::ryu::d2s_finite( 123456789.0, b ); } );
   tao::bench::mark( "pd", "12345678987654321.0", [&]() { tao::json::ryu::d2s_finite( 12345678987654321.0, b ); } );

   tao::bench::mark( "pd", "10000000000000000000.0", [&]() { tao::json::ryu::d2s_finite( 10000000000000000000.0, b ); } );
   tao::bench::mark( "pd", "12345678900000000000.0", [&]() { tao::json::ryu::d2s_finite( 12345678900000000000.0, b ); } );
   tao::bench::mark( "pd", "12345678987654321000.0", [&]() { tao::json::ryu::d2s_finite( 12345678987654321000.0, b ); } );

   tao::bench::mark( "pd", "0.0001", [&]() { tao::json::ryu::d2s_finite( 0.0001, b ); } );
   tao::bench::mark( "pd", "0.000123456789", [&]() { tao::json::ryu::d2s_finite( 0.000123456789, b ); } );
   tao::bench::mark( "pd", "0.00012345678987654321", [&]() { tao::json::ryu::d2s_finite( 0.00012345678987654321, b ); } );

   tao::bench::mark( "pd", "1.1", [&]() { tao::json::ryu::d2s_finite( 1.1, b ); } );
   tao::bench::mark( "pd", "1234567.1234567", [&]() { tao::json::ryu::d2s_finite( 1234567.1234567, b ); } );

   tao::bench::mark( "pd", "1e200", [&]() { tao::json::ryu::d2s_finite( 1e200, b ); } );
   tao::bench::mark( "pd", "123456789e200", [&]() { tao::json::ryu::d2s_finite( 123456789e200, b ); } );
   tao::bench::mark( "pd", "12345678987654321e200", [&]() { tao::json::ryu::d2s_finite( 12345678987654321e200, b ); } );

   return 0;
}
