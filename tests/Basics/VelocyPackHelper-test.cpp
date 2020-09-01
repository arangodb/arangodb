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
/// @author Copyright 2012, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "Basics/Common.h"

#include "gtest/gtest.h"

#include <velocypack/Builder.h>
#include <velocypack/Parser.h>
#include <velocypack/velocypack-aliases.h>

#include "Basics/VelocyPackHelper.h"

// -----------------------------------------------------------------------------
// --SECTION--                                                    private macros
// -----------------------------------------------------------------------------
  
#define VPACK_EXPECT_TRUE(expected, func, lValue, rValue)  \
  l = VPackParser::fromJson(lValue);  \
  r = VPackParser::fromJson(rValue);  \
  EXPECT_EQ(expected, func(l->slice(), r->slice(), true)); \

// -----------------------------------------------------------------------------
// --SECTION--                                                        test suite
// -----------------------------------------------------------------------------

TEST(VPackHelperTest, tst_patch_double) {
  VPackBuilder b;
  b.add(VPackValue(double(1.0)));
  
  EXPECT_DOUBLE_EQ(double(1.0), b.slice().getDouble());

  arangodb::basics::VelocyPackHelper::patchDouble(b.slice(), double(2.0));
  EXPECT_DOUBLE_EQ(double(2.0), b.slice().getDouble());
  
  arangodb::basics::VelocyPackHelper::patchDouble(b.slice(), double(-34.456));
  EXPECT_DOUBLE_EQ(double(-34.456), b.slice().getDouble());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test compare values with equal values
////////////////////////////////////////////////////////////////////////////////

TEST(VPackHelperTest, tst_compare_values_equal) {
  std::shared_ptr<VPackBuilder> l;
  std::shared_ptr<VPackBuilder> r;

  // With Utf8-mode:
  VPACK_EXPECT_TRUE(0, arangodb::basics::VelocyPackHelper::compare, "null", "null");
  VPACK_EXPECT_TRUE(0, arangodb::basics::VelocyPackHelper::compare, "false", "false");
  VPACK_EXPECT_TRUE(0, arangodb::basics::VelocyPackHelper::compare, "true", "true");
  VPACK_EXPECT_TRUE(0, arangodb::basics::VelocyPackHelper::compare, "0", "0");
  VPACK_EXPECT_TRUE(0, arangodb::basics::VelocyPackHelper::compare, "1", "1");
  VPACK_EXPECT_TRUE(0, arangodb::basics::VelocyPackHelper::compare, "1.5", "1.5");
  VPACK_EXPECT_TRUE(0, arangodb::basics::VelocyPackHelper::compare, "-43.2", "-43.2");
  VPACK_EXPECT_TRUE(0, arangodb::basics::VelocyPackHelper::compare, "\"\"", "\"\"");
  VPACK_EXPECT_TRUE(0, arangodb::basics::VelocyPackHelper::compare, "\" \"", "\" \"");
  VPACK_EXPECT_TRUE(0, arangodb::basics::VelocyPackHelper::compare, "\"the quick brown fox\"", "\"the quick brown fox\"");
  VPACK_EXPECT_TRUE(0, arangodb::basics::VelocyPackHelper::compare, "[]", "[]");
  VPACK_EXPECT_TRUE(0, arangodb::basics::VelocyPackHelper::compare, "[-1]", "[-1]");
  VPACK_EXPECT_TRUE(0, arangodb::basics::VelocyPackHelper::compare, "[0]", "[0]");
  VPACK_EXPECT_TRUE(0, arangodb::basics::VelocyPackHelper::compare, "[1]", "[1]");
  VPACK_EXPECT_TRUE(0, arangodb::basics::VelocyPackHelper::compare, "[true]", "[true]");
  VPACK_EXPECT_TRUE(0, arangodb::basics::VelocyPackHelper::compare, "{}", "{}");
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test compare values with unequal values
////////////////////////////////////////////////////////////////////////////////

TEST(VPackHelperTest, tst_compare_values_unequal) {
  std::shared_ptr<VPackBuilder> l;
  std::shared_ptr<VPackBuilder> r;
  VPACK_EXPECT_TRUE(-1, arangodb::basics::VelocyPackHelper::compare, "null", "false");
  VPACK_EXPECT_TRUE(-1, arangodb::basics::VelocyPackHelper::compare, "null", "true");
  VPACK_EXPECT_TRUE(-1, arangodb::basics::VelocyPackHelper::compare, "null", "-1");
  VPACK_EXPECT_TRUE(-1, arangodb::basics::VelocyPackHelper::compare, "null", "0");
  VPACK_EXPECT_TRUE(-1, arangodb::basics::VelocyPackHelper::compare, "null", "1");
  VPACK_EXPECT_TRUE(-1, arangodb::basics::VelocyPackHelper::compare, "null", "-10");
  VPACK_EXPECT_TRUE(-1, arangodb::basics::VelocyPackHelper::compare, "null", "\"\"");
  VPACK_EXPECT_TRUE(-1, arangodb::basics::VelocyPackHelper::compare, "null", "\"0\"");
  VPACK_EXPECT_TRUE(-1, arangodb::basics::VelocyPackHelper::compare, "null", "\" \"");
  VPACK_EXPECT_TRUE(-1, arangodb::basics::VelocyPackHelper::compare, "null", "[]");
  VPACK_EXPECT_TRUE(-1, arangodb::basics::VelocyPackHelper::compare, "null", "[null]");
  VPACK_EXPECT_TRUE(-1, arangodb::basics::VelocyPackHelper::compare, "null", "[false]");
  VPACK_EXPECT_TRUE(-1, arangodb::basics::VelocyPackHelper::compare, "null", "[true]");
  VPACK_EXPECT_TRUE(-1, arangodb::basics::VelocyPackHelper::compare, "null", "[0]");
  VPACK_EXPECT_TRUE(-1, arangodb::basics::VelocyPackHelper::compare, "null", "{}");
  
  VPACK_EXPECT_TRUE(-1, arangodb::basics::VelocyPackHelper::compare, "false", "true");
  VPACK_EXPECT_TRUE(-1, arangodb::basics::VelocyPackHelper::compare, "false", "-1");
  VPACK_EXPECT_TRUE(-1, arangodb::basics::VelocyPackHelper::compare, "false", "0");
  VPACK_EXPECT_TRUE(-1, arangodb::basics::VelocyPackHelper::compare, "false", "1");
  VPACK_EXPECT_TRUE(-1, arangodb::basics::VelocyPackHelper::compare, "false", "-10");
  VPACK_EXPECT_TRUE(-1, arangodb::basics::VelocyPackHelper::compare, "false", "\"\"");
  VPACK_EXPECT_TRUE(-1, arangodb::basics::VelocyPackHelper::compare, "false", "\"0\"");
  VPACK_EXPECT_TRUE(-1, arangodb::basics::VelocyPackHelper::compare, "false", "\" \"");
  VPACK_EXPECT_TRUE(-1, arangodb::basics::VelocyPackHelper::compare, "false", "[]");
  VPACK_EXPECT_TRUE(-1, arangodb::basics::VelocyPackHelper::compare, "false", "[null]");
  VPACK_EXPECT_TRUE(-1, arangodb::basics::VelocyPackHelper::compare, "false", "[false]");
  VPACK_EXPECT_TRUE(-1, arangodb::basics::VelocyPackHelper::compare, "false", "[true]");
  VPACK_EXPECT_TRUE(-1, arangodb::basics::VelocyPackHelper::compare, "false", "[0]");
  VPACK_EXPECT_TRUE(-1, arangodb::basics::VelocyPackHelper::compare, "false", "{}");
  
  VPACK_EXPECT_TRUE(-1, arangodb::basics::VelocyPackHelper::compare, "true", "-1");
  VPACK_EXPECT_TRUE(-1, arangodb::basics::VelocyPackHelper::compare, "true", "0");
  VPACK_EXPECT_TRUE(-1, arangodb::basics::VelocyPackHelper::compare, "true", "1");
  VPACK_EXPECT_TRUE(-1, arangodb::basics::VelocyPackHelper::compare, "true", "-10");
  VPACK_EXPECT_TRUE(-1, arangodb::basics::VelocyPackHelper::compare, "true", "\"\"");
  VPACK_EXPECT_TRUE(-1, arangodb::basics::VelocyPackHelper::compare, "true", "\"0\"");
  VPACK_EXPECT_TRUE(-1, arangodb::basics::VelocyPackHelper::compare, "true", "\" \"");
  VPACK_EXPECT_TRUE(-1, arangodb::basics::VelocyPackHelper::compare, "true", "[]");
  VPACK_EXPECT_TRUE(-1, arangodb::basics::VelocyPackHelper::compare, "true", "[null]");
  VPACK_EXPECT_TRUE(-1, arangodb::basics::VelocyPackHelper::compare, "true", "[false]");
  VPACK_EXPECT_TRUE(-1, arangodb::basics::VelocyPackHelper::compare, "true", "[true]");
  VPACK_EXPECT_TRUE(-1, arangodb::basics::VelocyPackHelper::compare, "true", "[0]");
  VPACK_EXPECT_TRUE(-1, arangodb::basics::VelocyPackHelper::compare, "true", "{}");
  
  VPACK_EXPECT_TRUE(-1, arangodb::basics::VelocyPackHelper::compare, "-2", "-1");
  VPACK_EXPECT_TRUE(-1, arangodb::basics::VelocyPackHelper::compare, "-10", "-9");
  VPACK_EXPECT_TRUE(-1, arangodb::basics::VelocyPackHelper::compare, "-20", "-5");
  VPACK_EXPECT_TRUE(-1, arangodb::basics::VelocyPackHelper::compare, "-5", "-2");
  VPACK_EXPECT_TRUE(-1, arangodb::basics::VelocyPackHelper::compare, "true", "1");
  VPACK_EXPECT_TRUE(-1, arangodb::basics::VelocyPackHelper::compare, "1.5", "1.6");
  VPACK_EXPECT_TRUE(-1, arangodb::basics::VelocyPackHelper::compare, "10.5", "10.51");
  VPACK_EXPECT_TRUE(-1, arangodb::basics::VelocyPackHelper::compare, "0", "\"\"");
  VPACK_EXPECT_TRUE(-1, arangodb::basics::VelocyPackHelper::compare, "0", "\"0\"");
  VPACK_EXPECT_TRUE(-1, arangodb::basics::VelocyPackHelper::compare, "0", "\"-1\"");
  VPACK_EXPECT_TRUE(-1, arangodb::basics::VelocyPackHelper::compare, "1", "\"-1\"");
  VPACK_EXPECT_TRUE(-1, arangodb::basics::VelocyPackHelper::compare, "1", "\" \"");
  VPACK_EXPECT_TRUE(-1, arangodb::basics::VelocyPackHelper::compare, "0", "[]");
  VPACK_EXPECT_TRUE(-1, arangodb::basics::VelocyPackHelper::compare, "0", "[-1]");
  VPACK_EXPECT_TRUE(-1, arangodb::basics::VelocyPackHelper::compare, "0", "[0]");
  VPACK_EXPECT_TRUE(-1, arangodb::basics::VelocyPackHelper::compare, "0", "[1]");
  VPACK_EXPECT_TRUE(-1, arangodb::basics::VelocyPackHelper::compare, "0", "[null]");
  VPACK_EXPECT_TRUE(-1, arangodb::basics::VelocyPackHelper::compare, "0", "[false]");
  VPACK_EXPECT_TRUE(-1, arangodb::basics::VelocyPackHelper::compare, "0", "[true]");
  VPACK_EXPECT_TRUE(-1, arangodb::basics::VelocyPackHelper::compare, "0", "{}");
  VPACK_EXPECT_TRUE(-1, arangodb::basics::VelocyPackHelper::compare, "1", "[]");
  VPACK_EXPECT_TRUE(-1, arangodb::basics::VelocyPackHelper::compare, "1", "[-1]");
  VPACK_EXPECT_TRUE(-1, arangodb::basics::VelocyPackHelper::compare, "1", "[0]");
  VPACK_EXPECT_TRUE(-1, arangodb::basics::VelocyPackHelper::compare, "1", "[1]");
  VPACK_EXPECT_TRUE(-1, arangodb::basics::VelocyPackHelper::compare, "1", "[null]");
  VPACK_EXPECT_TRUE(-1, arangodb::basics::VelocyPackHelper::compare, "1", "[false]");
  VPACK_EXPECT_TRUE(-1, arangodb::basics::VelocyPackHelper::compare, "1", "[true]");
  VPACK_EXPECT_TRUE(-1, arangodb::basics::VelocyPackHelper::compare, "1", "{}");
}
