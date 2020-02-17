// Copyright (c) 2018-2020 Dr. Colin Hirsch and Daniel Frey
// Please see LICENSE for license or visit https://github.com/taocpp/json/

#include <tao/json.hpp>
#include <tao/json/external/pegtl/contrib/json.hpp>

#include "bench_mark.hpp"

namespace pegtl = tao::json::pegtl;

// clang-format off

int main( int argc, char** argv )
{
   for( int i = 1; i < argc; ++i ) {
      const auto r = tao::bench::mark( "pegtl", argv[ i ], [&]() {
         pegtl::file_input< pegtl::tracking_mode::lazy > in( argv[ i ] );
         pegtl::parse< pegtl::must< pegtl::json::text, pegtl::eof > >( in );
      } );
      tao::bench::mark( "json", argv[ i ], [&]() {
         tao::json::events::discard consumer;
         tao::json::events::from_file( consumer, argv[ i ] );
      },
      r );
   }
   return 0;
}
