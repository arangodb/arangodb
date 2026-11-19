#pragma once

#include <chrono>

namespace arangodb {
struct iso_week_date {
  constexpr iso_week_date(std::int16_t y, std::uint8_t wn,
                          std::chrono::weekday wd) noexcept
      : year{y}, weeknum{wn}, dow{wd} {}
  constexpr iso_week_date(std::chrono::sys_days tp) noexcept
      : iso_week_date{0, 0, std::chrono::weekday{}} {
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
}  // namespace arangodb
