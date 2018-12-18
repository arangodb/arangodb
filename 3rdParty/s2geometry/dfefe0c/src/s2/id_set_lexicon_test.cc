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

#include "s2/id_set_lexicon.h"

#include <vector>

#include <gtest/gtest.h>

void ExpectIdSet(const std::vector<int32>& expected,
                 const IdSetLexicon::IdSet& actual) {
  EXPECT_EQ(expected.size(), actual.size());
  EXPECT_TRUE(std::equal(expected.begin(), expected.end(), actual.begin()));
}

using IdSet = IdSetLexicon::IdSet;
using Seq = std::vector<int32>;

TEST(IdSetLexicon, EmptySet) {
  IdSetLexicon lexicon;
  ExpectIdSet({}, lexicon.id_set(lexicon.Add(Seq{})));
}

TEST(IdSetLexicon, SingletonSets) {
  IdSetLexicon lexicon;
  EXPECT_EQ(5, lexicon.Add(Seq{5}));
  EXPECT_EQ(0, lexicon.Add(Seq{0}));
  EXPECT_EQ(1, lexicon.AddSingleton(1));
  int32 m = std::numeric_limits<int32>::max();
  EXPECT_EQ(m, lexicon.Add(&m, &m + 1));

  ExpectIdSet({0}, lexicon.id_set(0));
  ExpectIdSet({1}, lexicon.id_set(1));
  ExpectIdSet({5}, lexicon.id_set(5));
  ExpectIdSet({m}, lexicon.id_set(m));
}

TEST(IdSetLexicon, SetsAreSorted) {
  IdSetLexicon lexicon;
  EXPECT_EQ(~0, lexicon.Add(Seq{2, 5}));
  EXPECT_EQ(~1, lexicon.Add(Seq{3, 2, 5}));
  EXPECT_EQ(~0, lexicon.Add(Seq{5, 2}));
  EXPECT_EQ(~1, lexicon.Add(Seq{5, 3, 2, 5}));

  ExpectIdSet({2, 5}, lexicon.id_set(~0));
  ExpectIdSet({2, 3, 5}, lexicon.id_set(~1));
}

TEST(IdSetLexicon, Clear) {
  IdSetLexicon lexicon;
  EXPECT_EQ(~0, lexicon.Add(Seq{1, 2}));
  EXPECT_EQ(~1, lexicon.Add(Seq{3, 4}));
  lexicon.Clear();
  EXPECT_EQ(~0, lexicon.Add(Seq{3, 4}));
  EXPECT_EQ(~1, lexicon.Add(Seq{1, 2}));
}
