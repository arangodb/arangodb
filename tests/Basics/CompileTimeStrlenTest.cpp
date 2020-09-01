////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2020 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Apache License, Version 2.0 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     http://www.apache.org/licenses/LICENSE-2.0
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Jan Steemann
/// @author Copyright 2007-2012, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "Basics/Common.h"
#include "Basics/compile-time-strlen.h"

#include "gtest/gtest.h"

/// @brief test compileTimeStrlen
TEST(CompileTimStrlenTest, test_compile_time_strlen) {
  static_assert(arangodb::compileTimeStrlen("") == 0,
                "invalid compile time strlen value");
  static_assert(arangodb::compileTimeStrlen("foo") == 3,
                "invalid compile time strlen value");
  static_assert(arangodb::compileTimeStrlen("foobarbaz") == 9,
                "invalid compile time strlen value");
  static_assert(arangodb::compileTimeStrlen("the quick brown fox") == 19,
                "invalid compile time strlen value");

  EXPECT_EQ(arangodb::compileTimeStrlen(""), 0U);
  EXPECT_EQ(arangodb::compileTimeStrlen("foo"), 3U);
  EXPECT_EQ(arangodb::compileTimeStrlen("foobarbaz"), 9U);
  EXPECT_EQ(arangodb::compileTimeStrlen("the quick brown fox"), 19U);
}
