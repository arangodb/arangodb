// Copyright 2016 Google Inc. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS-IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//

// Author: ericv@google.com (Eric Veach)

#include "s2/value_lexicon.h"

#include <memory>

#include <gtest/gtest.h>
#include "s2/third_party/absl/memory/memory.h"
#include "s2/s1angle.h"
#include "s2/s2point.h"

using absl::make_unique;

TEST(ValueLexicon, DuplicateValues) {
  ValueLexicon<int64> lex;
  EXPECT_EQ(0, lex.Add(5));
  EXPECT_EQ(1, lex.Add(0));
  EXPECT_EQ(1, lex.Add(0));
  EXPECT_EQ(2, lex.Add(-3));
  EXPECT_EQ(0, lex.Add(5));
  EXPECT_EQ(1, lex.Add(0));
  EXPECT_EQ(3, lex.Add(0x7fffffffffffffff));
  EXPECT_EQ(4, lex.Add(-0x8000000000000000));
  EXPECT_EQ(3, lex.Add(0x7fffffffffffffff));
  EXPECT_EQ(4, lex.Add(-0x8000000000000000));
  EXPECT_EQ(5, lex.size());
  EXPECT_EQ(5, lex.value(0));
  EXPECT_EQ(0, lex.value(1));
  EXPECT_EQ(-3, lex.value(2));
  EXPECT_EQ(0x7fffffffffffffff, lex.value(3));
  EXPECT_EQ(-0x8000000000000000, lex.value(4));
}

TEST(ValueLexicon, Clear) {
  ValueLexicon<int64> lex;
  EXPECT_EQ(0, lex.Add(1));
  EXPECT_EQ(1, lex.Add(2));
  EXPECT_EQ(0, lex.Add(1));
  lex.Clear();
  EXPECT_EQ(0, lex.Add(2));
  EXPECT_EQ(1, lex.Add(1));
  EXPECT_EQ(0, lex.Add(2));
}

TEST(ValueLexicon, FloatEquality) {
  ValueLexicon<S2Point, S2PointHash> lex;
  S2Point a(1, 0.0, 0.0);
  S2Point b(1, -0.0, 0.0);
  S2Point c(1, 0.0, -0.0);
  EXPECT_NE(0, memcmp(&a, &b, sizeof(a)));
  EXPECT_NE(0, memcmp(&a, &c, sizeof(a)));
  EXPECT_NE(0, memcmp(&b, &c, sizeof(a)));
  EXPECT_EQ(0, lex.Add(a));
  EXPECT_EQ(0, lex.Add(b));
  EXPECT_EQ(0, lex.Add(c));
  EXPECT_EQ(1, lex.size());
  EXPECT_EQ(0, memcmp(&a, &lex.value(0), sizeof(a)));
}

TEST(ValueLexicon, CopyConstructor) {
  auto original = make_unique<ValueLexicon<int64>>();
  EXPECT_EQ(0, original->Add(5));
  auto lex = *original;
  original.reset(nullptr);
  EXPECT_EQ(1, lex.Add(10));
  EXPECT_EQ(5, lex.value(0));
  EXPECT_EQ(10, lex.value(1));
}

TEST(ValueLexicon, MoveConstructor) {
  auto original = make_unique<ValueLexicon<int64>>();
  EXPECT_EQ(0, original->Add(5));
  auto lex = std::move(*original);
  original.reset(nullptr);
  EXPECT_EQ(1, lex.Add(10));
  EXPECT_EQ(5, lex.value(0));
  EXPECT_EQ(10, lex.value(1));
}

TEST(ValueLexicon, CopyAssignmentOperator) {
  auto original = make_unique<ValueLexicon<int64>>();
  EXPECT_EQ(0, original->Add(5));
  ValueLexicon<int64> lex;
  EXPECT_EQ(0, lex.Add(10));
  EXPECT_EQ(1, lex.Add(15));
  lex = *original;
  original.reset(nullptr);
  lex = *&lex;  // Tests self-assignment.
  EXPECT_EQ(1, lex.Add(20));
  EXPECT_EQ(5, lex.value(0));
  EXPECT_EQ(20, lex.value(1));
}

TEST(ValueLexicon, MoveAssignmentOperator) {
  auto original = make_unique<ValueLexicon<int64>>();
  EXPECT_EQ(0, original->Add(5));
  ValueLexicon<int64> lex;
  EXPECT_EQ(0, lex.Add(10));
  EXPECT_EQ(1, lex.Add(15));
  lex = std::move(*original);
  original.reset(nullptr);
  EXPECT_EQ(1, lex.Add(20));
  EXPECT_EQ(5, lex.value(0));
  EXPECT_EQ(20, lex.value(1));
}

