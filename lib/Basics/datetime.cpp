////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2021 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
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

#include <date/iso_week.h>

#include "Basics/NumberUtils.h"
#include "Basics/datetime.h"
#include "Basics/debugging.h"
#include "Logger/LogMacros.h"

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
                                                                                ::weekDayNames[wd.c_encoding()]);
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
                                                                            wrk.append(weekDayNamesShort[wd.c_encoding()]);
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
                                                                                wd.c_encoding()));
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

struct ParsedDateTime {
  int year = 0;
  int month = 1;
  int day = 1;
  int hour = 0;
  int minute = 0;
  int second = 0;
  int millisecond = 0;
  int tzOffsetHour = 0;
  int tzOffsetMinute = 0;
};

/// @brief parses a number value, and returns its length
int parseNumber(arangodb::velocypack::StringRef const& dateTime, int& result) {
  char const* p = dateTime.data();
  char const* e = p + dateTime.size();

  while (p != e) {
    char c = *p;
    if (c < '0' || c > '9') {
      break;
    }
    ++p;
  }
  bool valid;
  result = arangodb::NumberUtils::atoi_positive<int>(dateTime.data(), p, valid);
  return static_cast<int>(p - dateTime.data());
}

bool parseDateTime(arangodb::velocypack::StringRef dateTime,
                   ParsedDateTime& result) {
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

  // year
  int length = parseNumber(dateTime, result.year);
  if (length == 0 || result.year > 9999) {
    return false;
  }
  if (ADB_UNLIKELY(length > 4)) {
    // we must have at least 4 digits for the year, however, we
    // allow any amount of leading zeroes
    size_t i = 0;
    while (dateTime[i] == '0') {
      ++i;
    }
    if (length - i > 4) {
      return false;
    }
  }
  dateTime = dateTime.substr(length);

  if (!dateTime.empty() && dateTime.front() == '-') {
    // month
    dateTime = dateTime.substr(1);

    length = parseNumber(dateTime, result.month);
    if (length == 0 || length > 2 || result.month < 1 || result.month > 12) {
      return false;
    }
    dateTime = dateTime.substr(length);
  
    if (!dateTime.empty() && dateTime.front() == '-') {
      // day
      dateTime = dateTime.substr(1);

      length = parseNumber(dateTime, result.day);
      if (length == 0 || length > 2 || result.day < 1 || result.day > 31) {
        return false;
      }
      dateTime = dateTime.substr(length);
    }
  }

  if (!dateTime.empty() && (dateTime.front() == ' ' || dateTime.front() == 'T')) {
    // time part following
    dateTime = dateTime.substr(1);
      
    // hour
    length = parseNumber(dateTime, result.hour);
    if (length == 0 || length > 2 || result.hour > 23) {
      return false;
    }
    dateTime = dateTime.substr(length);
  
    if (dateTime.empty() || dateTime.front() != ':') {
      return false;
    }
    
    dateTime = dateTime.substr(1);

    // minute
    length = parseNumber(dateTime, result.minute);
    if (length == 0 || length > 2 || result.minute > 59) {
      return false;
    }
    dateTime = dateTime.substr(length);
  
    if (!dateTime.empty() && dateTime.front() == ':') {
      dateTime = dateTime.substr(1);

      // second
      length = parseNumber(dateTime, result.second);
      if (length == 0 || length > 2 || result.second > 59) {
        return false;
      }
      dateTime = dateTime.substr(length);

      if (!dateTime.empty() && dateTime.front() == '.') {
        dateTime = dateTime.substr(1);

        // millisecond
        length = parseNumber(dateTime, result.millisecond);
        if (length == 0) {
          return false;
        }
        if (length >= 3) {
          // restrict milliseconds length to 3 digits
          parseNumber(dateTime.substr(0, 3), result.millisecond);
        } else if (length == 2) {
          result.millisecond *= 10;
        } else if (length == 1) {
          result.millisecond *= 100;
        }
        dateTime = dateTime.substr(length);
      }
    }
  }
    
  if (!dateTime.empty()) {
    if (dateTime.front() == 'z' || dateTime.front() == 'Z') {
      // z|Z timezone
      dateTime = dateTime.substr(1);
    } else if (dateTime.front() == '+' || dateTime.front() == '-') {
      // +|- timezone adjustment
      int factor = dateTime.front() == '+' ? 1 : -1;
      
      dateTime = dateTime.substr(1);

      // tz adjustment hours
      length = parseNumber(dateTime, result.tzOffsetHour);
      if (length == 0 || length > 2 || result.tzOffsetHour > 23) {
        return false;
      }
      result.tzOffsetHour *= factor;
      dateTime = dateTime.substr(length);
    
      if (dateTime.empty() || dateTime.front() != ':') {
        return false;
      }
      dateTime = dateTime.substr(1);
      
      // tz adjustment minutes
      length = parseNumber(dateTime, result.tzOffsetMinute);
      if (length == 0 || length > 2 || result.tzOffsetMinute > 59) {
        return false;
      }
      dateTime = dateTime.substr(length);
    }
  }
  
  return dateTime.empty();
}

}  // namespace

bool arangodb::basics::parseDateTime(arangodb::velocypack::StringRef dateTime,
                                     arangodb::tp_sys_clock_ms& date_tp) {
  ::ParsedDateTime result;
  if (!::parseDateTime(dateTime, result)) {
    return false;
  }

  using namespace date;
  using namespace std::chrono;

  date_tp = sys_days(year{result.year} / result.month / result.day);
  date_tp += hours{result.hour};
  date_tp += minutes{result.minute};
  date_tp += seconds{result.second};
  date_tp += milliseconds{result.millisecond};

  if (result.tzOffsetHour != 0 || result.tzOffsetMinute != 0) {
    minutes offset = hours{result.tzOffsetHour};
    offset += minutes{result.tzOffsetMinute};

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

  // LOG_TOPIC("51643", TRACE, Logger::FIXME)
  //           << "year: " << result.year 
  //           << ", month: " << result.month 
  //           << ", day: " << result.day 
  //           << ", hour: " << result.hour 
  //           << ", minute: " << result.minute 
  //           << ", second: " << result.second 
  //           << ", millisecond: " << result.millisecond 
  //           << ", tz hour: " << result.tzOffsetHour 
  //           << ", tz minute: " << result.tzOffsetMinute;
  
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

bool arangodb::basics::parseIsoDuration(arangodb::velocypack::StringRef duration,
                                        arangodb::basics::ParsedDuration& ret) {
  using namespace arangodb;

  std::match_results<char const*> durationParts;
  if (!arangodb::basics::regexIsoDuration(duration, durationParts)) {
    return false;
  }

  char const* begin;

  begin = duration.data() + durationParts.position(2);
  ret.years = NumberUtils::atoi_unchecked<int>(begin, begin + durationParts.length(2));

  begin = duration.data() + durationParts.position(4);
  ret.months = NumberUtils::atoi_unchecked<int>(begin, begin + durationParts.length(4));

  begin = duration.data() + durationParts.position(6);
  ret.weeks = NumberUtils::atoi_unchecked<int>(begin, begin + durationParts.length(6));

  begin = duration.data() + durationParts.position(8);
  ret.days = NumberUtils::atoi_unchecked<int>(begin, begin + durationParts.length(8));

  begin = duration.data() + durationParts.position(11);
  ret.hours = NumberUtils::atoi_unchecked<int>(begin, begin + durationParts.length(11));

  begin = duration.data() + durationParts.position(13);
  ret.minutes = NumberUtils::atoi_unchecked<int>(begin, begin + durationParts.length(13));

  begin = duration.data() + durationParts.position(15);
  ret.seconds = NumberUtils::atoi_unchecked<int>(begin, begin + durationParts.length(15));

  // The Milli seconds can be shortened:
  // .1 => 100ms
  // so we append 00 but only take the first 3 digits
  auto matchLength = durationParts.length(17);
  int number = 0;
  if (matchLength > 0) {
    if (matchLength > 3) {
      matchLength = 3;
    }
    begin = duration.data() + durationParts.position(17);
    number = NumberUtils::atoi_unchecked<int>(begin, begin + matchLength);
    if (matchLength == 2) {
      number *= 10;
    } else if (matchLength == 1) {
      number *= 100;
    }
  }
  ret.milliseconds = number;

  return true;
}
