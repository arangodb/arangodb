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
/// @author Andrey Abramov
/// @author Vasiliy Nabatchikov
////////////////////////////////////////////////////////////////////////////////

#include "gtest/gtest.h"
#include <unordered_set>

#include "IResearch/VelocyPackHelper.h"

#include "velocypack/Builder.h"
#include "velocypack/Iterator.h"
#include "velocypack/Parser.h"
#include "velocypack/velocypack-aliases.h"

TEST(IResearchVelocyPackHelperTest, test_getstring) {
  // string value
  {
    auto json =
        arangodb::velocypack::Parser::fromJson("{ \"key\": \"value\" }");
    auto slice = json->slice();
    std::string buf0;
    irs::string_ref buf1;
    bool seen;

    EXPECT_TRUE(
        (arangodb::iresearch::getString(buf0, slice, "key", seen, "abc")));
    EXPECT_TRUE(seen);
    EXPECT_EQ(buf0, "value");

    EXPECT_TRUE(
        (arangodb::iresearch::getString(buf1, slice, "key", seen, "abc")));
    EXPECT_TRUE(seen);
    EXPECT_EQ(buf1, "value");
  }

  // missing key
  {
    auto json = arangodb::velocypack::Parser::fromJson("{}");
    auto slice = json->slice();
    std::string buf0;
    irs::string_ref buf1;
    bool seen = true;

    EXPECT_TRUE(
        (arangodb::iresearch::getString(buf0, slice, "key", seen, "abc")));
    EXPECT_FALSE(seen);
    EXPECT_EQ(buf0, "abc");

    seen = true;

    EXPECT_TRUE(
        (arangodb::iresearch::getString(buf1, slice, "key", seen, "abc")));
    EXPECT_FALSE(seen);
    EXPECT_EQ(buf1, "abc");
  }

  // non-string value
  {
    auto json = arangodb::velocypack::Parser::fromJson("{ \"key\": 12345 }");
    auto slice = json->slice();
    std::string buf0;
    irs::string_ref buf1;
    bool seen;

    EXPECT_TRUE(
        (!arangodb::iresearch::getString(buf0, slice, "key", seen, "abc")));
    EXPECT_TRUE(seen);

    EXPECT_TRUE(
        (!arangodb::iresearch::getString(buf1, slice, "key", seen, "abc")));
    EXPECT_TRUE(seen);
  }
}
