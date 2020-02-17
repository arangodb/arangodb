// Copyright (c) 2018-2020 Dr. Colin Hirsch and Daniel Frey
// Please see LICENSE for license or visit https://github.com/taocpp/json/

#ifndef TAO_JSON_SRC_PERF_JSON_BENCH_MARK_HPP
#define TAO_JSON_SRC_PERF_JSON_BENCH_MARK_HPP

#include <chrono>
#include <cstddef>
#include <functional>
#include <iomanip>
#include <iostream>

namespace tao::bench
{
   inline std::uint64_t mark( const std::string& name, const std::string& type, const std::function< void() >& func, const std::uint64_t ref = 0 )
   {
      func();

      using clock = std::chrono::high_resolution_clock;

      for( std::uint64_t r = 0; r < 42; ++r ) {
         const std::uint64_t c = std::uint64_t( 1 ) << r;
         const auto start = clock::now();

         for( std::uint64_t i = 0; i < c; ++i ) {
            func();
         }
         const auto finish = clock::now();
         const std::uint64_t e = std::chrono::duration_cast< std::chrono::nanoseconds >( finish - start ).count();

         if( e > 1000000000 ) {
            const auto q = e / c;
            std::cout << "bench '" << std::setw( 10 ) << name << "' with '" << type << "':  iterations " << std::setw( 10 ) << c << "   elapsed " << std::setw( 10 ) << e << "   result " << std::setw( 10 ) << q << " nanos per iteration (" << ( ( 100 * q ) / ( bool( ref ) ? ref : q ) ) << "%)" << std::endl;
            return q;
         }
      }
      std::cout << " bench '" << name << "' did not complete" << std::endl;
      return 0;
   }

}  // namespace tao::bench

#endif
