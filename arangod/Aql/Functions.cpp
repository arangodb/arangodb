////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2016 ArangoDB GmbH, Cologne, Germany
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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#include "Functions.h"

#include "ApplicationFeatures/ApplicationServer.h"
#include "ApplicationFeatures/LanguageFeature.h"
#include "Aql/AqlFunctionFeature.h"
#include "Aql/Expression.h"
#include "Aql/Function.h"
#include "Aql/Query.h"
#include "Aql/RegexCache.h"
#include "Aql/V8Executor.h"
#include "Basics/Exceptions.h"
#include "Basics/StringBuffer.h"
#include "Basics/StringRef.h"
#include "Basics/StringUtils.h"
#include "Basics/Utf8Helper.h"
#include "Basics/VPackStringBufferAdapter.h"
#include "Basics/VelocyPackHelper.h"
#include "Basics/fpconv.h"
#include "Basics/tri-strings.h"
#include "V8/v8-vpack.h"
#include "GeneralServer/AuthenticationFeature.h"
#include "Geo/GeoParams.h"
#include "Geo/GeoUtils.h"
#include "Geo/GeoJson.h"
#include "Geo/ShapeContainer.h"
#include "Indexes/Index.h"
#include "Logger/Logger.h"
#include "Pregel/PregelFeature.h"
#include "Pregel/Worker.h"
#include "Random/UniformCharacter.h"
#include "Ssl/SslInterface.h"
#include "Transaction/Context.h"
#include "Transaction/Helpers.h"
#include "Transaction/Methods.h"
#include "Utils/CollectionNameResolver.h"
#include "Utils/ExecContext.h"
#include "V8Server/v8-collection.h"
#include "VocBase/KeyGenerator.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/ManagedDocumentResult.h"

#include <s2/s2loop.h>
#include <date/date.h>
#include <date/iso_week.h>

#include <unicode/stsearch.h>
#include <unicode/uchar.h>
#include <unicode/unistr.h>
#include <unicode/stsearch.h>
#include <unicode/schriter.h>

#include <velocypack/Collection.h>
#include <velocypack/Dumper.h>
#include <velocypack/Iterator.h>
#include <velocypack/velocypack-aliases.h>
#include <algorithm>
#include <regex>

using namespace arangodb;
using namespace arangodb::aql;
using namespace std::chrono;
using namespace date;

/*
- always specify your user facing function name MYFUNC in error generators
- errors are broadcasted like this:
    - Wrong parameter types: ::registerInvalidArgumentWarning(query, "MYFUNC")
    - Generic errors: ::registerWarning(query, "MYFUNC", TRI_ERROR_QUERY_INVALID_REGEX);
    - ICU related errors: if (U_FAILURE(status)) { ::registerICUWarning(query, "MYFUNC", status); }
    - close with: return AqlValue(AqlValueHintNull());
- specify the number of parameters you expect at least and at max using:
  ValidateParameters(parameters, "MYFUNC", 1, 3); (min: 1, max: 3); Max is optional.
- if you support optional parameters, first check whether the count is sufficient
  using parameters.size()
- fetch the values using:
  AqlValue value
  - Anonymous  = ExtractFunctionParameterValue(parameters, 0);
  - ::getBooleanParameter() if you expect a bool
  - Stringify() if you need a string.
  - ::extractKeys() if its an object and you need the keys
  - ::extractCollectionName() if you expect a collection
  - ::listContainsElement() search for a member
  - ::parameterToTimePoint / DateFromParameters get a time string as date.
- check the values whether they match your expectations i.e. using:
  - param.isNumber() then extract it using: param.toInt64(trx)
- Available helper functions for working with parameters:
  - ::variance()
  - ::sortNumberList()
  - ::unsetOrKeep ()
  - ::getDocumentByIdentifier ()
  - ::mergeParameters()
  - ::flattenList()

- now do your work with the parameters
- build up a result using a VPackbuilder like you would with regular velocpyack.
- return it wrapping it into an AqlValue

 */

namespace {

enum DateSelectionModifier {
  INVALID = 0,
  MILLI,
  SECOND,
  MINUTE,
  HOUR,
  DAY,
  WEEK,
  MONTH,
  YEAR
};
static_assert(DateSelectionModifier::INVALID < DateSelectionModifier::MILLI,
              "incorrect date selection order");
static_assert(DateSelectionModifier::MILLI < DateSelectionModifier::SECOND,
              "incorrect date selection order");
static_assert(DateSelectionModifier::SECOND < DateSelectionModifier::MINUTE,
              "incorrect date selection order");
static_assert(DateSelectionModifier::MINUTE < DateSelectionModifier::HOUR,
              "incorrect date selection order");
static_assert(DateSelectionModifier::HOUR < DateSelectionModifier::DAY,
              "incorrect date selection order");
static_assert(DateSelectionModifier::DAY < DateSelectionModifier::WEEK,
              "incorrect date selection order");
static_assert(DateSelectionModifier::WEEK < DateSelectionModifier::MONTH,
              "incorrect date selection order");
static_assert(DateSelectionModifier::MONTH < DateSelectionModifier::YEAR,
              "incorrect date selection order");

typedef void(*format_func_t)(std::string& wrk, tp_sys_clock_ms const&);
std::unordered_map<std::string, format_func_t> dateMap;
auto const unixEpoch = date::sys_seconds{seconds{0}};

// will be populated by Functions::Init()
std::regex theDateFormatRegex;

std::string executeDateFormatRegex(std::string const& search, tp_sys_clock_ms const& tp) {
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
    auto got = ::dateMap.find(match.str(0));
    if (got != ::dateMap.end()) {
      got->second(s, tp);
    }
    auto lengthOfMatch = match.length(0);

    positionOfLastMatch = positionOfThisMatch + lengthOfMatch;

    endOfLastMatch = startOfThisMatch;
    std::advance(endOfLastMatch, lengthOfMatch);
  };

  std::regex_iterator<std::string::const_iterator> end;
  std::regex_iterator<std::string::const_iterator> begin(first, last, ::theDateFormatRegex);
  std::for_each(begin, end, callback);

  s.append(endOfLastMatch, last);

  return s;
}

std::string tail(std::string const& source, size_t const length) {
  if (length >= source.size()) { 
    return source; 
  }
  return source.substr(source.size() - length);
} // tail

std::vector<std::string> const monthNames = {
  "January",
  "February",
  "March",
  "April",
  "May",
  "June",
  "July",
  "August",
  "September",
  "October",
  "November",
  "December"
};

std::vector<std::string> const monthNamesShort = {
  "Jan",
  "Feb",
  "Mar",
  "Apr",
  "May",
  "Jun",
  "Jul",
  "Aug",
  "Sep",
  "Oct",
  "Nov",
  "Dec"
};

std::vector<std::string> const weekDayNames = {
  "Sunday",
  "Monday",
  "Tuesday",
  "Wednesday",
  "Thursday",
  "Friday",
  "Saturday"
};

std::vector<std::string> const weekDayNamesShort = {
  "Sun",
  "Mon",
  "Tue",
  "Wed",
  "Thu",
  "Fri",
  "Sat"
};

std::vector<std::pair<std::string, format_func_t>> const sortedDateMap = {
  {"%&", [](std::string& wrk, tp_sys_clock_ms const& tp) { }}, // Allow for literal "m" after "%m" ("%mm" -> %m%&m)
  // zero-pad 4 digit years to length of 6 and add "+" prefix, keep negative as-is
  {"%yyyyyy", [](std::string& wrk, tp_sys_clock_ms const& tp) {
      auto ymd = year_month_day(floor<days>(tp));
      auto yearnum = static_cast<int>(ymd.year());
      if (yearnum < 0) {
        if (yearnum > -10) {
          wrk.append("-00000");
        } else if (yearnum > -100) {
          wrk.append("-0000");
        } else if (yearnum > -1000) {
          wrk.append("-000");
        } else if (yearnum > -10000) {
          wrk.append("-00");
        } else if (yearnum > -100000) {
          wrk.append("-0");
        } else {
          wrk.append("-");
        }
        wrk.append(std::to_string(abs(yearnum)));
        return;
      }
      if (yearnum > 99999) {
        // intentionally nothing
      } else if (yearnum > 9999) {
        wrk.append("+0");
      } else if (yearnum > 999) {
        wrk.append("+00");
      } else if (yearnum > 99) {
        wrk.append("+000");
      } else if (yearnum > 9) {
        wrk.append("+0000");
      } else if (yearnum >= 0) {
        wrk.append("+00000");
      } else {
        wrk.append("+");
      }
      wrk.append(std::to_string(yearnum));
    }},
  {"%mmmm", [](std::string& wrk, tp_sys_clock_ms const& tp) {
      auto ymd = year_month_day(floor<days>(tp));
      wrk.append(::monthNames[static_cast<unsigned>(ymd.month()) - 1]);
    }},
  {"%yyyy", [](std::string& wrk, tp_sys_clock_ms const& tp) {
      auto ymd = year_month_day(floor<days>(tp));
      auto yearnum = static_cast<int>(ymd.year());
      if (yearnum < 0) {
        if (yearnum > -10) {
          wrk.append("-000");
        } else if (yearnum > -100) {
          wrk.append("-00");
        } else if (yearnum > -1000) {
          wrk.append("-0");
        } else {
          wrk.append("-");
        }
        wrk.append(std::to_string(abs(yearnum)));
      }
      else {
        if (yearnum < 0) {
          wrk.append("0000");
          wrk.append(std::to_string(yearnum));
        } else if (yearnum < 9) {
          wrk.append("000");
          wrk.append(std::to_string(yearnum));
        } else if (yearnum < 99) {
          wrk.append("00");
          wrk.append(std::to_string(yearnum));
        } else if (yearnum < 999) {
          wrk.append("0");
          wrk.append(std::to_string(yearnum));
        } else {
          std::string yearstr(std::to_string(yearnum));
          wrk.append(::tail(yearstr, 4));
        }
      }
    }},

  {"%wwww", [](std::string& wrk, tp_sys_clock_ms const& tp) {
      weekday wd{floor<days>(tp)};
      wrk.append(::weekDayNames[static_cast<unsigned>(wd)]);
    }},

  {"%mmm", [](std::string& wrk, tp_sys_clock_ms const& tp) {
      auto ymd = year_month_day(floor<days>(tp));
      wrk.append(::monthNamesShort[static_cast<unsigned>(ymd.month()) - 1]);
    }},
  {"%www", [](std::string& wrk, tp_sys_clock_ms const& tp) {
      weekday wd{floor<days>(tp)};
      wrk.append(weekDayNamesShort[static_cast<unsigned>(wd)]);
    }},
  {"%fff", [](std::string& wrk, tp_sys_clock_ms const& tp) {
      auto day_time = make_time(tp - floor<days>(tp));
      uint64_t millis = day_time.subseconds().count();
      if (millis < 10) {
        wrk.append("00");
      } else if (millis < 100) {
        wrk.append("0");
      }
      wrk.append(std::to_string(millis));
    }},
  {"%xxx", [](std::string& wrk, tp_sys_clock_ms const& tp) {
      auto ymd = year_month_day(floor<days>(tp));
      auto yyyy = year{ymd.year()};
      // we construct the date with the first day in the year:
      auto firstDayInYear = yyyy / jan / day{0};
      uint64_t daysSinceFirst = duration_cast<days>(tp - sys_days(firstDayInYear)).count();
      if (daysSinceFirst < 10) {
        wrk.append("00");
      } else if (daysSinceFirst < 100) {
        wrk.append("0");
      } 
      wrk.append(std::to_string(daysSinceFirst));
    }},
  
  // there"s no really sensible way to handle negative years, but better not drop the sign
  {"%yy", [](std::string& wrk, tp_sys_clock_ms const& tp) {
      auto ymd = year_month_day(floor<days>(tp));
      auto yearnum = static_cast<int>(ymd.year());
      if (yearnum < 10 && yearnum > -10) {
        wrk.append("0");
        wrk.append(std::to_string(abs(yearnum)));
      } else {
        std::string yearstr(std::to_string(abs(yearnum)));
        wrk.append(tail(yearstr, 2));
      }
    }},
  {"%mm", [](std::string& wrk, tp_sys_clock_ms const& tp) {
      auto ymd = year_month_day(floor<days>(tp));
      auto month = static_cast<unsigned>(ymd.month());
      if (month < 10) {
        wrk.append("0");
      }
      wrk.append(std::to_string(month));
    }},
  {"%dd", [](std::string& wrk, tp_sys_clock_ms const& tp) {
      auto ymd = year_month_day(floor<days>(tp));
      auto day = static_cast<unsigned>(ymd.day());
      if (day < 10) {
        wrk.append("0");
      }
      wrk.append(std::to_string(day));
    }},
  {"%hh", [](std::string& wrk, tp_sys_clock_ms const& tp) {
      auto day_time = make_time(tp - floor<days>(tp));
      uint64_t hours = day_time.hours().count();
      if (hours < 10) {
        wrk.append("0");
      }
      wrk.append(std::to_string(hours));
    }},
  {"%ii", [](std::string& wrk, tp_sys_clock_ms const& tp) {
      auto day_time = make_time(tp - floor<days>(tp));
      uint64_t minutes = day_time.minutes().count();
      if (minutes < 10) {
        wrk.append("0");
      }
      wrk.append(std::to_string(minutes));
    }},
  {"%ss", [](std::string& wrk, tp_sys_clock_ms const& tp) {
      auto day_time = make_time(tp - floor<days>(tp));
      uint64_t seconds = day_time.seconds().count();
      if (seconds < 10) {
        wrk.append("0");
      }
      wrk.append(std::to_string(seconds));
    }},
  {"%kk", [](std::string& wrk, tp_sys_clock_ms const& tp) {
      iso_week::year_weeknum_weekday yww{floor<days>(tp)};
      uint64_t isoWeek = static_cast<unsigned>(yww.weeknum());
      if (isoWeek < 10) {
        wrk.append("0");
      }
      wrk.append(std::to_string(isoWeek));
    }},
  
  {"%t", [](std::string& wrk, tp_sys_clock_ms const& tp) {
      auto diffDuration = tp - unixEpoch;
      auto diff = duration_cast<duration<double, std::milli>>(diffDuration).count();
      wrk.append(std::to_string(static_cast<int64_t>(std::round(diff))));
    }},
  {"%z", [](std::string& wrk, tp_sys_clock_ms const& tp) {
      std::string formatted = format("%FT%TZ", floor<milliseconds>(tp));
      wrk.append(formatted);
    }},
  {"%w", [](std::string& wrk, tp_sys_clock_ms const& tp) {
      weekday wd{floor<days>(tp)};
      wrk.append(std::to_string(static_cast<unsigned>(wd)));
    }},
  {"%y", [](std::string& wrk, tp_sys_clock_ms const& tp) {
      auto ymd = year_month_day(floor<days>(tp));
      wrk.append(std::to_string(static_cast<int>(ymd.year())));
    }},
  {"%m", [](std::string& wrk, tp_sys_clock_ms const& tp) {
      auto ymd = year_month_day(floor<days>(tp));
      wrk.append(std::to_string(static_cast<unsigned>(ymd.month())));
    }},
  {"%d", [](std::string& wrk, tp_sys_clock_ms const& tp) {
      auto ymd = year_month_day(floor<days>(tp));
      wrk.append(std::to_string(static_cast<unsigned>(ymd.day())));
    }},
  {"%h", [](std::string& wrk, tp_sys_clock_ms const& tp) {
      auto day_time = make_time(tp - floor<days>(tp));
      uint64_t hours = day_time.hours().count();
      wrk.append(std::to_string(hours));
    }},
  {"%i", [](std::string& wrk, tp_sys_clock_ms const& tp) {
      auto day_time = make_time(tp - floor<days>(tp));
      uint64_t minutes = day_time.minutes().count();
      wrk.append(std::to_string(minutes));
    }},
  {"%s", [](std::string& wrk, tp_sys_clock_ms const& tp) {
      auto day_time = make_time(tp - floor<days>(tp));
      uint64_t seconds = day_time.seconds().count();
      wrk.append(std::to_string(seconds));
    }},
  {"%f", [](std::string& wrk, tp_sys_clock_ms const& tp) {
      auto day_time = make_time(tp - floor<days>(tp));
      uint64_t millis = day_time.subseconds().count();
      wrk.append(std::to_string(millis));
    }},
  {"%x", [](std::string& wrk, tp_sys_clock_ms const& tp) {
      auto ymd = year_month_day(floor<days>(tp));
      auto yyyy = year{ymd.year()};
      // We construct the date with the first day in the year:
      auto firstDayInYear = yyyy / jan / day{0};
      uint64_t daysSinceFirst = duration_cast<days>(tp - sys_days(firstDayInYear)).count();
      wrk.append(std::to_string(daysSinceFirst));
    }},
  {"%k", [](std::string& wrk, tp_sys_clock_ms const& tp) {
      iso_week::year_weeknum_weekday yww{floor<days>(tp)};
      uint64_t isoWeek = static_cast<unsigned>(yww.weeknum());
      wrk.append(std::to_string(isoWeek));
    }},
  {"%l", [](std::string& wrk, tp_sys_clock_ms const& tp) {
      year_month_day ymd{floor<days>(tp)};
      if (ymd.year().is_leap()) {
        wrk.append("1");
      } else {
        wrk.append("0");
      }
  }},
  {"%q", [](std::string& wrk, tp_sys_clock_ms const& tp) {
      year_month_day ymd{floor<days>(tp)};
      month m = ymd.month();
      uint64_t part = static_cast<uint64_t>(ceil(unsigned(m) / 3.0f));
      TRI_ASSERT(part <= 4);
      wrk.append(std::to_string(part));
    }},
  {"%a", [](std::string& wrk, tp_sys_clock_ms const& tp) {
      auto ymd = year_month_day{floor<days>(tp)};
      auto lastMonthDay = ymd.year() / ymd.month() / last;
      wrk.append(std::to_string(static_cast<unsigned>(lastMonthDay.day())));
    }},
  {"%%", [](std::string& wrk, tp_sys_clock_ms const& tp) { wrk.append("%"); }},
  {"%", [](std::string& wrk, tp_sys_clock_ms const& tp) { }}
};

/// @brief register warning
void registerWarning(arangodb::aql::Query* query, char const* fName, int code) {
  std::string msg;

  if (code == TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH) {
    msg = arangodb::basics::Exception::FillExceptionString(code, fName);
  } else {
    msg.append("in function '");
    msg.append(fName);
    msg.append("()': ");
    msg.append(TRI_errno_string(code));
  }

  query->registerWarning(code, msg.c_str());
}

/// @brief register warning
void registerWarning(arangodb::aql::Query* query, char const* fName, Result const& rr) {
  std::string msg = "in function '";
  msg.append(fName);
  msg.append("()': ");
  msg.append(rr.errorMessage());
  query->registerWarning(rr.errorNumber(), msg.c_str());
}

void registerICUWarning(arangodb::aql::Query* query,
                        char const* functionName,
                        UErrorCode status) {
  std::string msg;
  msg.append("in function '");
  msg.append(functionName);
  msg.append("()': ");
  msg.append(arangodb::basics::Exception::FillExceptionString(TRI_ERROR_ARANGO_ICU_ERROR,
                                                              u_errorName(status)));
  query->registerWarning(TRI_ERROR_ARANGO_ICU_ERROR, msg.c_str());
}

void registerError(arangodb::aql::Query* query, char const* fName, int code) {
  std::string msg;

  if (code == TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH) {
    msg = arangodb::basics::Exception::FillExceptionString(code, fName);
  } else {
    msg.append("in function '");
    msg.append(fName);
    msg.append("()': ");
    msg.append(TRI_errno_string(code));
  }

  query->registerError(code, msg.c_str());
}

/// @brief convert a number value into an AqlValue
inline AqlValue numberValue(transaction::Methods* trx, int value) {
  return AqlValue(AqlValueHintInt(value));
}

/// @brief convert a number value into an AqlValue
AqlValue numberValue(transaction::Methods* trx, double value, bool nullify) {
  if (std::isnan(value) || !std::isfinite(value) || value == HUGE_VAL ||
      value == -HUGE_VAL) {
    if (nullify) {
      // convert to null
      return AqlValue(AqlValueHintNull());
    }
    // convert to 0
    return AqlValue(AqlValueHintZero());
  }

  return AqlValue(AqlValueHintDouble(value));
}

inline AqlValue timeAqlValue(tp_sys_clock_ms const& tp) {
  std::string formatted = format("%FT%TZ", floor<milliseconds>(tp));
  return AqlValue(formatted);
}

DateSelectionModifier parseDateModifierFlag(VPackSlice flag) {
  if (!flag.isString()) {
    return INVALID;
  }

  std::string flagStr = flag.copyString();
  if (flagStr.empty()) {
    return INVALID;
  }
  TRI_ASSERT(flagStr.size() >= 1);

  std::transform(flagStr.begin(), flagStr.end(), flagStr.begin(), ::tolower);
  switch (flagStr.front()) {
    case 'y':
      if (flagStr == "years" || flagStr == "year" || flagStr == "y") {
        return YEAR;
      }
      break;
    case 'w':
      if (flagStr == "weeks" || flagStr == "week" || flagStr == "w") {
        return WEEK;
      }
      break;
    case 'm':
      if (flagStr == "months" || flagStr == "month" || flagStr == "m") {
        return MONTH;
      }
      // Can be minute as well
      if (flagStr == "minutes" || flagStr == "minute") {
        return MINUTE;
      }
      // Can be millisecond as well
      if (flagStr == "milliseconds" || flagStr == "millisecond") {
        return MILLI;
      }
      break;
    case 'd':
      if (flagStr == "days" || flagStr == "day" || flagStr == "d") {
        return DAY;
      }
      break;
    case 'h':
      if (flagStr == "hours" || flagStr == "hour" || flagStr == "h") {
        return HOUR;
      }
      break;
    case 's':
      if (flagStr == "seconds" || flagStr == "second" || flagStr == "s") {
        return SECOND;
      }
      break;
    case 'i':
      if (flagStr == "i") {
        return MINUTE;
      }
      break;
    case 'f':
      if (flagStr == "f") {
        return MILLI;
      }
      break;
  }
  // If we get here the flag is invalid
  return INVALID;
}

AqlValue addOrSubtractUnitFromTimestamp(Query* query,
                                        tp_sys_clock_ms const& tp,
                                        VPackSlice durationUnitsSlice,
                                        VPackSlice durationType,
                                        bool isSubtract) {
  bool isInteger = durationUnitsSlice.isInteger();
  double durationUnits = durationUnitsSlice.getNumber<double>();
  std::chrono::duration<double, std::ratio<1l, 1000l>> ms{};
  year_month_day ymd{floor<days>(tp)};
  auto day_time = make_time(tp - sys_days(ymd));

  DateSelectionModifier flag = ::parseDateModifierFlag(durationType);
  double intPart = 0.0;
  if (durationUnits < 0.0) {
    // Make sure duration is always positive. So we flip isSubtract in this
    // case.
    isSubtract = !isSubtract;
    durationUnits *= -1.0;
  }
  TRI_ASSERT(durationUnits >= 0.0);

  // All Fallthroughs intentional. We still have some remainder
  switch (flag) {
    case YEAR:
      durationUnits = std::modf(durationUnits, &intPart);
      if (isSubtract) {
        ymd -= years{static_cast<int64_t>(intPart)};
      } else {
        ymd += years{static_cast<int64_t>(intPart)};
      }
      if (isInteger || durationUnits == 0.0) {
        break;  // We are done
      }
      durationUnits *= 12;
      // intentionally falls through
    case MONTH:
      durationUnits = std::modf(durationUnits, &intPart);
      if (isSubtract) {
        ymd -= months{static_cast<int64_t>(intPart)};
      } else {
        ymd += months{static_cast<int64_t>(intPart)};
      }
      if (isInteger || durationUnits == 0.0) {
        break;  // We are done
      }
      durationUnits *= 30;  // 1 Month ~= 30 Days
      // intentionally falls through
    // After this fall through the date may actually a bit off
    case DAY:
      // From here on we do not need leap-day handling
      ms = days{1};
      ms *= durationUnits;
      break;
    case WEEK:
      ms = weeks{1};
      ms *= durationUnits;
      break;
    case HOUR:
      ms = hours{1};
      ms *= durationUnits;
      break;
    case MINUTE:
      ms = minutes{1};
      ms *= durationUnits;
      break;
    case SECOND:
      ms = seconds{1};
      ms *= durationUnits;
      break;
    case MILLI:
      ms = milliseconds{1};
      ms *= durationUnits;
      break;
    default:
      if (isSubtract) {
        ::registerWarning(query, "DATE_SUBTRACT",
                          TRI_ERROR_QUERY_INVALID_DATE_VALUE);
      } else {
        ::registerWarning(query, "DATE_ADD", TRI_ERROR_QUERY_INVALID_DATE_VALUE);
      }
      return AqlValue(AqlValueHintNull());
  }
  // Here we reconstruct the timepoint again

  tp_sys_clock_ms resTime;
  if (isSubtract) {
    resTime = tp_sys_clock_ms{sys_days(ymd) + day_time.to_duration() -
                              std::chrono::duration_cast<duration<int64_t, std::milli>>(ms)};
  } else {
    resTime = tp_sys_clock_ms{sys_days(ymd) + day_time.to_duration() +
                              std::chrono::duration_cast<duration<int64_t, std::milli>>(ms)};
  }
  return ::timeAqlValue(resTime);
}

AqlValue addOrSubtractIsoDurationFromTimestamp(
    Query* query, tp_sys_clock_ms const& tp, std::string const& duration,
    bool isSubtract) {
  year_month_day ymd{floor<days>(tp)};
  auto day_time = make_time(tp - sys_days(ymd));
  std::smatch duration_parts;
  if (!basics::regex_isoDuration(duration, duration_parts)) {
    if (isSubtract) {
      ::registerWarning(query, "DATE_SUBTRACT", TRI_ERROR_QUERY_INVALID_DATE_VALUE);
    } else {
      ::registerWarning(query, "DATE_ADD", TRI_ERROR_QUERY_INVALID_DATE_VALUE);
    }
    return AqlValue(AqlValueHintNull());
  }

  int number = basics::StringUtils::int32(duration_parts[2].str());
  if (isSubtract) {
    ymd -= years{number};
  } else {
    ymd += years{number};
  }

  number = basics::StringUtils::int32(duration_parts[4].str());
  if (isSubtract) {
    ymd -= months{number};
  } else {
    ymd += months{number};
  }

  milliseconds ms{0};
  number = basics::StringUtils::int32(duration_parts[6].str());
  ms += weeks{number};

  number = basics::StringUtils::int32(duration_parts[8].str());
  ms += days{number};

  number = basics::StringUtils::int32(duration_parts[11].str());
  ms += hours{number};

  number = basics::StringUtils::int32(duration_parts[13].str());
  ms += minutes{number};

  number = basics::StringUtils::int32(duration_parts[15].str());
  ms += seconds{number};

  // The Milli seconds can be shortened:
  // .1 => 100ms
  // so we append 00 but only take the first 3 digits
  number = basics::StringUtils::int32(
      (duration_parts[17].str() + "00").substr(0, 3));
  ms += milliseconds{number};

  tp_sys_clock_ms resTime;
  if (isSubtract) {
    resTime = tp_sys_clock_ms{sys_days(ymd) + day_time.to_duration() - ms};
  } else {
    resTime = tp_sys_clock_ms{sys_days(ymd) + day_time.to_duration() + ms};
  }
  return ::timeAqlValue(resTime);
}

/// @brief register usage of an invalid function argument
void registerInvalidArgumentWarning(arangodb::aql::Query* query,
                                    char const* functionName) {
  ::registerWarning(query, functionName, TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH);
}

bool parameterToTimePoint(Query* query, transaction::Methods* trx,
                          VPackFunctionParameters const& parameters,
                          tp_sys_clock_ms& tp,
                          char const* AFN,
                          size_t parameterIndex) {
  AqlValue value = Functions::ExtractFunctionParameterValue(parameters, parameterIndex);

  if (!value.isString() && !value.isNumber()) {
    ::registerInvalidArgumentWarning(query, AFN);
    return false;
  }

  if (value.isNumber()) {
    tp = tp_sys_clock_ms(milliseconds(value.toInt64(trx)));
  } else {
    std::string const dateVal = value.slice().copyString();
    if (!basics::parse_dateTime(dateVal, tp)) {
      ::registerWarning(query, AFN, TRI_ERROR_QUERY_INVALID_DATE_VALUE);
      return false;
    }
  }

  return true;
}

/// @brief converts a value into a number value
double valueToNumber(VPackSlice const& slice, bool& isValid) {
  if (slice.isNull()) {
    isValid = true;
    return 0.0;
  }
  if (slice.isBoolean()) {
    isValid = true;
    return (slice.getBoolean() ? 1.0 : 0.0);
  }
  if (slice.isNumber()) {
    isValid = true;
    return slice.getNumericValue<double>();
  }
  if (slice.isString()) {
    std::string const str = slice.copyString();
    try {
      if (str.empty()) {
        isValid = true;
        return 0.0;
      }
      size_t behind = 0;
      double value = std::stod(str, &behind);
      while (behind < str.size()) {
        char c = str[behind];
        if (c != ' ' && c != '\t' && c != '\r' && c != '\n' && c != '\f') {
          isValid = false;
          return 0.0;
        }
        ++behind;
      }
      isValid = true;
      return value;
    } catch (...) {
      size_t behind = 0;
      while (behind < str.size()) {
        char c = str[behind];
        if (c != ' ' && c != '\t' && c != '\r' && c != '\n' && c != '\f') {
          isValid = false;
          return 0.0;
        }
        ++behind;
      }
      // A string only containing whitespae-characters is valid and should
      // return 0.0
      // It throws in std::stod
      isValid = true;
      return 0.0;
    }
  }
  if (slice.isArray()) {
    VPackValueLength const n = slice.length();
    if (n == 0) {
      isValid = true;
      return 0.0;
    }
    if (n == 1) {
      return ::valueToNumber(slice.at(0), isValid);
    }
  }

  // All other values are invalid
  isValid = false;
  return 0.0;
}

/// @brief extract a boolean parameter from an array
bool getBooleanParameter(transaction::Methods* trx,
                         VPackFunctionParameters const& parameters,
                         size_t startParameter, bool defaultValue) {
  size_t const n = parameters.size();

  if (startParameter >= n) {
    return defaultValue;
  }

  return parameters[startParameter].toBoolean();
}

/// @brief extra a collection name from an AqlValue
std::string extractCollectionName(
    transaction::Methods* trx, VPackFunctionParameters const& parameters,
    size_t position) {
  AqlValue value = Functions::ExtractFunctionParameterValue(parameters, position);

  std::string identifier;

  if (value.isString()) {
    // already a string
    identifier = value.slice().copyString();
  } else {
    AqlValueMaterializer materializer(trx);
    VPackSlice slice = materializer.slice(value, true);
    VPackSlice id = slice;

    if (slice.isObject() && slice.hasKey(StaticStrings::IdString)) {
      id = slice.get(StaticStrings::IdString);
    }
    if (id.isString()) {
      identifier = id.copyString();
    } else if (id.isCustom()) {
      identifier = trx->extractIdString(slice);
    }
  }

  if (!identifier.empty()) {
    size_t pos = identifier.find('/');

    if (pos != std::string::npos) {
      return identifier.substr(0, pos);
    }

    return identifier;
  }

  return StaticStrings::Empty;
}

/// @brief extract attribute names from the arguments
void extractKeys(std::unordered_set<std::string>& names,
                 arangodb::aql::Query* query,
                 transaction::Methods* trx,
                 VPackFunctionParameters const& parameters,
                 size_t startParameter, char const* functionName) {
  size_t const n = parameters.size();

  for (size_t i = startParameter; i < n; ++i) {
    AqlValue param = Functions::ExtractFunctionParameterValue(parameters, i);

    if (param.isString()) {
      names.emplace(param.slice().copyString());
    } else if (param.isNumber()) {
      double number = param.toDouble(trx);

      if (std::isnan(number) || number == HUGE_VAL || number == -HUGE_VAL) {
        names.emplace("null");
      } else {
        char buffer[24];
        int length = fpconv_dtoa(number, &buffer[0]);
        names.emplace(std::string(&buffer[0], static_cast<size_t>(length)));
      }
    } else if (param.isArray()) {
      AqlValueMaterializer materializer(trx);
      VPackSlice s = materializer.slice(param, false);

      for (auto const& v : VPackArrayIterator(s)) {
        if (v.isString()) {
          names.emplace(v.copyString());
        } else {
          ::registerInvalidArgumentWarning(query, functionName);
        }
      }
    }
  }
}

/// @brief append the VelocyPack value to a string buffer
///        Note: Backwards compatibility. Is different than Slice.toJson()
void appendAsString(transaction::Methods* trx,
                    arangodb::basics::VPackStringBufferAdapter& buffer,
                    AqlValue const& value) {
  AqlValueMaterializer materializer(trx);
  VPackSlice slice = materializer.slice(value, false);

  Functions::Stringify(trx, buffer, slice);
}

/// @brief Checks if the given list contains the element
bool listContainsElement(transaction::Methods* trx,
                         VPackOptions const* options,
                         AqlValue const& list, AqlValue const& testee,
                         size_t& index) {
  TRI_ASSERT(list.isArray());
  AqlValueMaterializer materializer(trx);
  VPackSlice slice = materializer.slice(list, false);

  AqlValueMaterializer testeeMaterializer(trx);
  VPackSlice testeeSlice = testeeMaterializer.slice(testee, false);

  VPackArrayIterator it(slice);
  while (it.valid()) {
    if (arangodb::basics::VelocyPackHelper::compare(testeeSlice, it.value(),
                                                    false, options) == 0) {
      index = static_cast<size_t>(it.index());
      return true;
    }
    it.next();
  }
  return false;
}

/// @brief Checks if the given list contains the element
/// DEPRECATED
bool listContainsElement(VPackOptions const* options,
                         VPackSlice const& list,
                         VPackSlice const& testee, size_t& index) {
  TRI_ASSERT(list.isArray());
  for (size_t i = 0; i < static_cast<size_t>(list.length()); ++i) {
    if (arangodb::basics::VelocyPackHelper::compare(testee, list.at(i), false,
                                                    options) == 0) {
      index = i;
      return true;
    }
  }
  return false;
}

bool listContainsElement(VPackOptions const* options,
                         VPackSlice const& list,
                         VPackSlice const& testee) {
  size_t unused;
  return ::listContainsElement(options, list, testee, unused);
}

/// @brief Computes the Variance of the given list.
///        If successful value will contain the variance and count
///        will contain the number of elements.
///        If not successful value and count contain garbage.
bool variance(transaction::Methods* trx, AqlValue const& values, double& value, size_t& count) {
  TRI_ASSERT(values.isArray());
  value = 0.0;
  count = 0;
  bool unused = false;
  double mean = 0.0;

  AqlValueMaterializer materializer(trx);
  VPackSlice slice = materializer.slice(values, false);

  for (auto const& element : VPackArrayIterator(slice)) {
    if (!element.isNull()) {
      if (!element.isNumber()) {
        return false;
      }
      double current = ::valueToNumber(element, unused);
      count++;
      double delta = current - mean;
      mean += delta / count;
      value += delta * (current - mean);
    }
  }
  return true;
}

/// @brief Sorts the given list of Numbers in ASC order.
///        Removes all null entries.
///        Returns false if the list contains non-number values.
bool sortNumberList(transaction::Methods* trx, AqlValue const& values,
                    std::vector<double>& result) {
  TRI_ASSERT(values.isArray());
  TRI_ASSERT(result.empty());
  bool unused;
  AqlValueMaterializer materializer(trx);
  VPackSlice slice = materializer.slice(values, false);

  VPackArrayIterator it(slice);
  result.reserve(it.size());
  for (auto const& element : it) {
    if (!element.isNull()) {
      if (!element.isNumber()) {
        return false;
      }
      result.emplace_back(::valueToNumber(element, unused));
    }
  }
  std::sort(result.begin(), result.end());
  return true;
}

/// @brief Helper function to unset or keep all given names in the value.
///        Recursively iterates over sub-object and unsets or keeps their values
///        as well
void unsetOrKeep(transaction::Methods* trx, VPackSlice const& value,
                 std::unordered_set<std::string> const& names,
                 bool unset,  // true means unset, false means keep
                 bool recursive, VPackBuilder& result) {
  TRI_ASSERT(value.isObject());
  VPackObjectBuilder b(&result);  // Close the object after this function
  for (auto const& entry : VPackObjectIterator(value, false)) {
    TRI_ASSERT(entry.key.isString());
    std::string key = entry.key.copyString();
    if ((names.find(key) == names.end()) == unset) {
      // not found and unset or found and keep
      if (recursive && entry.value.isObject()) {
        result.add(entry.key);  // Add the key
        ::unsetOrKeep(trx, entry.value, names, unset, recursive,
                      result);  // Adds the object
      } else {
        if (entry.value.isCustom()) {
          result.add(key, VPackValue(trx->extractIdString(value)));
        } else {
          result.add(key, entry.value);
        }
      }
    }
  }
}

/// @brief Helper function to get a document by it's identifier
///        Lazy Locks the collection if necessary.
void getDocumentByIdentifier(transaction::Methods* trx,
                             std::string& collectionName,
                             std::string const& identifier,
                             bool ignoreError, VPackBuilder& result) {
  transaction::BuilderLeaser searchBuilder(trx);

  size_t pos = identifier.find('/');
  if (pos == std::string::npos) {
    searchBuilder->add(VPackValue(identifier));
  } else {
    if (collectionName.empty()) {
      searchBuilder->add(VPackValue(identifier.substr(pos + 1)));
      collectionName = identifier.substr(0, pos);
    } else if (identifier.substr(0, pos) != collectionName) {
      // Requesting an _id that cannot be stored in this collection
      if (ignoreError) {
        return;
      }
      THROW_ARANGO_EXCEPTION(TRI_ERROR_ARANGO_CROSS_COLLECTION_REQUEST);
    } else {
      searchBuilder->add(VPackValue(identifier.substr(pos + 1)));
    }
  }

  Result res;
  try {
    res = trx->documentFastPath(collectionName, nullptr, searchBuilder->slice(),
                                result, true);
  } catch (arangodb::basics::Exception const& ex) {
    res.reset(ex.code());
  }

  if (!res.ok()) {
    if (ignoreError) {
      if (res.errorNumber() == TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND ||
          res.errorNumber() == TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND ||
          res.errorNumber() == TRI_ERROR_ARANGO_CROSS_COLLECTION_REQUEST) {
        return;
      }
    }
    if (res.errorNumber() == TRI_ERROR_TRANSACTION_UNREGISTERED_COLLECTION) {
      // special error message to indicate which collection was undeclared
      THROW_ARANGO_EXCEPTION_MESSAGE(
          res.errorNumber(),
          res.errorMessage() + ": " + collectionName + " [" +
              AccessMode::typeString(AccessMode::Type::READ) + "]");
    }
    THROW_ARANGO_EXCEPTION(res);
  }
}

/// @brief Helper function to merge given parameters
///        Works for an array of objects as first parameter or arbitrary many
///        object parameters
AqlValue mergeParameters(arangodb::aql::Query* query,
                         transaction::Methods* trx,
                         VPackFunctionParameters const& parameters,
                         char const* funcName, bool recursive) {
  size_t const n = parameters.size();

  if (n == 0) {
    return AqlValue(arangodb::basics::VelocyPackHelper::EmptyObjectValue());
  }

  // use the first argument as the preliminary result
  AqlValue initial = Functions::ExtractFunctionParameterValue(parameters, 0);
  AqlValueMaterializer materializer(trx);
  VPackSlice initialSlice = materializer.slice(initial, true);

  VPackBuilder builder;

  if (initial.isArray() && n == 1) {
    // special case: a single array parameter
    // Create an empty document as start point
    builder.openObject();
    builder.close();
    // merge in all other arguments
    for (auto const& it : VPackArrayIterator(initialSlice)) {
      if (!it.isObject()) {
        ::registerInvalidArgumentWarning(query, funcName);
        return AqlValue(AqlValueHintNull());
      }
      builder = arangodb::basics::VelocyPackHelper::merge(builder.slice(), it,
                                                          false, recursive);
    }
    return AqlValue(builder);
  }

  if (!initial.isObject()) {
    ::registerInvalidArgumentWarning(query, funcName);
    return AqlValue(AqlValueHintNull());
  }

  // merge in all other arguments
  for (size_t i = 1; i < n; ++i) {
    AqlValue param = Functions::ExtractFunctionParameterValue(parameters, i);

    if (!param.isObject()) {
      ::registerInvalidArgumentWarning(query, funcName);
      return AqlValue(AqlValueHintNull());
    }

    AqlValueMaterializer materializer(trx);
    VPackSlice slice = materializer.slice(param, false);

    builder = arangodb::basics::VelocyPackHelper::merge(initialSlice, slice,
                                                        false, recursive);
    initialSlice = builder.slice();
  }
  if (n == 1) {
    // only one parameter. now add original document
    builder.add(initialSlice);
  }
  return AqlValue(builder);
}

/// @brief internal recursive flatten helper
void flattenList(VPackSlice const& array, size_t maxDepth,
                 size_t curDepth, VPackBuilder& result) {
  TRI_ASSERT(result.isOpenArray());
  for (auto const& tmp : VPackArrayIterator(array)) {
    if (tmp.isArray() && curDepth < maxDepth) {
      ::flattenList(tmp, maxDepth, curDepth + 1, result);
    } else {
      // Copy the content of tmp into the result
      result.add(tmp);
    }
  }
}

} // namespace

void Functions::init() {
  std::string myregex;

  dateMap.reserve(sortedDateMap.size());
  std::for_each(sortedDateMap.begin(), sortedDateMap.end(), [&myregex](std::pair<std::string const&, format_func_t> const& p) {
    (myregex.length() > 0) ? myregex += "|" + p.first : myregex = p.first;
    dateMap.insert(std::make_pair(p.first, p.second));
  });
  ::theDateFormatRegex = std::regex(myregex);
}

/// @brief validate the number of parameters
void Functions::ValidateParameters(VPackFunctionParameters const& parameters,
                                   char const* function, int minParams,
                                   int maxParams) {
  if (parameters.size() < static_cast<size_t>(minParams) ||
      parameters.size() > static_cast<size_t>(maxParams)) {
    THROW_ARANGO_EXCEPTION_PARAMS(
        TRI_ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH, function, minParams,
        maxParams);
  }
}

void Functions::ValidateParameters(VPackFunctionParameters const& parameters,
                                   char const* function, int minParams) {
  return ValidateParameters(parameters, function, minParams,
                            static_cast<int>(Function::MaxArguments));
}

/// @brief extract a function parameter from the arguments
AqlValue Functions::ExtractFunctionParameterValue(
    VPackFunctionParameters const& parameters, size_t position) {
  if (position >= parameters.size()) {
    // parameter out of range
    return AqlValue();
  }
  return parameters[position];
}

/// @brief append the VelocyPack value to a string buffer
///        Note: Backwards compatibility. Is different than Slice.toJson()
void Functions::Stringify(transaction::Methods* trx,
                          arangodb::basics::VPackStringBufferAdapter& buffer,
                          VPackSlice const& slice) {
  if (slice.isNull()) {
    // null is the empty string
    return;
  }

  if (slice.isString()) {
    // dumping adds additional ''
    VPackValueLength length;
    char const* p = slice.getString(length);
    buffer.append(p, length);
    return;
  }

  VPackOptions* options =
      trx->transactionContextPtr()->getVPackOptionsForDump();
  VPackOptions adjustedOptions = *options;
  adjustedOptions.escapeUnicode = false;
  adjustedOptions.escapeForwardSlashes = false;
  VPackDumper dumper(&buffer, &adjustedOptions);
  dumper.dump(slice);
}

/// @brief function IS_NULL
AqlValue Functions::IsNull(arangodb::aql::Query*,
                           transaction::Methods* trx,
                           VPackFunctionParameters const& parameters) {
  AqlValue a = ExtractFunctionParameterValue(parameters, 0);
  return AqlValue(AqlValueHintBool(a.isNull(true)));
}

/// @brief function IS_BOOL
AqlValue Functions::IsBool(arangodb::aql::Query*,
                           transaction::Methods* trx,
                           VPackFunctionParameters const& parameters) {
  AqlValue a = ExtractFunctionParameterValue(parameters, 0);
  return AqlValue(AqlValueHintBool(a.isBoolean()));
}

/// @brief function IS_NUMBER
AqlValue Functions::IsNumber(arangodb::aql::Query*,
                             transaction::Methods* trx,
                             VPackFunctionParameters const& parameters) {
  AqlValue a = ExtractFunctionParameterValue(parameters, 0);
  return AqlValue(AqlValueHintBool(a.isNumber()));
}

/// @brief function IS_STRING
AqlValue Functions::IsString(arangodb::aql::Query*,
                             transaction::Methods* trx,
                             VPackFunctionParameters const& parameters) {
  AqlValue a = ExtractFunctionParameterValue(parameters, 0);
  return AqlValue(AqlValueHintBool(a.isString()));
}

/// @brief function IS_ARRAY
AqlValue Functions::IsArray(arangodb::aql::Query*,
                            transaction::Methods* trx,
                            VPackFunctionParameters const& parameters) {
  AqlValue a = ExtractFunctionParameterValue(parameters, 0);
  return AqlValue(AqlValueHintBool(a.isArray()));
}

/// @brief function IS_OBJECT
AqlValue Functions::IsObject(arangodb::aql::Query*,
                             transaction::Methods* trx,
                             VPackFunctionParameters const& parameters) {
  AqlValue a = ExtractFunctionParameterValue(parameters, 0);
  return AqlValue(AqlValueHintBool(a.isObject()));
}

/// @brief function TYPENAME
AqlValue Functions::Typename(arangodb::aql::Query*,
                             transaction::Methods* trx,
                             VPackFunctionParameters const& parameters) {
  AqlValue value = ExtractFunctionParameterValue(parameters, 0);
  char const* type = value.getTypeString();

  return AqlValue(TRI_CHAR_LENGTH_PAIR(type));
}

/// @brief function TO_NUMBER
AqlValue Functions::ToNumber(arangodb::aql::Query*,
                             transaction::Methods* trx,
                             VPackFunctionParameters const& parameters) {
  AqlValue a = ExtractFunctionParameterValue(parameters, 0);
  bool failed;
  double value = a.toDouble(trx, failed);

  if (failed) {
    return AqlValue(AqlValueHintZero());
  }

  return AqlValue(AqlValueHintDouble(value));
}

/// @brief function TO_STRING
AqlValue Functions::ToString(arangodb::aql::Query*,
                             transaction::Methods* trx,
                             VPackFunctionParameters const& parameters) {
  AqlValue value = ExtractFunctionParameterValue(parameters, 0);

  transaction::StringBufferLeaser buffer(trx);
  arangodb::basics::VPackStringBufferAdapter adapter(buffer->stringBuffer());

  ::appendAsString(trx, adapter, value);
  return AqlValue(buffer->begin(), buffer->length());
}

/// @brief function TO_BOOL
AqlValue Functions::ToBool(arangodb::aql::Query*,
                           transaction::Methods* trx,
                           VPackFunctionParameters const& parameters) {
  AqlValue a = ExtractFunctionParameterValue(parameters, 0);
  return AqlValue(AqlValueHintBool(a.toBoolean()));
}

/// @brief function TO_ARRAY
AqlValue Functions::ToArray(arangodb::aql::Query*,
                            transaction::Methods* trx,
                            VPackFunctionParameters const& parameters) {
  AqlValue value = ExtractFunctionParameterValue(parameters, 0);

  if (value.isArray()) {
    // return copy of the original array
    return value.clone();
  }

  if (value.isNull(true)) {
    return AqlValue(arangodb::basics::VelocyPackHelper::EmptyArrayValue());
  }

  transaction::BuilderLeaser builder(trx);
  builder->openArray();
  if (value.isBoolean() || value.isNumber() || value.isString()) {
    // return array with single member
    builder->add(value.slice());
  } else if (value.isObject()) {
    AqlValueMaterializer materializer(trx);
    VPackSlice slice = materializer.slice(value, false);
    // return an array with the attribute values
    for (auto const& it : VPackObjectIterator(slice, true)) {
      if (it.value.isCustom()) {
        builder->add(VPackValue(trx->extractIdString(slice)));
      } else {
        builder->add(it.value);
      }
    }
  }
  builder->close();
  return AqlValue(builder.get());
}

/// @brief function LENGTH
AqlValue Functions::Length(arangodb::aql::Query*,
                           transaction::Methods* trx,
                           VPackFunctionParameters const& parameters) {
  static char const* AFN = "LENGTH";
  ValidateParameters(parameters, AFN, 1, 1);

  AqlValue value = ExtractFunctionParameterValue(parameters, 0);
  if (value.isArray()) {
    // shortcut!
    return AqlValue(AqlValueHintUInt(value.length()));
  }

  size_t length = 0;
  if (value.isNull(true)) {
    length = 0;
  } else if (value.isBoolean()) {
    if (value.toBoolean()) {
      length = 1;
    } else {
      length = 0;
    }
  } else if (value.isNumber()) {
    double tmp = value.toDouble(trx);
    if (std::isnan(tmp) || !std::isfinite(tmp)) {
      length = 0;
    } else {
      char buffer[24];
      length = static_cast<size_t>(fpconv_dtoa(tmp, buffer));
    }
  } else if (value.isString()) {
    VPackValueLength l;
    char const* p = value.slice().getString(l);
    length = TRI_CharLengthUtf8String(p, l);
  } else if (value.isObject()) {
    length = static_cast<size_t>(value.length());
  }

  return AqlValue(AqlValueHintUInt(length));
}

/// @brief function FIND_FIRST
/// FIND_FIRST(text, search, start, end) → position
AqlValue Functions::FindFirst(arangodb::aql::Query* query,
                              transaction::Methods* trx,
                              VPackFunctionParameters const& parameters) {
  static char const* AFN = "FIND_FIRST";
  ValidateParameters(parameters, AFN, 2, 4);

  AqlValue value = ExtractFunctionParameterValue(parameters, 0);
  AqlValue searchValue = ExtractFunctionParameterValue(parameters, 1);

  transaction::StringBufferLeaser buf1(trx);
  arangodb::basics::VPackStringBufferAdapter adapter(buf1->stringBuffer());
  ::appendAsString(trx, adapter, value);
  UnicodeString uBuf(buf1->c_str(), static_cast<int32_t>(buf1->length()));

  transaction::StringBufferLeaser buf2(trx);
  arangodb::basics::VPackStringBufferAdapter adapter2(buf2->stringBuffer());
  ::appendAsString(trx, adapter2, searchValue);
  UnicodeString uSearchBuf(buf2->c_str(), static_cast<int32_t>(buf2->length()));
  auto searchLen = uSearchBuf.length();

  int64_t startOffset = 0;
  int64_t maxEnd = -1;

  if (parameters.size() >= 3) {
    AqlValue optionalStartOffset = ExtractFunctionParameterValue(parameters, 2);
    startOffset = optionalStartOffset.toInt64(trx);
    if (startOffset < 0) {
      return AqlValue(AqlValueHintInt(-1));
    }
  }

  maxEnd = uBuf.length();
  if (parameters.size() == 4) {
    AqlValue optionalEndMax = ExtractFunctionParameterValue(parameters, 3);
    if (!optionalEndMax.isNull(true)) {
      maxEnd = optionalEndMax.toInt64(trx);
      if ((maxEnd < startOffset) || (maxEnd < 0)) {
        return AqlValue(AqlValueHintInt(-1));
      }
    }
  }

  if (searchLen == 0) {
    return AqlValue(AqlValueHintInt(startOffset));
  }
  if (uBuf.length() == 0) {
    return AqlValue(AqlValueHintInt(-1));
  }

  auto locale = LanguageFeature::instance()->getLocale();
  UErrorCode status = U_ZERO_ERROR;
  StringSearch search(uSearchBuf, uBuf, locale, nullptr, status);

  for(int pos = search.first(status);
      U_SUCCESS(status) && pos != USEARCH_DONE;
      pos = search.next(status)) {
    if (U_FAILURE(status)) {
      ::registerICUWarning(query, AFN, status);
      return AqlValue(AqlValueHintNull());
    }
    if ((pos >= startOffset) && ((pos + searchLen - 1) <= maxEnd)) {
      return AqlValue(AqlValueHintInt(pos));
    }
  }
  return AqlValue(AqlValueHintInt(-1));
}

/// @brief function FIND_LAST
/// FIND_FIRST(text, search, start, end) → position
AqlValue Functions::FindLast(arangodb::aql::Query* query,
                             transaction::Methods* trx,
                             VPackFunctionParameters const& parameters) {
  static char const* AFN = "FIND_LAST";
  ValidateParameters(parameters, AFN, 2, 4);

  AqlValue value = ExtractFunctionParameterValue(parameters, 0);
  AqlValue searchValue = ExtractFunctionParameterValue(parameters, 1);

  transaction::StringBufferLeaser buf1(trx);
  arangodb::basics::VPackStringBufferAdapter adapter(buf1->stringBuffer());
  ::appendAsString(trx, adapter, value);
  UnicodeString uBuf(buf1->c_str(), static_cast<int32_t>(buf1->length()));

  transaction::StringBufferLeaser buf2(trx);
  arangodb::basics::VPackStringBufferAdapter adapter2(buf2->stringBuffer());
  ::appendAsString(trx, adapter2, searchValue);
  UnicodeString uSearchBuf(buf2->c_str(), static_cast<int32_t>(buf2->length()));
  auto searchLen = uSearchBuf.length();

  int64_t startOffset = 0;
  int64_t maxEnd = -1;

  if (parameters.size() >= 3) {
    AqlValue optionalStartOffset = ExtractFunctionParameterValue(parameters, 2);
    startOffset = optionalStartOffset.toInt64(trx);
    if (startOffset < 0) {
      return AqlValue(AqlValueHintInt(-1));
    }
  }

  maxEnd = uBuf.length();
  int emptySearchCludge = 0;
  if (parameters.size() == 4) {
    AqlValue optionalEndMax = ExtractFunctionParameterValue(parameters, 3);
    if (!optionalEndMax.isNull(true)) {
      maxEnd = optionalEndMax.toInt64(trx);
      if ((maxEnd < startOffset) || (maxEnd < 0)) {
        return AqlValue(AqlValueHintInt(-1));
      }
      emptySearchCludge = 1;
    }
  }

  if (searchLen == 0) {
    return AqlValue(AqlValueHintInt(maxEnd + emptySearchCludge));
  }
  if (uBuf.length() == 0) {
    return AqlValue(AqlValueHintInt(-1));
  }

  auto locale = LanguageFeature::instance()->getLocale();
  UErrorCode status = U_ZERO_ERROR;
  StringSearch search(uSearchBuf, uBuf, locale, nullptr, status);

  int foundPos = -1;
  for(int pos = search.first(status);
      U_SUCCESS(status) && pos != USEARCH_DONE;
      pos = search.next(status)) {
    if (U_FAILURE(status)) {
      ::registerICUWarning(query, AFN, status);
      return AqlValue(AqlValueHintNull());
    }
    if ((pos >= startOffset) && ((pos + searchLen - 1) <= maxEnd)) {
      foundPos = pos;
    }
  }
  return AqlValue(AqlValueHintInt(foundPos));
}

/// @brief function REVERSE
AqlValue Functions::Reverse(arangodb::aql::Query* query,
                            transaction::Methods* trx,
                            VPackFunctionParameters const& parameters) {
  static char const* AFN = "REVERSE";
  ValidateParameters(parameters, AFN, 1, 1);
  AqlValue value = ExtractFunctionParameterValue(parameters, 0);

  if (value.isArray()) {
    transaction::BuilderLeaser builder(trx);
    AqlValueMaterializer materializer(trx);
    VPackSlice slice = materializer.slice(value, false);
    std::vector<VPackSlice> array;
    array.reserve(slice.length());
    for (auto const& it : VPackArrayIterator(slice)) {
      array.push_back(it);
    }
    std::reverse(std::begin(array), std::end(array));

    builder->openArray();
    for (auto const &it : array) {
      builder->add(it);
    }
    builder->close();
    return AqlValue(builder.get());
  }
  else if (value.isString()) {
    std::string utf8;
    transaction::StringBufferLeaser buf1(trx);
    arangodb::basics::VPackStringBufferAdapter adapter(buf1->stringBuffer());
    ::appendAsString(trx, adapter, value);
    UnicodeString uBuf(buf1->c_str(), static_cast<int32_t>(buf1->length()));
    // reserve the result buffer, but need to set empty afterwards:
    UnicodeString result;
    result.getBuffer(uBuf.length());
    result = "";
    StringCharacterIterator iter(uBuf, uBuf.length());
    UChar c = iter.previous();
    while (c != CharacterIterator::DONE) {
      result.append(c);
      c = iter.previous();
    }
    result.toUTF8String(utf8);

    return AqlValue(utf8);
  }
  else {
    // neither array nor string...
    ::registerWarning(query, AFN, TRI_ERROR_QUERY_ARRAY_EXPECTED);
    return AqlValue(AqlValueHintNull());
  }
}

/// @brief function FIRST
AqlValue Functions::First(arangodb::aql::Query* query,
                          transaction::Methods* trx,
                          VPackFunctionParameters const& parameters) {
  static char const* AFN = "FIRST";
  ValidateParameters(parameters, AFN, 1, 1);
  AqlValue value = ExtractFunctionParameterValue(parameters, 0);

  if (!value.isArray()) {
    // not an array
    ::registerWarning(query, AFN, TRI_ERROR_QUERY_ARRAY_EXPECTED);
    return AqlValue(AqlValueHintNull());
  }

  if (value.length() == 0) {
    return AqlValue(AqlValueHintNull());
  }

  bool mustDestroy;
  return value.at(trx, 0, mustDestroy, true);
}

/// @brief function LAST
AqlValue Functions::Last(arangodb::aql::Query* query,
                         transaction::Methods* trx,
                         VPackFunctionParameters const& parameters) {
  static char const* AFN = "LAST";
  ValidateParameters(parameters, AFN, 1, 1);
  AqlValue value = ExtractFunctionParameterValue(parameters, 0);

  if (!value.isArray()) {
    // not an array
    ::registerWarning(query, AFN, TRI_ERROR_QUERY_ARRAY_EXPECTED);
    return AqlValue(AqlValueHintNull());
  }

  VPackValueLength const n = value.length();

  if (n == 0) {
    return AqlValue(AqlValueHintNull());
  }

  bool mustDestroy;
  return value.at(trx, n - 1, mustDestroy, true);
}

/// @brief function NTH
AqlValue Functions::Nth(arangodb::aql::Query* query, transaction::Methods* trx,
                        VPackFunctionParameters const& parameters) {
  static char const* AFN = "NTH";
  ValidateParameters(parameters, AFN, 2, 2);
  AqlValue value = ExtractFunctionParameterValue(parameters, 0);

  if (!value.isArray()) {
    // not an array
    ::registerWarning(query, AFN, TRI_ERROR_QUERY_ARRAY_EXPECTED);
    return AqlValue(AqlValueHintNull());
  }

  VPackValueLength const n = value.length();

  if (n == 0) {
    return AqlValue(AqlValueHintNull());
  }

  AqlValue position = ExtractFunctionParameterValue(parameters, 1);
  int64_t index = position.toInt64(trx);

  if (index < 0 || index >= static_cast<int64_t>(n)) {
    return AqlValue(AqlValueHintNull());
  }

  bool mustDestroy;
  return value.at(trx, index, mustDestroy, true);
}

/// @brief function CONTAINS
AqlValue Functions::Contains(arangodb::aql::Query*,
                             transaction::Methods* trx,
                             VPackFunctionParameters const& parameters) {
  static char const* AFN = "CONTAINS";
  ValidateParameters(parameters, AFN, 2, 3);

  AqlValue value = ExtractFunctionParameterValue(parameters, 0);
  AqlValue search = ExtractFunctionParameterValue(parameters, 1);
  AqlValue returnIndex = ExtractFunctionParameterValue(parameters, 2);

  bool const willReturnIndex = returnIndex.toBoolean();

  int result = -1;  // default is "not found"
  {
    transaction::StringBufferLeaser buffer(trx);
    arangodb::basics::VPackStringBufferAdapter adapter(buffer->stringBuffer());

    ::appendAsString(trx, adapter, value);
    size_t const valueLength = buffer->length();

    size_t const searchOffset = buffer->length();
    ::appendAsString(trx, adapter, search);
    size_t const searchLength = buffer->length() - valueLength;

    if (searchLength > 0) {
      char const* found = static_cast<char const*>(
          memmem(buffer->c_str(), valueLength, buffer->c_str() + searchOffset,
                 searchLength));

      if (found != nullptr) {
        if (willReturnIndex) {
          // find offset into string
          int bytePosition = static_cast<int>(found - buffer->c_str());
          char const* p = buffer->c_str();
          int pos = 0;
          while (pos < bytePosition) {
            unsigned char c = static_cast<unsigned char>(*p);
            if (c < 128) {
              ++pos;
            } else if (c < 224) {
              pos += 2;
            } else if (c < 240) {
              pos += 3;
            } else if (c < 248) {
              pos += 4;
            }
          }
          result = pos;
        } else {
          // fake result position, but it does not matter as it will
          // only be compared to -1 later
          result = 0;
        }
      }
    }
  }

  if (willReturnIndex) {
    // return numeric value
    return ::numberValue(trx, result);
  }

  // return boolean
  return AqlValue(AqlValueHintBool(result != -1));
}

/// @brief function CONCAT
AqlValue Functions::Concat(arangodb::aql::Query*,
                           transaction::Methods* trx,
                           VPackFunctionParameters const& parameters) {
  transaction::StringBufferLeaser buffer(trx);
  arangodb::basics::VPackStringBufferAdapter adapter(buffer->stringBuffer());

  size_t const n = parameters.size();

  if (n == 1) {
    AqlValue member = ExtractFunctionParameterValue(parameters, 0);
    if (member.isArray()) {
      AqlValueMaterializer materializer(trx);
      VPackSlice slice = materializer.slice(member, false);

      for (auto const& it : VPackArrayIterator(slice)) {
        if (it.isNull()) {
          continue;
        }
        // convert member to a string and append
        ::appendAsString(trx, adapter, AqlValue(it.begin()));
      }
      return AqlValue(buffer->c_str(), buffer->length());
    }
  }

  for (size_t i = 0; i < n; ++i) {
    AqlValue member = ExtractFunctionParameterValue(parameters, i);

    if (member.isNull(true)) {
      continue;
    }

    // convert member to a string and append
    ::appendAsString(trx, adapter, member);
  }

  return AqlValue(buffer->c_str(), buffer->length());
}

/// @brief function CONCAT_SEPARATOR
AqlValue Functions::ConcatSeparator(arangodb::aql::Query*,
                                    transaction::Methods* trx,
                                    VPackFunctionParameters const& parameters) {
  transaction::StringBufferLeaser buffer(trx);
  arangodb::basics::VPackStringBufferAdapter adapter(buffer->stringBuffer());

  bool found = false;
  size_t const n = parameters.size();

  AqlValue separator = ExtractFunctionParameterValue(parameters, 0);
  ::appendAsString(trx, adapter, separator);
  std::string const plainStr(buffer->c_str(), buffer->length());

  buffer->clear();

  if (n == 2) {
    AqlValue member = ExtractFunctionParameterValue(parameters, 1);

    if (member.isArray()) {
      // reserve *some* space
      buffer->reserve((plainStr.size() + 10) * member.length());

      AqlValueMaterializer materializer(trx);
      VPackSlice slice = materializer.slice(member, false);

      for (auto const& it : VPackArrayIterator(slice)) {
        if (it.isNull()) {
          continue;
        }
        if (found) {
          buffer->appendText(plainStr);
        }
        // convert member to a string and append
        ::appendAsString(trx, adapter, AqlValue(it.begin()));
        found = true;
      }
      return AqlValue(buffer->c_str(), buffer->length());
    }
  }

  // reserve *some* space
  buffer->reserve((plainStr.size() + 10) * n);
  for (size_t i = 1; i < n; ++i) {
    AqlValue member = ExtractFunctionParameterValue(parameters, i);

    if (member.isNull(true)) {
      continue;
    }
    if (found) {
      buffer->appendText(plainStr);
    }

    // convert member to a string and append
    ::appendAsString(trx, adapter, member);
    found = true;
  }

  return AqlValue(buffer->c_str(), buffer->length());
}

/// @brief function CHAR_LENGTH
AqlValue Functions::CharLength(arangodb::aql::Query*,
                               transaction::Methods* trx,
                               VPackFunctionParameters const& parameters) {
  static char const* AFN = "CHAR_LENGTH";
  ValidateParameters(parameters, AFN, 1, 1);

  AqlValue value = ExtractFunctionParameterValue(parameters, 0);
  size_t length = 0;

  if (value.isArray() || value.isObject()) {
    AqlValueMaterializer materializer(trx);
    VPackSlice slice = materializer.slice(value, false);

    transaction::StringBufferLeaser buffer(trx);
    arangodb::basics::VPackStringBufferAdapter adapter(buffer->stringBuffer());

    VPackDumper dumper(&adapter,
                       trx->transactionContextPtr()->getVPackOptions());
    dumper.dump(slice);

    length = buffer->length();

  } else if (value.isNull(true)) {
    length = 0;

  } else if (value.isBoolean()) {
    if (value.toBoolean()) {
      length = 4;
    } else {
      length = 5;
    }

  } else if (value.isNumber()) {
    double tmp = value.toDouble(trx);
    if (std::isnan(tmp) || !std::isfinite(tmp)) {
      length = 0;
    } else {
      char buffer[24];
      length = static_cast<size_t>(fpconv_dtoa(tmp, buffer));
    }

  } else if (value.isString()) {
    VPackValueLength l;
    char const* p = value.slice().getString(l);
    length = TRI_CharLengthUtf8String(p, l);
  }

  return AqlValue(AqlValueHintUInt(length));
}

/// @brief function LOWER
AqlValue Functions::Lower(arangodb::aql::Query*,
                          transaction::Methods* trx,
                          VPackFunctionParameters const& parameters) {
  static char const* AFN = "LOWER";
  ValidateParameters(parameters, AFN, 1, 1);

  std::string utf8;
  AqlValue value = ExtractFunctionParameterValue(parameters, 0);

  transaction::StringBufferLeaser buffer(trx);
  arangodb::basics::VPackStringBufferAdapter adapter(buffer->stringBuffer());

  ::appendAsString(trx, adapter, value);

  UnicodeString unicodeStr(buffer->c_str(),
                           static_cast<int32_t>(buffer->length()));
  unicodeStr.toLower(NULL);
  unicodeStr.toUTF8String(utf8);

  return AqlValue(utf8);
}

/// @brief function UPPER
AqlValue Functions::Upper(arangodb::aql::Query*,
                          transaction::Methods* trx,
                          VPackFunctionParameters const& parameters) {
  static char const* AFN = "UPPER";
  ValidateParameters(parameters, AFN, 1, 1);

  std::string utf8;
  AqlValue value = ExtractFunctionParameterValue(parameters, 0);

  transaction::StringBufferLeaser buffer(trx);
  arangodb::basics::VPackStringBufferAdapter adapter(buffer->stringBuffer());

  ::appendAsString(trx, adapter, value);

  UnicodeString unicodeStr(buffer->c_str(),
                           static_cast<int32_t>(buffer->length()));
  unicodeStr.toUpper(NULL);
  unicodeStr.toUTF8String(utf8);

  return AqlValue(utf8);
}

/// @brief function SUBSTRING
AqlValue Functions::Substring(arangodb::aql::Query*,
                              transaction::Methods* trx,
                              VPackFunctionParameters const& parameters) {
  static char const* AFN = "SUBSTRING";
  ValidateParameters(parameters, AFN, 2, 3);
  AqlValue value = ExtractFunctionParameterValue(parameters, 0);

  int32_t length = INT32_MAX;

  transaction::StringBufferLeaser buffer(trx);
  arangodb::basics::VPackStringBufferAdapter adapter(buffer->stringBuffer());

  ::appendAsString(trx, adapter, value);
  UnicodeString unicodeStr(buffer->c_str(),
                           static_cast<int32_t>(buffer->length()));

  int32_t offset = static_cast<int32_t>(
      ExtractFunctionParameterValue(parameters, 1).toInt64(trx));

  if (parameters.size() == 3) {
    length = static_cast<int32_t>(
        ExtractFunctionParameterValue(parameters, 2).toInt64(trx));
  }

  if (offset < 0) {
    offset = unicodeStr.moveIndex32(
        unicodeStr.moveIndex32(unicodeStr.length(), 0), offset);
  } else {
    offset = unicodeStr.moveIndex32(0, offset);
  }

  std::string utf8;
  unicodeStr
      .tempSubString(offset, unicodeStr.moveIndex32(offset, length) - offset)
      .toUTF8String(utf8);

  return AqlValue(utf8);
}
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

AqlValue Functions::Substitute(arangodb::aql::Query* query,
                               transaction::Methods* trx,
                               VPackFunctionParameters const& parameters) {
  static char const* AFN = "SUBSTITUTE";
  ValidateParameters(parameters, AFN, 2, 4);

  AqlValue search = ExtractFunctionParameterValue(parameters, 1);
  int64_t limit = -1;
  AqlValueMaterializer materializer(trx);
  std::vector<UnicodeString> matchPatterns;
  std::vector<UnicodeString> replacePatterns;
  bool replaceWasPlainString = false;

  if (search.isObject()) {
    if (parameters.size() > 3) {
      ::registerWarning(query, AFN, TRI_ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH);
      return AqlValue(AqlValueHintNull());
    }
    if (parameters.size() == 3) {
      limit = ExtractFunctionParameterValue(parameters, 2).toInt64(trx);
    }
    VPackSlice slice = materializer.slice(search, false);
    matchPatterns.reserve(slice.length());
    replacePatterns.reserve(slice.length());
    for (auto const& it : VPackObjectIterator(slice)) {
      arangodb::velocypack::ValueLength length;
      const char *str = it.key.getString(length);
      matchPatterns.push_back(UnicodeString(str, static_cast<int32_t>(length)));
      if (!it.value.isString()) {
        ::registerInvalidArgumentWarning(query, AFN);
        return AqlValue(AqlValueHintNull());
      }
      str = it.value.getString(length);
      replacePatterns.push_back(UnicodeString(str, static_cast<int32_t>(length)));
    }
  }
  else {
    if (parameters.size() < 2) {
      ::registerWarning(query, AFN, TRI_ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH);
      return AqlValue(AqlValueHintNull());
    }
    if (parameters.size() == 4) {
      limit = ExtractFunctionParameterValue(parameters, 3).toInt64(trx);
    }

    VPackSlice slice = materializer.slice(search, false);
    if (search.isArray()) {
      for (auto const& it : VPackArrayIterator(slice)) {
        if (!it.isString()) {
          ::registerInvalidArgumentWarning(query, AFN);
          return AqlValue(AqlValueHintNull());
        }
        arangodb::velocypack::ValueLength length;
        const char *str = it.getString(length);
        matchPatterns.push_back(UnicodeString(str, static_cast<int32_t>(length)));
      }
    }
    else {
      if (!search.isString()) {
        ::registerInvalidArgumentWarning(query, AFN);
        return AqlValue(AqlValueHintNull());
      }
      arangodb::velocypack::ValueLength length;
      const char *str = slice.getString(length);
      matchPatterns.push_back(UnicodeString(str, static_cast<int32_t>(length)));
    }
    if (parameters.size() > 2) {
      AqlValue replace = ExtractFunctionParameterValue(parameters, 2);
      VPackSlice rslice = materializer.slice(replace, false);
      if (replace.isArray()) {
        for (auto const& it : VPackArrayIterator(rslice)) {
          if (!it.isString()) {
            ::registerInvalidArgumentWarning(query, AFN);
            return AqlValue(AqlValueHintNull());
          }
          arangodb::velocypack::ValueLength length;
          const char *str = it.getString(length);
          replacePatterns.push_back(UnicodeString(str, static_cast<int32_t>(length)));
        }
      }
      else if (replace.isString()) {
        // If we have a string as replacement,
        // it counts in for all found values.
        replaceWasPlainString = true;
        arangodb::velocypack::ValueLength length;
        const char *str = rslice.getString(length);
        replacePatterns.push_back(UnicodeString(str, static_cast<int32_t>(length)));
      }
      else {
        ::registerInvalidArgumentWarning(query, AFN);
        return AqlValue(AqlValueHintNull());
      }
    }
  }

  AqlValue value = ExtractFunctionParameterValue(parameters, 0);
  if ((limit == 0) || (matchPatterns.size() == 0)) {
    // if the limit is 0, or we don't have any match pattern, return the source string.
    return AqlValue(value);
  }

  transaction::StringBufferLeaser buffer(trx);
  arangodb::basics::VPackStringBufferAdapter adapter(buffer->stringBuffer());

  ::appendAsString(trx, adapter, value);
  UnicodeString unicodeStr(buffer->c_str(), static_cast<int32_t>(buffer->length()));

  auto locale = LanguageFeature::instance()->getLocale();
  // we can't copy the search instances, thus use pointers:
  std::vector<std::unique_ptr<StringSearch>> searchVec;
  searchVec.reserve(matchPatterns.size());
  UErrorCode status = U_ZERO_ERROR;
  for (auto const& searchStr : matchPatterns) {
    // create a vector of string searches
    searchVec.push_back(std::make_unique<StringSearch>(searchStr, unicodeStr, locale, nullptr, status));
    if (U_FAILURE(status)) {
      ::registerICUWarning(query, AFN, status);
      return AqlValue(AqlValueHintNull());
    }
  }

  std::vector<std::pair<int32_t, int32_t>> srchResultPtrs;
  std::string utf8;
  srchResultPtrs.reserve(matchPatterns.size());
  for (auto& search : searchVec) {
    // We now find the first hit for each search string.
    auto pos = search->first(status);
    if (U_FAILURE(status)) {
      ::registerICUWarning(query, AFN, status);
      return AqlValue(AqlValueHintNull());
    }

    int32_t len = 0;
    if (pos != USEARCH_DONE) {
      len = search->getMatchedLength();
    }
    srchResultPtrs.push_back(std::make_pair(pos, len));
  }

  UnicodeString result;
  int32_t lastStart = 0;
  int64_t count = 0;
  while (true) {
    int which = -1;
    int32_t pos = USEARCH_DONE;
    int32_t mLen = 0;
    int i = 0;
    for (auto resultPair : srchResultPtrs) {
      // We locate the nearest matching search result.
      int32_t thisPos;
      thisPos = resultPair.first;
      if ((pos == USEARCH_DONE) || (pos > thisPos)) {
        if (thisPos != USEARCH_DONE) {
          pos = thisPos;
          which = i;
          mLen = resultPair.second;
        }
      }
      i++;
    }
    if (which == -1) {
      break;
    }
    // from last match to this match, copy the original string.
    result.append(unicodeStr, lastStart, pos - lastStart);
    if (replacePatterns.size() != 0) {
      if (replacePatterns.size() > (size_t) which) {
        result.append(replacePatterns[which]);
      }
      else if (replaceWasPlainString) {
        result.append(replacePatterns[0]);
      }
    }

    // lastStart is the place up to we searched the source string
    lastStart = pos + mLen;

    // we try to search the next occurance of this string
    auto& search = searchVec[which];
    pos = search->next(status);
    if (U_FAILURE(status)) {
      ::registerICUWarning(query, AFN, status);
      return AqlValue(AqlValueHintNull());
    }
    if (pos != USEARCH_DONE) {
      mLen = search->getMatchedLength();
    }
    else {
      mLen = -1;
    }
    srchResultPtrs[which] = std::make_pair(pos, mLen);

    which = 0;
    for (auto searchPair : srchResultPtrs) {
      // now we invalidate all search results that overlap with
      // our last search result and see whether we can find the
      // overlapped pattern again.
      // However, that mustn't overlap with the current lastStart
      // position either.
      int32_t thisPos;
      thisPos = searchPair.first;
      if ((thisPos != USEARCH_DONE) && (thisPos < lastStart)) {
        auto &search = searchVec[which];
        pos = thisPos;
        while ((pos < lastStart) && (pos != USEARCH_DONE)) {
          pos = search->next(status);
          if (U_FAILURE(status)) {
            ::registerICUWarning(query, AFN, status);
            return AqlValue(AqlValueHintNull());
          }
          if (pos != USEARCH_DONE) {
            mLen = search->getMatchedLength();
          }
          srchResultPtrs[which] = std::make_pair(pos, mLen);
        }
      }
      which++;
    }

    count ++;
    if ((limit != -1) && (count >= limit)) {
      // Do we have a limit count?
      break;
    }
    // check whether none of our search objects has any more results
    bool allFound = true;
    for (auto resultPair : srchResultPtrs) {
      if (resultPair.first != USEARCH_DONE) {
        allFound = false;
        break;
      }
    }
    if (allFound) {
      break;
    }
  }
  // Append from the last found:
  result.append(unicodeStr, lastStart, unicodeStr.length() - lastStart);

  result.toUTF8String(utf8);
  return AqlValue(utf8);
}

/// @brief function LEFT str, length
AqlValue Functions::Left(arangodb::aql::Query*, transaction::Methods* trx,
                         VPackFunctionParameters const& parameters) {
  static char const* AFN = "LEFT";
  ValidateParameters(parameters, AFN, 2, 2);

  AqlValue value = ExtractFunctionParameterValue(parameters, 0);
  uint32_t length = static_cast<int32_t>(
      ExtractFunctionParameterValue(parameters, 1).toInt64(trx));

  std::string utf8;
  transaction::StringBufferLeaser buffer(trx);
  arangodb::basics::VPackStringBufferAdapter adapter(buffer->stringBuffer());

  ::appendAsString(trx, adapter, value);

  UnicodeString unicodeStr(buffer->c_str(),
                           static_cast<int32_t>(buffer->length()));
  UnicodeString left =
      unicodeStr.tempSubString(0, unicodeStr.moveIndex32(0, length));

  left.toUTF8String(utf8);
  return AqlValue(utf8);
}

/// @brief function RIGHT
AqlValue Functions::Right(arangodb::aql::Query*,
                          transaction::Methods* trx,
                          VPackFunctionParameters const& parameters) {
  ValidateParameters(parameters, "RIGHT", 2, 2);

  AqlValue value = ExtractFunctionParameterValue(parameters, 0);
  uint32_t length = static_cast<int32_t>(
      ExtractFunctionParameterValue(parameters, 1).toInt64(trx));

  std::string utf8;
  transaction::StringBufferLeaser buffer(trx);
  arangodb::basics::VPackStringBufferAdapter adapter(buffer->stringBuffer());

  ::appendAsString(trx, adapter, value);

  UnicodeString unicodeStr(buffer->c_str(),
                           static_cast<int32_t>(buffer->length()));
  UnicodeString right = unicodeStr.tempSubString(unicodeStr.moveIndex32(
      unicodeStr.length(), -static_cast<int32_t>(length)));

  right.toUTF8String(utf8);
  return AqlValue(utf8);
}

namespace {
void ltrimInternal(uint32_t& startOffset, uint32_t& endOffset,
                   UnicodeString& unicodeStr, uint32_t numWhitespaces,
                   UChar32* spaceChars) {
  for (; startOffset < endOffset;
       startOffset = unicodeStr.moveIndex32(startOffset, 1)) {
    bool found = false;

    for (uint32_t pos = 0; pos < numWhitespaces; pos++) {
      if (unicodeStr.char32At(startOffset) == spaceChars[pos]) {
        found = true;
        break;
      }
    }

    if (!found) {
      break;
    }
  }  // for
}
void rtrimInternal(uint32_t& startOffset, uint32_t& endOffset,
                   UnicodeString& unicodeStr, uint32_t numWhitespaces,
                   UChar32* spaceChars) {
  for (uint32_t codeUnitPos = unicodeStr.moveIndex32(unicodeStr.length(), -1);
       startOffset < codeUnitPos;
       codeUnitPos = unicodeStr.moveIndex32(codeUnitPos, -1)) {
    bool found = false;

    for (uint32_t pos = 0; pos < numWhitespaces; pos++) {
      if (unicodeStr.char32At(codeUnitPos) == spaceChars[pos]) {
        found = true;
        break;
      }
    }

    endOffset = unicodeStr.moveIndex32(codeUnitPos, 1);
    if (!found) {
      break;
    }
  }  // for
}
}

/// @brief function TRIM
AqlValue Functions::Trim(arangodb::aql::Query* query, transaction::Methods* trx,
                         VPackFunctionParameters const& parameters) {
  static char const* AFN = "TRIM";
  ValidateParameters(parameters, AFN, 1, 2);

  AqlValue value = ExtractFunctionParameterValue(parameters, 0);
  transaction::StringBufferLeaser buffer(trx);
  arangodb::basics::VPackStringBufferAdapter adapter(buffer->stringBuffer());
  ::appendAsString(trx, adapter, value);
  UnicodeString unicodeStr(buffer->c_str(),
                           static_cast<int32_t>(buffer->length()));

  int64_t howToTrim = 0;
  UnicodeString whitespace("\r\n\t ");

  if (parameters.size() == 2) {
    AqlValue optional = ExtractFunctionParameterValue(parameters, 1);

    if (optional.isNumber()) {
      howToTrim = optional.toInt64(trx);

      if (howToTrim < 0 || 2 < howToTrim) {
        howToTrim = 0;
      }
    } else if (optional.isString()) {
      buffer->clear();
      ::appendAsString(trx, adapter, optional);
      whitespace = UnicodeString(buffer->c_str(),
                                 static_cast<int32_t>(buffer->length()));
    }
  }

  uint32_t numWhitespaces = whitespace.countChar32();
  UErrorCode errorCode = U_ZERO_ERROR;
  auto spaceChars = std::make_unique<UChar32[]>(numWhitespaces);

  whitespace.toUTF32(spaceChars.get(), numWhitespaces, errorCode);
  if (U_FAILURE(errorCode)) {
    ::registerICUWarning(query, AFN, errorCode);
    return AqlValue(AqlValueHintNull());
  }

  uint32_t startOffset = 0, endOffset = unicodeStr.length();

  if (howToTrim <= 1) {
    ltrimInternal(startOffset, endOffset, unicodeStr, numWhitespaces,
                  spaceChars.get());
  }

  if (howToTrim == 2 || howToTrim == 0) {
    rtrimInternal(startOffset, endOffset, unicodeStr, numWhitespaces,
                  spaceChars.get());
  }

  UnicodeString result =
      unicodeStr.tempSubString(startOffset, endOffset - startOffset);
  std::string utf8;
  result.toUTF8String(utf8);
  return AqlValue(utf8);
}

/// @brief function LTRIM
AqlValue Functions::LTrim(arangodb::aql::Query* query,
                          transaction::Methods* trx,
                          VPackFunctionParameters const& parameters) {
  static char const* AFN = "LTRIM";
  ValidateParameters(parameters, AFN, 1, 2);

  AqlValue value = ExtractFunctionParameterValue(parameters, 0);
  transaction::StringBufferLeaser buffer(trx);
  arangodb::basics::VPackStringBufferAdapter adapter(buffer->stringBuffer());
  ::appendAsString(trx, adapter, value);
  UnicodeString unicodeStr(buffer->c_str(),
                           static_cast<int32_t>(buffer->length()));
  UnicodeString whitespace("\r\n\t ");

  if (parameters.size() == 2) {
    AqlValue pWhitespace = ExtractFunctionParameterValue(parameters, 1);
    buffer->clear();
    ::appendAsString(trx, adapter, pWhitespace);
    whitespace =
        UnicodeString(buffer->c_str(), static_cast<int32_t>(buffer->length()));
  }

  uint32_t numWhitespaces = whitespace.countChar32();
  UErrorCode errorCode = U_ZERO_ERROR;
  auto spaceChars = std::make_unique<UChar32[]>(numWhitespaces);

  whitespace.toUTF32(spaceChars.get(), numWhitespaces, errorCode);
  if (U_FAILURE(errorCode)) {
    ::registerICUWarning(query, AFN, errorCode);
    return AqlValue(AqlValueHintNull());
  }

  uint32_t startOffset = 0, endOffset = unicodeStr.length();

  ltrimInternal(startOffset, endOffset, unicodeStr, numWhitespaces,
                spaceChars.get());

  UnicodeString result =
      unicodeStr.tempSubString(startOffset, endOffset - startOffset);
  std::string utf8;
  result.toUTF8String(utf8);
  return AqlValue(utf8);
}

/// @brief function RTRIM
AqlValue Functions::RTrim(arangodb::aql::Query* query,
                          transaction::Methods* trx,
                          VPackFunctionParameters const& parameters) {
  static char const* AFN = "RTRIM";
  ValidateParameters(parameters, AFN, 1, 2);

  AqlValue value = ExtractFunctionParameterValue(parameters, 0);
  transaction::StringBufferLeaser buffer(trx);
  arangodb::basics::VPackStringBufferAdapter adapter(buffer->stringBuffer());
  ::appendAsString(trx, adapter, value);
  UnicodeString unicodeStr(buffer->c_str(),
                           static_cast<int32_t>(buffer->length()));
  UnicodeString whitespace("\r\n\t ");

  if (parameters.size() == 2) {
    AqlValue pWhitespace = ExtractFunctionParameterValue(parameters, 1);
    buffer->clear();
    ::appendAsString(trx, adapter, pWhitespace);
    whitespace =
        UnicodeString(buffer->c_str(), static_cast<int32_t>(buffer->length()));
  }

  uint32_t numWhitespaces = whitespace.countChar32();
  UErrorCode errorCode = U_ZERO_ERROR;
  auto spaceChars = std::make_unique<UChar32[]>(numWhitespaces);

  whitespace.toUTF32(spaceChars.get(), numWhitespaces, errorCode);
  if (U_FAILURE(errorCode)) {
    ::registerICUWarning(query, AFN, errorCode);
    return AqlValue(AqlValueHintNull());
  }

  uint32_t startOffset = 0, endOffset = unicodeStr.length();

  rtrimInternal(startOffset, endOffset, unicodeStr, numWhitespaces,
                spaceChars.get());

  UnicodeString result =
      unicodeStr.tempSubString(startOffset, endOffset - startOffset);
  std::string utf8;
  result.toUTF8String(utf8);
  return AqlValue(utf8);
}

/// @brief function LIKE
AqlValue Functions::Like(arangodb::aql::Query* query, transaction::Methods* trx,
                         VPackFunctionParameters const& parameters) {
  static char const* AFN = "LIKE";
  ValidateParameters(parameters, AFN, 2, 3);
  bool const caseInsensitive = ::getBooleanParameter(trx, parameters, 2, false);
  transaction::StringBufferLeaser buffer(trx);
  arangodb::basics::VPackStringBufferAdapter adapter(buffer->stringBuffer());

  // build pattern from parameter #1
  AqlValue regex = ExtractFunctionParameterValue(parameters, 1);
  ::appendAsString(trx, adapter, regex);

  // the matcher is owned by the query!
  ::RegexMatcher* matcher = query->regexCache()->buildLikeMatcher(
      buffer->c_str(), buffer->length(), caseInsensitive);

  if (matcher == nullptr) {
    // compiling regular expression failed
    ::registerWarning(query, AFN, TRI_ERROR_QUERY_INVALID_REGEX);
    return AqlValue(AqlValueHintNull());
  }

  // extract value
  buffer->clear();
  AqlValue value = ExtractFunctionParameterValue(parameters, 0);
  ::appendAsString(trx, adapter, value);

  bool error = false;
  bool const result = arangodb::basics::Utf8Helper::DefaultUtf8Helper.matches(
      matcher, buffer->c_str(), buffer->length(), false, error);

  if (error) {
    // compiling regular expression failed
    ::registerWarning(query, AFN, TRI_ERROR_QUERY_INVALID_REGEX);
    return AqlValue(AqlValueHintNull());
  }

  return AqlValue(AqlValueHintBool(result));
}

/// @brief function SPLIT
AqlValue Functions::Split(arangodb::aql::Query* query,
                          transaction::Methods* trx,
                          VPackFunctionParameters const& parameters) {
  static char const* AFN = "SPLIT";
  ValidateParameters(parameters, AFN, 1, 3);

  // cheapest parameter checks first:
  int64_t limitNumber = -1;
  if (parameters.size() == 3) {
    AqlValue aqlLimit = ExtractFunctionParameterValue(parameters, 2);
    if (aqlLimit.isNumber()) {
      limitNumber = aqlLimit.toInt64(trx);
    } else {
      ::registerInvalidArgumentWarning(query, AFN);
      return AqlValue(AqlValueHintNull());
    }

    // these are edge cases which are documented to have these return values:
    if (limitNumber < 0) {
      return AqlValue(AqlValueHintNull());
    }
    if (limitNumber == 0) {
      return AqlValue(VPackSlice::emptyArraySlice());
    }
  }

  transaction::StringBufferLeaser regexBuffer(trx);
  AqlValue aqlSeparatorExpression;
  if (parameters.size() >= 2) {
    aqlSeparatorExpression = ExtractFunctionParameterValue(parameters, 1);
    if (aqlSeparatorExpression.isObject()) {
      ::registerInvalidArgumentWarning(query, AFN);
      return AqlValue(AqlValueHintNull());
    }
  }

  AqlValueMaterializer materializer(trx);
  AqlValue aqlValueToSplit = ExtractFunctionParameterValue(parameters, 0);

  if (parameters.size() == 1) {
    // pre-documented edge-case: if we only have the first parameter, return it.
    VPackBuilder result;
    result.openArray();
    result.add(aqlValueToSplit.slice());
    result.close();
    return AqlValue(result);
  }

   // Get ready for ICU
  transaction::StringBufferLeaser buffer(trx);
  arangodb::basics::VPackStringBufferAdapter adapter(buffer->stringBuffer());
  Stringify(trx, adapter, aqlValueToSplit.slice());
  UnicodeString valueToSplit(buffer->c_str(), static_cast<int32_t>(buffer->length()));
  bool isEmptyExpression = false;
  // the matcher is owned by the query!
  ::RegexMatcher* matcher = query->regexCache()->buildSplitMatcher(aqlSeparatorExpression, trx, isEmptyExpression);

  if (matcher == nullptr) {
    // compiling regular expression failed
    ::registerWarning(query, AFN, TRI_ERROR_QUERY_INVALID_REGEX);
    return AqlValue(AqlValueHintNull());
  }

  VPackBuilder result;
  result.openArray();
  if (!isEmptyExpression && (buffer->length() == 0)) {
    // Edge case: splitting an empty string by non-empty expression produces an empty string again.
    result.add(VPackValue(""));
    result.close();
    return AqlValue(result);
  }

  std::string utf8;
  static const uint16_t nrResults = 16;
  UnicodeString uResults[nrResults];
  int64_t totalCount = 0;
  while (true) {
    UErrorCode errorCode = U_ZERO_ERROR;
    auto uCount = matcher->split(valueToSplit, uResults, nrResults, errorCode);
    uint16_t copyThisTime = uCount;

    if (U_FAILURE(errorCode)) {
      ::registerWarning(query, AFN, TRI_ERROR_QUERY_INVALID_REGEX);
      return AqlValue(AqlValueHintNull());
    }

    if ((copyThisTime > 0) && (copyThisTime > nrResults)) {
      // last hit is the remaining string to be fed into split in a subsequent invocation
      copyThisTime --;
    }

    if ((copyThisTime > 0) && ((copyThisTime == nrResults) || isEmptyExpression)) {
      // ICU will give us a traling empty string we don't care for if we split
      // with empty strings.
      copyThisTime --;
    }

    int64_t i = 0;
    while ((i < copyThisTime) &&
           ((limitNumber < 0 ) || (totalCount < limitNumber))) {
      if ((i == 0) && isEmptyExpression) {
        // ICU will give us an empty string that we don't care for
        // as first value of one match-chunk
        i++;
        continue;
      }
      uResults[i].toUTF8String(utf8);
      result.add(VPackValue(utf8));
      utf8.clear();
      i++;
      totalCount++;
    }

    if (((uCount != nrResults)) || // fetch any / found less then N
        ((limitNumber >= 0) && (totalCount >= limitNumber))) { // fetch N
      break;
    }
    // ok, we have more to parse in the last result slot, reiterate with it:
    if(uCount == nrResults) {
      valueToSplit = uResults[nrResults - 1];
    }
    else {
      // should not go beyound the last match!
      TRI_ASSERT(false);
      break;
    }
  }

  result.close();
  return AqlValue(result);
}
/// @brief function REGEX_TEST
AqlValue Functions::RegexTest(arangodb::aql::Query* query,
                              transaction::Methods* trx,
                              VPackFunctionParameters const& parameters) {
  static char const* AFN = "REGEX_TEST";
  ValidateParameters(parameters, AFN, 2, 3);
  bool const caseInsensitive = ::getBooleanParameter(trx, parameters, 2, false);
  transaction::StringBufferLeaser buffer(trx);
  arangodb::basics::VPackStringBufferAdapter adapter(buffer->stringBuffer());

  // build pattern from parameter #1
  AqlValue regex = ExtractFunctionParameterValue(parameters, 1);
  ::appendAsString(trx, adapter, regex);

  // the matcher is owned by the query!
  ::RegexMatcher* matcher = query->regexCache()->buildRegexMatcher(
      buffer->c_str(), buffer->length(), caseInsensitive);

  if (matcher == nullptr) {
    // compiling regular expression failed
    ::registerWarning(query, AFN, TRI_ERROR_QUERY_INVALID_REGEX);
    return AqlValue(AqlValueHintNull());
  }

  // extract value
  buffer->clear();
  AqlValue value = ExtractFunctionParameterValue(parameters, 0);
  ::appendAsString(trx, adapter, value);

  bool error = false;
  bool const result = arangodb::basics::Utf8Helper::DefaultUtf8Helper.matches(
      matcher, buffer->c_str(), buffer->length(), true, error);

  if (error) {
    // compiling regular expression failed
    ::registerWarning(query, AFN, TRI_ERROR_QUERY_INVALID_REGEX);
    return AqlValue(AqlValueHintNull());
  }

  return AqlValue(AqlValueHintBool(result));
}

/// @brief function REGEX_REPLACE
AqlValue Functions::RegexReplace(arangodb::aql::Query* query,
                                 transaction::Methods* trx,
                                 VPackFunctionParameters const& parameters) {
  static char const* AFN = "REGEX_REPLACE";
  ValidateParameters(parameters, AFN, 3, 4);
  bool const caseInsensitive = ::getBooleanParameter(trx, parameters, 3, false);
  transaction::StringBufferLeaser buffer(trx);
  arangodb::basics::VPackStringBufferAdapter adapter(buffer->stringBuffer());

  // build pattern from parameter #1
  AqlValue regex = ExtractFunctionParameterValue(parameters, 1);
  ::appendAsString(trx, adapter, regex);

  // the matcher is owned by the query!
  ::RegexMatcher* matcher = query->regexCache()->buildRegexMatcher(
      buffer->c_str(), buffer->length(), caseInsensitive);

  if (matcher == nullptr) {
    // compiling regular expression failed
    ::registerWarning(query, AFN, TRI_ERROR_QUERY_INVALID_REGEX);
    return AqlValue(AqlValueHintNull());
  }

  // extract value
  buffer->clear();
  AqlValue value = ExtractFunctionParameterValue(parameters, 0);
  ::appendAsString(trx, adapter, value);

  size_t const split = buffer->length();
  AqlValue replace = ExtractFunctionParameterValue(parameters, 2);
  ::appendAsString(trx, adapter, replace);

  bool error = false;
  std::string result = arangodb::basics::Utf8Helper::DefaultUtf8Helper.replace(
      matcher, buffer->c_str(), split, buffer->c_str() + split,
      buffer->length() - split, false, error);

  if (error) {
    // compiling regular expression failed
    ::registerWarning(query, AFN, TRI_ERROR_QUERY_INVALID_REGEX);
    return AqlValue(AqlValueHintNull());
  }

  return AqlValue(result);
}

/// @brief function DATE_NOW
AqlValue Functions::DateNow(arangodb::aql::Query*, transaction::Methods*,
                            VPackFunctionParameters const&) {
  auto millis =
      std::chrono::duration_cast<duration<int64_t, std::milli>>(system_clock::now().time_since_epoch());
  uint64_t dur = millis.count();
  return AqlValue(AqlValueHintUInt(dur));
}

/**
 * @brief Parses 1 or 3-7 input parameters and creates a Date object out of it.
 *        This object can either be a timestamp in milliseconds or an ISO_8601
 * DATE
 *
 * @param query The AQL query
 * @param trx The used transaction
 * @param parameters list of parameters, only 1 or 3-7 are allowed
 * @param asTimestamp If it should return a timestamp (true) or ISO_DATE (false)
 *
 * @return Returns a timestamp if asTimestamp is true, an ISO_DATE otherwise
 */
AqlValue Functions::DateFromParameters(
    arangodb::aql::Query* query, transaction::Methods* trx,
    VPackFunctionParameters const& parameters,
    char const* AFN,
    bool asTimestamp) {
  tp_sys_clock_ms tp;
  duration<int64_t, std::milli> time;

  if (parameters.size() == 1) {
    if (!::parameterToTimePoint(query, trx, parameters, tp, AFN, 0)) {
      return AqlValue(AqlValueHintNull());
    }
    time = tp.time_since_epoch();
  } else {
    if (parameters.size() < 3 || parameters.size() > 7) {
      // YMD is a must
      ::registerInvalidArgumentWarning(query, AFN);
      return AqlValue(AqlValueHintNull());
    }

    for (uint8_t i = 0; i < parameters.size(); i++) {
      AqlValue value = ExtractFunctionParameterValue(parameters, i);

      // All Parameters have to be a number or a string
      if (!value.isNumber() && !value.isString()) {
        ::registerInvalidArgumentWarning(query, AFN);
        return AqlValue(AqlValueHintNull());
      }
    }

    years y{ExtractFunctionParameterValue(parameters, 0).toInt64(trx)};
    months m{ExtractFunctionParameterValue(parameters, 1).toInt64(trx)};
    days d{ExtractFunctionParameterValue(parameters, 2).toInt64(trx)};

    if ( (y < years{0}) || (m < months{0}) || (d < days {0}) ) {
      ::registerWarning(query, AFN, TRI_ERROR_QUERY_INVALID_DATE_VALUE);
      return AqlValue(AqlValueHintNull());
    }
    year_month_day ymd = year{y.count()} / m.count() / d.count();

    // Parse the time
    hours h(0);
    minutes min(0);
    seconds s(0);
    milliseconds ms(0);

    if (parameters.size() >= 4) {
      h = hours((ExtractFunctionParameterValue(parameters, 3).toInt64(trx)));
    }
    if (parameters.size() >= 5) {
      min = minutes((ExtractFunctionParameterValue(parameters, 4).toInt64(trx))); 
    }
    if (parameters.size() >= 6) {
      s = seconds((ExtractFunctionParameterValue(parameters, 5).toInt64(trx)));
    }
    if (parameters.size() == 7) {
      ms = milliseconds(
          (ExtractFunctionParameterValue(parameters, 6).toInt64(trx)));
    }

    if ((h < hours{0}) ||
        (min < minutes{0}) ||
        (s < seconds{0}) ||
        (ms < milliseconds{0})) {
      ::registerWarning(query, AFN,
                      TRI_ERROR_QUERY_INVALID_DATE_VALUE);
      return AqlValue(AqlValueHintNull());
    }

    time = sys_days(ymd).time_since_epoch();
    time += h;
    time += min;
    time += s;
    time += ms;
    tp = tp_sys_clock_ms(time);
  }

  if (asTimestamp) {
    return AqlValue(AqlValueHintInt(time.count()));
  } 
  return ::timeAqlValue(tp);
}

/// @brief function DATE_ISO8601
AqlValue Functions::DateIso8601(arangodb::aql::Query* query,
                                transaction::Methods* trx,
                                VPackFunctionParameters const& parameters) {
  static char const* AFN = "DATE_ISO8601";
  return DateFromParameters(query, trx, parameters, AFN, false);
}

/// @brief function DATE_TIMESTAMP
AqlValue Functions::DateTimestamp(arangodb::aql::Query* query,
                                  transaction::Methods* trx,
                                  VPackFunctionParameters const& parameters) {
  static char const* AFN = "DATE_TIMESTAMP";
  return DateFromParameters(query, trx, parameters, AFN, true);
}

/// @brief function IS_DATESTRING
AqlValue Functions::IsDatestring(arangodb::aql::Query*, transaction::Methods*,
                                 VPackFunctionParameters const& parameters) {
  AqlValue value = ExtractFunctionParameterValue(parameters, 0);

  bool isValid = false;

  if (value.isString()) {
    tp_sys_clock_ms tp;  // unused
    isValid = basics::parse_dateTime(value.slice().copyString(), tp);
  }

  return AqlValue(AqlValueHintBool(isValid));
}

/// @brief function DATE_DAYOFWEEK
AqlValue Functions::DateDayOfWeek(arangodb::aql::Query* query,
                                  transaction::Methods* trx,
                                  VPackFunctionParameters const& parameters) {
  static char const* AFN = "DATE_DAYOFWEEK";
  tp_sys_clock_ms tp;
  if (!::parameterToTimePoint(query, trx, parameters, tp, AFN, 0)) {
    return AqlValue(AqlValueHintNull());
  }
  weekday wd{floor<days>(tp)};

  // Library has unsigned operator implemented
  return AqlValue(AqlValueHintUInt(static_cast<uint64_t>(unsigned(wd))));
}

/// @brief function DATE_YEAR
AqlValue Functions::DateYear(arangodb::aql::Query* query,
                             transaction::Methods* trx,
                             VPackFunctionParameters const& parameters) {
  static char const* AFN = "DATE_YEAR";
  tp_sys_clock_ms tp;

  if (!::parameterToTimePoint(query, trx, parameters, tp, AFN, 0)) {
    return AqlValue(AqlValueHintNull());
  }
  auto ymd = year_month_day(floor<days>(tp));
  // Not the library has operator (int) implemented...
  int64_t year = static_cast<int64_t>((int)ymd.year());
  return AqlValue(AqlValueHintInt(year));
}

/// @brief function DATE_MONTH
AqlValue Functions::DateMonth(arangodb::aql::Query* query,
                              transaction::Methods* trx,
                              VPackFunctionParameters const& parameters) {
  static char const* AFN = "DATE_MONTH";
  tp_sys_clock_ms tp;

  if (!::parameterToTimePoint(query, trx, parameters, tp, AFN, 0)) {
    return AqlValue(AqlValueHintNull());
  }
  auto ymd = year_month_day(floor<days>(tp));
  // The library has operator (unsigned) implemented
  uint64_t month = static_cast<uint64_t>((unsigned)ymd.month());
  return AqlValue(AqlValueHintUInt(month));
}

/// @brief function DATE_DAY
AqlValue Functions::DateDay(arangodb::aql::Query* query,
                            transaction::Methods* trx,
                            VPackFunctionParameters const& parameters) {
  static char const* AFN = "DATE_DAY";
  tp_sys_clock_ms tp;

  if (!::parameterToTimePoint(query, trx, parameters, tp, AFN, 0)) {
    return AqlValue(AqlValueHintNull());
  }

  auto ymd = year_month_day(floor<days>(tp));
  // The library has operator (unsigned) implemented
  uint64_t day = static_cast<uint64_t>((unsigned)ymd.day());
  return AqlValue(AqlValueHintUInt(day));
}

/// @brief function DATE_HOUR
AqlValue Functions::DateHour(arangodb::aql::Query* query,
                             transaction::Methods* trx,
                             VPackFunctionParameters const& parameters) {
  static char const* AFN = "DATE_HOUR";
  tp_sys_clock_ms tp;

  if (!::parameterToTimePoint(query, trx, parameters, tp, AFN, 0)) {
    return AqlValue(AqlValueHintNull());
  }

  auto day_time = make_time(tp - floor<days>(tp));
  uint64_t hours = day_time.hours().count();
  return AqlValue(AqlValueHintUInt(hours));
}

/// @brief function DATE_MINUTE
AqlValue Functions::DateMinute(arangodb::aql::Query* query,
                               transaction::Methods* trx,
                               VPackFunctionParameters const& parameters) {
  static char const* AFN = "DATE_MINUTE";
  tp_sys_clock_ms tp;

  if (!::parameterToTimePoint(query, trx, parameters, tp, AFN, 0)) {
    return AqlValue(AqlValueHintNull());
  }

  auto day_time = make_time(tp - floor<days>(tp));
  uint64_t minutes = day_time.minutes().count();
  return AqlValue(AqlValueHintUInt(minutes));
}

/// @brief function DATE_SECOND
AqlValue Functions::DateSecond(arangodb::aql::Query* query,
                               transaction::Methods* trx,
                               VPackFunctionParameters const& parameters) {
  static char const* AFN = "DATE_SECOND";
  tp_sys_clock_ms tp;

  if (!::parameterToTimePoint(query, trx, parameters, tp, AFN, 0)) {
    return AqlValue(AqlValueHintNull());
  }

  auto day_time = make_time(tp - floor<days>(tp));
  uint64_t seconds = day_time.seconds().count();
  return AqlValue(AqlValueHintUInt(seconds));
}

/// @brief function DATE_MILLISECOND
AqlValue Functions::DateMillisecond(arangodb::aql::Query* query,
                                    transaction::Methods* trx,
                                    VPackFunctionParameters const& parameters) {
  static char const* AFN = "DATE_MILLISECOND";
  tp_sys_clock_ms tp;

  if (!::parameterToTimePoint(query, trx, parameters, tp, AFN, 0)) {
    return AqlValue(AqlValueHintNull());
  }
  auto day_time = make_time(tp - floor<days>(tp));
  uint64_t millis = day_time.subseconds().count();
  return AqlValue(AqlValueHintUInt(millis));
}

/// @brief function DATE_DAYOFYEAR
AqlValue Functions::DateDayOfYear(arangodb::aql::Query* query,
                                  transaction::Methods* trx,
                                  VPackFunctionParameters const& parameters) {
  static char const* AFN = "DATE_DAYOFYEAR";
  tp_sys_clock_ms tp;

  if (!::parameterToTimePoint(query, trx, parameters, tp, AFN, 0)) {
    return AqlValue(AqlValueHintNull());
  }

  auto ymd = year_month_day(floor<days>(tp));
  auto yyyy = year{ymd.year()};
  // we construct the date with the first day in the year:
  auto firstDayInYear = yyyy / jan / day{0};
  uint64_t daysSinceFirst =
      duration_cast<days>(tp - sys_days(firstDayInYear)).count();

  return AqlValue(AqlValueHintUInt(daysSinceFirst));
}

/// @brief function DATE_ISOWEEK
AqlValue Functions::DateIsoWeek(arangodb::aql::Query* query,
                                transaction::Methods* trx,
                                VPackFunctionParameters const& parameters) {
  static char const* AFN = "DATE_ISOWEEK";
  tp_sys_clock_ms tp;

  if (!::parameterToTimePoint(query, trx, parameters, tp, AFN, 0)) {
    return AqlValue(AqlValueHintNull());
  }

  iso_week::year_weeknum_weekday yww{floor<days>(tp)};
  // The (unsigned) operator is overloaded...
  uint64_t isoWeek = static_cast<uint64_t>((unsigned)(yww.weeknum()));
  return AqlValue(AqlValueHintUInt(isoWeek));
}

/// @brief function DATE_LEAPYEAR
AqlValue Functions::DateLeapYear(arangodb::aql::Query* query,
                                 transaction::Methods* trx,
                                 VPackFunctionParameters const& parameters) {
  static char const* AFN = "DATE_LEAPYEAR";
  tp_sys_clock_ms tp;

  if (!::parameterToTimePoint(query, trx, parameters, tp, AFN, 0)) {
    return AqlValue(AqlValueHintNull());
  }

  year_month_day ymd{floor<days>(tp)};

  return AqlValue(AqlValueHintBool(ymd.year().is_leap()));
}

/// @brief function DATE_QUARTER
AqlValue Functions::DateQuarter(arangodb::aql::Query* query,
                                transaction::Methods* trx,
                                VPackFunctionParameters const& parameters) {
  static char const* AFN = "DATE_QUARTER";
  tp_sys_clock_ms tp;

  if (!::parameterToTimePoint(query, trx, parameters, tp, AFN, 0)) {
    return AqlValue(AqlValueHintNull());
  }

  year_month_day ymd{floor<days>(tp)};
  month m = ymd.month();

  // Library has unsigned operator implemented.
  uint64_t part = static_cast<uint64_t>(ceil(unsigned(m) / 3.0f));
  // We only have 4 quarters ;)
  TRI_ASSERT(part <= 4);
  return AqlValue(AqlValueHintUInt(part));
}

/// @brief function DATE_DAYS_IN_MONTH
AqlValue Functions::DateDaysInMonth(arangodb::aql::Query* query,
                                    transaction::Methods* trx,
                                    VPackFunctionParameters const& parameters) {
  static char const* AFN = "DATE_DAYS_IN_MONTH";
  tp_sys_clock_ms tp;

  if (!::parameterToTimePoint(query, trx, parameters, tp, AFN, 0)) {
    return AqlValue(AqlValueHintNull());
  }

  auto ymd = year_month_day{floor<days>(tp)};
  auto lastMonthDay = ymd.year() / ymd.month() / last;

  // The Library has operator unsigned implemented
  return AqlValue(AqlValueHintUInt(static_cast<uint64_t>(unsigned(lastMonthDay.day()))));
}

/// @brief function DATE_TRUNC
AqlValue Functions::DateTrunc(arangodb::aql::Query* query,
                            transaction::Methods* trx,
                            VPackFunctionParameters const& parameters) {
  static char const* AFN = "DATE_TRUNC";
  using namespace std::chrono;
  using namespace date;

  tp_sys_clock_ms tp;

  if (!::parameterToTimePoint(query, trx, parameters, tp, AFN, 0)) {
    return AqlValue(AqlValueHintNull());
  }

  AqlValue durationType = ExtractFunctionParameterValue(parameters, 1);

  if (!durationType.isString()) { // unit type must be string
    ::registerInvalidArgumentWarning(query, AFN);
    return AqlValue(AqlValueHintNull());
  }

  std::string duration = durationType.slice().copyString();
  std::transform(duration.begin(), duration.end(), duration.begin(), ::tolower);

  year_month_day ymd{floor<days>(tp)};
  auto day_time = make_time(tp - sys_days(ymd));
  milliseconds ms{0};
  if (duration == "y" || duration == "year" || duration == "years") {
    ymd = year{ymd.year()}/jan/day{1};
  } else if (duration == "m" || duration == "month" || duration == "months") {
    ymd = year{ymd.year()}/ymd.month()/day{1};
  } else if (duration == "d" || duration == "day" || duration == "days") {
    ;
    // this would be: ymd = year{ymd.year()}/ymd.month()/ymd.day();
    // However, we already split ymd to the precision of days,
    // and ms to cary the timestamp part, so nothing needs to be done here.
  } else if (duration == "h" || duration == "hour" || duration == "hours") {
    ms = day_time.hours();
  } else if (duration == "i" || duration == "minute" || duration == "minutes") {
    ms = day_time.hours() + day_time.minutes();
  } else if (duration == "s" || duration == "second" || duration == "seconds") {
    ms = day_time.to_duration() - day_time.subseconds();
  } else if (duration == "f" || duration == "millisecond" || duration == "milliseconds") {
    ms = day_time.to_duration();
  } else {
    ::registerWarning(query, AFN, TRI_ERROR_QUERY_INVALID_DATE_VALUE);
    return AqlValue(AqlValueHintNull());
  }
  tp = tp_sys_clock_ms{sys_days(ymd) + ms};

  return AqlValue( format("%FT%TZ", floor<milliseconds>(tp) ));
}

/// @brief function DATE_ADD
AqlValue Functions::DateAdd(arangodb::aql::Query* query,
                            transaction::Methods* trx,
                            VPackFunctionParameters const& parameters) {
  static char const* AFN = "DATE_ADD";
  tp_sys_clock_ms tp;

  if (!::parameterToTimePoint(query, trx, parameters, tp, AFN, 0)) {
    return AqlValue(AqlValueHintNull());
  }

  // size == 3 unit / unit type
  // size == 2 iso duration

  if (parameters.size() == 3) {
    AqlValue durationUnit = ExtractFunctionParameterValue(parameters, 1);
    if (!durationUnit.isNumber()) {  // unit must be number
      ::registerInvalidArgumentWarning(query, AFN);
      return AqlValue(AqlValueHintNull());
    }

    AqlValue durationType = ExtractFunctionParameterValue(parameters, 2);
    if (!durationType.isString()) {  // unit type must be string
      ::registerInvalidArgumentWarning(query, AFN);
      return AqlValue(AqlValueHintNull());
    }

    // Numbers and Strings can both be sliced
    return ::addOrSubtractUnitFromTimestamp(query, tp, durationUnit.slice(),
                                            durationType.slice(), false);
  } else {  // iso duration
    AqlValue isoDuration = ExtractFunctionParameterValue(parameters, 1);
    if (!isoDuration.isString()) {
      ::registerInvalidArgumentWarning(query, AFN);
      return AqlValue(AqlValueHintNull());
    }

    std::string const duration = isoDuration.slice().copyString();
    return ::addOrSubtractIsoDurationFromTimestamp(query, tp, duration, false);
  }
}

/// @brief function DATE_SUBTRACT
AqlValue Functions::DateSubtract(arangodb::aql::Query* query,
                                 transaction::Methods* trx,
                                 VPackFunctionParameters const& parameters) {
  static char const* AFN = "DATE_SUBTRACT";
  tp_sys_clock_ms tp;

  if (!::parameterToTimePoint(query, trx, parameters, tp, AFN, 0)) {
    return AqlValue(AqlValueHintNull());
  }

  // size == 3 unit / unit type
  // size == 2 iso duration

  year_month_day ymd{floor<days>(tp)};
  if (parameters.size() == 3) {
    AqlValue durationUnit = ExtractFunctionParameterValue(parameters, 1);
    if (!durationUnit.isNumber()) {  // unit must be number
      ::registerInvalidArgumentWarning(query, AFN);
      return AqlValue(AqlValueHintNull());
    }

    AqlValue durationType = ExtractFunctionParameterValue(parameters, 2);
    if (!durationType.isString()) {  // unit type must be string
      ::registerInvalidArgumentWarning(query, AFN);
      return AqlValue(AqlValueHintNull());
    }

    // Numbers and Strings can both be sliced
    return ::addOrSubtractUnitFromTimestamp(query, tp, durationUnit.slice(),
                                            durationType.slice(), true);
  } else {  // iso duration
    AqlValue isoDuration = ExtractFunctionParameterValue(parameters, 1);
    if (!isoDuration.isString()) {
      ::registerInvalidArgumentWarning(query, AFN);
      return AqlValue(AqlValueHintNull());
    }

    std::string const duration = isoDuration.slice().copyString();
    return ::addOrSubtractIsoDurationFromTimestamp(query, tp, duration, true);
  }
}

/// @brief function DATE_DIFF
AqlValue Functions::DateDiff(arangodb::aql::Query* query,
                             transaction::Methods* trx,
                             VPackFunctionParameters const& parameters) {
  static char const* AFN = "DATE_DIFF";
  // Extract first date
  tp_sys_clock_ms tp1;
  if (!::parameterToTimePoint(query, trx, parameters, tp1, AFN, 0)) {
    return AqlValue(AqlValueHintNull());
  }

  // Extract second date
  tp_sys_clock_ms tp2;
  if (!::parameterToTimePoint(query, trx, parameters, tp2, AFN, 1)) {
    return AqlValue(AqlValueHintNull());
  }

  double diff = 0.0;
  bool asFloat = false;
  auto diffDuration = tp2 - tp1;

  AqlValue unitValue = ExtractFunctionParameterValue(parameters, 2);
  if (!unitValue.isString()) {
    ::registerInvalidArgumentWarning(query, AFN);
    return AqlValue(AqlValueHintNull());
  }

  DateSelectionModifier flag = ::parseDateModifierFlag(unitValue.slice());

  if (parameters.size() == 4) {
    AqlValue asFloatValue = ExtractFunctionParameterValue(parameters, 3);
    if (!asFloatValue.isBoolean()) {
      ::registerInvalidArgumentWarning(query, AFN);
      return AqlValue(AqlValueHintNull());
    }
    asFloat = asFloatValue.toBoolean();
  }

  switch (flag) {
    case YEAR:
      diff = duration_cast<duration<
          double, std::ratio_multiply<std::ratio<146097, 400>, days::period>>>(
                 diffDuration)
                 .count();
      break;
    case MONTH:
      diff =
          duration_cast<
              duration<double, std::ratio_divide<years::period, std::ratio<12>>>>(
              diffDuration)
              .count();
      break;
    case WEEK:
      diff = duration_cast<
                 duration<double, std::ratio_multiply<std::ratio<7>, days::period>>>(
                 diffDuration)
                 .count();
      break;
    case DAY:
      diff = duration_cast<duration<
          double,
          std::ratio_multiply<std::ratio<24>, std::chrono::hours::period>>>(
                 diffDuration)
                 .count();
      break;
    case HOUR:
      diff =
          duration_cast<duration<double, std::ratio<3600>>>(diffDuration).count();
      break;
    case MINUTE:
      diff =
          duration_cast<duration<double, std::ratio<60>>>(diffDuration).count();
      break;
    case SECOND:
      diff = duration_cast<duration<double>>(diffDuration).count();
      break;
    case MILLI:
      diff = duration_cast<duration<double, std::milli>>(diffDuration).count();
      break;
    case INVALID:
      ::registerWarning(query, AFN, TRI_ERROR_QUERY_INVALID_DATE_VALUE);
      return AqlValue(AqlValueHintNull());
  }

  if (asFloat) {
    return AqlValue(AqlValueHintDouble(diff));
  } 
  return AqlValue(AqlValueHintInt(static_cast<int64_t>(std::round(diff))));
}

/// @brief function DATE_COMPARE
AqlValue Functions::DateCompare(arangodb::aql::Query* query,
                                transaction::Methods* trx,
                                VPackFunctionParameters const& parameters) {
  static char const* AFN = "DATE_COMPARE";
  tp_sys_clock_ms tp1;
  if (!::parameterToTimePoint(query, trx, parameters, tp1, AFN, 0)) {
    return AqlValue(AqlValueHintNull());
  }

  tp_sys_clock_ms tp2;
  if (!::parameterToTimePoint(query, trx, parameters, tp2, AFN, 1)) {
    return AqlValue(AqlValueHintNull());
  }

  AqlValue rangeStartValue = ExtractFunctionParameterValue(parameters, 2);

  DateSelectionModifier rangeStart =
      ::parseDateModifierFlag(rangeStartValue.slice());

  if (rangeStart == INVALID) {
    ::registerWarning(query, AFN, TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH);
    return AqlValue(AqlValueHintNull());
  }

  DateSelectionModifier rangeEnd = rangeStart;
  if (parameters.size() == 4) {
    AqlValue rangeEndValue = ExtractFunctionParameterValue(parameters, 3);
    rangeEnd = ::parseDateModifierFlag(rangeEndValue.slice());

    if (rangeEnd == INVALID) {
      ::registerWarning(query, AFN, TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH);
      return AqlValue(AqlValueHintNull());
    }
  }
  auto ymd1 = year_month_day{floor<days>(tp1)};
  auto ymd2 = year_month_day{floor<days>(tp2)};
  auto time1 = make_time(tp1 - floor<days>(tp1));
  auto time2 = make_time(tp2 - floor<days>(tp2));

  // This switch has the following feature:
  // It is ordered by the Highest value of
  // the Modifier (YEAR) and flows down to
  // lower values.
  // In each case if the value is significant
  // (above or equal the endRange) we compare it.
  // If this part is not equal we return false.
  // Otherwise we fall down to the next part.
  // As soon as we are below the endRange
  // we bail out.
  // So all Fall throughs here are intentional
  switch (rangeStart) {
    case YEAR:
      // Always check for the year
      if (ymd1.year() != ymd2.year()) {
        return AqlValue(AqlValueHintBool(false));
      }
      // intentionally falls through
    case MONTH:
      if (rangeEnd > MONTH) {
        break;
      }
      if (ymd1.month() != ymd2.month()) {
        return AqlValue(AqlValueHintBool(false));
      }
      // intentionally falls through
    case DAY:
      if (rangeEnd > DAY) {
        break;
      }
      if (ymd1.day() != ymd2.day()) {
        return AqlValue(AqlValueHintBool(false));
      }
      // intentionally falls through
    case HOUR:
      if (rangeEnd > HOUR) {
        break;
      }
      if (time1.hours() != time2.hours()) {
        return AqlValue(AqlValueHintBool(false));
      }
      // intentionally falls through
    case MINUTE:
      if (rangeEnd > MINUTE) {
        break;
      }
      if (time1.minutes() != time2.minutes()) {
        return AqlValue(AqlValueHintBool(false));
      }
      // intentionally falls through
    case SECOND:
      if (rangeEnd > SECOND) {
        break;
      }
      if (time1.seconds() != time2.seconds()) {
        return AqlValue(AqlValueHintBool(false));
      }
      // intentionally falls through
    case MILLI:
      if (rangeEnd > MILLI) {
        break;
      }
      if (time1.subseconds() != time2.subseconds()) {
        return AqlValue(AqlValueHintBool(false));
      }
      break;
    case INVALID:
    case WEEK:
      // Was handled before
      TRI_ASSERT(false);
  }

  // If we get here all significant places are equal
  // Name these two dates as equal
  return AqlValue(AqlValueHintBool(true));
}

/// @brief function PASSTHRU
AqlValue Functions::Passthru(arangodb::aql::Query*,
                             transaction::Methods* trx,
                             VPackFunctionParameters const& parameters) {
  if (parameters.empty()) {
    return AqlValue(AqlValueHintNull());
  }

  return ExtractFunctionParameterValue(parameters, 0).clone();
}

/// @brief function UNSET
AqlValue Functions::Unset(arangodb::aql::Query* query,
                          transaction::Methods* trx,
                          VPackFunctionParameters const& parameters) {
  static char const* AFN = "UNSET";
  ValidateParameters(parameters, AFN, 2);
  AqlValue value = ExtractFunctionParameterValue(parameters, 0);

  if (!value.isObject()) {
    ::registerInvalidArgumentWarning(query, AFN);
    return AqlValue(AqlValueHintNull());
  }

  std::unordered_set<std::string> names;
  ::extractKeys(names, query, trx, parameters, 1, AFN);

  AqlValueMaterializer materializer(trx);
  VPackSlice slice = materializer.slice(value, false);
  transaction::BuilderLeaser builder(trx);
  ::unsetOrKeep(trx, slice, names, true, false, *builder.get());
  return AqlValue(builder.get());
}

/// @brief function UNSET_RECURSIVE
AqlValue Functions::UnsetRecursive(arangodb::aql::Query* query,
                                   transaction::Methods* trx,
                                   VPackFunctionParameters const& parameters) {
  static char const* AFN = "UNSET_RECURSIVE";
  ValidateParameters(parameters, AFN, 2);
  AqlValue value = ExtractFunctionParameterValue(parameters, 0);

  if (!value.isObject()) {
    ::registerInvalidArgumentWarning(query, AFN);
    return AqlValue(AqlValueHintNull());
  }

  std::unordered_set<std::string> names;
  ::extractKeys(names, query, trx, parameters, 1, AFN);

  AqlValueMaterializer materializer(trx);
  VPackSlice slice = materializer.slice(value, false);
  transaction::BuilderLeaser builder(trx);
  ::unsetOrKeep(trx, slice, names, true, true, *builder.get());
  return AqlValue(builder.get());
}

/// @brief function KEEP
AqlValue Functions::Keep(arangodb::aql::Query* query, transaction::Methods* trx,
                         VPackFunctionParameters const& parameters) {
  static char const* AFN = "KEEP";
  ValidateParameters(parameters, AFN, 2);
  AqlValue value = ExtractFunctionParameterValue(parameters, 0);

  if (!value.isObject()) {
    ::registerInvalidArgumentWarning(query, AFN);
    return AqlValue(AqlValueHintNull());
  }

  std::unordered_set<std::string> names;
  ::extractKeys(names, query, trx, parameters, 1, AFN);

  AqlValueMaterializer materializer(trx);
  VPackSlice slice = materializer.slice(value, false);
  transaction::BuilderLeaser builder(trx);
  ::unsetOrKeep(trx, slice, names, false, false, *builder.get());
  return AqlValue(builder.get());
}

/// @brief function TRANSLATE
AqlValue Functions::Translate(arangodb::aql::Query* query,
                              transaction::Methods* trx,
                              VPackFunctionParameters const& parameters) {
  static char const* AFN = "TRANSLATE";
  ValidateParameters(parameters, AFN, 2, 3);
  AqlValue key = ExtractFunctionParameterValue(parameters, 0);
  AqlValue lookupDocument = ExtractFunctionParameterValue(parameters, 1);

  if (!lookupDocument.isObject()) {
    ::registerInvalidArgumentWarning(query, AFN);
    return AqlValue(AqlValueHintNull());
  }

  AqlValueMaterializer materializer(trx);
  VPackSlice slice = materializer.slice(lookupDocument, true);
  TRI_ASSERT(slice.isObject());

  VPackSlice result;
  if (key.isString()) {
    result = slice.get(key.slice().copyString());
  } else {
    transaction::StringBufferLeaser buffer(trx);
    arangodb::basics::VPackStringBufferAdapter adapter(buffer->stringBuffer());
    Functions::Stringify(trx, adapter, key.slice());
    result = slice.get(buffer->toString());
  }

  if (!result.isNone()) {
    return AqlValue(result);
  }

  // attribute not found, now return the default value
  // we must create copy of it however
  AqlValue defaultValue = ExtractFunctionParameterValue(parameters, 2);
  if (defaultValue.isNone()) {
    return key.clone();
  }
  return defaultValue.clone();
}

/// @brief function MERGE
AqlValue Functions::Merge(arangodb::aql::Query* query,
                          transaction::Methods* trx,
                          VPackFunctionParameters const& parameters) {
  return ::mergeParameters(query, trx, parameters, "MERGE", false);
}

/// @brief function MERGE_RECURSIVE
AqlValue Functions::MergeRecursive(arangodb::aql::Query* query,
                                   transaction::Methods* trx,
                                   VPackFunctionParameters const& parameters) {
  return ::mergeParameters(query, trx, parameters, "MERGE_RECURSIVE", true);
}

/// @brief function HAS
AqlValue Functions::Has(arangodb::aql::Query*, transaction::Methods* trx,
                        VPackFunctionParameters const& parameters) {
  size_t const n = parameters.size();
  if (n < 2) {
    // no parameters
    return AqlValue(AqlValueHintBool(false));
  }

  AqlValue value = ExtractFunctionParameterValue(parameters, 0);

  if (!value.isObject()) {
    // not an object
    return AqlValue(AqlValueHintBool(false));
  }

  AqlValue name = ExtractFunctionParameterValue(parameters, 1);
  std::string p;
  if (!name.isString()) {
    transaction::StringBufferLeaser buffer(trx);
    arangodb::basics::VPackStringBufferAdapter adapter(buffer->stringBuffer());
    ::appendAsString(trx, adapter, name);
    p = std::string(buffer->c_str(), buffer->length());
  } else {
    p = name.slice().copyString();
  }

  return AqlValue(AqlValueHintBool(value.hasKey(trx, p)));
}

/// @brief function ATTRIBUTES
AqlValue Functions::Attributes(arangodb::aql::Query* query,
                               transaction::Methods* trx,
                               VPackFunctionParameters const& parameters) {
  size_t const n = parameters.size();

  if (n < 1) {
    // no parameters
    return AqlValue(AqlValueHintNull());
  }

  AqlValue value = ExtractFunctionParameterValue(parameters, 0);
  if (!value.isObject()) {
    // not an object
    ::registerWarning(query, "ATTRIBUTES",
                    TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH);
    return AqlValue(AqlValueHintNull());
  }

  bool const removeInternal = ::getBooleanParameter(trx, parameters, 1, false);
  bool const doSort = ::getBooleanParameter(trx, parameters, 2, false);

  TRI_ASSERT(value.isObject());
  if (value.length() == 0) {
    return AqlValue(arangodb::basics::VelocyPackHelper::EmptyArrayValue());
  }

  AqlValueMaterializer materializer(trx);
  VPackSlice slice = materializer.slice(value, false);

  if (doSort) {
    std::set<std::string,
             arangodb::basics::VelocyPackHelper::AttributeSorterUTF8>
        keys;

    VPackCollection::keys(slice, keys);
    VPackBuilder result;
    result.openArray();
    for (auto const& it : keys) {
      TRI_ASSERT(!it.empty());
      if (removeInternal && !it.empty() && it.at(0) == '_') {
        continue;
      }
      result.add(VPackValue(it));
    }
    result.close();

    return AqlValue(result);
  }

  std::unordered_set<std::string> keys;
  VPackCollection::keys(slice, keys);

  VPackBuilder result;
  result.openArray();
  for (auto const& it : keys) {
    if (removeInternal && !it.empty() && it.at(0) == '_') {
      continue;
    }
    result.add(VPackValue(it));
  }
  result.close();
  return AqlValue(result);
}

/// @brief function VALUES
AqlValue Functions::Values(arangodb::aql::Query* query,
                           transaction::Methods* trx,
                           VPackFunctionParameters const& parameters) {
  size_t const n = parameters.size();

  if (n < 1) {
    // no parameters
    return AqlValue(AqlValueHintNull());
  }

  AqlValue value = ExtractFunctionParameterValue(parameters, 0);
  if (!value.isObject()) {
    // not an object
    ::registerWarning(query, "VALUES",
                    TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH);
    return AqlValue(AqlValueHintNull());
  }

  bool const removeInternal = ::getBooleanParameter(trx, parameters, 1, false);

  TRI_ASSERT(value.isObject());
  if (value.length() == 0) {
    return AqlValue(arangodb::basics::VelocyPackHelper::EmptyArrayValue());
  }

  AqlValueMaterializer materializer(trx);
  VPackSlice slice = materializer.slice(value, false);
  transaction::BuilderLeaser builder(trx);
  builder->openArray();
  for (auto const& entry : VPackObjectIterator(slice, true)) {
    if (!entry.key.isString()) {
      // somehow invalid
      continue;
    }
    if (removeInternal) {
      VPackValueLength l;
      char const* p = entry.key.getString(l);
      if (l > 0 && *p == '_') {
        // skip attribute
        continue;
      }
    }
    if (entry.value.isCustom()) {
      builder->add(VPackValue(trx->extractIdString(slice)));
    } else {
      builder->add(entry.value);
    }
  }
  builder->close();

  return AqlValue(builder.get());
}

/// @brief function MIN
AqlValue Functions::Min(arangodb::aql::Query* query, transaction::Methods* trx,
                        VPackFunctionParameters const& parameters) {
  AqlValue value = ExtractFunctionParameterValue(parameters, 0);

  if (!value.isArray()) {
    // not an array
    ::registerWarning(query, "MIN", TRI_ERROR_QUERY_ARRAY_EXPECTED);
    return AqlValue(AqlValueHintNull());
  }

  AqlValueMaterializer materializer(trx);
  VPackSlice slice = materializer.slice(value, false);

  VPackSlice minValue;
  auto options = trx->transactionContextPtr()->getVPackOptions();
  for (auto const& it : VPackArrayIterator(slice)) {
    if (it.isNull()) {
      continue;
    }
    if (minValue.isNone() ||
        arangodb::basics::VelocyPackHelper::compare(it, minValue, true,
                                                    options) < 0) {
      minValue = it;
    }
  }
  if (minValue.isNone()) {
    return AqlValue(AqlValueHintNull());
  }
  return AqlValue(minValue);
}

/// @brief function MAX
AqlValue Functions::Max(arangodb::aql::Query* query, transaction::Methods* trx,
                        VPackFunctionParameters const& parameters) {
  AqlValue value = ExtractFunctionParameterValue(parameters, 0);

  if (!value.isArray()) {
    // not an array
    ::registerWarning(query, "MAX", TRI_ERROR_QUERY_ARRAY_EXPECTED);
    return AqlValue(AqlValueHintNull());
  }

  AqlValueMaterializer materializer(trx);
  VPackSlice slice = materializer.slice(value, false);
  VPackSlice maxValue;
  auto options = trx->transactionContextPtr()->getVPackOptions();
  for (auto const& it : VPackArrayIterator(slice)) {
    if (maxValue.isNone() ||
        arangodb::basics::VelocyPackHelper::compare(it, maxValue, true,
                                                    options) > 0) {
      maxValue = it;
    }
  }
  if (maxValue.isNone()) {
    return AqlValue(AqlValueHintNull());
  }
  return AqlValue(maxValue);
}

/// @brief function SUM
AqlValue Functions::Sum(arangodb::aql::Query* query, transaction::Methods* trx,
                        VPackFunctionParameters const& parameters) {
  AqlValue value = ExtractFunctionParameterValue(parameters, 0);

  if (!value.isArray()) {
    // not an array
    ::registerWarning(query, "SUM", TRI_ERROR_QUERY_ARRAY_EXPECTED);
    return AqlValue(AqlValueHintNull());
  }

  AqlValueMaterializer materializer(trx);
  VPackSlice slice = materializer.slice(value, false);
  double sum = 0.0;
  for (auto const& it : VPackArrayIterator(slice)) {
    if (it.isNull()) {
      continue;
    }
    if (!it.isNumber()) {
      return AqlValue(AqlValueHintNull());
    }
    double const number = it.getNumericValue<double>();

    if (!std::isnan(number) && number != HUGE_VAL && number != -HUGE_VAL) {
      sum += number;
    }
  }

  return ::numberValue(trx, sum, false);
}

/// @brief function AVERAGE
AqlValue Functions::Average(arangodb::aql::Query* query,
                            transaction::Methods* trx,
                            VPackFunctionParameters const& parameters) {
  static char const* AFN = "AVERAGE";
  AqlValue value = ExtractFunctionParameterValue(parameters, 0);

  if (!value.isArray()) {
    // not an array
    ::registerWarning(query, AFN, TRI_ERROR_QUERY_ARRAY_EXPECTED);
    return AqlValue(AqlValueHintNull());
  }

  AqlValueMaterializer materializer(trx);
  VPackSlice slice = materializer.slice(value, false);

  double sum = 0.0;
  size_t count = 0;
  for (auto const& v : VPackArrayIterator(slice)) {
    if (v.isNull()) {
      continue;
    }
    if (!v.isNumber()) {
      ::registerWarning(query, AFN, TRI_ERROR_QUERY_ARRAY_EXPECTED);
      return AqlValue(AqlValueHintNull());
    }

    // got a numeric value
    double const number = v.getNumericValue<double>();

    if (!std::isnan(number) && number != HUGE_VAL && number != -HUGE_VAL) {
      sum += number;
      ++count;
    }
  }

  if (count > 0 && !std::isnan(sum) && sum != HUGE_VAL && sum != -HUGE_VAL) {
    return ::numberValue(trx, sum / static_cast<size_t>(count), false);
  }

  return AqlValue(AqlValueHintNull());
}

/// @brief function SLEEP
AqlValue Functions::Sleep(arangodb::aql::Query* query,
                          transaction::Methods* trx,
                          VPackFunctionParameters const& parameters) {
  AqlValue value = ExtractFunctionParameterValue(parameters, 0);

  if (!value.isNumber() || value.toDouble(trx) < 0) {
    ::registerWarning(query, "SLEEP",
                    TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH);
    return AqlValue(AqlValueHintNull());
  }

  double const until = TRI_microtime() + value.toDouble(trx);

  while (TRI_microtime() < until) {
    std::this_thread::sleep_for(std::chrono::microseconds(30000));

    if (query->killed()) {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_QUERY_KILLED);
    } else if (application_features::ApplicationServer::isStopping()) {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_SHUTTING_DOWN);
    }
  }
  return AqlValue(AqlValueHintNull());
}

/// @brief function COLLECTIONS
AqlValue Functions::Collections(arangodb::aql::Query* query,
                                transaction::Methods* trx,
                                VPackFunctionParameters const& parameters) {
  transaction::BuilderLeaser builder(trx);
  builder->openArray();

  auto& vocbase = query->vocbase();
  std::vector<LogicalCollection*> colls;

  // clean memory
  std::function<void()> cleanup;

  // if we are a coordinator, we need to fetch the collection info from the
  // agency
  if (ServerState::instance()->isCoordinator()) {
    cleanup = [&colls]() {
      for (auto& it : colls) {
        if (it != nullptr) {
          delete it;
        }
      }
    };

    colls = GetCollectionsCluster(&vocbase);
  } else {
    colls = vocbase.collections(false);
    cleanup = []() {};
  }

  // make sure memory is cleaned up
  TRI_DEFER(cleanup());

  std::sort(colls.begin(), colls.end(),
            [](LogicalCollection* lhs, LogicalCollection* rhs) -> bool {
              return basics::StringUtils::tolower(lhs->name()) <
                     basics::StringUtils::tolower(rhs->name());
            });

  size_t const n = colls.size();

  for (size_t i = 0; i < n; ++i) {
    LogicalCollection* coll = colls[i];

    if (ExecContext::CURRENT != nullptr &&
        !ExecContext::CURRENT->canUseCollection(vocbase.name(), coll->name(), auth::Level::RO)) {
      continue;
    }

    builder->openObject();
    builder->add("_id", VPackValue(std::to_string(coll->id())));
    builder->add("name", VPackValue(coll->name()));
    builder->close();
  }

  builder->close();

  return AqlValue(builder.get());
}

/// @brief function RANDOM_TOKEN
AqlValue Functions::RandomToken(arangodb::aql::Query*,
                                transaction::Methods* trx,
                                VPackFunctionParameters const& parameters) {
  AqlValue value = ExtractFunctionParameterValue(parameters, 0);

  int64_t const length = value.toInt64(trx);
  if (length <= 0 || length > 65536) {
    THROW_ARANGO_EXCEPTION_PARAMS(
        TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH, "RANDOM_TOKEN");
  }

  UniformCharacter JSNumGenerator(
      "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789");
  return AqlValue(JSNumGenerator.random(static_cast<size_t>(length)));
}

/// @brief function MD5
AqlValue Functions::Md5(arangodb::aql::Query*, transaction::Methods* trx,
                        VPackFunctionParameters const& parameters) {
  AqlValue value = ExtractFunctionParameterValue(parameters, 0);
  transaction::StringBufferLeaser buffer(trx);
  arangodb::basics::VPackStringBufferAdapter adapter(buffer->stringBuffer());

  ::appendAsString(trx, adapter, value);

  // create md5
  char hash[17];
  char* p = &hash[0];
  size_t length;

  arangodb::rest::SslInterface::sslMD5(buffer->c_str(), buffer->length(), p,
                                       length);

  // as hex
  char hex[33];
  p = &hex[0];

  arangodb::rest::SslInterface::sslHEX(hash, 16, p, length);

  return AqlValue(&hex[0], 32);
}

/// @brief function SHA1
AqlValue Functions::Sha1(arangodb::aql::Query*, transaction::Methods* trx,
                         VPackFunctionParameters const& parameters) {
  AqlValue value = ExtractFunctionParameterValue(parameters, 0);
  transaction::StringBufferLeaser buffer(trx);
  arangodb::basics::VPackStringBufferAdapter adapter(buffer->stringBuffer());

  ::appendAsString(trx, adapter, value);

  // create sha1
  char hash[21];
  char* p = &hash[0];
  size_t length;

  arangodb::rest::SslInterface::sslSHA1(buffer->c_str(), buffer->length(), p,
                                        length);

  // as hex
  char hex[41];
  p = &hex[0];

  arangodb::rest::SslInterface::sslHEX(hash, 20, p, length);

  return AqlValue(&hex[0], 40);
}

/// @brief function SHA512
AqlValue Functions::Sha512(arangodb::aql::Query*,
                           transaction::Methods* trx,
                           VPackFunctionParameters const& parameters) {
  AqlValue value = ExtractFunctionParameterValue(parameters, 0);
  transaction::StringBufferLeaser buffer(trx);
  arangodb::basics::VPackStringBufferAdapter adapter(buffer->stringBuffer());

  ::appendAsString(trx, adapter, value);

  // create sha512
  char hash[65];
  char* p = &hash[0];
  size_t length;

  arangodb::rest::SslInterface::sslSHA512(buffer->c_str(), buffer->length(), p,
                                          length);

  // as hex
  char hex[129];
  p = &hex[0];

  arangodb::rest::SslInterface::sslHEX(hash, 64, p, length);

  return AqlValue(&hex[0], 128);
}

/// @brief function HASH
AqlValue Functions::Hash(arangodb::aql::Query*, transaction::Methods* trx,
                         VPackFunctionParameters const& parameters) {
  AqlValue value = ExtractFunctionParameterValue(parameters, 0);

  // throw away the top bytes so the hash value can safely be used
  // without precision loss when storing in JavaScript etc.
  uint64_t hash = value.hash(trx) & 0x0007ffffffffffffULL;

  return AqlValue(AqlValueHintUInt(hash));
}

/// @brief function IS_KEY
AqlValue Functions::IsKey(arangodb::aql::Query*,
                          transaction::Methods* trx,
                          VPackFunctionParameters const& parameters) {
  AqlValue value = ExtractFunctionParameterValue(parameters, 0);
  if (!value.isString()) {
    // not a string, so no valid key
    return AqlValue(AqlValueHintBool(false));
  }

  VPackValueLength l;
  char const* p = value.slice().getString(l);
  return AqlValue(AqlValueHintBool(TraditionalKeyGenerator::validateKey(p, l)));
}

/// @brief function UNIQUE
AqlValue Functions::Unique(arangodb::aql::Query* query,
                           transaction::Methods* trx,
                           VPackFunctionParameters const& parameters) {
  static char const* AFN = "UNIQUE";
  ValidateParameters(parameters, AFN, 1, 1);

  AqlValue value = ExtractFunctionParameterValue(parameters, 0);

  if (!value.isArray()) {
    // not an array
    ::registerWarning(query, AFN, TRI_ERROR_QUERY_ARRAY_EXPECTED);
    return AqlValue(AqlValueHintNull());
  }

  AqlValueMaterializer materializer(trx);
  VPackSlice slice = materializer.slice(value, false);

  auto options = trx->transactionContextPtr()->getVPackOptions();
  std::unordered_set<VPackSlice, arangodb::basics::VelocyPackHelper::VPackHash,
                     arangodb::basics::VelocyPackHelper::VPackEqual>
      values(512, arangodb::basics::VelocyPackHelper::VPackHash(),
             arangodb::basics::VelocyPackHelper::VPackEqual(options));

  for (VPackSlice s : VPackArrayIterator(slice)) {
    if (!s.isNone()) {
      values.emplace(s.resolveExternal());
    }
  }

  transaction::BuilderLeaser builder(trx);
  builder->openArray();
  for (auto const& it : values) {
    builder->add(it);
  }
  builder->close();
  return AqlValue(builder.get());
}

/// @brief function SORTED_UNIQUE
AqlValue Functions::SortedUnique(arangodb::aql::Query* query,
                                 transaction::Methods* trx,
                                 VPackFunctionParameters const& parameters) {
  static char const* AFN = "SORTED_UNIQUE";
  ValidateParameters(parameters, AFN, 1, 1);
  AqlValue value = ExtractFunctionParameterValue(parameters, 0);

  if (!value.isArray()) {
    // not an array
    ::registerWarning(query, AFN, TRI_ERROR_QUERY_ARRAY_EXPECTED);
    return AqlValue(AqlValueHintNull());
  }

  AqlValueMaterializer materializer(trx);
  VPackSlice slice = materializer.slice(value, false);

  arangodb::basics::VelocyPackHelper::VPackLess<true> less(
      trx->transactionContext()->getVPackOptions(), &slice, &slice);
  std::set<VPackSlice, arangodb::basics::VelocyPackHelper::VPackLess<true>>
      values(less);
  for (auto const& it : VPackArrayIterator(slice)) {
    if (!it.isNone()) {
      values.insert(it);
    }
  }

  transaction::BuilderLeaser builder(trx);
  builder->openArray();
  for (auto const& it : values) {
    builder->add(it);
  }
  builder->close();
  return AqlValue(builder.get());
}

/// @brief function SORTED
AqlValue Functions::Sorted(arangodb::aql::Query* query,
                           transaction::Methods* trx,
                           VPackFunctionParameters const& parameters) {
  static char const* AFN = "SORTED";
  ValidateParameters(parameters, AFN, 1, 1);
  AqlValue value = ExtractFunctionParameterValue(parameters, 0);

  if (!value.isArray()) {
    // not an array
    ::registerWarning(query, AFN, TRI_ERROR_QUERY_ARRAY_EXPECTED);
    return AqlValue(AqlValueHintNull());
  }

  AqlValueMaterializer materializer(trx);
  VPackSlice slice = materializer.slice(value, false);

  arangodb::basics::VelocyPackHelper::VPackLess<true> less(
      trx->transactionContext()->getVPackOptions(), &slice, &slice);
  std::map<VPackSlice, size_t,
           arangodb::basics::VelocyPackHelper::VPackLess<true>>
      values(less);
  for (auto const& it : VPackArrayIterator(slice)) {
    if (!it.isNone()) {
      auto f = values.emplace(it, 1);
      if (!f.second) {
        ++(*f.first).second;
      }
    }
  }

  transaction::BuilderLeaser builder(trx);
  builder->openArray();
  for (auto const& it : values) {
    for (size_t i = 0; i < it.second; ++i) {
      builder->add(it.first);
    }
  }
  builder->close();
  return AqlValue(builder.get());
}

/// @brief function UNION
AqlValue Functions::Union(arangodb::aql::Query* query,
                          transaction::Methods* trx,
                          VPackFunctionParameters const& parameters) {
  static char const* AFN = "UNION";
  ValidateParameters(parameters, AFN, 2);

  transaction::BuilderLeaser builder(trx);
  builder->openArray();
  size_t const n = parameters.size();
  for (size_t i = 0; i < n; ++i) {
    AqlValue value = ExtractFunctionParameterValue(parameters, i);

    if (!value.isArray()) {
      // not an array
      ::registerInvalidArgumentWarning(query, AFN);
      return AqlValue(AqlValueHintNull());
    }

    TRI_IF_FAILURE("AqlFunctions::OutOfMemory1") {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
    }

    AqlValueMaterializer materializer(trx);
    VPackSlice slice = materializer.slice(value, false);

    // this passes ownership for the JSON contens into result
    for (auto const& it : VPackArrayIterator(slice)) {
      builder->add(it);
      TRI_IF_FAILURE("AqlFunctions::OutOfMemory2") {
        THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
      }
    }
  }
  builder->close();
  TRI_IF_FAILURE("AqlFunctions::OutOfMemory3") {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
  }

  return AqlValue(builder.get());
}

/// @brief function UNION_DISTINCT
AqlValue Functions::UnionDistinct(arangodb::aql::Query* query,
                                  transaction::Methods* trx,
                                  VPackFunctionParameters const& parameters) {
  static char const* AFN = "UNION_DISTINCT";
  ValidateParameters(parameters, AFN, 2);
  size_t const n = parameters.size();

  auto options = trx->transactionContextPtr()->getVPackOptions();
  std::unordered_set<VPackSlice, arangodb::basics::VelocyPackHelper::VPackHash,
                     arangodb::basics::VelocyPackHelper::VPackEqual>
      values(512, arangodb::basics::VelocyPackHelper::VPackHash(),
             arangodb::basics::VelocyPackHelper::VPackEqual(options));

  std::vector<AqlValueMaterializer> materializers;
  materializers.reserve(n);
  for (size_t i = 0; i < n; ++i) {
    AqlValue value = ExtractFunctionParameterValue(parameters, i);

    if (!value.isArray()) {
      // not an array
      ::registerInvalidArgumentWarning(query, AFN);
      return AqlValue(AqlValueHintNull());
    }

    materializers.emplace_back(trx);
    VPackSlice slice = materializers.back().slice(value, false);

    for (VPackSlice v : VPackArrayIterator(slice)) {
      v = v.resolveExternal();
      if (values.find(v) == values.end()) {
        TRI_IF_FAILURE("AqlFunctions::OutOfMemory1") {
          THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
        }

        values.emplace(v);
      }
    }
  }

  TRI_IF_FAILURE("AqlFunctions::OutOfMemory2") {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
  }

  transaction::BuilderLeaser builder(trx);
  builder->openArray();
  for (auto const& it : values) {
    builder->add(it);
  }
  builder->close();

  TRI_IF_FAILURE("AqlFunctions::OutOfMemory3") {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
  }

  return AqlValue(builder.get());
}

/// @brief function INTERSECTION
AqlValue Functions::Intersection(arangodb::aql::Query* query,
                                 transaction::Methods* trx,
                                 VPackFunctionParameters const& parameters) {
  static char const* AFN = "INTERSECTION";
  ValidateParameters(parameters, AFN, 2);

  auto options = trx->transactionContextPtr()->getVPackOptions();
  std::unordered_map<VPackSlice, size_t,
                     arangodb::basics::VelocyPackHelper::VPackHash,
                     arangodb::basics::VelocyPackHelper::VPackEqual>
      values(512, arangodb::basics::VelocyPackHelper::VPackHash(),
             arangodb::basics::VelocyPackHelper::VPackEqual(options));

  size_t const n = parameters.size();
  std::vector<AqlValueMaterializer> materializers;
  materializers.reserve(n);
  for (size_t i = 0; i < n; ++i) {
    AqlValue value = ExtractFunctionParameterValue(parameters, i);

    if (!value.isArray()) {
      // not an array
      ::registerWarning(query, AFN, TRI_ERROR_QUERY_ARRAY_EXPECTED);
      return AqlValue(AqlValueHintNull());
    }

    materializers.emplace_back(trx);
    VPackSlice slice = materializers.back().slice(value, false);

    for (auto const& it : VPackArrayIterator(slice)) {
      if (i == 0) {
        // round one

        TRI_IF_FAILURE("AqlFunctions::OutOfMemory1") {
          THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
        }

        values.emplace(it, 1);
      } else {
        // check if we have seen the same element before
        auto found = values.find(it);
        if (found != values.end()) {
          // already seen
          if ((*found).second < i) {
            (*found).second = 0;
          } else {
            (*found).second = i + 1;
          }
        }
      }
    }
  }

  TRI_IF_FAILURE("AqlFunctions::OutOfMemory2") {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
  }

  transaction::BuilderLeaser builder(trx);
  builder->openArray();
  for (auto const& it : values) {
    if (it.second == n) {
      builder->add(it.first);
    }
  }
  builder->close();

  TRI_IF_FAILURE("AqlFunctions::OutOfMemory3") {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
  }
  return AqlValue(builder.get());
}

/// @brief function OUTERSECTION
AqlValue Functions::Outersection(arangodb::aql::Query* query,
                                 transaction::Methods* trx,
                                 VPackFunctionParameters const& parameters) {
  static char const* AFN = "OUTERSECTION";
  ValidateParameters(parameters, AFN, 2);

  auto options = trx->transactionContextPtr()->getVPackOptions();
  std::unordered_map<VPackSlice, size_t,
                     arangodb::basics::VelocyPackHelper::VPackHash,
                     arangodb::basics::VelocyPackHelper::VPackEqual>
      values(512, arangodb::basics::VelocyPackHelper::VPackHash(),
             arangodb::basics::VelocyPackHelper::VPackEqual(options));

  size_t const n = parameters.size();
  std::vector<AqlValueMaterializer> materializers;
  materializers.reserve(n);
  for (size_t i = 0; i < n; ++i) {
    AqlValue value = ExtractFunctionParameterValue(parameters, i);

    if (!value.isArray()) {
      // not an array
      ::registerWarning(query, AFN, TRI_ERROR_QUERY_ARRAY_EXPECTED);
      return AqlValue(AqlValueHintNull());
    }

    materializers.emplace_back(trx);
    VPackSlice slice = materializers.back().slice(value, false);

    for (auto const& it : VPackArrayIterator(slice)) {
      // check if we have seen the same element before
      auto found = values.find(it);
      if (found != values.end()) {
        // already seen
        TRI_ASSERT((*found).second > 0);
        ++(found->second);
      } else {
        values.emplace(it, 1);
      }
    }
  }

  TRI_IF_FAILURE("AqlFunctions::OutOfMemory2") {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
  }

  transaction::BuilderLeaser builder(trx);
  builder->openArray();
  for (auto const& it : values) {
    if (it.second == 1) {
      builder->add(it.first);
    }
  }
  builder->close();

  TRI_IF_FAILURE("AqlFunctions::OutOfMemory3") {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
  }
  return AqlValue(builder.get());
}

/// @brief function DISTANCE
AqlValue Functions::Distance(arangodb::aql::Query* query,
                             transaction::Methods* trx,
                             VPackFunctionParameters const& parameters) {
  static char const* AFN = "DISTANCE";
  ValidateParameters(parameters, AFN, 4, 4);

  AqlValue lat1 = ExtractFunctionParameterValue(parameters, 0);
  AqlValue lon1 = ExtractFunctionParameterValue(parameters, 1);
  AqlValue lat2 = ExtractFunctionParameterValue(parameters, 2);
  AqlValue lon2 = ExtractFunctionParameterValue(parameters, 3);

  // non-numeric input...
  if (!lat1.isNumber() || !lon1.isNumber() || !lat2.isNumber() ||
      !lon2.isNumber()) {
    ::registerWarning(query, AFN, TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH);
    return AqlValue(AqlValueHintNull());
  }

  bool failed;
  bool error = false;
  double lat1Value = lat1.toDouble(trx, failed);
  error |= failed;
  double lon1Value = lon1.toDouble(trx, failed);
  error |= failed;
  double lat2Value = lat2.toDouble(trx, failed);
  error |= failed;
  double lon2Value = lon2.toDouble(trx, failed);
  error |= failed;

  if (error) {
    ::registerWarning(query, AFN, TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH);
    return AqlValue(AqlValueHintNull());
  }

  auto toRadians = [](double degrees) -> double {
    return degrees * (std::acos(-1.0) / 180.0);
  };

  double p1 = toRadians(lat1Value);
  double p2 = toRadians(lat2Value);
  double d1 = toRadians(lat2Value - lat1Value);
  double d2 = toRadians(lon2Value - lon1Value);

  double a =
      std::sin(d1 / 2.0) * std::sin(d1 / 2.0) +
      std::cos(p1) * std::cos(p2) * std::sin(d2 / 2.0) * std::sin(d2 / 2.0);

  double c = 2.0 * std::atan2(std::sqrt(a), std::sqrt(1.0 - a));
  double const EARTHRADIAN = 6371000.0;  // metres

  return ::numberValue(trx, EARTHRADIAN * c, true);
}


/// @brief function GEO_DISTANCE
AqlValue Functions::GeoDistance(arangodb::aql::Query* query,
                             transaction::Methods* trx,
                             VPackFunctionParameters const& parameters) {
  ValidateParameters(parameters, "GEO_DISTANCE", 2, 2);

  AqlValue loc1 = ExtractFunctionParameterValue(parameters, 0);
  AqlValue loc2 = ExtractFunctionParameterValue(parameters, 1);

  Result res(TRI_ERROR_BAD_PARAMETER, "Requires coordinate pair or GeoJSON");
  AqlValueMaterializer mat1(trx);
  geo::ShapeContainer shape1, shape2;
  if (loc1.isArray() && loc1.length() >= 2) {
    res = shape1.parseCoordinates(mat1.slice(loc1, true), /*geoJson*/true);
  } else if (loc1.isObject()) {
    res = geo::geojson::parseRegion(mat1.slice(loc1, true), shape1);
  }
  if (res.fail()) {
    ::registerWarning(query, "GEO_DISTANCE", res);
    return AqlValue(AqlValueHintNull());
  }

  AqlValueMaterializer mat2(trx);
  res.reset(TRI_ERROR_BAD_PARAMETER, "Requires coordinate pair or GeoJSON");
  if (loc2.isArray() && loc2.length() >= 2) {
    res = shape2.parseCoordinates(mat2.slice(loc2, true), /*geoJson*/true);
  } else if (loc2.isObject()) {
    res = geo::geojson::parseRegion(mat2.slice(loc2, true), shape2);
  }
  if (res.fail()) {
    ::registerWarning(query, "GEO_DISTANCE", res);
    return AqlValue(AqlValueHintNull());
  }

  return ::numberValue(trx, shape1.distanceFrom(shape2.centroid()), true);
}

static AqlValue GeoContainsIntersect(arangodb::aql::Query* query,
                                     transaction::Methods* trx,
                                     VPackFunctionParameters const& parameters,
                                     char const* func, bool contains) {
  Functions::ValidateParameters(parameters, func, 2, 2);
  AqlValue p1 = Functions::ExtractFunctionParameterValue(parameters, 0);
  AqlValue p2 = Functions::ExtractFunctionParameterValue(parameters, 1);

  if (!p1.isObject()) {
    ::registerWarning(query, func, Result(
      TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH, "Expecting GeoJSON object"));
    return AqlValue(AqlValueHintNull());
  }

  AqlValueMaterializer mat1(trx);
  geo::ShapeContainer outer, inner;
  Result res = geo::geojson::parseRegion(mat1.slice(p1, true), outer);
  if (res.fail()) {
    ::registerWarning(query, func, res);
    return AqlValue(AqlValueHintNull());
  }
  if (contains && !outer.isAreaType()) {
    ::registerWarning(query, func, Result(TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH,
                    "Only Polygon and MultiPolygon types are valid as first argument"));
    return AqlValue(AqlValueHintNull());
  }

  AqlValueMaterializer mat2(trx);
  res.reset(TRI_ERROR_BAD_PARAMETER, "Second arg requires coordinate pair or GeoJSON");
  if (p2.isArray() && p2.length() >= 2) {
    res = inner.parseCoordinates(mat2.slice(p2, true), /*geoJson*/true);
  } else if (p2.isObject()) {
    res = geo::geojson::parseRegion(mat2.slice(p2, true), inner);
  }
  if (res.fail()) {
    ::registerWarning(query, func, res);
    return AqlValue(AqlValueHintNull());
  }

  bool result = contains ? outer.contains(&inner) : outer.intersects(&inner);
  return AqlValue(AqlValueHintBool(result));
}

/// @brief function GEO_CONTAINS
AqlValue Functions::GeoContains(arangodb::aql::Query* query,
                                transaction::Methods* trx,
                                VPackFunctionParameters const& parameters) {
  return GeoContainsIntersect(query, trx, parameters, "GEO_CONTAINS", true);
}

/// @brief function GEO_INTERSECTS
AqlValue Functions::GeoIntersects(arangodb::aql::Query* query,
                                  transaction::Methods* trx,
                                  VPackFunctionParameters const& parameters) {
  return GeoContainsIntersect(query, trx, parameters, "GEO_INTERSECTS", false);
}

/// @brief function GEO_EQUALS
AqlValue Functions::GeoEquals(arangodb::aql::Query* query,
                             transaction::Methods* trx,
                             VPackFunctionParameters const& parameters) {
  ValidateParameters(parameters, "GEO_EQUALS", 2, 2);

  AqlValue p1 = Functions::ExtractFunctionParameterValue(parameters, 0);
  AqlValue p2 = Functions::ExtractFunctionParameterValue(parameters, 1);

  if (!p1.isObject() || !p2.isObject()) {
    ::registerWarning(query, "GEO_EQUALS", Result(
      TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH, "Expecting GeoJSON object"));
    return AqlValue(AqlValueHintNull());
  }

  AqlValueMaterializer mat1(trx);
  AqlValueMaterializer mat2(trx);

  geo::ShapeContainer first, second;
  Result res1 = geo::geojson::parseRegion(mat1.slice(p1, true), first);
  Result res2 = geo::geojson::parseRegion(mat2.slice(p2, true), second);

  if (res1.fail()) {
    ::registerWarning(query, "GEO_EQUALS", res1);
    return AqlValue(AqlValueHintNull());
  }
  if (res2.fail()) {
    ::registerWarning(query, "GEO_EQUALS", res2);
    return AqlValue(AqlValueHintNull());
  }

  bool result = first.equals(&second);
  return AqlValue(AqlValueHintBool(result));
}


/// @brief function IS_IN_POLYGON
AqlValue Functions::IsInPolygon(arangodb::aql::Query* query,
                                transaction::Methods* trx,
                                VPackFunctionParameters const& parameters) {
  ValidateParameters(parameters, "IS_IN_POLYGON", 2, 3);

  AqlValue coords = ExtractFunctionParameterValue(parameters, 0);
  AqlValue p2 = ExtractFunctionParameterValue(parameters, 1);
  AqlValue p3 = ExtractFunctionParameterValue(parameters, 2);

  if (!coords.isArray()) {
    ::registerWarning(query, "IS_IN_POLYGON", TRI_ERROR_QUERY_ARRAY_EXPECTED);
    return AqlValue(AqlValueHintNull());
  }

  double latitude, longitude;
  bool geoJson = false;
  if (p2.isArray()) {
    if (p2.length() < 2) {
      ::registerInvalidArgumentWarning(query, "IS_IN_POLYGON");
      return AqlValue(AqlValueHintNull());
    }
    AqlValueMaterializer materializer(trx);
    VPackSlice arr = materializer.slice(p2, false);
    geoJson = p3.isBoolean() && p3.toBoolean();
    // if geoJson, map [lon, lat] -> lat, lon
    VPackSlice lat = geoJson ? arr[1] : arr[0];
    VPackSlice lon = geoJson ? arr[0] : arr[1];
    if (!lat.isNumber() || !lon.isNumber()) {
      ::registerInvalidArgumentWarning(query, "IS_IN_POLYGON");
      return AqlValue(AqlValueHintNull());
    }
    latitude = lat.getNumber<double>();
    longitude = lon.getNumber<double>();
  } else if (p2.isNumber() && p3.isNumber()) {
    bool failed1 = false, failed2 = false;
    latitude = p2.toDouble(trx, failed1);
    longitude = p3.toDouble(trx, failed2);
    if (failed1 || failed2) {
      ::registerInvalidArgumentWarning(query, "IS_IN_POLYGON");
      return AqlValue(AqlValueHintNull());
    }
  } else {
    ::registerInvalidArgumentWarning(query, "IS_IN_POLYGON");
    return AqlValue(AqlValueHintNull());
  }

  S2Loop loop;
  Result res = geo::geojson::parseLoop(coords.slice(), geoJson, loop);
  if (res.fail() || !loop.IsValid()) {
    ::registerWarning(query, "IS_IN_POLYGON", res);
    return AqlValue(AqlValueHintNull());
  }

  S2LatLng latLng = S2LatLng::FromDegrees(latitude, longitude);
  return AqlValue(AqlValueHintBool(loop.Contains(latLng.ToPoint())));
}

/// @brief geo constructors

/// @brief function GEO_POINT
AqlValue Functions::GeoPoint(arangodb::aql::Query* query,
                             transaction::Methods* trx,
                             VPackFunctionParameters const& parameters) {
  ValidateParameters(parameters, "GEO_POINT", 2, 2);

  size_t const n = parameters.size();

  if (n < 2) {
    // no parameters
    return AqlValue(AqlValueHintNull());
  }

  AqlValue lon1 = ExtractFunctionParameterValue(parameters, 0);
  AqlValue lat1 = ExtractFunctionParameterValue(parameters, 1);

  // non-numeric input
  if (!lat1.isNumber() || !lon1.isNumber()) {
    ::registerWarning(query, "GEO_POINT",
                    TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH);
    return AqlValue(arangodb::basics::VelocyPackHelper::NullValue());
  }

  bool failed;
  bool error = false;
  double lon1Value = lon1.toDouble(trx, failed);
  error |= failed;
  double lat1Value = lat1.toDouble(trx, failed);
  error |= failed;

  if (error) {
    ::registerWarning(query, "GEO_POINT",
                      TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH);
    return AqlValue(arangodb::basics::VelocyPackHelper::NullValue());
  }

  VPackBuilder b;

  b.add(VPackValue(VPackValueType::Object));
  b.add("type", VPackValue("Point"));
  b.add("coordinates", VPackValue(VPackValueType::Array));
  b.add(VPackValue(lon1Value));
  b.add(VPackValue(lat1Value));
  b.close();
  b.close();

  return AqlValue(b);
}

/// @brief function GEO_MULTIPOINT
AqlValue Functions::GeoMultiPoint(arangodb::aql::Query* query,
                                  transaction::Methods* trx,
                                  VPackFunctionParameters const& parameters) {

  ValidateParameters(parameters, "GEO_MULTIPOINT", 1, 1);

  size_t const n = parameters.size();

  if (n < 1) {
    // no parameters
    return AqlValue(AqlValueHintNull());
  }

  AqlValue geoArray = ExtractFunctionParameterValue(parameters, 0);

  if (!geoArray.isArray()) {
    ::registerWarning(query, "GEO_MULTIPOINT",
                    TRI_ERROR_QUERY_ARRAY_EXPECTED);
    return AqlValue(arangodb::basics::VelocyPackHelper::NullValue());
  }
  if (geoArray.length() < 2) {
    ::registerWarning(query, "GEO_MULTIPOINT", Result(
          TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH,
          "a MultiPoint needs at least two positions"));
    return AqlValue(arangodb::basics::VelocyPackHelper::NullValue());
  }

  VPackBuilder b;

  b.add(VPackValue(VPackValueType::Object));
  b.add("type", VPackValue("MultiPoint"));
  b.add("coordinates", VPackValue(VPackValueType::Array));

  AqlValueMaterializer materializer(trx);
  VPackSlice s = materializer.slice(geoArray, false);
  for (auto const& v : VPackArrayIterator(s)) {
    if (v.isArray()) {
      b.openArray();
      for (auto const& coord : VPackArrayIterator(v)) {
        if (coord.isNumber()) {
          b.add(VPackValue(coord.getNumber<double>()));
        } else {
          ::registerWarning(query, "GEO_MULTIPOINT", Result(
                TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH,
                "not a numeric value"));
          return AqlValue(arangodb::basics::VelocyPackHelper::NullValue());
        }
      }
      b.close();
    } else {
      ::registerWarning(query, "GEO_MULTIPOINT", Result(
            TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH,
            "not an array containing positions"));
      return AqlValue(arangodb::basics::VelocyPackHelper::NullValue());
    }
  }

  b.close();
  b.close();

  return AqlValue(b);
}

/// @brief function GEO_POLYGON
AqlValue Functions::GeoPolygon(arangodb::aql::Query* query,
                               transaction::Methods* trx,
                               VPackFunctionParameters const& parameters) {
  ValidateParameters(parameters, "GEO_POLYGON", 1, 1);

  size_t const n = parameters.size();

  if (n < 1) {
    // no parameters
    return AqlValue(AqlValueHintNull());
  }

  AqlValue geoArray = ExtractFunctionParameterValue(parameters, 0);

  if (!geoArray.isArray()) {
    ::registerWarning(query, "GEO_POLYGON",
                      TRI_ERROR_QUERY_ARRAY_EXPECTED);
    return AqlValue(arangodb::basics::VelocyPackHelper::NullValue());
  }

  VPackBuilder b;
  b.openObject();
  b.add("type", VPackValue("Polygon"));
  b.add("coordinates", VPackValue(VPackValueType::Array));

  AqlValueMaterializer materializer(trx);
  VPackSlice s = materializer.slice(geoArray, false);

  // check if nested or not
  bool unnested = false;
  for (auto const& v : VPackArrayIterator(s)) {
    if (v.isArray() && v.length() == 2) {
      unnested = true;
    }
  }
  if (unnested) {
    b.openArray();
  }

  for (auto const& v : VPackArrayIterator(s)) {
    if (v.isArray() && v.length() > 2) {
      b.openArray();
      for (auto const& coord : VPackArrayIterator(v)) {
        if (coord.isNumber()) {
          b.add(VPackValue(coord.getNumber<double>()));
        } else if (coord.isArray()) {
          if (coord.length() < 2) {
            ::registerWarning(query, "GEO_POLYGON", Result(
                  TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH,
                  "a Position needs at least two numeric values"));
            return AqlValue(arangodb::basics::VelocyPackHelper::NullValue());
          } else {
            b.openArray();
            for (auto const& innercord : VPackArrayIterator(coord)) {
              if (innercord.isNumber()) {
                b.add(VPackValue(innercord.getNumber<double>()));
              } else if (innercord.isArray()) {
                if (innercord.at(0).isNumber() && innercord.at(1).isNumber()) {
                  b.openArray();
                  b.add(VPackValue(innercord.at(0).getNumber<double>()));
                  b.add(VPackValue(innercord.at(1).getNumber<double>()));
                  b.close();
                } else {
                  ::registerWarning(query, "GEO_POLYGON", Result(
                        TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH,
                        "not a number"));
                  return AqlValue(arangodb::basics::VelocyPackHelper::NullValue());
                }
              } else {
                ::registerWarning(query, "GEO_POLYGON", Result(
                      TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH,
                      "not an array describing a position"));
                return AqlValue(arangodb::basics::VelocyPackHelper::NullValue());
              }
            }
            b.close();
          }
        } else {
          ::registerWarning(query, "GEO_POLYGON", Result(
                TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH,
                "not an array containing positions"));
          return AqlValue(arangodb::basics::VelocyPackHelper::NullValue());
        }
      }
      b.close();
    } else if (v.isArray() && v.length() == 2) {
        if (s.length() > 2) {
        b.openArray();
        for (auto const& innercord : VPackArrayIterator(v)) {
          if (innercord.isNumber()) {
            b.add(VPackValue(innercord.getNumber<double>()));
          } else if (innercord.isArray()) {
            if (innercord.at(0).isNumber() && innercord.at(1).isNumber()) {
              b.openArray();
              b.add(VPackValue(innercord.at(0).getNumber<double>()));
              b.add(VPackValue(innercord.at(1).getNumber<double>()));
              b.close();
            } else {
              ::registerWarning(query, "GEO_POLYGON", Result(
                    TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH,
                    "not a number"));
              return AqlValue(arangodb::basics::VelocyPackHelper::NullValue());
            }
          } else {
            ::registerWarning(query, "GEO_POLYGON", Result(
                  TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH,
                  "not a numeric value"));
            return AqlValue(arangodb::basics::VelocyPackHelper::NullValue());
          }
        }
        b.close();
      } else {
        ::registerWarning(query, "GEO_POLYGON", Result(
              TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH,
              "a Polygon needs at least three positions"));
        return AqlValue(arangodb::basics::VelocyPackHelper::NullValue());
      }
    } else {
      ::registerWarning(query, "GEO_POLYGON", Result(
            TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH,
            "not an array containing positions"));
      return AqlValue(arangodb::basics::VelocyPackHelper::NullValue());
    }
  }

  b.close();
  b.close();

  if (unnested) {
    b.close();
  }

  return AqlValue(b);
}

/// @brief function GEO_LINESTRING
AqlValue Functions::GeoLinestring(arangodb::aql::Query* query,
                                  transaction::Methods* trx,
                                  VPackFunctionParameters const& parameters) {
  ValidateParameters(parameters, "GEO_LINESTRING", 1, 1);

  size_t const n = parameters.size();

  if (n < 1) {
    // no parameters
    return AqlValue(AqlValueHintNull());
  }

  AqlValue geoArray = ExtractFunctionParameterValue(parameters, 0);

  if (!geoArray.isArray()) {
    ::registerWarning(query, "GEO_LINESTRING",
                    TRI_ERROR_QUERY_ARRAY_EXPECTED);
    return AqlValue(arangodb::basics::VelocyPackHelper::NullValue());
  }
  if (geoArray.length() < 2) {
    ::registerWarning(query, "GEO_LINESTRING", Result(
          TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH,
          "a LineString needs at least two positions"));
    return AqlValue(arangodb::basics::VelocyPackHelper::NullValue());
  }

  VPackBuilder b;

  b.add(VPackValue(VPackValueType::Object));
  b.add("type", VPackValue("LineString"));
  b.add("coordinates", VPackValue(VPackValueType::Array));

  AqlValueMaterializer materializer(trx);
  VPackSlice s = materializer.slice(geoArray, false);
  for (auto const& v : VPackArrayIterator(s)) {
    if (v.isArray()) {
      b.openArray();
      for (auto const& coord : VPackArrayIterator(v)) {
        if (coord.isNumber()) {
          b.add(VPackValue(coord.getNumber<double>()));
        } else {
          ::registerWarning(query, "GEO_LINESTRING", Result(
                TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH,
                "not a numeric value"));
          return AqlValue(arangodb::basics::VelocyPackHelper::NullValue());
        }
      }
      b.close();
    } else {
      ::registerWarning(query, "GEO_LINESTRING", Result(
            TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH,
            "not an array containing positions"));
      return AqlValue(arangodb::basics::VelocyPackHelper::NullValue());
    }
  }

  b.close();
  b.close();

  return AqlValue(b);
}

/// @brief function GEO_MULTILINESTRING
AqlValue Functions::GeoMultiLinestring(arangodb::aql::Query* query,
                                       transaction::Methods* trx,
                                       VPackFunctionParameters const& parameters) {
  ValidateParameters(parameters, "GEO_MULTILINESTRING", 1, 1);

  size_t const n = parameters.size();

  if (n < 1) {
    // no parameters
    return AqlValue(AqlValueHintNull());
  }

  AqlValue geoArray = ExtractFunctionParameterValue(parameters, 0);

  if (!geoArray.isArray()) {
    ::registerWarning(query, "GEO_MULTILINESTRING",
                    TRI_ERROR_QUERY_ARRAY_EXPECTED);
    return AqlValue(arangodb::basics::VelocyPackHelper::NullValue());
  }
  if (geoArray.length() < 1) {
    ::registerWarning(query, "GEO_MULTILINESTRING", Result(
          TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH,
          "a MultiLineString needs at least one array of linestrings"));
    return AqlValue(arangodb::basics::VelocyPackHelper::NullValue());
  }

  VPackBuilder b;

  b.add(VPackValue(VPackValueType::Object));
  b.add("type", VPackValue("MultiLineString"));
  b.add("coordinates", VPackValue(VPackValueType::Array));

  AqlValueMaterializer materializer(trx);
  VPackSlice s = materializer.slice(geoArray, false);
  for (auto const& v : VPackArrayIterator(s)) {
    if (v.isArray()) {
      if (v.length() > 1) {
        b.openArray();
        for (auto const& inner : VPackArrayIterator(v)) {
          if (inner.isArray()) {
            b.openArray();
            for (auto const& coord : VPackArrayIterator(inner)) {
              if (coord.isNumber()) {
                b.add(VPackValue(coord.getNumber<double>()));
              } else {
                ::registerWarning(query, "GEO_MULTILINESTRING", Result(
                      TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH,
                      "not a numeric value"));
                return AqlValue(arangodb::basics::VelocyPackHelper::NullValue());
              }
            }
            b.close();
          } else {
            ::registerWarning(query, "GEO_MULTILINESTRING", Result(
                  TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH,
                  "not an array containing positions"));
            return AqlValue(arangodb::basics::VelocyPackHelper::NullValue());
          }
        }
        b.close();
      } else {
        ::registerWarning(query, "GEO_MULTILINESTRING", Result(
              TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH,
              "not an array containing linestrings"));
        return AqlValue(arangodb::basics::VelocyPackHelper::NullValue());
      }
    } else {
      ::registerWarning(query, "GEO_MULTILINESTRING", Result(
            TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH,
            "not an array containing positions"));
      return AqlValue(arangodb::basics::VelocyPackHelper::NullValue());
    }
  }

  b.close();
  b.close();

  return AqlValue(b);
}

/// @brief function FLATTEN
AqlValue Functions::Flatten(arangodb::aql::Query* query,
                            transaction::Methods* trx,
                            VPackFunctionParameters const& parameters) {
  static char const* AFN = "FLATTEN";
  ValidateParameters(parameters, AFN, 1, 2);

  AqlValue list = ExtractFunctionParameterValue(parameters, 0);
  if (!list.isArray()) {
    ::registerWarning(query, AFN, TRI_ERROR_QUERY_ARRAY_EXPECTED);
    return AqlValue(AqlValueHintNull());
  }

  size_t maxDepth = 1;
  if (parameters.size() == 2) {
    AqlValue maxDepthValue = ExtractFunctionParameterValue(parameters, 1);
    bool failed;
    double tmpMaxDepth = maxDepthValue.toDouble(trx, failed);
    if (failed || tmpMaxDepth < 1) {
      maxDepth = 1;
    } else {
      maxDepth = static_cast<size_t>(tmpMaxDepth);
    }
  }

  AqlValueMaterializer materializer(trx);
  VPackSlice listSlice = materializer.slice(list, false);

  transaction::BuilderLeaser builder(trx);
  builder->openArray();
  ::flattenList(listSlice, maxDepth, 0, *builder.get());
  builder->close();
  return AqlValue(builder.get());
}

/// @brief function ZIP
AqlValue Functions::Zip(arangodb::aql::Query* query, transaction::Methods* trx,
                        VPackFunctionParameters const& parameters) {
  static char const* AFN = "ZIP";
  ValidateParameters(parameters, AFN, 2, 2);

  AqlValue keys = ExtractFunctionParameterValue(parameters, 0);
  AqlValue values = ExtractFunctionParameterValue(parameters, 1);

  if (!keys.isArray() || !values.isArray() ||
      keys.length() != values.length()) {
    ::registerWarning(query, AFN,
                      TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH);
    return AqlValue(AqlValueHintNull());
  }

  AqlValueMaterializer keyMaterializer(trx);
  VPackSlice keysSlice = keyMaterializer.slice(keys, false);

  AqlValueMaterializer valueMaterializer(trx);
  VPackSlice valuesSlice = valueMaterializer.slice(values, false);

  transaction::BuilderLeaser builder(trx);
  builder->openObject();

  // Buffer will temporarily hold the keys
  std::unordered_set<std::string> keysSeen;
  transaction::StringBufferLeaser buffer(trx);
  arangodb::basics::VPackStringBufferAdapter adapter(buffer->stringBuffer());
 
  VPackArrayIterator keysIt(keysSlice);
  VPackArrayIterator valuesIt(valuesSlice);

  TRI_ASSERT(keysIt.size() == valuesIt.size());

  while (keysIt.valid()) {
    TRI_ASSERT(valuesIt.valid());

    // stringify key
    buffer->reset();
    Stringify(trx, adapter, keysIt.value());

    if (keysSeen.emplace(buffer->c_str(), buffer->length()).second) {
      // non-duplicate key
      builder->add(buffer->c_str(), buffer->length(), valuesIt.value());
    }

    keysIt.next();
    valuesIt.next();
  }

  builder->close();

  return AqlValue(builder.get());
}

/// @brief function JSON_STRINGIFY
AqlValue Functions::JsonStringify(arangodb::aql::Query*,
                                  transaction::Methods* trx,
                                  VPackFunctionParameters const& parameters) {
  ValidateParameters(parameters, "JSON_STRINGIFY", 1, 1);

  AqlValue value = ExtractFunctionParameterValue(parameters, 0);
  AqlValueMaterializer materializer(trx);
  VPackSlice slice = materializer.slice(value, false);

  transaction::StringBufferLeaser buffer(trx);
  arangodb::basics::VPackStringBufferAdapter adapter(buffer->stringBuffer());

  VPackDumper dumper(&adapter, trx->transactionContextPtr()->getVPackOptions());
  dumper.dump(slice);

  return AqlValue(buffer->begin(), buffer->length());
}

/// @brief function JSON_PARSE
AqlValue Functions::JsonParse(arangodb::aql::Query* query,
                              transaction::Methods* trx,
                              VPackFunctionParameters const& parameters) {
  static char const* AFN = "JSON_PARSE";
  ValidateParameters(parameters, AFN, 1, 1);

  AqlValue value = ExtractFunctionParameterValue(parameters, 0);
  AqlValueMaterializer materializer(trx);
  VPackSlice slice = materializer.slice(value, false);

  if (!slice.isString()) {
    ::registerWarning(query, AFN, TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH);
    return AqlValue(AqlValueHintNull());
  }

  VPackValueLength l;
  char const* p = slice.getString(l);

  try {
    std::shared_ptr<VPackBuilder> builder = VPackParser::fromJson(p, l);
    return AqlValue(*builder);
  } catch (...) {
    ::registerWarning(query, AFN, TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH);
    return AqlValue(AqlValueHintNull());
  }
}

/// @brief function PARSE_IDENTIFIER
AqlValue Functions::ParseIdentifier(arangodb::aql::Query* query,
                                    transaction::Methods* trx,
                                    VPackFunctionParameters const& parameters) {
  static char const* AFN = "PARSE_IDENTIFIER";
  ValidateParameters(parameters, AFN, 1, 1);

  AqlValue value = ExtractFunctionParameterValue(parameters, 0);
  std::string identifier;
  if (value.isObject() && value.hasKey(trx, StaticStrings::IdString)) {
    bool localMustDestroy;
    AqlValue valueStr =
        value.get(trx, StaticStrings::IdString, localMustDestroy, false);
    AqlValueGuard guard(valueStr, localMustDestroy);

    if (valueStr.isString()) {
      identifier = valueStr.slice().copyString();
    }
  } else if (value.isString()) {
    identifier = value.slice().copyString();
  }

  if (identifier.empty()) {
    ::registerWarning(query, AFN, TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH);
    return AqlValue(AqlValueHintNull());
  }

  std::vector<std::string> parts =
      arangodb::basics::StringUtils::split(identifier, "/");

  if (parts.size() != 2) {
    ::registerWarning(query, AFN, TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH);
    return AqlValue(AqlValueHintNull());
  }

  transaction::BuilderLeaser builder(trx);
  builder->openObject();
  builder->add("collection", VPackValue(parts[0]));
  builder->add("key", VPackValue(parts[1]));
  builder->close();
  return AqlValue(builder.get());
}

/// @brief function Slice
AqlValue Functions::Slice(arangodb::aql::Query* query,
                          transaction::Methods* trx,
                          VPackFunctionParameters const& parameters) {
  static char const* AFN = "SLICE";
  ValidateParameters(parameters, AFN, 2, 3);

  AqlValue baseArray = ExtractFunctionParameterValue(parameters, 0);

  if (!baseArray.isArray()) {
    ::registerWarning(query, AFN, TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH);
    return AqlValue(AqlValueHintNull());
  }

  // determine lower bound
  AqlValue fromValue = ExtractFunctionParameterValue(parameters, 1);
  int64_t from = fromValue.toInt64(trx);
  if (from < 0) {
    from = baseArray.length() + from;
    if (from < 0) {
      from = 0;
    }
  }

  // determine upper bound
  AqlValue toValue = ExtractFunctionParameterValue(parameters, 2);
  int64_t to;
  if (toValue.isNull(true)) {
    to = baseArray.length();
  } else {
    to = toValue.toInt64(trx);
    if (to >= 0) {
      to += from;
    } else {
      // negative to value
      to = baseArray.length() + to;
      if (to < 0) {
        to = 0;
      }
    }
  }

  AqlValueMaterializer materializer(trx);
  VPackSlice arraySlice = materializer.slice(baseArray, false);

  transaction::BuilderLeaser builder(trx);
  builder->openArray();

  int64_t pos = 0;
  VPackArrayIterator it(arraySlice);
  while (it.valid()) {
    if (pos >= from && pos < to) {
      builder->add(it.value());
    }
    ++pos;
    if (pos >= to) {
      // done
      break;
    }
    it.next();
  }

  builder->close();
  return AqlValue(builder.get());
}

/// @brief function Minus
AqlValue Functions::Minus(arangodb::aql::Query* query,
                          transaction::Methods* trx,
                          VPackFunctionParameters const& parameters) {
  static char const* AFN = "MINUS";
  ValidateParameters(parameters, AFN, 2);

  AqlValue baseArray = ExtractFunctionParameterValue(parameters, 0);

  if (!baseArray.isArray()) {
    ::registerWarning(query, AFN, TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH);
    return AqlValue(AqlValueHintNull());
  }

  auto options = trx->transactionContextPtr()->getVPackOptions();
  std::unordered_map<VPackSlice, size_t,
                     arangodb::basics::VelocyPackHelper::VPackHash,
                     arangodb::basics::VelocyPackHelper::VPackEqual>
      contains(512, arangodb::basics::VelocyPackHelper::VPackHash(),
               arangodb::basics::VelocyPackHelper::VPackEqual(options));

  // Fill the original map
  AqlValueMaterializer materializer(trx);
  VPackSlice arraySlice = materializer.slice(baseArray, false);

  VPackArrayIterator it(arraySlice);
  while (it.valid()) {
    contains.emplace(it.value(), it.index());
    it.next();
  }

  // Iterate through all following parameters and delete found elements from the
  // map
  for (size_t k = 1; k < parameters.size(); ++k) {
    AqlValue next = ExtractFunctionParameterValue(parameters, k);
    if (!next.isArray()) {
      ::registerWarning(query, AFN, TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH);
      return AqlValue(AqlValueHintNull());
    }

    AqlValueMaterializer materializer(trx);
    VPackSlice arraySlice = materializer.slice(next, false);

    for (auto const& search : VPackArrayIterator(arraySlice)) {
      auto find = contains.find(search);

      if (find != contains.end()) {
        contains.erase(find);
      }
    }
  }

  // We omit the normalize part from js, cannot occur here
  transaction::BuilderLeaser builder(trx);
  builder->openArray();
  for (auto const& it : contains) {
    builder->add(it.first);
  }
  builder->close();
  return AqlValue(builder.get());
}

/// @brief function Document
AqlValue Functions::Document(arangodb::aql::Query* query,
                             transaction::Methods* trx,
                             VPackFunctionParameters const& parameters) {
  static char const* AFN = "DOCUMENT";
  ValidateParameters(parameters, AFN, 1, 2);

  if (parameters.size() == 1) {
    AqlValue id = ExtractFunctionParameterValue(parameters, 0);
    transaction::BuilderLeaser builder(trx);
    if (id.isString()) {
      std::string identifier(id.slice().copyString());
      std::string colName;
      ::getDocumentByIdentifier(trx, colName, identifier, true, *builder.get());
      if (builder->isEmpty()) {
        // not found
        return AqlValue(AqlValueHintNull());
      }
      return AqlValue(builder.get());
    }
    if (id.isArray()) {
      AqlValueMaterializer materializer(trx);
      VPackSlice idSlice = materializer.slice(id, false);
      builder->openArray();
      for (auto const& next : VPackArrayIterator(idSlice)) {
        if (next.isString()) {
          std::string identifier = next.copyString();
          std::string colName;
          ::getDocumentByIdentifier(trx, colName, identifier, true,
                                    *builder.get());
        }
      }
      builder->close();
      return AqlValue(builder.get());
    }
    return AqlValue(AqlValueHintNull());
  }

  AqlValue collectionValue = ExtractFunctionParameterValue(parameters, 0);
  if (!collectionValue.isString()) {
    ::registerWarning(query, AFN, TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH);
    return AqlValue(AqlValueHintNull());
  }
  std::string collectionName(collectionValue.slice().copyString());

  AqlValue id = ExtractFunctionParameterValue(parameters, 1);
  if (id.isString()) {
    transaction::BuilderLeaser builder(trx);
    std::string identifier(id.slice().copyString());
    ::getDocumentByIdentifier(trx, collectionName, identifier, true,
                              *builder.get());
    if (builder->isEmpty()) {
      return AqlValue(AqlValueHintNull());
    }
    return AqlValue(builder.get());
  }

  if (id.isArray()) {
    transaction::BuilderLeaser builder(trx);
    builder->openArray();

    AqlValueMaterializer materializer(trx);
    VPackSlice idSlice = materializer.slice(id, false);
    for (auto const& next : VPackArrayIterator(idSlice)) {
      if (next.isString()) {
        std::string identifier(next.copyString());
        ::getDocumentByIdentifier(trx, collectionName, identifier, true,
                                  *builder.get());
      }
    }

    builder->close();
    return AqlValue(builder.get());
  }

  // Id has invalid format
  return AqlValue(AqlValueHintNull());
}

/// @brief function MATCHES
AqlValue Functions::Matches(arangodb::aql::Query* query,
                            transaction::Methods* trx,
                            VPackFunctionParameters const& parameters) {
  static char const* AFN = "MATCHES";
  ValidateParameters(parameters, AFN, 2, 3);

  AqlValue docToFind = ExtractFunctionParameterValue(parameters, 0);

  if (!docToFind.isObject()) {
    return AqlValue(AqlValueHintBool(false));
  }

  AqlValue exampleDocs = ExtractFunctionParameterValue(parameters, 1);

  bool retIdx = false;
  if (parameters.size() == 3) {
    retIdx = ExtractFunctionParameterValue(parameters, 2).toBoolean();
  }

  AqlValueMaterializer materializer(trx);
  VPackSlice docSlice = materializer.slice(docToFind, false);

  transaction::BuilderLeaser builder(trx);
  VPackSlice examples = materializer.slice(exampleDocs, false);

  if (!examples.isArray()) {
    builder->openArray();
    builder->add(examples);
    builder->close();
    examples = builder->slice();
  }

  auto options = trx->transactionContextPtr()->getVPackOptions();

  bool foundMatch;
  int32_t idx = -1;

  for (auto const& example : VPackArrayIterator(examples)) {
    idx++;

    if (!example.isObject()) {
      ::registerWarning(query, AFN, TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH);
      continue;
    }

    foundMatch = true;

    for (auto const& it : VPackObjectIterator(example, true)) {
      std::string key = it.key.copyString();

      if (it.value.isNull() && !docSlice.hasKey(key)) {
        continue;
      }

      if (!docSlice.hasKey(key) ||
          // compare inner content
          basics::VelocyPackHelper::compare(docSlice.get(key), it.value, false,
                                            options, &docSlice,
                                            &example) != 0) {
        foundMatch = false;
        break;
      }
    }

    if (foundMatch) {
      if (retIdx) {
        return AqlValue(AqlValueHintInt(idx));
      } else {
        return AqlValue(AqlValueHintBool(true));
      }
    }
  }

  if (retIdx) {
    return AqlValue(AqlValueHintInt(-1));
  }

  return AqlValue(AqlValueHintBool(false));
}

/// @brief function ROUND
AqlValue Functions::Round(arangodb::aql::Query*,
                          transaction::Methods* trx,
                          VPackFunctionParameters const& parameters) {
  ValidateParameters(parameters, "ROUND", 1, 1);

  AqlValue value = ExtractFunctionParameterValue(parameters, 0);

  double input = value.toDouble(trx);

  // Rounds down for < x.4999 and up for > x.50000
  return ::numberValue(trx, std::floor(input + 0.5), true);
}

/// @brief function ABS
AqlValue Functions::Abs(arangodb::aql::Query*, transaction::Methods* trx,
                        VPackFunctionParameters const& parameters) {
  ValidateParameters(parameters, "ABS", 1, 1);

  AqlValue value = ExtractFunctionParameterValue(parameters, 0);

  double input = value.toDouble(trx);
  return ::numberValue(trx, std::abs(input), true);
}

/// @brief function CEIL
AqlValue Functions::Ceil(arangodb::aql::Query*, transaction::Methods* trx,
                         VPackFunctionParameters const& parameters) {
  ValidateParameters(parameters, "CEIL", 1, 1);

  AqlValue value = ExtractFunctionParameterValue(parameters, 0);

  double input = value.toDouble(trx);
  return ::numberValue(trx, std::ceil(input), true);
}

/// @brief function FLOOR
AqlValue Functions::Floor(arangodb::aql::Query*,
                          transaction::Methods* trx,
                          VPackFunctionParameters const& parameters) {
  ValidateParameters(parameters, "FLOOR", 1, 1);

  AqlValue value = ExtractFunctionParameterValue(parameters, 0);

  double input = value.toDouble(trx);
  return ::numberValue(trx, std::floor(input), true);
}

/// @brief function SQRT
AqlValue Functions::Sqrt(arangodb::aql::Query*, transaction::Methods* trx,
                         VPackFunctionParameters const& parameters) {
  ValidateParameters(parameters, "SQRT", 1, 1);

  AqlValue value = ExtractFunctionParameterValue(parameters, 0);

  double input = value.toDouble(trx);
  return ::numberValue(trx, std::sqrt(input), true);
}

/// @brief function POW
AqlValue Functions::Pow(arangodb::aql::Query*, transaction::Methods* trx,
                        VPackFunctionParameters const& parameters) {
  ValidateParameters(parameters, "POW", 2, 2);

  AqlValue baseValue = ExtractFunctionParameterValue(parameters, 0);
  AqlValue expValue = ExtractFunctionParameterValue(parameters, 1);

  double base = baseValue.toDouble(trx);
  double exp = expValue.toDouble(trx);

  return ::numberValue(trx, std::pow(base, exp), true);
}

/// @brief function LOG
AqlValue Functions::Log(arangodb::aql::Query*, transaction::Methods* trx,
                        VPackFunctionParameters const& parameters) {
  ValidateParameters(parameters, "LOG", 1, 1);

  AqlValue value = ExtractFunctionParameterValue(parameters, 0);

  double input = value.toDouble(trx);
  return ::numberValue(trx, std::log(input), true);
}

/// @brief function LOG2
AqlValue Functions::Log2(arangodb::aql::Query*, transaction::Methods* trx,
                         VPackFunctionParameters const& parameters) {
  ValidateParameters(parameters, "LOG2", 1, 1);

  AqlValue value = ExtractFunctionParameterValue(parameters, 0);

  double input = value.toDouble(trx);
  return ::numberValue(trx, std::log2(input), true);
}

/// @brief function LOG10
AqlValue Functions::Log10(arangodb::aql::Query*,
                          transaction::Methods* trx,
                          VPackFunctionParameters const& parameters) {
  ValidateParameters(parameters, "LOG10", 1, 1);

  AqlValue value = ExtractFunctionParameterValue(parameters, 0);

  double input = value.toDouble(trx);
  return ::numberValue(trx, std::log10(input), true);
}

/// @brief function EXP
AqlValue Functions::Exp(arangodb::aql::Query*, transaction::Methods* trx,
                        VPackFunctionParameters const& parameters) {
  ValidateParameters(parameters, "EXP", 1, 1);

  AqlValue value = ExtractFunctionParameterValue(parameters, 0);

  double input = value.toDouble(trx);
  return ::numberValue(trx, std::exp(input), true);
}

/// @brief function EXP2
AqlValue Functions::Exp2(arangodb::aql::Query*, transaction::Methods* trx,
                         VPackFunctionParameters const& parameters) {
  ValidateParameters(parameters, "EXP2", 1, 1);

  AqlValue value = ExtractFunctionParameterValue(parameters, 0);

  double input = value.toDouble(trx);
  return ::numberValue(trx, std::exp2(input), true);
}

/// @brief function SIN
AqlValue Functions::Sin(arangodb::aql::Query*, transaction::Methods* trx,
                        VPackFunctionParameters const& parameters) {
  ValidateParameters(parameters, "SIN", 1, 1);

  AqlValue value = ExtractFunctionParameterValue(parameters, 0);

  double input = value.toDouble(trx);
  return ::numberValue(trx, std::sin(input), true);
}

/// @brief function COS
AqlValue Functions::Cos(arangodb::aql::Query*, transaction::Methods* trx,
                        VPackFunctionParameters const& parameters) {
  ValidateParameters(parameters, "COS", 1, 1);

  AqlValue value = ExtractFunctionParameterValue(parameters, 0);

  double input = value.toDouble(trx);
  return ::numberValue(trx, std::cos(input), true);
}

/// @brief function TAN
AqlValue Functions::Tan(arangodb::aql::Query*, transaction::Methods* trx,
                        VPackFunctionParameters const& parameters) {
  ValidateParameters(parameters, "TAN", 1, 1);

  AqlValue value = ExtractFunctionParameterValue(parameters, 0);

  double input = value.toDouble(trx);
  return ::numberValue(trx, std::tan(input), true);
}

/// @brief function ASIN
AqlValue Functions::Asin(arangodb::aql::Query*, transaction::Methods* trx,
                         VPackFunctionParameters const& parameters) {
  ValidateParameters(parameters, "ASIN", 1, 1);

  AqlValue value = ExtractFunctionParameterValue(parameters, 0);

  double input = value.toDouble(trx);
  return ::numberValue(trx, std::asin(input), true);
}

/// @brief function ACOS
AqlValue Functions::Acos(arangodb::aql::Query*, transaction::Methods* trx,
                         VPackFunctionParameters const& parameters) {
  ValidateParameters(parameters, "ACOS", 1, 1);

  AqlValue value = ExtractFunctionParameterValue(parameters, 0);

  double input = value.toDouble(trx);
  return ::numberValue(trx, std::acos(input), true);
}

/// @brief function ATAN
AqlValue Functions::Atan(arangodb::aql::Query*, transaction::Methods* trx,
                         VPackFunctionParameters const& parameters) {
  ValidateParameters(parameters, "ATAN", 1, 1);

  AqlValue value = ExtractFunctionParameterValue(parameters, 0);

  double input = value.toDouble(trx);
  return ::numberValue(trx, std::atan(input), true);
}

/// @brief function ATAN2
AqlValue Functions::Atan2(arangodb::aql::Query*,
                          transaction::Methods* trx,
                          VPackFunctionParameters const& parameters) {
  ValidateParameters(parameters, "ATAN2", 2, 2);

  AqlValue value1 = ExtractFunctionParameterValue(parameters, 0);
  AqlValue value2 = ExtractFunctionParameterValue(parameters, 1);

  double input1 = value1.toDouble(trx);
  double input2 = value2.toDouble(trx);
  return ::numberValue(trx, std::atan2(input1, input2), true);
}

/// @brief function RADIANS
AqlValue Functions::Radians(arangodb::aql::Query*,
                            transaction::Methods* trx,
                            VPackFunctionParameters const& parameters) {
  ValidateParameters(parameters, "RADIANS", 1, 1);

  AqlValue value = ExtractFunctionParameterValue(parameters, 0);

  double degrees = value.toDouble(trx);
  // acos(-1) == PI
  return ::numberValue(trx, degrees * (std::acos(-1.0) / 180.0), true);
}

/// @brief function DEGREES
AqlValue Functions::Degrees(arangodb::aql::Query*,
                            transaction::Methods* trx,
                            VPackFunctionParameters const& parameters) {
  ValidateParameters(parameters, "DEGREES", 1, 1);

  AqlValue value = ExtractFunctionParameterValue(parameters, 0);

  double radians = value.toDouble(trx);
  // acos(-1) == PI
  return ::numberValue(trx, radians * (180.0 / std::acos(-1.0)), true);
}

/// @brief function PI
AqlValue Functions::Pi(arangodb::aql::Query*, transaction::Methods* trx,
                       VPackFunctionParameters const& parameters) {
  ValidateParameters(parameters, "PI", 0, 0);

  // acos(-1) == PI
  return ::numberValue(trx, std::acos(-1.0), true);
}

/// @brief function RAND
AqlValue Functions::Rand(arangodb::aql::Query*, transaction::Methods* trx,
                         VPackFunctionParameters const& parameters) {
  ValidateParameters(parameters, "RAND", 0, 0);

  // This random functionality is not too good yet...
  return ::numberValue(trx, static_cast<double>(std::rand()) / RAND_MAX, true);
}

/// @brief function FIRST_DOCUMENT
AqlValue Functions::FirstDocument(arangodb::aql::Query*,
                                  transaction::Methods* trx,
                                  VPackFunctionParameters const& parameters) {
  size_t const n = parameters.size();
  for (size_t i = 0; i < n; ++i) {
    AqlValue a = ExtractFunctionParameterValue(parameters, i);
    if (a.isObject()) {
      return a.clone();
    }
  }

  return AqlValue(AqlValueHintNull());
}

/// @brief function FIRST_LIST
AqlValue Functions::FirstList(arangodb::aql::Query*,
                              transaction::Methods* trx,
                              VPackFunctionParameters const& parameters) {
  size_t const n = parameters.size();
  for (size_t i = 0; i < n; ++i) {
    AqlValue a = ExtractFunctionParameterValue(parameters, i);
    if (a.isArray()) {
      return a.clone();
    }
  }

  return AqlValue(AqlValueHintNull());
}

/// @brief function PUSH
AqlValue Functions::Push(arangodb::aql::Query* query, transaction::Methods* trx,
                         VPackFunctionParameters const& parameters) {
  static char const* AFN = "PUSH";
  ValidateParameters(parameters, AFN, 2, 3);

  AqlValue list = ExtractFunctionParameterValue(parameters, 0);
  AqlValue toPush = ExtractFunctionParameterValue(parameters, 1);

  AqlValueMaterializer toPushMaterializer(trx);
  VPackSlice p = toPushMaterializer.slice(toPush, false);

  if (list.isNull(true)) {
    transaction::BuilderLeaser builder(trx);
    builder->openArray();
    builder->add(p);
    builder->close();
    return AqlValue(builder.get());
  }

  if (!list.isArray()) {
    ::registerInvalidArgumentWarning(query, AFN);
    return AqlValue(AqlValueHintNull());
  }

  transaction::BuilderLeaser builder(trx);
  builder->openArray();
  AqlValueMaterializer materializer(trx);
  VPackSlice l = materializer.slice(list, false);

  for (auto const& it : VPackArrayIterator(l)) {
    builder->add(it);
  }
  if (parameters.size() == 3) {
    auto options = trx->transactionContextPtr()->getVPackOptions();
    AqlValue unique = ExtractFunctionParameterValue(parameters, 2);
    if (!unique.toBoolean() || !::listContainsElement(options, l, p)) {
      builder->add(p);
    }
  } else {
    builder->add(p);
  }
  builder->close();
  return AqlValue(builder.get());
}

/// @brief function POP
AqlValue Functions::Pop(arangodb::aql::Query* query, transaction::Methods* trx,
                        VPackFunctionParameters const& parameters) {
  static char const* AFN = "POP";
  ValidateParameters(parameters, AFN, 1, 1);
  AqlValue list = ExtractFunctionParameterValue(parameters, 0);

  if (list.isNull(true)) {
    return AqlValue(AqlValueHintNull());
  }

  if (!list.isArray()) {
    ::registerWarning(query, AFN, TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH);
    return AqlValue(AqlValueHintNull());
  }

  AqlValueMaterializer materializer(trx);
  VPackSlice slice = materializer.slice(list, false);

  transaction::BuilderLeaser builder(trx);
  builder->openArray();
  auto iterator = VPackArrayIterator(slice);
  while (iterator.valid() && !iterator.isLast()) {
    builder->add(iterator.value());
    iterator.next();
  }
  builder->close();
  return AqlValue(builder.get());
}

/// @brief function APPEND
AqlValue Functions::Append(arangodb::aql::Query* query,
                           transaction::Methods* trx,
                           VPackFunctionParameters const& parameters) {
  static char const* AFN = "APPEND";
  ValidateParameters(parameters, AFN, 2, 3);
  AqlValue list = ExtractFunctionParameterValue(parameters, 0);
  AqlValue toAppend = ExtractFunctionParameterValue(parameters, 1);

  if (toAppend.isNull(true)) {
    return list.clone();
  }

  AqlValueMaterializer toAppendMaterializer(trx);
  VPackSlice t = toAppendMaterializer.slice(toAppend, false);

  if (t.isArray() && t.length() == 0) {
    return list.clone();
  }

  bool unique = false;
  if (parameters.size() == 3) {
    AqlValue a = ExtractFunctionParameterValue(parameters, 2);
    unique = a.toBoolean();
  }

  AqlValueMaterializer materializer(trx);
  VPackSlice l = materializer.slice(list, false);

  if (l.isNull()) {
    return toAppend.clone();
  }

  if (!l.isArray()) {
    ::registerInvalidArgumentWarning(query, AFN);
    return AqlValue(AqlValueHintNull());
  }

  std::unordered_set<VPackSlice> added;

  transaction::BuilderLeaser builder(trx);
  builder->openArray();

  for (auto const& it : VPackArrayIterator(l)) {
    if (unique) {
      if (added.find(it) == added.end()) {
        builder->add(it);
        added.emplace(it);
      }
    } else {
      builder->add(it);
    }
  }

  AqlValueMaterializer materializer2(trx);
  VPackSlice slice = materializer2.slice(toAppend, false);

  if (!slice.isArray()) {
    if (!unique || added.find(slice) == added.end()) {
      builder->add(slice);
    }
  } else {
    for (auto const& it : VPackArrayIterator(slice)) {
      if (unique) {
        if (added.find(it) == added.end()) {
          builder->add(it);
          added.emplace(it);
        }
      } else {
        builder->add(it);
      }
    }
  }
  builder->close();
  return AqlValue(builder.get());
}

/// @brief function UNSHIFT
AqlValue Functions::Unshift(arangodb::aql::Query* query,
                            transaction::Methods* trx,
                            VPackFunctionParameters const& parameters) {
  static char const* AFN = "UNSHIFT";
  ValidateParameters(parameters, AFN, 2, 3);
  AqlValue list = ExtractFunctionParameterValue(parameters, 0);

  if (!list.isNull(true) && !list.isArray()) {
    ::registerInvalidArgumentWarning(query, AFN);
    return AqlValue(AqlValueHintNull());
  }

  AqlValue toAppend = ExtractFunctionParameterValue(parameters, 1);
  bool unique = false;
  if (parameters.size() == 3) {
    AqlValue a = ExtractFunctionParameterValue(parameters, 2);
    unique = a.toBoolean();
  }

  auto options = trx->transactionContextPtr()->getVPackOptions();
  size_t unused;
  if (unique && list.isArray() &&
      ::listContainsElement(trx, options, list, toAppend, unused)) {
    // Short circuit, nothing to do return list
    return list.clone();
  }

  AqlValueMaterializer materializer(trx);
  VPackSlice a = materializer.slice(toAppend, false);

  transaction::BuilderLeaser builder(trx);
  builder->openArray();
  builder->add(a);

  if (list.isArray()) {
    AqlValueMaterializer materializer(trx);
    VPackSlice v = materializer.slice(list, false);
    for (auto const& it : VPackArrayIterator(v)) {
      builder->add(it);
    }
  }
  builder->close();
  return AqlValue(builder.get());
}

/// @brief function SHIFT
AqlValue Functions::Shift(arangodb::aql::Query* query,
                          transaction::Methods* trx,
                          VPackFunctionParameters const& parameters) {
  static char const* AFN = "SHIFT";
  ValidateParameters(parameters, AFN, 1, 1);

  AqlValue list = ExtractFunctionParameterValue(parameters, 0);
  if (list.isNull(true)) {
    return AqlValue(AqlValueHintNull());
  }

  if (!list.isArray()) {
    ::registerInvalidArgumentWarning(query, AFN);
    return AqlValue(AqlValueHintNull());
  }

  transaction::BuilderLeaser builder(trx);
  builder->openArray();

  if (list.length() > 0) {
    AqlValueMaterializer materializer(trx);
    VPackSlice l = materializer.slice(list, false);

    auto iterator = VPackArrayIterator(l);
    // This jumps over the first element
    iterator.next();
    while (iterator.valid()) {
      builder->add(iterator.value());
      iterator.next();
    }
  }
  builder->close();

  return AqlValue(builder.get());
}

/// @brief function REMOVE_VALUE
AqlValue Functions::RemoveValue(arangodb::aql::Query* query,
                                transaction::Methods* trx,
                                VPackFunctionParameters const& parameters) {
  static char const* AFN = "REMOVE_VALUE";
  ValidateParameters(parameters, AFN, 2, 3);

  AqlValue list = ExtractFunctionParameterValue(parameters, 0);

  if (list.isNull(true)) {
    return AqlValue(arangodb::basics::VelocyPackHelper::EmptyArrayValue());
  }

  if (!list.isArray()) {
    ::registerInvalidArgumentWarning(query, AFN);
    return AqlValue(AqlValueHintNull());
  }

  auto options = trx->transactionContextPtr()->getVPackOptions();

  transaction::BuilderLeaser builder(trx);
  builder->openArray();
  bool useLimit = false;
  int64_t limit = list.length();

  if (parameters.size() == 3) {
    AqlValue limitValue = ExtractFunctionParameterValue(parameters, 2);
    if (!limitValue.isNull(true)) {
      limit = limitValue.toInt64(trx);
      useLimit = true;
    }
  }

  AqlValue toRemove = ExtractFunctionParameterValue(parameters, 1);
  AqlValueMaterializer toRemoveMaterializer(trx);
  VPackSlice r = toRemoveMaterializer.slice(toRemove, false);

  AqlValueMaterializer materializer(trx);
  VPackSlice v = materializer.slice(list, false);

  for (auto const& it : VPackArrayIterator(v)) {
    if (useLimit && limit == 0) {
      // Just copy
      builder->add(it);
      continue;
    }
    if (arangodb::basics::VelocyPackHelper::compare(r, it, false, options) ==
        0) {
      --limit;
      continue;
    }
    builder->add(it);
  }
  builder->close();
  return AqlValue(builder.get());
}

/// @brief function REMOVE_VALUES
AqlValue Functions::RemoveValues(arangodb::aql::Query* query,
                                 transaction::Methods* trx,
                                 VPackFunctionParameters const& parameters) {
  static char const* AFN = "REMOVE_VALUES";
  ValidateParameters(parameters, AFN, 2, 2);

  AqlValue list = ExtractFunctionParameterValue(parameters, 0);
  AqlValue values = ExtractFunctionParameterValue(parameters, 1);

  if (values.isNull(true)) {
    return list.clone();
  }

  if (list.isNull(true)) {
    return AqlValue(arangodb::basics::VelocyPackHelper::EmptyArrayValue());
  }

  if (!list.isArray() || !values.isArray()) {
    ::registerInvalidArgumentWarning(query, AFN);
    return AqlValue(AqlValueHintNull());
  }

  auto options = trx->transactionContextPtr()->getVPackOptions();
  AqlValueMaterializer valuesMaterializer(trx);
  VPackSlice v = valuesMaterializer.slice(values, false);

  AqlValueMaterializer listMaterializer(trx);
  VPackSlice l = listMaterializer.slice(list, false);

  transaction::BuilderLeaser builder(trx);
  builder->openArray();
  for (auto const& it : VPackArrayIterator(l)) {
    if (!::listContainsElement(options, v, it)) {
      builder->add(it);
    }
  }
  builder->close();
  return AqlValue(builder.get());
}

/// @brief function REMOVE_NTH
AqlValue Functions::RemoveNth(arangodb::aql::Query* query,
                              transaction::Methods* trx,
                              VPackFunctionParameters const& parameters) {
  static char const* AFN = "REMOVE_NTH";
  ValidateParameters(parameters, AFN, 2, 2);

  AqlValue list = ExtractFunctionParameterValue(parameters, 0);

  if (list.isNull(true)) {
    return AqlValue(arangodb::basics::VelocyPackHelper::EmptyArrayValue());
  }

  if (!list.isArray()) {
    ::registerInvalidArgumentWarning(query, AFN);
    return AqlValue(AqlValueHintNull());
  }

  double const count = static_cast<double>(list.length());
  AqlValue position = ExtractFunctionParameterValue(parameters, 1);
  double p = position.toDouble(trx);
  if (p >= count || p < -count) {
    // out of bounds
    return list.clone();
  }

  if (p < 0) {
    p += count;
  }

  AqlValueMaterializer materializer(trx);
  VPackSlice v = materializer.slice(list, false);

  transaction::BuilderLeaser builder(trx);
  size_t target = static_cast<size_t>(p);
  size_t cur = 0;
  builder->openArray();
  for (auto const& it : VPackArrayIterator(v)) {
    if (cur != target) {
      builder->add(it);
    }
    cur++;
  }
  builder->close();
  return AqlValue(builder.get());
}

/// @brief function NOT_NULL
AqlValue Functions::NotNull(arangodb::aql::Query*,
                            transaction::Methods* trx,
                            VPackFunctionParameters const& parameters) {
  size_t const n = parameters.size();
  for (size_t i = 0; i < n; ++i) {
    AqlValue element = ExtractFunctionParameterValue(parameters, i);
    if (!element.isNull(true)) {
      return element.clone();
    }
  }
  return AqlValue(AqlValueHintNull());
}

/// @brief function CURRENT_DATABASE
AqlValue Functions::CurrentDatabase(arangodb::aql::Query* query,
                                    transaction::Methods* trx,
                                    VPackFunctionParameters const& parameters) {
  ValidateParameters(parameters, "CURRENT_DATABASE", 0, 0);

  return AqlValue(query->vocbase().name());
}

/// @brief function CURRENT_USER
AqlValue Functions::CurrentUser(
    arangodb::aql::Query*, transaction::Methods* trx,
    VPackFunctionParameters const& parameters) {

  if (ExecContext::CURRENT == nullptr) {
    return AqlValue(AqlValueHintNull());
  }

  std::string const& username = ExecContext::CURRENT->user();

  if (username.empty()) {
    return AqlValue(AqlValueHintNull());
  }

  return AqlValue(username);
}

/// @brief function COLLECTION_COUNT
AqlValue Functions::CollectionCount(arangodb::aql::Query*,
                                    transaction::Methods* trx,
                                    VPackFunctionParameters const& parameters) {
  static char const* AFN = "COLLECTION_COUNT";
  ValidateParameters(parameters, AFN, 1, 1);

  AqlValue element = ExtractFunctionParameterValue(parameters, 0);
  if (!element.isString()) {
    THROW_ARANGO_EXCEPTION_PARAMS(
        TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH, AFN);
  }

  TRI_ASSERT(ServerState::instance()->isSingleServerOrCoordinator());
  std::string const collectionName = element.slice().copyString();
  OperationResult res = trx->count(collectionName, true);
  if (res.fail()) {
    THROW_ARANGO_EXCEPTION(res.result);
  }

  return AqlValue(res.slice());
}

/// @brief function VARIANCE_SAMPLE
AqlValue Functions::VarianceSample(arangodb::aql::Query* query,
                                   transaction::Methods* trx,
                                   VPackFunctionParameters const& parameters) {
  static char const* AFN = "VARIANCE_SAMPLE";
  ValidateParameters(parameters, AFN, 1, 1);

  AqlValue list = ExtractFunctionParameterValue(parameters, 0);

  if (!list.isArray()) {
    ::registerWarning(query, AFN, TRI_ERROR_QUERY_ARRAY_EXPECTED);
    return AqlValue(AqlValueHintNull());
  }

  double value = 0.0;
  size_t count = 0;

  if (!::variance(trx, list, value, count)) {
    ::registerWarning(query, AFN, TRI_ERROR_QUERY_INVALID_ARITHMETIC_VALUE);
    return AqlValue(AqlValueHintNull());
  }

  if (count < 2) {
    return AqlValue(AqlValueHintNull());
  }

  return ::numberValue(trx, value / (count - 1), true);
}

/// @brief function VARIANCE_POPULATION
AqlValue Functions::VariancePopulation(
    arangodb::aql::Query* query, transaction::Methods* trx,
    VPackFunctionParameters const& parameters) {
  static char const* AFN = "VARIANCE_POPULATION";
  ValidateParameters(parameters, AFN, 1, 1);

  AqlValue list = ExtractFunctionParameterValue(parameters, 0);

  if (!list.isArray()) {
    ::registerWarning(query, AFN, TRI_ERROR_QUERY_ARRAY_EXPECTED);
    return AqlValue(AqlValueHintNull());
  }

  double value = 0.0;
  size_t count = 0;

  if (!::variance(trx, list, value, count)) {
    ::registerWarning(query, AFN, TRI_ERROR_QUERY_INVALID_ARITHMETIC_VALUE);
    return AqlValue(AqlValueHintNull());
  }

  if (count < 1) {
    return AqlValue(AqlValueHintNull());
  }

  return ::numberValue(trx, value / count, true);
}

/// @brief function STDDEV_SAMPLE
AqlValue Functions::StdDevSample(arangodb::aql::Query* query,
                                 transaction::Methods* trx,
                                 VPackFunctionParameters const& parameters) {
  static char const* AFN = "STDDEV_SAMPLE";
  ValidateParameters(parameters, AFN, 1, 1);

  AqlValue list = ExtractFunctionParameterValue(parameters, 0);

  if (!list.isArray()) {
    ::registerWarning(query, AFN, TRI_ERROR_QUERY_ARRAY_EXPECTED);
    return AqlValue(AqlValueHintNull());
  }

  double value = 0.0;
  size_t count = 0;

  if (!::variance(trx, list, value, count)) {
    ::registerWarning(query, AFN, TRI_ERROR_QUERY_INVALID_ARITHMETIC_VALUE);
    return AqlValue(AqlValueHintNull());
  }

  if (count < 2) {
    return AqlValue(AqlValueHintNull());
  }

  return ::numberValue(trx, std::sqrt(value / (count - 1)), true);
}

/// @brief function STDDEV_POPULATION
AqlValue Functions::StdDevPopulation(
    arangodb::aql::Query* query, transaction::Methods* trx,
    VPackFunctionParameters const& parameters) {
  static char const* AFN = "STDDEV_POPULATION";
  ValidateParameters(parameters, AFN, 1, 1);

  AqlValue list = ExtractFunctionParameterValue(parameters, 0);

  if (!list.isArray()) {
    ::registerWarning(query, AFN, TRI_ERROR_QUERY_ARRAY_EXPECTED);
    return AqlValue(AqlValueHintNull());
  }

  double value = 0.0;
  size_t count = 0;

  if (!::variance(trx, list, value, count)) {
    ::registerWarning(query, AFN, TRI_ERROR_QUERY_INVALID_ARITHMETIC_VALUE);
    return AqlValue(AqlValueHintNull());
  }

  if (count < 1) {
    return AqlValue(AqlValueHintNull());
  }

  return ::numberValue(trx, std::sqrt(value / count), true);
}

/// @brief function MEDIAN
AqlValue Functions::Median(arangodb::aql::Query* query,
                           transaction::Methods* trx,
                           VPackFunctionParameters const& parameters) {
  static char const* AFN = "MEDIAN";
  ValidateParameters(parameters, AFN, 1, 1);

  AqlValue list = ExtractFunctionParameterValue(parameters, 0);

  if (!list.isArray()) {
    ::registerWarning(query, AFN, TRI_ERROR_QUERY_ARRAY_EXPECTED);
    return AqlValue(AqlValueHintNull());
  }

  std::vector<double> values;
  if (!::sortNumberList(trx, list, values)) {
    ::registerWarning(query, AFN, TRI_ERROR_QUERY_INVALID_ARITHMETIC_VALUE);
    return AqlValue(AqlValueHintNull());
  }

  if (values.empty()) {
    return AqlValue(AqlValueHintNull());
  }
  size_t const l = values.size();
  size_t midpoint = l / 2;

  if (l % 2 == 0) {
    return ::numberValue(trx, (values[midpoint - 1] + values[midpoint]) / 2,
                       true);
  }
  return ::numberValue(trx, values[midpoint], true);
}

/// @brief function PERCENTILE
AqlValue Functions::Percentile(arangodb::aql::Query* query,
                               transaction::Methods* trx,
                               VPackFunctionParameters const& parameters) {
  static char const* AFN = "PERCENTILE";
  ValidateParameters(parameters, AFN, 2, 3);

  AqlValue list = ExtractFunctionParameterValue(parameters, 0);

  if (!list.isArray()) {
    ::registerWarning(query, AFN, TRI_ERROR_QUERY_ARRAY_EXPECTED);
    return AqlValue(AqlValueHintNull());
  }

  AqlValue border = ExtractFunctionParameterValue(parameters, 1);

  if (!border.isNumber()) {
    ::registerWarning(query, AFN, TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH);
    return AqlValue(AqlValueHintNull());
  }

  bool unused = false;
  double p = border.toDouble(trx, unused);
  if (p <= 0.0 || p > 100.0) {
    ::registerWarning(query, AFN, TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH);
    return AqlValue(AqlValueHintNull());
  }

  bool useInterpolation = false;

  if (parameters.size() == 3) {
    AqlValue methodValue = ExtractFunctionParameterValue(parameters, 2);
    if (!methodValue.isString()) {
      ::registerWarning(query, AFN, TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH);
      return AqlValue(AqlValueHintNull());
    }
    std::string method = methodValue.slice().copyString();
    if (method == "interpolation") {
      useInterpolation = true;
    } else if (method == "rank") {
      useInterpolation = false;
    } else {
      ::registerWarning(query, AFN, TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH);
      return AqlValue(AqlValueHintNull());
    }
  }

  std::vector<double> values;
  if (!::sortNumberList(trx, list, values)) {
    ::registerWarning(query, AFN, TRI_ERROR_QUERY_INVALID_ARITHMETIC_VALUE);
    return AqlValue(AqlValueHintNull());
  }

  if (values.empty()) {
    return AqlValue(AqlValueHintNull());
  }

  size_t l = values.size();
  if (l == 1) {
    return ::numberValue(trx, values[0], true);
  }

  TRI_ASSERT(l > 1);

  if (useInterpolation) {
    double const idx = p * (l + 1) / 100.0;
    double const pos = floor(idx);

    if (pos >= l) {
      return ::numberValue(trx, values[l - 1], true);
    }
    if (pos <= 0) {
      return AqlValue(AqlValueHintNull());
    }

    double const delta = idx - pos;
    return ::numberValue(trx, delta * (values[static_cast<size_t>(pos)] -
                                     values[static_cast<size_t>(pos) - 1]) +
                                values[static_cast<size_t>(pos) - 1],
                       true);
  }

  double const idx = p * l / 100.0;
  double const pos = ceil(idx);
  if (pos >= l) {
    return ::numberValue(trx, values[l - 1], true);
  }
  if (pos <= 0) {
    return AqlValue(AqlValueHintNull());
  }

  return ::numberValue(trx, values[static_cast<size_t>(pos) - 1], true);
}

/// @brief function RANGE
AqlValue Functions::Range(arangodb::aql::Query* query,
                          transaction::Methods* trx,
                          VPackFunctionParameters const& parameters) {
  static char const* AFN = "RANGE";
  ValidateParameters(parameters, AFN, 2, 3);

  AqlValue left = ExtractFunctionParameterValue(parameters, 0);
  AqlValue right = ExtractFunctionParameterValue(parameters, 1);

  double from = left.toDouble(trx);
  double to = right.toDouble(trx);

  if (parameters.size() < 3) {
    return AqlValue(left.toInt64(trx), right.toInt64(trx));
  }

  AqlValue stepValue = ExtractFunctionParameterValue(parameters, 2);
  if (stepValue.isNull(true)) {
    // no step specified. return a real range object
    return AqlValue(left.toInt64(trx), right.toInt64(trx));
  }

  double step = stepValue.toDouble(trx);

  if (step == 0.0 || (from < to && step < 0.0) || (from > to && step > 0.0)) {
    ::registerWarning(query, AFN, TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH);
    return AqlValue(AqlValueHintNull());
  }

  transaction::BuilderLeaser builder(trx);
  builder->openArray();
  if (step < 0.0 && to <= from) {
    for (; from >= to; from += step) {
      builder->add(VPackValue(from));
    }
  } else {
    for (; from <= to; from += step) {
      builder->add(VPackValue(from));
    }
  }
  builder->close();
  return AqlValue(builder.get());
}

/// @brief function POSITION
AqlValue Functions::Position(arangodb::aql::Query* query,
                             transaction::Methods* trx,
                             VPackFunctionParameters const& parameters) {
  static char const* AFN = "POSITION";
  ValidateParameters(parameters, AFN, 2, 3);

  AqlValue list = ExtractFunctionParameterValue(parameters, 0);

  if (!list.isArray()) {
    ::registerWarning(query, AFN, TRI_ERROR_QUERY_ARRAY_EXPECTED);
    return AqlValue(AqlValueHintNull());
  }

  bool returnIndex = false;
  if (parameters.size() == 3) {
    AqlValue a = ExtractFunctionParameterValue(parameters, 2);
    returnIndex = a.toBoolean();
  }

  if (list.length() > 0) {
    AqlValue searchValue = ExtractFunctionParameterValue(parameters, 1);
    auto options = trx->transactionContextPtr()->getVPackOptions();

    size_t index;
    if (::listContainsElement(trx, options, list, searchValue, index)) {
      if (!returnIndex) {
        // return true
        return AqlValue(arangodb::basics::VelocyPackHelper::TrueValue());
      }
      // return position
      transaction::BuilderLeaser builder(trx);
      builder->add(VPackValue(index));
      return AqlValue(builder.get());
    }
  }

  // not found
  if (!returnIndex) {
    // return false
    return AqlValue(arangodb::basics::VelocyPackHelper::FalseValue());
  }

  // return -1
  transaction::BuilderLeaser builder(trx);
  builder->add(VPackValue(-1));
  return AqlValue(builder.get());
}

AqlValue Functions::CallApplyBackend(arangodb::aql::Query* query,
                                     transaction::Methods* trx,
                                     VPackFunctionParameters const& parameters,
                                     char const* AFN,
                                     AqlValue const& invokeFN,
                                     VPackFunctionParameters const& invokeParams) {
  std::string ucInvokeFN;
  transaction::StringBufferLeaser buffer(trx);
  arangodb::basics::VPackStringBufferAdapter adapter(buffer->stringBuffer());

  ::appendAsString(trx, adapter, invokeFN);

  UnicodeString unicodeStr(buffer->c_str(),
                           static_cast<int32_t>(buffer->length()));
  unicodeStr.toUpper(nullptr);
  unicodeStr.toUTF8String(ucInvokeFN);

  arangodb::aql::Function const* func = nullptr;
  if (ucInvokeFN.find("::") == std::string::npos) {
    func = AqlFunctionFeature::getFunctionByName(ucInvokeFN);
    if (func->implementation != nullptr) {
      return func->implementation(query, trx, invokeParams);
    }
  }

  {
    ISOLATE;
    TRI_V8_CURRENT_GLOBALS_AND_SCOPE;
    query->prepareV8Context();

    auto old = v8g->_query;
    v8g->_query = query;
    TRI_DEFER(v8g->_query = old);

    std::string jsName;
    int const n = static_cast<int>(invokeParams.size());
    int const callArgs = (func == nullptr ? 3 : n);
    auto args = std::make_unique<v8::Handle<v8::Value>[]>(callArgs);

    if (func == nullptr) {
      // a call to a user-defined function
      jsName = "FCALL_USER";

      // function name
      args[0] = TRI_V8_STD_STRING(isolate, ucInvokeFN);
      // call parameters
      v8::Handle<v8::Array> params = v8::Array::New(isolate, static_cast<int>(n));

      for (int i = 0; i < n; ++i) {
        params->Set(static_cast<uint32_t>(i), invokeParams[i].toV8(isolate, trx));
      }
      args[1] = params;
      args[2] = TRI_V8_ASCII_STRING(isolate, AFN);
    } else {
      // a call to a built-in V8 function
      jsName = "AQL_" + func->name;
      for (int i = 0; i < n; ++i) {
        args[i] = invokeParams[i].toV8(isolate, trx);
      }
    }

    bool dummy;
    return Expression::invokeV8Function(query, trx, jsName, ucInvokeFN, AFN, false, callArgs, args.get(), dummy);
  }
}

/// @brief function CALL
AqlValue Functions::Call(arangodb::aql::Query* query,
                         transaction::Methods* trx,
                         VPackFunctionParameters const& parameters) {
  static char const* AFN = "CALL";
  ValidateParameters(parameters, AFN, 1);

  AqlValue invokeFN = ExtractFunctionParameterValue(parameters, 0);
  if (!invokeFN.isString()) {
    ::registerError(query, AFN, TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH);
    return AqlValue(AqlValueHintNull());
  }

  SmallVector<AqlValue>::allocator_type::arena_type arena;
  VPackFunctionParameters invokeParams{arena};
  if (parameters.size() >= 2) {
    // we have a list of parameters, need to copy them over except the functionname:
    invokeParams.reserve(parameters.size() -1);

    for (uint64_t i = 1; i < parameters.size(); i++) {
      invokeParams.push_back(ExtractFunctionParameterValue(parameters, i));
    }
  }

  return CallApplyBackend(query, trx, parameters, AFN, invokeFN, invokeParams);
}

/// @brief function APPLY
AqlValue Functions::Apply(
    arangodb::aql::Query* query, transaction::Methods* trx,
    VPackFunctionParameters const& parameters) {
  static char const* AFN = "APPLY";
  ValidateParameters(parameters, AFN, 1, 2);

  AqlValue invokeFN = ExtractFunctionParameterValue(parameters, 0);
  if (!invokeFN.isString()) {
    ::registerError(query, AFN, TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH);
    return AqlValue(AqlValueHintNull());
  }

  SmallVector<AqlValue>::allocator_type::arena_type arena;
  VPackFunctionParameters invokeParams{arena};
  AqlValue rawParamArray;
  std::vector<bool> mustFree;

  auto guard = scopeGuard([&mustFree, &invokeParams]() {
    for (size_t i = 0; i < mustFree.size(); ++i) {
      if (mustFree[i]) {
        invokeParams[i].destroy();
      }
    }
  });

  if (parameters.size() == 2) {
    // We have a parameter that should be an array, whichs content we need to make
    // the sub functions parameters.
    rawParamArray = ExtractFunctionParameterValue(parameters, 1);

    if (!rawParamArray.isArray()) {
      ::registerWarning(query, AFN, TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH);
      return AqlValue(AqlValueHintNull());
    }
    uint64_t len = rawParamArray.length();
    invokeParams.reserve(len);
    mustFree.reserve(len);
    for (uint64_t i = 0; i < len; i++) {
      bool f;
      invokeParams.push_back(rawParamArray.at(trx, i, f, false));
      mustFree.push_back(f);
    }
  }

  return CallApplyBackend(query, trx, parameters, AFN, invokeFN, invokeParams);
}

/// @brief function IS_SAME_COLLECTION
AqlValue Functions::IsSameCollection(
    arangodb::aql::Query* query, transaction::Methods* trx,
    VPackFunctionParameters const& parameters) {
  static char const* AFN = "IS_SAME_COLLECTION";
  ValidateParameters(parameters, AFN, 2, 2);

  std::string const first = ::extractCollectionName(trx, parameters, 0);
  std::string const second = ::extractCollectionName(trx, parameters, 1);

  if (!first.empty() && !second.empty()) {
    return AqlValue(AqlValueHintBool(first == second));
  }

  ::registerWarning(query, AFN, TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH);
  return AqlValue(AqlValueHintNull());
}

AqlValue Functions::PregelResult(arangodb::aql::Query* query,
                                 transaction::Methods* trx,
                                 VPackFunctionParameters const& parameters) {
  static char const* AFN = "PREGEL_RESULT";
  ValidateParameters(parameters, AFN, 1, 1);

  AqlValue arg1 = ExtractFunctionParameterValue(parameters, 0);
  if (!arg1.isNumber()) {
    THROW_ARANGO_EXCEPTION_PARAMS(TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH, AFN);
  }

  uint64_t execNr = arg1.toInt64(trx);
  pregel::PregelFeature* feature = pregel::PregelFeature::instance();
  if (feature) {
    std::shared_ptr<pregel::IWorker> worker = feature->worker(execNr);
    if (!worker) {
      ::registerWarning(query, AFN, TRI_ERROR_QUERY_FUNCTION_INVALID_CODE);
      return AqlValue(arangodb::basics::VelocyPackHelper::EmptyArrayValue());
    }
    transaction::BuilderLeaser builder(trx);
    worker->aqlResult(builder.get());
    return AqlValue(builder.get());
  } 
    
  ::registerWarning(query, AFN, TRI_ERROR_QUERY_FUNCTION_INVALID_CODE);
  return AqlValue(arangodb::basics::VelocyPackHelper::EmptyArrayValue());
}

AqlValue Functions::Assert(arangodb::aql::Query* query,
                           transaction::Methods* trx,
                           VPackFunctionParameters const& parameters) {
  static char const* AFN = "ASSERT";
  ValidateParameters(parameters, AFN, 2, 2);
  auto const expr = ExtractFunctionParameterValue(parameters, 0);
  auto const message = ExtractFunctionParameterValue(parameters, 1);

  if (!message.isString()) {
    ::registerInvalidArgumentWarning(query, AFN);
    return AqlValue(AqlValueHintNull());
  }
  if (!expr.toBoolean()) {
    std::string msg = message.slice().copyString();
    query->registerError(TRI_ERROR_QUERY_USER_ASSERT, msg.data());
  }
  return AqlValue(AqlValueHintBool(true));
}

AqlValue Functions::Warn(arangodb::aql::Query* query, transaction::Methods* trx,
                         VPackFunctionParameters const& parameters) {
  static char const* AFN = "WARN";
  ValidateParameters(parameters, AFN, 2, 2);
  auto const expr = ExtractFunctionParameterValue(parameters, 0);
  auto const message = ExtractFunctionParameterValue(parameters, 1);

  if (!message.isString()) {
    ::registerInvalidArgumentWarning(query, AFN);
    return AqlValue(AqlValueHintNull());
  }

  if (!expr.toBoolean()) {
    std::string msg = message.slice().copyString();
    query->registerWarning(TRI_ERROR_QUERY_USER_WARN, msg.data());
    return AqlValue(AqlValueHintBool(false));
  }
  return AqlValue(AqlValueHintBool(true));
}

AqlValue Functions::Fail(arangodb::aql::Query*, transaction::Methods* trx,
                         VPackFunctionParameters const& parameters) {
  static char const* AFN = "FAIL";
  ValidateParameters(parameters, AFN, 0, 1);
  if (parameters.size() == 0) {
    THROW_ARANGO_EXCEPTION_PARAMS(TRI_ERROR_QUERY_FAIL_CALLED, "");
  }

  AqlValue value = ExtractFunctionParameterValue(parameters, 0);

  if (!value.isString()) {
    THROW_ARANGO_EXCEPTION_PARAMS(TRI_ERROR_QUERY_FAIL_CALLED, "");
  }

  AqlValueMaterializer materializer(trx);
  VPackSlice str = materializer.slice(value, false);
  THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_QUERY_FAIL_CALLED, str.copyString());
}

/// @brief function DATE_FORMAT
AqlValue Functions::DateFormat(arangodb::aql::Query* query,
                               transaction::Methods* trx,
                               VPackFunctionParameters const& params) {
  static char const* AFN = "DATE_FORMAT";
  tp_sys_clock_ms tp;
  ValidateParameters(params, AFN, 2, 2);

  if (!::parameterToTimePoint(query, trx, params, tp, AFN, 0)) {
    return AqlValue(AqlValueHintNull());
  }

  AqlValue aqlFormatString = ExtractFunctionParameterValue(params, 1);
  if (!aqlFormatString.isString()) {
    ::registerInvalidArgumentWarning(query, AFN);
    return AqlValue(AqlValueHintNull());
  }      

  std::string const formatString = aqlFormatString.slice().copyString();
  return AqlValue(::executeDateFormatRegex(formatString, tp));
}
