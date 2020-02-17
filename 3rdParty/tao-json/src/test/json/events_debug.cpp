// Copyright (c) 2016-2020 Dr. Colin Hirsch and Daniel Frey
// Please see LICENSE for license or visit https://github.com/taocpp/json/

#include "test.hpp"

#include <tao/json/events/debug.hpp>
#include <tao/json/events/from_string.hpp>

namespace tao::json
{
   void test( const std::string& v )
   {
      events::debug consumer( std::cout );
      events::from_string( consumer, v );
   }

   void unit_test()
   {
      test( "[null,true,false,-42,43.0,\"foo\",[1,2,3],{\"a\":\"b\",\"c\":\"d\"}]" );
   }

}  // namespace tao::json

#include "main.hpp"
