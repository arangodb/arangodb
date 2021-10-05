////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2021 ArangoDB GmbH, Cologne, Germany
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
////////////////////////////////////////////////////////////////////////////////

#include "tests_shared.hpp"
#include "utils/misc.hpp"

TEST(misc_test, cached_func_contexpr) {
  constexpr auto cached_func = irs::cache_func<uint32_t, 3>(
    0, [](uint32_t v) { return v + 1; });
  static_assert (3 == cached_func.size());
  static_assert (1 == cached_func(0));
  static_assert (2 == cached_func(1));
  static_assert (3 == cached_func(2));
  static_assert (4 == cached_func(3));
  static_assert (5 == cached_func(4));
}

TEST(misc_test, cached_func) {
  size_t calls = 0;
  auto cached_func = irs::cache_func<uint32_t, 3>(0, [&](uint32_t v) {
    ++calls;
    return v + 1;
  });
  ASSERT_EQ(calls, cached_func.size());
  ASSERT_EQ(3, cached_func.size());
  ASSERT_EQ(3, cached_func.size());
  ASSERT_EQ(1, cached_func(0));
  ASSERT_EQ(3, calls);
  ASSERT_EQ(2, cached_func(1));
  ASSERT_EQ(3, calls);
  ASSERT_EQ(3, cached_func(2));
  ASSERT_EQ(3, calls);
  ASSERT_EQ(4, cached_func(3));
  ASSERT_EQ(4, calls);
  ASSERT_EQ(5, cached_func(4));
  ASSERT_EQ(5, calls);
}
