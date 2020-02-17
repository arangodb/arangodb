// Copyright (c) 2019-2020 Dr. Colin Hirsch and Daniel Frey
// Please see LICENSE for license or visit https://github.com/taocpp/json/

#include <tao/json.hpp>

#include "bench_mark.hpp"

int main( int argc, char** argv )
{
   for( int i = 1; i < argc; ++i ) {
      tao::json::events::to_value consumer;
      tao::json::events::from_file( consumer, argv[ i ] );
      tao::json::events::to_value consumer2;
      tao::json::events::from_file( consumer2, argv[ i ] );

      tao::bench::mark( "parse", argv[ i ], [&]() {
         tao::json::events::to_value tmp;
         tao::json::events::from_file( tmp, argv[ i ] );
      } );

      tao::bench::mark( "compare", argv[ i ], [&]() {
         (void)( consumer.value == consumer2.value );
      } );

      tao::bench::mark( "stringify", argv[ i ], [&]() {
         (void)tao::json::to_string( consumer.value );
      } );

      tao::bench::mark( "prettify", argv[ i ], [&]() {
         (void)tao::json::to_string( consumer.value, 4 );
      } );
   }
   return 0;
}
