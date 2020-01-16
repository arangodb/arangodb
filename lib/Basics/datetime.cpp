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

#include <tao/json.hpp>

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

namespace {
  using namespace tao::json::pegtl;

  //2012-10-06T04:13:00+00:00

  struct dash : internal::one< internal::result_on_found::success, internal::peek_char, '-' > {};
  struct colon : internal::one< internal::result_on_found::success, internal::peek_char, ':' > {};
  struct dot : internal::one< internal::result_on_found::success, internal::peek_char, '.' > {};
  struct T : internal::one< internal::result_on_found::success, internal::peek_char, 'T' > {};
  struct W : internal::one< internal::result_on_found::success, internal::peek_char, 'W' > {};
  struct Z : internal::one< internal::result_on_found::success, internal::peek_char, 'Z' > {};
  struct plus_minus : internal::one< internal::result_on_found::success, internal::peek_char, '+', '-' > {};

  struct zero_or_one : internal::one< internal::result_on_found::success, internal::peek_char, '0', '1' > {};
  struct zero_to_three : internal::one< internal::result_on_found::success, internal::peek_char, '0', '1', '2', '3' > {};
  struct zero_to_five : internal::one< internal::result_on_found::success, internal::peek_char, '0', '1', '2', '3', '4', '5' > {};

  struct YYYY : seq<digit, digit, digit, digit> {};
  struct YY : seq<digit, digit> {};
  struct MM : seq<zero_or_one, digit> {};
  struct DD : seq<zero_to_three, digit> {};
  struct cdate : seq<sor<YYYY,YY>, dash, MM, dash, DD> {};
  struct date : cdate {};

  struct hh : seq<zero_to_five, digit> {};
  struct mm : seq<zero_to_five, digit> {};
  struct ss : seq<zero_to_five, digit> {};
  struct sss : seq<digit, digit, digit> {};
  struct dot_sss : seq<dot, sss> {};
  struct hhcmmcss : seq<hh, opt<colon, mm, opt<colon, ss, opt<dot_sss>>>>  {};
  struct hhmmss : seq<hh, opt<mm, opt<ss, opt<dot_sss>>>>  {};


  struct zhh : seq<zero_to_five, digit> {};
  struct zmm : seq<zero_to_five, digit> {};
  struct TZD : sor<Z, seq<opt<plus_minus>, zhh, colon, zmm>> {};
  struct time : sor<hhcmmcss, hhmmss, opt<TZD>> {};

  struct iso8601 : seq<date, opt<seq<opt<T>, time>>> {};

  // default action
  template<typename rule>
  struct iso_action : nothing<rule>{};

  // actions for tokens
  template<>
  struct iso_action<YYYY>{
      template <typename input>
      static void apply(input const& in, ParsedDateTime& state){
          state.year = arangodb::NumberUtils::atoi_positive_unchecked<int>(in.begin(), in.end());
      }
	};

  template<>
  struct iso_action<MM>{
      template <typename input>
      static void apply(input const& in, ParsedDateTime& state){
          state.month = arangodb::NumberUtils::atoi_positive_unchecked<int>(in.begin(), in.end());
      }
	};

	template<>
  struct iso_action<DD>{
      template <typename input>
      static void apply(input const& in, ParsedDateTime& state){
          state.day = arangodb::NumberUtils::atoi_positive_unchecked<int>(in.begin(), in.end());
      }
	};

  template<>
  struct iso_action<hh>{
      template <typename input>
      static void apply(input const& in, ParsedDateTime& state){
          state.hour = arangodb::NumberUtils::atoi_positive_unchecked<int>(in.begin(), in.end());
      }
	};

  template<>
  struct iso_action<mm>{
      template <typename input>
      static void apply(input const& in, ParsedDateTime& state){
          state.minute = arangodb::NumberUtils::atoi_positive_unchecked<int>(in.begin(), in.end());
      }
	};

  template<>
  struct iso_action<ss>{
      template <typename input>
      static void apply(input const& in, ParsedDateTime& state){
          state.second = arangodb::NumberUtils::atoi_positive_unchecked<int>(in.begin(), in.end());
      }
	};

  template<>
  struct iso_action<sss>{
      template <typename input>
      static void apply(input const& in, ParsedDateTime& state){
          state.millisecond = arangodb::NumberUtils::atoi_positive_unchecked<int>(in.begin(), in.end());
      }
	};

  template<>
  struct iso_action<zhh>{
      template <typename input>
      static void apply(input const& in, ParsedDateTime& state){
          state.tzOffsetHour = arangodb::NumberUtils::atoi_positive_unchecked<int>(in.begin(), in.end());
      }
	};

template<>
  struct iso_action<zmm>{
      template <typename input>
      static void apply(input const& in, ParsedDateTime& state){
          state.tzOffsetMinute = arangodb::NumberUtils::atoi_positive_unchecked<int>(in.begin(), in.end());
      }
	};
}

bool parseDateTimePEGTL(arangodb::velocypack::StringRef dateTime,
                   ParsedDateTime& result) {

  using namespace tao::json;
  pegtl::memory_input<tao::json::pegtl::tracking_mode::lazy, tao::json::pegtl::eol::crlf > in( dateTime.begin(), dateTime.end(), "datatime.cpp" );
  auto parse_result = pegtl::parse<iso8601, iso_action>(in, result);
  return parse_result;
}

}  // namespace

bool arangodb::basics::parseDateTime(arangodb::velocypack::StringRef dateTime,
                                     arangodb::tp_sys_clock_ms& date_tp) {
  ::ParsedDateTime result;
  if (!::parseDateTimePEGTL(dateTime, result)) {
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
      auto ymd = year_month_day(floor<::date::days>(date_tp));
      int year = static_cast<int>(ymd.year());
      if (year < 0 || year > 9999) {
        return false;
      }
    }
  }

  // LOG_DEVEL << "year: " << result.year
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

