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

#include <gtest/gtest.h>
#include <chrono>

#include "DateTime/IsoWeek.h"

namespace arangodb::datetime {

TEST(IsoWeekTest, works) {
  auto const y = std::chrono::year{2011};
  auto const m = std::chrono::January;
  auto const d = std::chrono::day{1};
  auto days = std::chrono::year_month_day(y, m, d);

  auto iwd = IsoWeekDate(days);

  ASSERT_EQ(iwd.year, 2010);
  ASSERT_EQ(iwd.weeknum, 52);
  ASSERT_EQ(iwd.dow, std::chrono::Saturday);
}
}  // namespace arangodb::datetime
