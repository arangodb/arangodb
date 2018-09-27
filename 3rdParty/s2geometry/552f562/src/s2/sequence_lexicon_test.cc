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

#include "s2/sequence_lexicon.h"

#include <array>
#include <memory>
#include <vector>

#include "s2/base/logging.h"
#include <gtest/gtest.h>
#include "s2/third_party/absl/memory/memory.h"

using absl::make_unique;

template <class T>
void ExpectSequence(const std::vector<T>& expected,
                    const typename SequenceLexicon<T>::Sequence& actual) {
  EXPECT_EQ(expected.size(), actual.size());
  EXPECT_TRUE(std::equal(expected.begin(), expected.end(), actual.begin()));
}

using Seq = std::vector<int64>;

TEST(SequenceLexicon, int64) {
  SequenceLexicon<int64> lex;
  EXPECT_EQ(0, lex.Add(Seq{}));
  EXPECT_EQ(1, lex.Add(Seq{5}));
  EXPECT_EQ(0, lex.Add(Seq{}));
  EXPECT_EQ(2, lex.Add(Seq{5, 5}));
  EXPECT_EQ(3, lex.Add(Seq{5, 0, -3}));
  EXPECT_EQ(1, lex.Add(Seq{5}));
  EXPECT_EQ(4, lex.Add(Seq{0x7fffffffffffffff}));
  EXPECT_EQ(3, lex.Add(Seq{5, 0, -3}));
  EXPECT_EQ(0, lex.Add(Seq{}));
  EXPECT_EQ(5, lex.size());
  ExpectSequence(Seq{}, lex.sequence(0));
  ExpectSequence(Seq{5}, lex.sequence(1));
  ExpectSequence(Seq{5, 5}, lex.sequence(2));
  ExpectSequence(Seq{5, 0, -3}, lex.sequence(3));
  ExpectSequence(Seq{0x7fffffffffffffff}, lex.sequence(4));
}

TEST(SequenceLexicon, Clear) {
  SequenceLexicon<int64> lex;
  EXPECT_EQ(0, lex.Add(Seq{1}));
  EXPECT_EQ(1, lex.Add(Seq{2}));
  lex.Clear();
  EXPECT_EQ(0, lex.Add(Seq{2}));
  EXPECT_EQ(1, lex.Add(Seq{1}));
}

TEST(SequenceLexicon, CopyConstructor) {
  auto original = make_unique<SequenceLexicon<int64>>();
  EXPECT_EQ(0, original->Add(Seq{1, 2}));
  auto lex = *original;
  original.reset(nullptr);
  EXPECT_EQ(1, lex.Add(Seq{3, 4}));
  ExpectSequence(Seq{1, 2}, lex.sequence(0));
  ExpectSequence(Seq{3, 4}, lex.sequence(1));
}

TEST(SequenceLexicon, MoveConstructor) {
  auto original = make_unique<SequenceLexicon<int64>>();
  EXPECT_EQ(0, original->Add(Seq{1, 2}));
  auto lex = std::move(*original);
  original.reset(nullptr);
  EXPECT_EQ(1, lex.Add(Seq{3, 4}));
  ExpectSequence(Seq{1, 2}, lex.sequence(0));
  ExpectSequence(Seq{3, 4}, lex.sequence(1));
}

TEST(SequenceLexicon, CopyAssignmentOperator) {
  auto original = make_unique<SequenceLexicon<int64>>();
  EXPECT_EQ(0, original->Add(Seq{1, 2}));
  SequenceLexicon<int64> lex;
  EXPECT_EQ(0, lex.Add(Seq{3, 4}));
  EXPECT_EQ(1, lex.Add(Seq{5, 6}));
  lex = *original;
  original.reset(nullptr);
  lex = *&lex;  // Tests self-assignment.
  EXPECT_EQ(1, lex.Add(Seq{7, 8}));
  ExpectSequence(Seq{1, 2}, lex.sequence(0));
  ExpectSequence(Seq{7, 8}, lex.sequence(1));
}

TEST(SequenceLexicon, MoveAssignmentOperator) {
  auto original = make_unique<SequenceLexicon<int64>>();
  EXPECT_EQ(0, original->Add(Seq{1, 2}));
  SequenceLexicon<int64> lex;
  EXPECT_EQ(0, lex.Add(Seq{3, 4}));
  EXPECT_EQ(1, lex.Add(Seq{5, 6}));
  lex = std::move(*original);
  original.reset(nullptr);
  EXPECT_EQ(1, lex.Add(Seq{7, 8}));
  ExpectSequence(Seq{1, 2}, lex.sequence(0));
  ExpectSequence(Seq{7, 8}, lex.sequence(1));
}

