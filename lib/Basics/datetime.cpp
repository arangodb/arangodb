////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2017 ArangoDB GmbH, Cologne, Germany
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
/// @author Manuel Baesler
////////////////////////////////////////////////////////////////////////////////

#include <stdlib.h>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <iterator>
#include <map>
#include <ratio>
#include <regex>
#include <sstream>
#include <unordered_map>
#include <utility>
#include <vector>

#include <date/date.h>
#include <date/iso_week.h>

#include "Basics/NumberUtils.h"
#include "Basics/datetime.h"
#include "Basics/debugging.h"
#include "Logger/LogMacros.h"
#include "Logger/Logger.h"
#include "Logger/LoggerStream.h"

#include <velocypack/StringRef.h>

namespace {
using namespace date;
using namespace std::chrono;

std::string tail(std::string const& source, size_t const length) {
  if (length >= source.size()) {
    return source;
  }
  return source.substr(source.size() - length);
}  // tail

typedef void (*format_func_t)(std::string& wrk, arangodb::tp_sys_clock_ms const&);
std::unordered_map<std::string, format_func_t> dateMap;
auto const unixEpoch = date::sys_seconds{std::chrono::seconds{0}};

std::vector<std::string> const monthNames = {"January", "February", "March",
                                             "April",   "May",      "June",
                                             "July",    "August",   "September",
                                             "October", "November", "December"};

std::vector<std::string> const monthNamesShort = {"Jan", "Feb", "Mar", "Apr",
                                                  "May", "Jun", "Jul", "Aug",
                                                  "Sep", "Oct", "Nov", "Dec"};

std::vector<std::string> const weekDayNames = {"Sunday",   "Monday",
                                               "Tuesday",  "Wednesday",
                                               "Thursday", "Friday",
                                               "Saturday"};

std::vector<std::string> const weekDayNamesShort = {"Sun", "Mon", "Tue", "Wed",
                                                    "Thu", "Fri", "Sat"};

std::
    vector<std::pair<std::string, format_func_t>> const sortedDateMap = {{"%&",
                                                                          [](std::string& wrk,
                                                                             arangodb::tp_sys_clock_ms const& tp) {
                                                                          }},  // Allow for literal "m" after "%m" ("%mm" ->
                                                                               // %m%&m)
                                                                         // zero-pad 4 digit years to length of 6 and add "+"
                                                                         // prefix, keep negative as-is
                                                                         {"%yyyyyy",
                                                                          [](std::string& wrk, arangodb::tp_sys_clock_ms const& tp) {
                                                                            auto ymd = year_month_day(
                                                                                floor<date::days>(tp));
                                                                            auto yearnum = static_cast<int>(
                                                                                ymd.year());
                                                                            if (yearnum < 0) {
                                                                              if (yearnum > -10) {
                                                                                wrk.append(
                                                                                    "-00000");
                                                                              } else if (yearnum > -100) {
                                                                                wrk.append(
                                                                                    "-0000");
                                                                              } else if (yearnum > -1000) {
                                                                                wrk.append(
                                                                                    "-000");
                                                                              } else if (yearnum > -10000) {
                                                                                wrk.append(
                                                                                    "-00");
                                                                              } else if (yearnum > -100000) {
                                                                                wrk.append(
                                                                                    "-0");
                                                                              } else {
                                                                                wrk.append(
                                                                                    "-");
                                                                              }
                                                                              wrk.append(std::to_string(
                                                                                  abs(yearnum)));
                                                                              return;
                                                                            }

                                                                            TRI_ASSERT(yearnum >= 0);

                                                                            if (yearnum > 99999) {
                                                                              // intentionally nothing
                                                                            } else if (yearnum > 9999) {
                                                                              wrk.append(
                                                                                  "+0");
                                                                            } else if (yearnum > 999) {
                                                                              wrk.append(
                                                                                  "+00");
                                                                            } else if (yearnum > 99) {
                                                                              wrk.append(
                                                                                  "+000");
                                                                            } else if (yearnum > 9) {
                                                                              wrk.append(
                                                                                  "+0000");
                                                                            } else {
                                                                              wrk.append(
                                                                                  "+00000");
                                                                            }
                                                                            wrk.append(std::to_string(yearnum));
                                                                          }},
                                                                         {"%mmmm",
                                                                          [](std::string& wrk, arangodb::tp_sys_clock_ms const& tp) {
                                                                            auto ymd = year_month_day(
                                                                                floor<date::days>(tp));
                                                                            wrk.append(
                                                                                ::monthNames[static_cast<unsigned>(ymd.month()) - 1]);
                                                                          }},
                                                                         {"%yyyy",
                                                                          [](std::string& wrk, arangodb::tp_sys_clock_ms const& tp) {
                                                                            auto ymd = year_month_day(
                                                                                floor<date::days>(tp));
                                                                            auto yearnum = static_cast<int>(
                                                                                ymd.year());
                                                                            if (yearnum < 0) {
                                                                              if (yearnum > -10) {
                                                                                wrk.append(
                                                                                    "-000");
                                                                              } else if (yearnum > -100) {
                                                                                wrk.append(
                                                                                    "-00");
                                                                              } else if (yearnum > -1000) {
                                                                                wrk.append(
                                                                                    "-0");
                                                                              } else {
                                                                                wrk.append(
                                                                                    "-");
                                                                              }
                                                                              wrk.append(std::to_string(
                                                                                  abs(yearnum)));
                                                                            } else {
                                                                              TRI_ASSERT(yearnum >= 0);
                                                                              if (yearnum < 9) {
                                                                                wrk.append(
                                                                                    "000");
                                                                                wrk.append(std::to_string(yearnum));
                                                                              } else if (yearnum < 99) {
                                                                                wrk.append(
                                                                                    "00");
                                                                                wrk.append(std::to_string(yearnum));
                                                                              } else if (yearnum < 999) {
                                                                                wrk.append(
                                                                                    "0");
                                                                                wrk.append(std::to_string(yearnum));
                                                                              } else {
                                                                                std::string yearstr(
                                                                                    std::to_string(yearnum));
                                                                                wrk.append(tail(yearstr, 4));
                                                                              }
                                                                            }
                                                                          }},

                                                                         {"%wwww",
                                                                          [](std::string& wrk, arangodb::tp_sys_clock_ms const& tp) {
                                                                            weekday wd{floor<date::days>(tp)};
                                                                            wrk.append(
                                                                                ::weekDayNames[static_cast<unsigned>(wd)]);
                                                                          }},

                                                                         {"%mmm",
                                                                          [](std::string& wrk, arangodb::tp_sys_clock_ms const& tp) {
                                                                            auto ymd = year_month_day(
                                                                                floor<date::days>(tp));
                                                                            wrk.append(
                                                                                ::monthNamesShort[static_cast<unsigned>(ymd.month()) - 1]);
                                                                          }},
                                                                         {"%www",
                                                                          [](std::string& wrk, arangodb::tp_sys_clock_ms const& tp) {
                                                                            weekday wd{floor<date::days>(tp)};
                                                                            wrk.append(
                                                                                weekDayNamesShort[static_cast<unsigned>(wd)]);
                                                                          }},
                                                                         {"%fff",
                                                                          [](std::string& wrk, arangodb::tp_sys_clock_ms const& tp) {
                                                                            auto day_time = make_time(
                                                                                tp - floor<date::days>(tp));
                                                                            uint64_t millis =
                                                                                day_time
                                                                                    .subseconds()
                                                                                    .count();
                                                                            if (millis < 10) {
                                                                              wrk.append(
                                                                                  "00");
                                                                            } else if (millis < 100) {
                                                                              wrk.append(
                                                                                  "0");
                                                                            }
                                                                            wrk.append(std::to_string(millis));
                                                                          }},
                                                                         {"%xxx",
                                                                          [](std::string& wrk, arangodb::tp_sys_clock_ms const& tp) {
                                                                            auto ymd = year_month_day(
                                                                                floor<date::days>(tp));
                                                                            auto yyyy =
                                                                                year{ymd.year()};
                                                                            // we construct the date with the first day in the
                                                                            // year:
                                                                            auto firstDayInYear =
                                                                                yyyy / jan /
                                                                                day{0};
                                                                            uint64_t daysSinceFirst =
                                                                                duration_cast<date::days>(
                                                                                    tp - sys_days(firstDayInYear))
                                                                                    .count();
                                                                            if (daysSinceFirst < 10) {
                                                                              wrk.append(
                                                                                  "00");
                                                                            } else if (daysSinceFirst < 100) {
                                                                              wrk.append(
                                                                                  "0");
                                                                            }
                                                                            wrk.append(std::to_string(daysSinceFirst));
                                                                          }},

                                                                         // there"s no really sensible way to handle negative
                                                                         // years, but better not drop the sign
                                                                         {"%yy",
                                                                          [](std::string& wrk, arangodb::tp_sys_clock_ms const& tp) {
                                                                            auto ymd = year_month_day(
                                                                                floor<date::days>(tp));
                                                                            auto yearnum = static_cast<int>(
                                                                                ymd.year());
                                                                            if (yearnum < 10 &&
                                                                                yearnum > -10) {
                                                                              wrk.append(
                                                                                  "0");
                                                                              wrk.append(std::to_string(
                                                                                  abs(yearnum)));
                                                                            } else {
                                                                              std::string yearstr(std::to_string(
                                                                                  abs(yearnum)));
                                                                              wrk.append(tail(yearstr, 2));
                                                                            }
                                                                          }},
                                                                         {"%mm",
                                                                          [](std::string& wrk, arangodb::tp_sys_clock_ms const& tp) {
                                                                            auto ymd = year_month_day(
                                                                                floor<date::days>(tp));
                                                                            auto month = static_cast<unsigned>(
                                                                                ymd.month());
                                                                            if (month < 10) {
                                                                              wrk.append(
                                                                                  "0");
                                                                            }
                                                                            wrk.append(std::to_string(month));
                                                                          }},
                                                                         {"%dd",
                                                                          [](std::string& wrk, arangodb::tp_sys_clock_ms const& tp) {
                                                                            auto ymd = year_month_day(
                                                                                floor<date::days>(tp));
                                                                            auto day = static_cast<unsigned>(
                                                                                ymd.day());
                                                                            if (day < 10) {
                                                                              wrk.append(
                                                                                  "0");
                                                                            }
                                                                            wrk.append(std::to_string(day));
                                                                          }},
                                                                         {"%hh",
                                                                          [](std::string& wrk, arangodb::tp_sys_clock_ms const& tp) {
                                                                            auto day_time = make_time(
                                                                                tp - floor<date::days>(tp));
                                                                            uint64_t hours =
                                                                                day_time
                                                                                    .hours()
                                                                                    .count();
                                                                            if (hours < 10) {
                                                                              wrk.append(
                                                                                  "0");
                                                                            }
                                                                            wrk.append(std::to_string(hours));
                                                                          }},
                                                                         {"%ii",
                                                                          [](std::string& wrk, arangodb::tp_sys_clock_ms const& tp) {
                                                                            auto day_time = make_time(
                                                                                tp - floor<date::days>(tp));
                                                                            uint64_t minutes =
                                                                                day_time
                                                                                    .minutes()
                                                                                    .count();
                                                                            if (minutes < 10) {
                                                                              wrk.append(
                                                                                  "0");
                                                                            }
                                                                            wrk.append(std::to_string(minutes));
                                                                          }},
                                                                         {"%ss",
                                                                          [](std::string& wrk, arangodb::tp_sys_clock_ms const& tp) {
                                                                            auto day_time = make_time(
                                                                                tp - floor<date::days>(tp));
                                                                            uint64_t seconds =
                                                                                day_time
                                                                                    .seconds()
                                                                                    .count();
                                                                            if (seconds < 10) {
                                                                              wrk.append(
                                                                                  "0");
                                                                            }
                                                                            wrk.append(std::to_string(seconds));
                                                                          }},
                                                                         {"%kk",
                                                                          [](std::string& wrk, arangodb::tp_sys_clock_ms const& tp) {
                                                                            iso_week::year_weeknum_weekday yww{
                                                                                floor<date::days>(tp)};
                                                                            uint64_t isoWeek =
                                                                                static_cast<unsigned>(
                                                                                    yww.weeknum());
                                                                            if (isoWeek < 10) {
                                                                              wrk.append(
                                                                                  "0");
                                                                            }
                                                                            wrk.append(std::to_string(isoWeek));
                                                                          }},

                                                                         {"%t",
                                                                          [](std::string& wrk, arangodb::tp_sys_clock_ms const& tp) {
                                                                            auto diffDuration =
                                                                                tp - unixEpoch;
                                                                            auto diff =
                                                                                duration_cast<duration<double, std::milli>>(
                                                                                    diffDuration)
                                                                                    .count();
                                                                            wrk.append(std::to_string(static_cast<int64_t>(
                                                                                std::round(diff))));
                                                                          }},
                                                                         {"%z",
                                                                          [](std::string& wrk,
                                                                             arangodb::tp_sys_clock_ms const& tp) {
                                                                            std::string formatted = format(
                                                                                "%FT%TZ",
                                                                                floor<milliseconds>(tp));
                                                                            wrk.append(formatted);
                                                                          }},
                                                                         {"%w",
                                                                          [](std::string& wrk,
                                                                             arangodb::tp_sys_clock_ms const& tp) {
                                                                            weekday wd{floor<date::days>(tp)};
                                                                            wrk.append(std::to_string(
                                                                                static_cast<unsigned>(wd)));
                                                                          }},
                                                                         {"%y",
                                                                          [](std::string& wrk,
                                                                             arangodb::tp_sys_clock_ms const& tp) {
                                                                            auto ymd = year_month_day(
                                                                                floor<date::days>(tp));
                                                                            wrk.append(std::to_string(static_cast<int>(
                                                                                ymd.year())));
                                                                          }},
                                                                         {"%m",
                                                                          [](std::string& wrk,
                                                                             arangodb::tp_sys_clock_ms const& tp) {
                                                                            auto ymd = year_month_day(
                                                                                floor<date::days>(tp));
                                                                            wrk.append(std::to_string(static_cast<unsigned>(
                                                                                ymd.month())));
                                                                          }},
                                                                         {"%d",
                                                                          [](std::string& wrk,
                                                                             arangodb::tp_sys_clock_ms const& tp) {
                                                                            auto ymd = year_month_day(
                                                                                floor<date::days>(tp));
                                                                            wrk.append(std::to_string(static_cast<unsigned>(
                                                                                ymd.day())));
                                                                          }},
                                                                         {"%h",
                                                                          [](std::string& wrk,
                                                                             arangodb::tp_sys_clock_ms const& tp) {
                                                                            auto day_time = make_time(
                                                                                tp - floor<date::days>(tp));
                                                                            uint64_t hours =
                                                                                day_time
                                                                                    .hours()
                                                                                    .count();
                                                                            wrk.append(std::to_string(hours));
                                                                          }},
                                                                         {"%i",
                                                                          [](std::string& wrk,
                                                                             arangodb::tp_sys_clock_ms const& tp) {
                                                                            auto day_time = make_time(
                                                                                tp - floor<date::days>(tp));
                                                                            uint64_t minutes =
                                                                                day_time
                                                                                    .minutes()
                                                                                    .count();
                                                                            wrk.append(std::to_string(minutes));
                                                                          }},
                                                                         {"%s",
                                                                          [](std::string& wrk,
                                                                             arangodb::tp_sys_clock_ms const& tp) {
                                                                            auto day_time = make_time(
                                                                                tp - floor<date::days>(tp));
                                                                            uint64_t seconds =
                                                                                day_time
                                                                                    .seconds()
                                                                                    .count();
                                                                            wrk.append(std::to_string(seconds));
                                                                          }},
                                                                         {"%f",
                                                                          [](std::string& wrk,
                                                                             arangodb::tp_sys_clock_ms const& tp) {
                                                                            auto day_time = make_time(
                                                                                tp - floor<date::days>(tp));
                                                                            uint64_t millis =
                                                                                day_time
                                                                                    .subseconds()
                                                                                    .count();
                                                                            wrk.append(std::to_string(millis));
                                                                          }},
                                                                         {"%x",
                                                                          [](std::string& wrk,
                                                                             arangodb::tp_sys_clock_ms const& tp) {
                                                                            auto ymd = year_month_day(
                                                                                floor<date::days>(tp));
                                                                            auto yyyy =
                                                                                year{ymd.year()};
                                                                            // We construct the date with the first day in the
                                                                            // year:
                                                                            auto firstDayInYear =
                                                                                yyyy / jan /
                                                                                day{0};
                                                                            uint64_t daysSinceFirst =
                                                                                duration_cast<date::days>(
                                                                                    tp - sys_days(firstDayInYear))
                                                                                    .count();
                                                                            wrk.append(std::to_string(daysSinceFirst));
                                                                          }},
                                                                         {"%k",
                                                                          [](std::string& wrk,
                                                                             arangodb::tp_sys_clock_ms const& tp) {
                                                                            iso_week::year_weeknum_weekday yww{
                                                                                floor<date::days>(tp)};
                                                                            uint64_t isoWeek =
                                                                                static_cast<unsigned>(
                                                                                    yww.weeknum());
                                                                            wrk.append(std::to_string(isoWeek));
                                                                          }},
                                                                         {"%l",
                                                                          [](std::string& wrk,
                                                                             arangodb::tp_sys_clock_ms const& tp) {
                                                                            year_month_day ymd{
                                                                                floor<date::days>(tp)};
                                                                            if (ymd.year()
                                                                                    .is_leap()) {
                                                                              wrk.append(
                                                                                  "1");
                                                                            } else {
                                                                              wrk.append(
                                                                                  "0");
                                                                            }
                                                                          }},
                                                                         {"%q",
                                                                          [](std::string& wrk,
                                                                             arangodb::tp_sys_clock_ms const& tp) {
                                                                            year_month_day ymd{
                                                                                floor<date::days>(tp)};
                                                                            month m = ymd.month();
                                                                            uint64_t part = static_cast<uint64_t>(
                                                                                ceil(unsigned(m) / 3.0f));
                                                                            TRI_ASSERT(part <= 4);
                                                                            wrk.append(std::to_string(part));
                                                                          }},
                                                                         {"%a",
                                                                          [](std::string& wrk,
                                                                             arangodb::tp_sys_clock_ms const& tp) {
                                                                            auto ymd = year_month_day{
                                                                                floor<date::days>(tp)};
                                                                            auto lastMonthDay =
                                                                                ymd.year() /
                                                                                ymd.month() / last;
                                                                            wrk.append(std::to_string(static_cast<unsigned>(
                                                                                lastMonthDay
                                                                                    .day())));
                                                                          }},
                                                                         {"%%",
                                                                          [](std::string& wrk,
                                                                             arangodb::tp_sys_clock_ms const& tp) {
                                                                            wrk.append(
                                                                                "%");
                                                                          }},
                                                                         {"%", [](std::string& wrk,
                                                                                  arangodb::tp_sys_clock_ms const& tp) {
                                                                          }}};

// will be populated by DateRegexInitializer
std::regex dateFormatRegex;

std::regex const iso8601Regex(
    "^\\d+(\\-\\d{1,2}(\\-\\d{1,2})?)?"  // YY[YY]-MM-DD
    "("
    "("
    // Time is optional
    "(\\ |T)"                     // T or blank separates date and time
    "\\d\\d\\:\\d\\d"             // time: hh:mm
    "(\\:\\d\\d(\\.\\d{1,})?)?"   // Optional: :ss.mmms
    "("
    "z|Z|"  // trailing Z or start of timezone
    "(\\+|\\-)"
    "\\d?\\d\\:\\d\\d"  // timezone hh:mm
    ")?"
    ")|"
    "(z|Z)"  // Z
    ")?", std::regex::optimize);

/* REGEX GROUPS
12:34:56.789-12:34
submatch 0: '12:34:56.789-12:34'
submatch 1: '12'
submatch 2: '34'
submatch 3: ':56.789'
submatch 4: '56'
submatch 5: '.789'
submatch 6: '789'
submatch 7: '-12:34'
submatch 8: '-'
submatch 9: '12'
submatch 10: '34'
*/

std::regex const timeRegex(
    "^(\\d\\d)\\:(\\d\\d)(\\:(\\d\\d)(\\.(\\d{1,}))?)?((\\+|\\-)(\\d?\\d)\\:"
    "(\\d\\d))?", std::regex::optimize);

/* REGEX GROUPS
P1Y2M3W4DT5H6M7.891S
  submatch 0: P1Y2M3W4DT5H6M7.891S
  submatch 1: 1Y
  submatch 2: 1
  submatch 3: 2M
  submatch 4: 2
  submatch 5: 3W
  submatch 6: 3
  submatch 7: 4D
  submatch 8: 4
  submatch 9: T5H6M7.891S
  submatch 10: 5H
  submatch 11: 5
  submatch 12: 6M
  submatch 13: 6
  submatch 14: 7.891S
  submatch 15: 7
  submatch 16: .891
  submatch 17: 891
*/
std::regex const durationRegex(
    "^P((\\d+)Y)?((\\d+)M)?((\\d+)W)?((\\d+)D)?(T((\\d+)H)?((\\d+)M)?((\\d+)(\\."
    "(\\d{1,3}))?S)?)?", std::regex::optimize);

struct DateRegexInitializer {
  DateRegexInitializer() {
    std::string myregex;

    dateMap.reserve(sortedDateMap.size());
    std::for_each(sortedDateMap.begin(), sortedDateMap.end(),
                  [&myregex](std::pair<std::string const&, format_func_t> const& p) {
                    (myregex.length() > 0) ? myregex += "|" + p.first : myregex = p.first;
                    dateMap.insert(std::make_pair(p.first, p.second));
                  });
    dateFormatRegex = std::regex(myregex);
  }
};

std::string executeDateFormatRegex(std::string const& search, 
                                   arangodb::tp_sys_clock_ms const& tp) {
  std::string s;

  auto first = search.begin();
  auto last = search.end();
  typename std::smatch::difference_type positionOfLastMatch = 0;
  auto endOfLastMatch = first;

  auto callback = [&tp, &endOfLastMatch, &positionOfLastMatch, &s](std::smatch const& match) {
    auto positionOfThisMatch = match.position(0);
    auto diff = positionOfThisMatch - positionOfLastMatch;

    auto startOfThisMatch = endOfLastMatch;
    std::advance(startOfThisMatch, diff);

    s.append(endOfLastMatch, startOfThisMatch);
    auto got = dateMap.find(match.str(0));
    if (got != dateMap.end()) {
      got->second(s, tp);
    }
    auto lengthOfMatch = match.length(0);

    positionOfLastMatch = positionOfThisMatch + lengthOfMatch;

    endOfLastMatch = startOfThisMatch;
    std::advance(endOfLastMatch, lengthOfMatch);
  };

  std::regex_iterator<std::string::const_iterator> end;
  std::regex_iterator<std::string::const_iterator> begin(first, last, dateFormatRegex);
  std::for_each(begin, end, callback);

  s.append(endOfLastMatch, last);

  return s;
}

// populates dateFormatRegex
static DateRegexInitializer const initializer;

}  // namespace

bool arangodb::basics::parseDateTime(arangodb::velocypack::StringRef dateTime, 
                                     arangodb::tp_sys_clock_ms& date_tp) {
  using namespace date;
  using namespace std::chrono;
 
  // trim input string
  while (!dateTime.empty()) {
    char c = dateTime.front();
    if (c != ' ' && c != '\t' && c != '\r' && c != '\n') {
      break;
    }
    dateTime = dateTime.substr(1);
  }

  if (!dateTime.empty()) {
    if (dateTime.front() == '+') {
      // skip over initial +
      dateTime = dateTime.substr(1);
    } else if (dateTime.front() == '-') {
      // we can't handle negative date values at all
      return false;
    }
  }

  while (!dateTime.empty()) {
    char c = dateTime.back();
    if (c != ' ' && c != '\t' && c != '\r' && c != '\n') {
      break;
    }
    dateTime.pop_back();
  }
      
  if (!std::regex_match(dateTime.begin(), dateTime.end(), ::iso8601Regex)) {
    LOG_TOPIC("f19ee", DEBUG, arangodb::Logger::FIXME)
        << "regex failed for datetime '" << dateTime << "'";
    return false;
  }

  if (dateTime.back() == 'Z' || dateTime.back() == 'z') {
    dateTime.pop_back();
  }

  LOG_TOPIC("c3c7a", TRACE, arangodb::Logger::FIXME) << "parse datetime '" << dateTime << "'";

  size_t pos = dateTime.find('T');
  if (pos == std::string::npos) {
    pos = dateTime.find(' ');
  }

  arangodb::velocypack::StringRef strDate(dateTime);
  arangodb::velocypack::StringRef strTime;

  // split into ymd / time
  if (pos != std::string::npos) {
    strDate = arangodb::velocypack::StringRef(dateTime.data(), pos);
    strTime = arangodb::velocypack::StringRef(dateTime.data() + pos + 1, dateTime.size() - pos - 1);
  }

  // parse date component, moves pointer "p" forward
  // returns true if "p" pointed to a valid number, and false otherwise
  auto parseDateComponent = [](char const*& p, char const* e, int& result) -> bool {
    char const* s = p;  // remember initial start
    if (p < e && *p == '-') {
      // skip over initial '-'
      ++p;
    }
    while (p < e && *p >= '0' && *p <= '9') {
      ++p;
    }
    if (p == s) {
      // did not find any valid character
      return false;
    }
    result = NumberUtils::atoi_unchecked<int>(s, p);
    if (p < e && *p == '-') {
      ++p;
    }
    return true;
  };

  char const* p = strDate.data();
  char const* e = p + strDate.size();

  // month and day are optional, so they intentionally default to 1
  int parsedYear, parsedMonth = 1, parsedDay = 1;

  // at least year must be valid
  if (!parseDateComponent(p, e, parsedYear)) {
    return false;
  }

  if (ADB_UNLIKELY(parsedYear < 0 || parsedYear > 9999)) {
    // outside the allowed range
    return false;
  }

  parseDateComponent(p, e, parsedMonth);
  parseDateComponent(p, e, parsedDay);

  if (ADB_UNLIKELY(parsedMonth < 1 || parsedMonth > 12 || parsedDay < 1 || parsedDay > 31)) {
    // definitely invalid
    return false;
  }

  LOG_TOPIC("29671", DEBUG, arangodb::Logger::FIXME)
      << "parsed YMD " << parsedYear << " " << parsedMonth << " " << parsedDay;

  date_tp = sys_days(year{parsedYear} / parsedMonth / parsedDay);

  // parse Time HH:MM:SS(.SSS)((+|-)HH:MM)
  if (1 < strTime.size()) {
    std::match_results<char const*> timeParts;

    if (!std::regex_match(strTime.begin(), strTime.end(), timeParts, ::timeRegex)) {
      LOG_TOPIC("33cf9", DEBUG, arangodb::Logger::FIXME) << "regex failed for time " << strTime;
      return false;
    }
    
    int v;
    char const* begin;

    // hour
    begin = strTime.data() + timeParts.position(1);
    v = NumberUtils::atoi_unchecked<int>(begin, begin + timeParts.length(1));
    hours parsedHours{v};

    if (hours{23} < parsedHours) {
      return false;
    }
    date_tp += parsedHours;

    // minute
    begin = strTime.data() + timeParts.position(2);
    v = NumberUtils::atoi_unchecked<int>(begin, begin + timeParts.length(2));
    minutes parsedMinutes{v};

    if (minutes{59} < parsedMinutes) {
      return false;
    }
    date_tp += parsedMinutes;

    // seconds
    begin = strTime.data() + timeParts.position(4);
    v = NumberUtils::atoi_unchecked<int>(begin, begin + timeParts.length(4));
    seconds parsedSeconds{v};

    if (seconds{59} < parsedSeconds) {
      return false;
    }
    date_tp += parsedSeconds;

    // milliseconds .9 -> 900ms
    std::size_t matchLength = timeParts.length(6);
    if (matchLength > 0) {    
      if (matchLength > 3) {
        matchLength = 3;
      }
      begin = strTime.data() + timeParts.position(6);
      v = NumberUtils::atoi_unchecked<int>(begin, begin + matchLength);
      if (matchLength == 2) {
        v *= 10;
      } else if (matchLength == 1) {
        v *= 100;
      }
      date_tp += milliseconds{v};
    }

    // timezone offset
    if (timeParts.length(8) > 0) {
      // hours
      begin = strTime.data() + timeParts.position(9);
      v = NumberUtils::atoi_unchecked<int>(begin, begin + timeParts.length(9));
      hours parsedHours{v}; 

      if (hours{23} < parsedHours) {
        return false;
      }
      minutes offset = parsedHours;

      // minutes
      begin = strTime.data() + timeParts.position(10);
      v = NumberUtils::atoi_unchecked<int>(begin, begin + timeParts.length(10));
      minutes parsedMinutes{v}; 

      if (minutes{59} < parsedMinutes) {
        return false;
      }
      offset += parsedMinutes;

      if (timeParts.length(8) == 1 &&
          strTime[timeParts.position(8)] == '-') {
        // minus sign!
        offset *= -1;
      }

      if (offset.count() != 0) {
        // apply timezone adjustment
        date_tp -= offset;

        // revalidate date after timezone adjustment
        auto ymd = year_month_day(floor<date::days>(date_tp));
        int year = static_cast<int>(ymd.year());
        if (year < 0 || year > 9999) {
          return false;
        }
      }
    }
  }  // if
  
  return true;
}

bool arangodb::basics::regexIsoDuration(arangodb::velocypack::StringRef isoDuration,
                                        std::match_results<char const*>& durationParts) {
  if (isoDuration.length() <= 1) {
    return false;
  }

  return std::regex_match(isoDuration.begin(), isoDuration.end(), durationParts, ::durationRegex);
}

std::string arangodb::basics::formatDate(std::string const& formatString,
                                         arangodb::tp_sys_clock_ms const& dateValue) {
  return ::executeDateFormatRegex(formatString, dateValue);
}

