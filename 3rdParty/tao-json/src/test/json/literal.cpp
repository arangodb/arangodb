// Copyright (c) 2015-2020 Dr. Colin Hirsch and Daniel Frey
// Please see LICENSE for license or visit https://github.com/taocpp/json/

#include "test.hpp"

#include <tao/json/from_string.hpp>
#include <tao/json/value.hpp>

namespace tao::json
{
   void unit_test()
   {
      TEST_ASSERT( "42"_json.get_unsigned() == 42 );
   }

}  // namespace tao::json

#include "main.hpp"
