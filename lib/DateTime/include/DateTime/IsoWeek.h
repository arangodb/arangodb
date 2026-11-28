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
#pragma once

#include <chrono>

// Shamelessly lifted from Hinnant's stackoverflow post
namespace arangodb::datetime {

struct IsoWeekDate {
  constexpr IsoWeekDate(std::int16_t y, std::uint8_t wn,
                        std::chrono::weekday wd) noexcept
      : year{y}, weeknum{wn}, dow{wd} {}
  constexpr IsoWeekDate(std::chrono::sys_days tp) noexcept
      : IsoWeekDate{0, 0, std::chrono::weekday{}} {
    using std::chrono::days;
    using std::chrono::floor;
    using std::chrono::Monday;
    using std::chrono::sys_days;
    using std::chrono::Thursday;
    using std::chrono::weekday;
    using std::chrono::weeks;
    using std::chrono::year_month_day;

    dow = weekday{tp};
    auto closest_thursday = [this](sys_days tp) {
      auto i = static_cast<int>(dow.iso_encoding());
      tp += days{4 - i};
      return tp;
    };

    auto y = year_month_day{closest_thursday(tp)}.year();
    auto start = sys_days{y / 1 / Thursday[1]} - (Thursday - Monday);
    year = int{y};
    weeknum = floor<weeks>(tp - start) / weeks{1} + 1;
  }

  constexpr operator std::chrono::sys_days() const noexcept {
    using std::chrono::Monday;
    using std::chrono::sys_days;
    using std::chrono::Thursday;
    using std::chrono::weeks;

    auto start = sys_days{std::chrono::year{year} / 1 / Thursday[1]} -
                 (Thursday - Monday);
    return start + weeks{weeknum - 1} + (dow - Monday);
  }

  std::int16_t year;
  std::uint8_t weeknum;
  std::chrono::weekday dow;
};
}  // namespace arangodb::datetime
