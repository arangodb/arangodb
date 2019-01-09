////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2018 ArangoDB GmbH, Cologne, Germany
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

#include "tests_shared.hpp"
#include "utils/fst_string_weight.h"

#include <string>

TEST(fst_string_weight_tests, static_const) {
  ASSERT_EQ("left_string", fst::StringLeftWeight<char>::Type());
  ASSERT_EQ(fst::StringLeftWeight<char>(fst::kStringInfinity), fst::StringLeftWeight<char>::Zero());
  ASSERT_TRUE(fst::StringLeftWeight<char>::Zero().Member());
  ASSERT_EQ(fst::StringLeftWeight<char>(), fst::StringLeftWeight<char>::One());
  ASSERT_TRUE(fst::StringLeftWeight<char>::One().Member());
  ASSERT_EQ(fst::StringLeftWeight<char>(fst::kStringBad), fst::StringLeftWeight<char>::NoWeight());
  ASSERT_FALSE(fst::StringLeftWeight<char>::NoWeight().Member());
}

TEST(fst_string_weight_tests, construct) {
  fst::StringLeftWeight<char> w;
  ASSERT_TRUE(w.Empty());
  ASSERT_EQ(0, w.Size());
}

TEST(fst_string_weight_tests, read_write) {
  std::stringstream ss;

  fst::StringLeftWeight<char> w0;
  w0.PushBack(1);
  w0.PushBack(char(255));
  w0.PushBack(3);
  w0.PushBack(3);
  w0.PushBack(char(129));
  w0.Write(ss);
  ASSERT_EQ(5, w0.Size());
  ASSERT_TRUE(w0.Member());

  ss.seekg(0);
  fst::StringLeftWeight<char> w1;
  w1.Read(ss);
  ASSERT_EQ(w0, w1);
  ASSERT_EQ(w0.Hash(), w1.Hash());
}
