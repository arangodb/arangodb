// Copyright (c) 2019-2020 Dr. Colin Hirsch and Daniel Frey
// Please see LICENSE for license or visit https://github.com/taocpp/json/

#include "test.hpp"

#include <tao/json/stream.hpp>
#include <tao/json/value.hpp>

#include <tao/json/contrib/diff.hpp>

namespace tao::json
{
   void unit_test()
   {
      const value source = {
         { "foo", 42 },
         { "blue", 3.14 },
         { "bar", value::array( { 1, 2, 3, 4, 5 } ) },
         { "baz", 2 }
      };

      const value destination = {
         { "bar", value::array( { 1, 2, 7 } ) },
         { "foo", 42 },
         { "yellow", empty_object },
         { "baz", "hello" }
      };

      const auto d = diff( source, destination );

      // clang-format off
      TEST_ASSERT( d == value::array( { { { "op", "replace" }, { "path", "/bar/2" }, { "value", 7 } },
                                        { { "op", "remove" }, { "path", "/bar/4" } },
                                        { { "op", "remove" }, { "path", "/bar/3" } },
                                        { { "op", "replace" }, { "path", "/baz" }, { "value", "hello" } },
                                        { { "op", "remove" }, { "path", "/blue" } },
                                        { { "op", "add" }, { "path", "/yellow" }, { "value", empty_object } } } ) );
      // clang-format on
   }

}  // namespace tao::json

#include "main.hpp"
