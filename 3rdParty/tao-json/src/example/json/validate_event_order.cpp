// Copyright (c) 2018-2020 Dr. Colin Hirsch and Daniel Frey
// Please see LICENSE for license or visit https://github.com/taocpp/json/

#include <cstddef>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <utility>

#include <tao/json/events/from_file.hpp>
#include <tao/json/events/from_value.hpp>
#include <tao/json/events/to_value.hpp>
#include <tao/json/events/validate_event_order.hpp>

int main( int argc, char** argv )
{
   for( int i = 1; i < argc; ++i ) {
      tao::json::events::validate_event_order c1;
      tao::json::events::from_file( c1, argv[ i ] );
      tao::json::events::to_value c2;
      tao::json::events::from_file( c2, argv[ i ] );
      tao::json::events::validate_event_order c3;
      tao::json::events::from_value( c3, c2.value );
   }
   return 0;
}
