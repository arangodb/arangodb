////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Business Source License 1.1 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     https://github.com/arangodb/arangodb/blob/devel/LICENSE
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
////////////////////////////////////////////////////////////////////////////////

#include "gtest/gtest.h"

#include "Basics/Meta/TypeList.h"

using TypeList = arangodb::meta::TypeList<int, double, bool>;

static_assert(TypeList::size() == 3);
static_assert(TypeList::contains<int>());
static_assert(TypeList::contains<double>());
static_assert(TypeList::contains<bool>());
static_assert(not TypeList::contains<float>());
static_assert(not TypeList::contains<char>());

static_assert(TypeList::index<int>() == 0);
static_assert(TypeList::index<double>() == 1);
static_assert(TypeList::index<bool>() == 2);

TEST(TypeList, foreach) {
  int cnt = 0;
  TypeList::foreach ([&cnt]<typename T>() {
    static_assert(TypeList::contains<T>());
    switch (cnt) {
      case 0:
        EXPECT_TRUE((std::is_same_v<T, int>));
        break;
      case 1:
        EXPECT_TRUE((std::is_same_v<T, double>));
        break;
      case 2:
        EXPECT_TRUE((std::is_same_v<T, bool>));
        break;
      default:
        FAIL();
    }
    ++cnt;
  });
  ASSERT_EQ(cnt, TypeList::size());
}
