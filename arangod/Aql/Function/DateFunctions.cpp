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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#include "Aql/AqlValue.h"
#include "Aql/AqlValueMaterializer.h"
#include "Aql/AstNode.h"
#include "Aql/ExpressionContext.h"
#include "Aql/Function.h"
#include "Aql/Functions.h"
#include "Basics/datetime.h"
#include "Transaction/Helpers.h"
#include "Transaction/Methods.h"

#include <date/date.h>
#include <date/iso_week.h>
#include <date/tz.h>

#include <velocypack/Builder.h>
#include <velocypack/Iterator.h>
#include <velocypack/Slice.h>

#include <chrono>

using namespace arangodb;
using namespace std::chrono;

namespace arangodb::aql {

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

/// @brief optimized version of datetime stringification
/// string format is hard-coded to YYYY-MM-DDTHH:MM:SS.XXXZ
AqlValue timeAqlValue(ExpressionContext* expressionContext, char const* AFN,
                      tp_sys_clock_ms const& tp, bool utc = true,
                      bool registerWarning = true) {
  char formatted[24];

  date::year_month_day ymd{floor<date::days>(tp)};
  auto day_time = date::make_time(tp - date::sys_days(ymd));

  auto y = static_cast<int>(ymd.year());
  // quick basic check here for dates outside the allowed range
  if (y < 0 || y > 9999) {
    if (registerWarning) {
      aql::functions::registerWarning(expressionContext, AFN,
                                      TRI_ERROR_QUERY_INVALID_DATE_VALUE);
    }
    return AqlValue(AqlValueHintNull());
  }

  formatted[0] = '0' + (y / 1000);
  formatted[1] = '0' + ((y % 1000) / 100);
  formatted[2] = '0' + ((y % 100) / 10);
  formatted[3] = '0' + (y % 10);
  formatted[4] = '-';
  auto m = static_cast<unsigned>(ymd.month());
  formatted[5] = '0' + (m / 10);
  formatted[6] = '0' + (m % 10);
  formatted[7] = '-';
  auto d = static_cast<unsigned>(ymd.day());
  formatted[8] = '0' + (d / 10);
  formatted[9] = '0' + (d % 10);
  formatted[10] = 'T';
  auto h = day_time.hours().count();
  formatted[11] = '0' + (h / 10);
  formatted[12] = '0' + (h % 10);
  formatted[13] = ':';
  auto i = day_time.minutes().count();
  formatted[14] = '0' + (i / 10);
  formatted[15] = '0' + (i % 10);
  formatted[16] = ':';
  auto s = day_time.seconds().count();
  formatted[17] = '0' + (s / 10);
  formatted[18] = '0' + (s % 10);
  formatted[19] = '.';
  uint64_t millis = day_time.subseconds().count();
  if (millis > 999) {
    millis = 999;
  }
  formatted[20] = '0' + (millis / 100);
  formatted[21] = '0' + ((millis % 100) / 10);
  formatted[22] = '0' + (millis % 10);
  formatted[23] = 'Z';

  return AqlValue(std::string_view{
      &formatted[0], utc ? sizeof(formatted) : sizeof(formatted) - 1});
}

DateSelectionModifier parseDateModifierFlag(VPackSlice flag) {
  if (!flag.isString()) {
    return INVALID;
  }

  // must be copied because we are lower-casing the string
  std::string flagStr = flag.copyString();
  if (flagStr.empty()) {
    return INVALID;
  }
  TRI_ASSERT(flagStr.size() >= 1);

  basics::StringUtils::tolowerInPlace(flagStr);
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

date::sys_info localizeTimePoint(std::string const& timezone,
                                 tp_sys_clock_ms& localTimePoint) {
  auto const utc = floor<milliseconds>(localTimePoint);
  auto const zoned = date::make_zoned(timezone, utc);
  localTimePoint = tp_sys_clock_ms{zoned.get_local_time().time_since_epoch()};
  return zoned.get_info();
}

date::sys_info unlocalizeTimePoint(std::string const& timezone,
                                   tp_sys_clock_ms& utcTimePoint) {
  auto const local = date::local_time<milliseconds>{
      floor<milliseconds>(utcTimePoint).time_since_epoch()};
  auto const zoned = date::make_zoned(timezone, local);
  utcTimePoint = tp_sys_clock_ms{zoned.get_sys_time().time_since_epoch()};
  return zoned.get_info();
}

AqlValue addOrSubtractUnitFromTimestamp(ExpressionContext* expressionContext,
                                        tp_sys_clock_ms const& tp,
                                        VPackSlice durationUnitsSlice,
                                        VPackSlice durationType,
                                        char const* AFN, bool isSubtract,
                                        std::string const& timezone) {
  bool isInteger = durationUnitsSlice.isInteger();
  double durationUnits = durationUnitsSlice.getNumber<double>();
  std::chrono::duration<double, std::ratio<1l, 1000l>> ms{};
  date::year_month_day ymd{floor<date::days>(tp)};
  auto day_time = date::make_time(tp - date::sys_days(ymd));

  DateSelectionModifier flag = parseDateModifierFlag(durationType);
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
        ymd -= date::years{static_cast<int64_t>(intPart)};
      } else {
        ymd += date::years{static_cast<int64_t>(intPart)};
      }
      if (isInteger || durationUnits == 0.0) {
        break;  // We are done
      }
      durationUnits *= 12;
      [[fallthrough]];
    case MONTH:
      durationUnits = std::modf(durationUnits, &intPart);
      if (isSubtract) {
        ymd -= date::months{static_cast<int64_t>(intPart)};
      } else {
        ymd += date::months{static_cast<int64_t>(intPart)};
      }
      if (isInteger || durationUnits == 0.0) {
        break;  // We are done
      }
      durationUnits *= 30;  // 1 Month ~= 30 Days
      [[fallthrough]];
    // After this fall through the date may actually a bit off
    case DAY:
      // From here on we do not need leap-day handling
      ms = date::days{1};
      ms *= durationUnits;
      break;
    case WEEK:
      ms = date::weeks{1};
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
      aql::functions::registerWarning(expressionContext, AFN,
                                      TRI_ERROR_QUERY_INVALID_DATE_VALUE);
      return AqlValue(AqlValueHintNull());
  }
  // Here we reconstruct the timepoint again

  tp_sys_clock_ms resTime;
  if (isSubtract) {
    resTime = tp_sys_clock_ms{
        date::sys_days(ymd) + day_time.to_duration() -
        std::chrono::duration_cast<duration<int64_t, std::milli>>(ms)};
  } else {
    resTime = tp_sys_clock_ms{
        date::sys_days(ymd) + day_time.to_duration() +
        std::chrono::duration_cast<duration<int64_t, std::milli>>(ms)};
  }

  if (timezone != "UTC") {
    unlocalizeTimePoint(timezone, resTime);
  }

  return timeAqlValue(expressionContext, AFN, resTime);
}

AqlValue addOrSubtractIsoDurationFromTimestamp(
    ExpressionContext* expressionContext, tp_sys_clock_ms const& tp,
    std::string_view duration, char const* AFN, bool isSubtract,
    std::string const& timezone) {
  date::year_month_day ymd{floor<date::days>(tp)};
  auto day_time = date::make_time(tp - date::sys_days(ymd));

  basics::ParsedDuration parsedDuration;
  if (!basics::parseIsoDuration(duration, parsedDuration)) {
    aql::functions::registerWarning(expressionContext, AFN,
                                    TRI_ERROR_QUERY_INVALID_DATE_VALUE);
    return AqlValue(AqlValueHintNull());
  }

  if (isSubtract) {
    ymd -= date::years{parsedDuration.years};
    ymd -= date::months{parsedDuration.months};
  } else {
    ymd += date::years{parsedDuration.years};
    ymd += date::months{parsedDuration.months};
  }

  milliseconds ms{0};
  ms += date::weeks{parsedDuration.weeks};
  ms += date::days{parsedDuration.days};
  ms += hours{parsedDuration.hours};
  ms += minutes{parsedDuration.minutes};
  ms += seconds{parsedDuration.seconds};
  ms += milliseconds{parsedDuration.milliseconds};

  tp_sys_clock_ms resTime;
  if (isSubtract) {
    resTime =
        tp_sys_clock_ms{date::sys_days(ymd) + day_time.to_duration() - ms};
  } else {
    resTime =
        tp_sys_clock_ms{date::sys_days(ymd) + day_time.to_duration() + ms};
  }

  if (timezone != "UTC") {
    unlocalizeTimePoint(timezone, resTime);
  }

  return timeAqlValue(expressionContext, AFN, resTime);
}

bool parameterToTimePoint(
    ExpressionContext* expressionContext,
    aql::functions::VPackFunctionParametersView parameters, tp_sys_clock_ms& tp,
    char const* AFN, size_t parameterIndex) {
  AqlValue const& value =
      aql::functions::extractFunctionParameterValue(parameters, parameterIndex);

  if (value.isNumber()) {
    int64_t v = value.toInt64();
    if (ADB_UNLIKELY(v < -62167219200000 || v > 253402300799999)) {
      // check if value is between "0000-01-01T00:00:00.000Z" and
      // "9999-12-31T23:59:59.999Z" -62167219200000: "0000-01-01T00:00:00.000Z"
      // 253402300799999: "9999-12-31T23:59:59.999Z"
      aql::functions::registerWarning(expressionContext, AFN,
                                      TRI_ERROR_QUERY_INVALID_DATE_VALUE);
      return false;
    }
    tp = tp_sys_clock_ms(milliseconds(v));
    return true;
  }

  if (value.isString()) {
    if (!basics::parseDateTime(value.slice().stringView(), tp)) {
      aql::functions::registerWarning(expressionContext, AFN,
                                      TRI_ERROR_QUERY_INVALID_DATE_VALUE);
      return false;
    }
    return true;
  }

  aql::functions::registerInvalidArgumentWarning(expressionContext, AFN);
  return false;
}

/**
 * @brief Parses 1 or 3-7 input parameters and creates a Date object out of it.
 *        This object can either be a timestamp in milliseconds or an ISO_8601
 * DATE
 *
 * @param expressionContext The AQL expression context
 * @param trx The used transaction
 * @param parameters list of parameters, only 1 or 3-7 are allowed
 * @param asTimestamp If it should return a timestamp (true) or ISO_DATE (false)
 *
 * @return Returns a timestamp if asTimestamp is true, an ISO_DATE otherwise
 */
AqlValue dateFromParameters(
    ExpressionContext* expressionContext,
    aql::functions::VPackFunctionParametersView parameters, char const* AFN,
    bool asTimestamp) {
  tp_sys_clock_ms tp;
  duration<int64_t, std::milli> time;

  if (parameters.size() == 1) {
    if (!parameterToTimePoint(expressionContext, parameters, tp, AFN, 0)) {
      return AqlValue(AqlValueHintNull());
    }
    time = tp.time_since_epoch();
  } else {
    if (parameters.size() < 3 || parameters.size() > 7) {
      // YMD is a must
      aql::functions::registerInvalidArgumentWarning(expressionContext, AFN);
      return AqlValue(AqlValueHintNull());
    }

    for (uint8_t i = 0; i < parameters.size(); i++) {
      AqlValue const& value =
          aql::functions::extractFunctionParameterValue(parameters, i);

      // All Parameters have to be a number or a string
      if (!value.isNumber() && !value.isString()) {
        aql::functions::registerInvalidArgumentWarning(expressionContext, AFN);
        return AqlValue(AqlValueHintNull());
      }
    }

    date::years y{
        aql::functions::extractFunctionParameterValue(parameters, 0).toInt64()};
    date::months m{
        aql::functions::extractFunctionParameterValue(parameters, 1).toInt64()};
    date::days d{
        aql::functions::extractFunctionParameterValue(parameters, 2).toInt64()};

    if ((y < date::years{0} || y > date::years{9999}) ||
        (m < date::months{0}) || (d < date::days{0})) {
      aql::functions::registerWarning(expressionContext, AFN,
                                      TRI_ERROR_QUERY_INVALID_DATE_VALUE);
      return AqlValue(AqlValueHintNull());
    }
    date::year_month_day ymd = date::year{y.count()} / m.count() / d.count();

    // Parse the time
    hours h(0);
    minutes min(0);
    seconds s(0);
    milliseconds ms(0);

    if (parameters.size() >= 4) {
      h = hours((aql::functions::extractFunctionParameterValue(parameters, 3)
                     .toInt64()));
    }
    if (parameters.size() >= 5) {
      min =
          minutes((aql::functions::extractFunctionParameterValue(parameters, 4)
                       .toInt64()));
    }
    if (parameters.size() >= 6) {
      s = seconds((aql::functions::extractFunctionParameterValue(parameters, 5)
                       .toInt64()));
    }
    if (parameters.size() == 7) {
      int64_t v = aql::functions::extractFunctionParameterValue(parameters, 6)
                      .toInt64();
      if (v > 999) {
        v = 999;
      }
      ms = milliseconds(v);
    }

    if ((h < hours{0}) || (min < minutes{0}) || (s < seconds{0}) ||
        (ms < milliseconds{0})) {
      aql::functions::registerWarning(expressionContext, AFN,
                                      TRI_ERROR_QUERY_INVALID_DATE_VALUE);
      return AqlValue(AqlValueHintNull());
    }

    time = date::sys_days(ymd).time_since_epoch();
    time += h;
    time += min;
    time += s;
    time += ms;
    tp = tp_sys_clock_ms(time);
  }

  if (asTimestamp) {
    return AqlValue(AqlValueHintInt(time.count()));
  }
  return timeAqlValue(expressionContext, AFN, tp);
}

}  // namespace

/// @brief function DATE_NOW
AqlValue functions::DateNow(ExpressionContext*, AstNode const&,
                            VPackFunctionParametersView) {
  auto millis = std::chrono::duration_cast<duration<int64_t, std::milli>>(
      system_clock::now().time_since_epoch());
  uint64_t dur = millis.count();
  return AqlValue(AqlValueHintUInt(dur));
}

/// @brief function DATE_ISO8601
AqlValue functions::DateIso8601(ExpressionContext* expressionContext,
                                AstNode const&,
                                VPackFunctionParametersView parameters) {
  static char const* AFN = "DATE_ISO8601";
  return dateFromParameters(expressionContext, parameters, AFN, false);
}

/// @brief function DATE_TIMESTAMP
AqlValue functions::DateTimestamp(ExpressionContext* expressionContext,
                                  AstNode const&,
                                  VPackFunctionParametersView parameters) {
  static char const* AFN = "DATE_TIMESTAMP";
  return dateFromParameters(expressionContext, parameters, AFN, true);
}

/// @brief function IS_DATESTRING
AqlValue functions::IsDatestring(ExpressionContext*, AstNode const&,
                                 VPackFunctionParametersView parameters) {
  AqlValue const& value =
      aql::functions::extractFunctionParameterValue(parameters, 0);

  bool isValid = false;

  if (value.isString()) {
    tp_sys_clock_ms tp;  // unused
    isValid = basics::parseDateTime(value.slice().stringView(), tp);
  }

  return AqlValue(AqlValueHintBool(isValid));
}

/// @brief function DATE_DAYOFWEEK
AqlValue functions::DateDayOfWeek(ExpressionContext* expressionContext,
                                  AstNode const&,
                                  VPackFunctionParametersView parameters) {
  static char const* AFN = "DATE_DAYOFWEEK";
  tp_sys_clock_ms tp;
  if (!parameterToTimePoint(expressionContext, parameters, tp, AFN, 0)) {
    return AqlValue(AqlValueHintNull());
  }

  if (parameters.size() == 2) {
    AqlValue const timezoneParam =
        aql::functions::extractFunctionParameterValue(parameters, 1);

    if (!timezoneParam.isString()) {  // timezone type must be string
      registerInvalidArgumentWarning(expressionContext, AFN);
      return AqlValue(AqlValueHintNull());
    }

    std::string inputTimezone = timezoneParam.slice().copyString();

    localizeTimePoint(inputTimezone, tp);
  }

  date::weekday wd{floor<date::days>(tp)};

  return AqlValue(AqlValueHintUInt(wd.c_encoding()));
}

/// @brief function DATE_YEAR
AqlValue functions::DateYear(ExpressionContext* expressionContext,
                             AstNode const&,
                             VPackFunctionParametersView parameters) {
  static char const* AFN = "DATE_YEAR";
  tp_sys_clock_ms tp;

  if (!parameterToTimePoint(expressionContext, parameters, tp, AFN, 0)) {
    return AqlValue(AqlValueHintNull());
  }

  if (parameters.size() == 2) {
    AqlValue const timezoneParam =
        aql::functions::extractFunctionParameterValue(parameters, 1);

    if (!timezoneParam.isString()) {  // timezone type must be string
      registerInvalidArgumentWarning(expressionContext, AFN);
      return AqlValue(AqlValueHintNull());
    }

    std::string inputTimezone = timezoneParam.slice().copyString();

    localizeTimePoint(inputTimezone, tp);
  }

  auto ymd = date::year_month_day(floor<date::days>(tp));
  // Not the library has operator (int) implemented...
  int64_t year = static_cast<int64_t>((int)ymd.year());
  return AqlValue(AqlValueHintInt(year));
}

/// @brief function DATE_MONTH
AqlValue functions::DateMonth(ExpressionContext* expressionContext,
                              AstNode const&,
                              VPackFunctionParametersView parameters) {
  static char const* AFN = "DATE_MONTH";
  tp_sys_clock_ms tp;

  if (!parameterToTimePoint(expressionContext, parameters, tp, AFN, 0)) {
    return AqlValue(AqlValueHintNull());
  }

  if (parameters.size() == 2) {
    AqlValue const timezoneParam =
        aql::functions::extractFunctionParameterValue(parameters, 1);

    if (!timezoneParam.isString()) {  // timezone type must be string
      registerInvalidArgumentWarning(expressionContext, AFN);
      return AqlValue(AqlValueHintNull());
    }

    std::string inputTimezone = timezoneParam.slice().copyString();

    localizeTimePoint(inputTimezone, tp);
  }

  auto ymd = date::year_month_day(floor<date::days>(tp));
  // The library has operator (unsigned) implemented
  uint64_t month = static_cast<uint64_t>((unsigned)ymd.month());
  return AqlValue(AqlValueHintUInt(month));
}

/// @brief function DATE_DAY
AqlValue functions::DateDay(ExpressionContext* expressionContext,
                            AstNode const&,
                            VPackFunctionParametersView parameters) {
  static char const* AFN = "DATE_DAY";
  tp_sys_clock_ms tp;

  if (!parameterToTimePoint(expressionContext, parameters, tp, AFN, 0)) {
    return AqlValue(AqlValueHintNull());
  }

  if (parameters.size() == 2) {
    AqlValue const timezoneParam =
        aql::functions::extractFunctionParameterValue(parameters, 1);

    if (!timezoneParam.isString()) {  // timezone type must be string
      registerInvalidArgumentWarning(expressionContext, AFN);
      return AqlValue(AqlValueHintNull());
    }

    std::string inputTimezone = timezoneParam.slice().copyString();

    localizeTimePoint(inputTimezone, tp);
  }

  auto ymd = date::year_month_day(floor<date::days>(tp));
  // The library has operator (unsigned) implemented
  uint64_t day = static_cast<uint64_t>((unsigned)ymd.day());
  return AqlValue(AqlValueHintUInt(day));
}

/// @brief function DATE_HOUR
AqlValue functions::DateHour(ExpressionContext* expressionContext,
                             AstNode const&,
                             VPackFunctionParametersView parameters) {
  static char const* AFN = "DATE_HOUR";
  tp_sys_clock_ms tp;

  if (!parameterToTimePoint(expressionContext, parameters, tp, AFN, 0)) {
    return AqlValue(AqlValueHintNull());
  }

  if (parameters.size() == 2) {
    AqlValue const timezoneParam =
        aql::functions::extractFunctionParameterValue(parameters, 1);

    if (!timezoneParam.isString()) {  // timezone type must be string
      registerInvalidArgumentWarning(expressionContext, AFN);
      return AqlValue(AqlValueHintNull());
    }

    std::string inputTimezone = timezoneParam.slice().copyString();

    localizeTimePoint(inputTimezone, tp);
  }

  auto day_time = date::make_time(tp - floor<date::days>(tp));
  uint64_t hours = day_time.hours().count();
  return AqlValue(AqlValueHintUInt(hours));
}

/// @brief function DATE_MINUTE
AqlValue functions::DateMinute(ExpressionContext* expressionContext,
                               AstNode const&,
                               VPackFunctionParametersView parameters) {
  static char const* AFN = "DATE_MINUTE";
  tp_sys_clock_ms tp;

  if (!parameterToTimePoint(expressionContext, parameters, tp, AFN, 0)) {
    return AqlValue(AqlValueHintNull());
  }

  if (parameters.size() == 2) {
    AqlValue const timezoneParam =
        aql::functions::extractFunctionParameterValue(parameters, 1);

    if (!timezoneParam.isString()) {  // timezone type must be string
      registerInvalidArgumentWarning(expressionContext, AFN);
      return AqlValue(AqlValueHintNull());
    }

    std::string inputTimezone = timezoneParam.slice().copyString();

    localizeTimePoint(inputTimezone, tp);
  }

  auto day_time = date::make_time(tp - floor<date::days>(tp));
  uint64_t minutes = day_time.minutes().count();
  return AqlValue(AqlValueHintUInt(minutes));
}

/// @brief function DATE_SECOND
AqlValue functions::DateSecond(ExpressionContext* expressionContext,
                               AstNode const&,
                               VPackFunctionParametersView parameters) {
  static char const* AFN = "DATE_SECOND";
  tp_sys_clock_ms tp;

  if (!parameterToTimePoint(expressionContext, parameters, tp, AFN, 0)) {
    return AqlValue(AqlValueHintNull());
  }

  auto day_time = date::make_time(tp - floor<date::days>(tp));
  uint64_t seconds = day_time.seconds().count();
  return AqlValue(AqlValueHintUInt(seconds));
}

/// @brief function DATE_MILLISECOND
AqlValue functions::DateMillisecond(ExpressionContext* expressionContext,
                                    AstNode const&,
                                    VPackFunctionParametersView parameters) {
  static char const* AFN = "DATE_MILLISECOND";
  tp_sys_clock_ms tp;

  if (!parameterToTimePoint(expressionContext, parameters, tp, AFN, 0)) {
    return AqlValue(AqlValueHintNull());
  }
  auto day_time = date::make_time(tp - floor<date::days>(tp));
  uint64_t millis = day_time.subseconds().count();
  return AqlValue(AqlValueHintUInt(millis));
}

/// @brief function DATE_DAYOFYEAR
AqlValue functions::DateDayOfYear(ExpressionContext* expressionContext,
                                  AstNode const&,
                                  VPackFunctionParametersView parameters) {
  static char const* AFN = "DATE_DAYOFYEAR";
  tp_sys_clock_ms tp;

  if (!parameterToTimePoint(expressionContext, parameters, tp, AFN, 0)) {
    return AqlValue(AqlValueHintNull());
  }

  if (parameters.size() == 2) {
    AqlValue const timezoneParam =
        aql::functions::extractFunctionParameterValue(parameters, 1);

    if (!timezoneParam.isString()) {  // timezone type must be string
      registerInvalidArgumentWarning(expressionContext, AFN);
      return AqlValue(AqlValueHintNull());
    }

    std::string inputTimezone = timezoneParam.slice().copyString();

    localizeTimePoint(inputTimezone, tp);
  }

  auto ymd = date::year_month_day(floor<date::days>(tp));
  auto yyyy = date::year{ymd.year()};
  // we construct the date with the first day in the year:
  auto firstDayInYear = yyyy / date::jan / date::day{0};
  uint64_t daysSinceFirst =
      duration_cast<date::days>(tp - date::sys_days(firstDayInYear)).count();

  return AqlValue(AqlValueHintUInt(daysSinceFirst));
}

/// @brief function DATE_ISOWEEK
AqlValue functions::DateIsoWeek(ExpressionContext* expressionContext,
                                AstNode const&,
                                VPackFunctionParametersView parameters) {
  static char const* AFN = "DATE_ISOWEEK";
  tp_sys_clock_ms tp;

  if (!parameterToTimePoint(expressionContext, parameters, tp, AFN, 0)) {
    return AqlValue(AqlValueHintNull());
  }

  if (parameters.size() == 2) {
    AqlValue const timezoneParam =
        aql::functions::extractFunctionParameterValue(parameters, 1);

    if (!timezoneParam.isString()) {  // timezone type must be string
      registerInvalidArgumentWarning(expressionContext, AFN);
      return AqlValue(AqlValueHintNull());
    }

    std::string inputTimezone = timezoneParam.slice().copyString();

    localizeTimePoint(inputTimezone, tp);
  }

  iso_week::year_weeknum_weekday yww{floor<date::days>(tp)};
  // The (unsigned) operator is overloaded...
  uint64_t isoWeek = static_cast<uint64_t>((unsigned)(yww.weeknum()));
  return AqlValue(AqlValueHintUInt(isoWeek));
}

/// @brief function DATE_ISOWEEKYEAR
AqlValue functions::DateIsoWeekYear(ExpressionContext* expressionContext,
                                    AstNode const&,
                                    VPackFunctionParametersView parameters) {
  static char const* AFN = "DATE_ISOWEEKYEAR";
  tp_sys_clock_ms tp;

  if (!parameterToTimePoint(expressionContext, parameters, tp, AFN, 0)) {
    return AqlValue(AqlValueHintNull());
  }

  if (parameters.size() == 2) {
    AqlValue const timezoneParam =
        aql::functions::extractFunctionParameterValue(parameters, 1);

    if (!timezoneParam.isString()) {  // timezone type must be string
      registerInvalidArgumentWarning(expressionContext, AFN);
      return AqlValue(AqlValueHintNull());
    }

    std::string inputTimezone = timezoneParam.slice().copyString();

    localizeTimePoint(inputTimezone, tp);
  }

  iso_week::year_weeknum_weekday yww{floor<date::days>(tp)};
  // The (unsigned) operator is overloaded...
  uint64_t isoWeek = static_cast<uint64_t>((unsigned)(yww.weeknum()));
  int isoYear = (int)(yww.year());
  transaction::Methods* trx = &expressionContext->trx();
  transaction::BuilderLeaser builder(trx);
  builder->openObject();
  builder->add("week", VPackValue(isoWeek));
  builder->add("year", VPackValue(isoYear));
  builder->close();

  return AqlValue(builder->slice(), builder->size());
}

/// @brief function DATE_LEAPYEAR
AqlValue functions::DateLeapYear(ExpressionContext* expressionContext,
                                 AstNode const&,
                                 VPackFunctionParametersView parameters) {
  static char const* AFN = "DATE_LEAPYEAR";
  tp_sys_clock_ms tp;

  if (!parameterToTimePoint(expressionContext, parameters, tp, AFN, 0)) {
    return AqlValue(AqlValueHintNull());
  }

  if (parameters.size() == 2) {
    AqlValue const timezoneParam =
        aql::functions::extractFunctionParameterValue(parameters, 1);

    if (!timezoneParam.isString()) {  // timezone type must be string
      registerInvalidArgumentWarning(expressionContext, AFN);
      return AqlValue(AqlValueHintNull());
    }

    std::string inputTimezone = timezoneParam.slice().copyString();

    localizeTimePoint(inputTimezone, tp);
  }

  date::year_month_day ymd{floor<date::days>(tp)};

  return AqlValue(AqlValueHintBool(ymd.year().is_leap()));
}

/// @brief function DATE_QUARTER
AqlValue functions::DateQuarter(ExpressionContext* expressionContext,
                                AstNode const&,
                                VPackFunctionParametersView parameters) {
  static char const* AFN = "DATE_QUARTER";
  tp_sys_clock_ms tp;

  if (!parameterToTimePoint(expressionContext, parameters, tp, AFN, 0)) {
    return AqlValue(AqlValueHintNull());
  }

  if (parameters.size() == 2) {
    AqlValue const timezoneParam =
        aql::functions::extractFunctionParameterValue(parameters, 1);

    if (!timezoneParam.isString()) {  // timezone type must be string
      registerInvalidArgumentWarning(expressionContext, AFN);
      return AqlValue(AqlValueHintNull());
    }

    std::string inputTimezone = timezoneParam.slice().copyString();

    localizeTimePoint(inputTimezone, tp);
  }

  date::year_month_day ymd{floor<date::days>(tp)};
  date::month m = ymd.month();

  // Library has unsigned operator implemented.
  uint64_t part = static_cast<uint64_t>(ceil(unsigned(m) / 3.0f));
  // We only have 4 quarters ;)
  TRI_ASSERT(part <= 4);
  return AqlValue(AqlValueHintUInt(part));
}

/// @brief function DATE_DAYS_IN_MONTH
AqlValue functions::DateDaysInMonth(ExpressionContext* expressionContext,
                                    AstNode const&,
                                    VPackFunctionParametersView parameters) {
  static char const* AFN = "DATE_DAYS_IN_MONTH";
  tp_sys_clock_ms tp;

  if (!parameterToTimePoint(expressionContext, parameters, tp, AFN, 0)) {
    return AqlValue(AqlValueHintNull());
  }

  if (parameters.size() == 2) {
    AqlValue const timezoneParam =
        aql::functions::extractFunctionParameterValue(parameters, 1);

    if (!timezoneParam.isString()) {  // timezone type must be string
      registerInvalidArgumentWarning(expressionContext, AFN);
      return AqlValue(AqlValueHintNull());
    }

    std::string inputTimezone = timezoneParam.slice().copyString();

    localizeTimePoint(inputTimezone, tp);
  }

  auto ymd = date::year_month_day{floor<date::days>(tp)};
  auto lastMonthDay = ymd.year() / ymd.month() / date::last;

  // The Library has operator unsigned implemented
  return AqlValue(
      AqlValueHintUInt(static_cast<uint64_t>(unsigned(lastMonthDay.day()))));
}

/// @brief function DATE_TRUNC
AqlValue functions::DateTrunc(ExpressionContext* expressionContext,
                              AstNode const&,
                              VPackFunctionParametersView parameters) {
  static char const* AFN = "DATE_TRUNC";

  tp_sys_clock_ms tp;

  if (!parameterToTimePoint(expressionContext, parameters, tp, AFN, 0)) {
    return AqlValue(AqlValueHintNull());
  }

  AqlValue const& durationType =
      aql::functions::extractFunctionParameterValue(parameters, 1);

  if (!durationType.isString()) {  // unit type must be string
    registerInvalidArgumentWarning(expressionContext, AFN);
    return AqlValue(AqlValueHintNull());
  }

  std::string duration = durationType.slice().copyString();
  basics::StringUtils::tolowerInPlace(duration);

  std::string inputTimezone = "UTC";

  if (parameters.size() == 3) {
    AqlValue const timezoneParam =
        aql::functions::extractFunctionParameterValue(parameters, 2);

    if (!timezoneParam.isString()) {  // timezone type must be string
      registerInvalidArgumentWarning(expressionContext, AFN);
      return AqlValue(AqlValueHintNull());
    }

    inputTimezone = timezoneParam.slice().copyString();

    localizeTimePoint(inputTimezone, tp);
  }

  date::year_month_day ymd{floor<date::days>(tp)};
  auto day_time = date::make_time(tp - date::sys_days(ymd));
  milliseconds ms{0};
  if (duration == "y" || duration == "year" || duration == "years") {
    ymd = date::year{ymd.year()} / date::jan / date::day{1};
  } else if (duration == "m" || duration == "month" || duration == "months") {
    ymd = date::year{ymd.year()} / ymd.month() / date::day{1};
  } else if (duration == "d" || duration == "day" || duration == "days") {
    // this would be: ymd = year{ymd.year()}/ymd.month()/ymd.day();
    // However, we already split ymd to the precision of days,
    // and ms to cary the timestamp part, so nothing needs to be done here.
  } else if (duration == "h" || duration == "hour" || duration == "hours") {
    ms = day_time.hours();
  } else if (duration == "i" || duration == "minute" || duration == "minutes") {
    ms = day_time.hours() + day_time.minutes();
  } else if (duration == "s" || duration == "second" || duration == "seconds") {
    ms = day_time.to_duration() - day_time.subseconds();
  } else if (duration == "f" || duration == "millisecond" ||
             duration == "milliseconds") {
    ms = day_time.to_duration();
  } else {
    registerWarning(expressionContext, AFN, TRI_ERROR_QUERY_INVALID_DATE_VALUE);
    return AqlValue(AqlValueHintNull());
  }
  tp = tp_sys_clock_ms{date::sys_days(ymd) + ms};

  if (parameters.size() == 3) {
    unlocalizeTimePoint(inputTimezone, tp);
  }

  return timeAqlValue(expressionContext, AFN, tp);
}

/// @brief function DATE_UTCTOLOCAL
AqlValue functions::DateUtcToLocal(ExpressionContext* expressionContext,
                                   AstNode const&,
                                   VPackFunctionParametersView parameters) {
  static char const* AFN = "DATE_UTCTOLOCAL";

  tp_sys_clock_ms tp_utc;

  if (!parameterToTimePoint(expressionContext, parameters, tp_utc, AFN, 0)) {
    return AqlValue(AqlValueHintNull());
  }

  AqlValue const& timezoneParam =
      aql::functions::extractFunctionParameterValue(parameters, 1);

  if (!timezoneParam.isString()) {  // timezone type must be string
    registerInvalidArgumentWarning(expressionContext, AFN);
    return AqlValue(AqlValueHintNull());
  }

  auto showDetail = false;

  if (parameters.size() == 3) {
    AqlValue const& detailParam =
        aql::functions::extractFunctionParameterValue(parameters, 2);
    if (!detailParam.isBoolean()) {  // detail type must be bool
      registerInvalidArgumentWarning(expressionContext, AFN);
      return AqlValue(AqlValueHintNull());
    }

    showDetail = detailParam.slice().getBoolean();
  }

  std::string const tz = timezoneParam.slice().copyString();
  auto const utc = floor<milliseconds>(tp_utc);
  auto const zoned = date::make_zoned(tz, utc);
  auto const info = zoned.get_info();
  auto const tp_local =
      tp_sys_clock_ms{zoned.get_local_time().time_since_epoch()};
  AqlValue aqlLocal =
      timeAqlValue(expressionContext, AFN, tp_local,
                   info.offset.count() == 0 && info.save.count() == 0);

  AqlValueGuard aqlLocalGuard(aqlLocal, true);

  if (showDetail) {
    auto const tp_begin = tp_sys_clock_ms{info.begin.time_since_epoch()};
    AqlValue aqlBegin =
        timeAqlValue(expressionContext, AFN, tp_begin, true, false);
    AqlValueGuard aqlBeginGuard(aqlBegin, true);

    auto const tp_end = tp_sys_clock_ms{info.end.time_since_epoch()};
    AqlValue aqlEnd = timeAqlValue(expressionContext, AFN, tp_end, true, false);
    AqlValueGuard aqlEndGuard(aqlEnd, true);

    transaction::Methods* trx = &expressionContext->trx();
    transaction::BuilderLeaser builder(trx);
    builder->openObject();
    builder->add("local", aqlLocal.slice());
    builder->add("tzdb", VPackValue(date::get_tzdb().version));
    builder->add("zoneInfo", VPackValue(VPackValueType::Object));
    builder->add("name", VPackValue(info.abbrev));
    builder->add("begin", aqlBegin.slice());
    builder->add("end", aqlEnd.slice());
    builder->add("dst", VPackValue(info.save.count() != 0));
    builder->add("offset", VPackValue(info.offset.count()));
    builder->close();
    builder->close();

    return AqlValue(builder->slice(), builder->size());
  }

  aqlLocalGuard.steal();
  return aqlLocal;
}

/// @brief function DATE_LOCALTOUTC
AqlValue functions::DateLocalToUtc(ExpressionContext* expressionContext,
                                   AstNode const&,
                                   VPackFunctionParametersView parameters) {
  static char const* AFN = "DATE_LOCALTOUTC";

  tp_sys_clock_ms tp_local;

  if (!parameterToTimePoint(expressionContext, parameters, tp_local, AFN, 0)) {
    return AqlValue(AqlValueHintNull());
  }

  AqlValue const& timezoneParam =
      aql::functions::extractFunctionParameterValue(parameters, 1);

  if (!timezoneParam.isString()) {  // timezone type must be string
    registerInvalidArgumentWarning(expressionContext, AFN);
    return AqlValue(AqlValueHintNull());
  }

  auto showDetail = false;

  if (parameters.size() == 3) {
    AqlValue const& detailParam =
        aql::functions::extractFunctionParameterValue(parameters, 2);
    if (!detailParam.isBoolean()) {  // detail type must be bool
      registerInvalidArgumentWarning(expressionContext, AFN);
      return AqlValue(AqlValueHintNull());
    }

    showDetail = detailParam.slice().getBoolean();
  }

  std::string inputTimezone = timezoneParam.slice().copyString();
  // converts tp_local to tp_utc
  auto const info = unlocalizeTimePoint(inputTimezone, tp_local);
  AqlValue aqlUtc = timeAqlValue(expressionContext, AFN, tp_local);

  AqlValueGuard aqlUtcGuard(aqlUtc, true);

  if (showDetail) {
    auto const tp_begin = tp_sys_clock_ms{info.begin.time_since_epoch()};
    AqlValue aqlBegin =
        timeAqlValue(expressionContext, AFN, tp_begin, true, false);
    AqlValueGuard aqlBeginGuard(aqlBegin, true);

    auto const tp_end = tp_sys_clock_ms{info.end.time_since_epoch()};
    AqlValue aqlEnd = timeAqlValue(expressionContext, AFN, tp_end, true, false);
    AqlValueGuard aqlEndGuard(aqlEnd, true);

    transaction::Methods* trx = &expressionContext->trx();
    transaction::BuilderLeaser builder(trx);
    builder->openObject();
    builder->add("utc", aqlUtc.slice());
    builder->add("tzdb", VPackValue(date::get_tzdb().version));
    builder->add("zoneInfo", VPackValue(VPackValueType::Object));
    builder->add("name", VPackValue(info.abbrev));
    builder->add("begin", aqlBegin.slice());
    builder->add("end", aqlEnd.slice());
    builder->add("dst", VPackValue(info.save.count() != 0));
    builder->add("offset", VPackValue(info.offset.count()));
    builder->close();
    builder->close();

    return AqlValue(builder->slice(), builder->size());
  }

  aqlUtcGuard.steal();
  return aqlUtc;
}

/// @brief function DATE_TIMEZONE
AqlValue functions::DateTimeZone(ExpressionContext* expressionContext,
                                 AstNode const&,
                                 VPackFunctionParametersView parameters) {
  auto const* zone = date::current_zone();

  if (zone != nullptr) {
    return AqlValue(zone->name());
  }

  return AqlValue(AqlValueHintNull());
}

/// @brief function DATE_TIMEZONES
AqlValue functions::DateTimeZones(ExpressionContext* expressionContext,
                                  AstNode const&,
                                  VPackFunctionParametersView parameters) {
  auto& list = date::get_tzdb_list();
  auto& db = list.front();

  transaction::Methods* trx = &expressionContext->trx();
  transaction::BuilderLeaser result(trx);
  result->openArray();

  for (auto& zone : db.zones) {
    result->add(VPackValue(zone.name()));
  }

  for (auto& link : db.links) {
    result->add(VPackValue(link.name()));
  }

  result->close();
  return AqlValue(result->slice(), result->size());
}

/// @brief function DATE_ADD
AqlValue functions::DateAdd(ExpressionContext* expressionContext,
                            AstNode const&,
                            VPackFunctionParametersView parameters) {
  static char const* AFN = "DATE_ADD";
  tp_sys_clock_ms tp;

  if (!parameterToTimePoint(expressionContext, parameters, tp, AFN, 0)) {
    return AqlValue(AqlValueHintNull());
  }

  // size == 4 unit / unit type / timezone
  // size == 3 unit / unit type
  // size == 3 iso duration / timezone
  // size == 2 iso duration

  AqlValue const& parameter1 =
      aql::functions::extractFunctionParameterValue(parameters, 1);

  auto const isTimezoned = parameters.size() == 4 ||
                           (parameters.size() == 3 && parameter1.isString());
  auto const timezoneParameterIndex = parameters.size() == 4 ? 3 : 2;
  std::string inputTimezone = "UTC";

  if (isTimezoned) {
    AqlValue const timezoneParam =
        aql::functions::extractFunctionParameterValue(parameters,
                                                      timezoneParameterIndex);

    if (!timezoneParam.isString()) {  // timezone type must be string
      registerInvalidArgumentWarning(expressionContext, AFN);
      return AqlValue(AqlValueHintNull());
    }

    inputTimezone = timezoneParam.slice().copyString();

    localizeTimePoint(inputTimezone, tp);
  }

  if ((parameters.size() == 3 && !isTimezoned) || parameters.size() == 4) {
    AqlValue const& durationUnit =
        aql::functions::extractFunctionParameterValue(parameters, 1);
    if (!durationUnit.isNumber()) {  // unit must be number
      registerInvalidArgumentWarning(expressionContext, AFN);
      return AqlValue(AqlValueHintNull());
    }

    AqlValue const& durationType =
        aql::functions::extractFunctionParameterValue(parameters, 2);
    if (!durationType.isString()) {  // unit type must be string
      registerInvalidArgumentWarning(expressionContext, AFN);
      return AqlValue(AqlValueHintNull());
    }

    // Numbers and Strings can both be sliced
    return addOrSubtractUnitFromTimestamp(
        expressionContext, tp, durationUnit.slice(), durationType.slice(), AFN,
        false, inputTimezone);
  } else {  // iso duration
    AqlValue const& isoDuration =
        aql::functions::extractFunctionParameterValue(parameters, 1);
    if (!isoDuration.isString()) {
      registerInvalidArgumentWarning(expressionContext, AFN);
      return AqlValue(AqlValueHintNull());
    }

    return addOrSubtractIsoDurationFromTimestamp(
        expressionContext, tp, isoDuration.slice().stringView(), AFN, false,
        inputTimezone);
  }
}

/// @brief function DATE_SUBTRACT
AqlValue functions::DateSubtract(ExpressionContext* expressionContext,
                                 AstNode const&,
                                 VPackFunctionParametersView parameters) {
  static char const* AFN = "DATE_SUBTRACT";
  tp_sys_clock_ms tp;

  if (!parameterToTimePoint(expressionContext, parameters, tp, AFN, 0)) {
    return AqlValue(AqlValueHintNull());
  }

  // size == 4 unit / unit type / timezone
  // size == 3 unit / unit type
  // size == 3 iso duration / timezone
  // size == 2 iso duration

  AqlValue const& parameter1 =
      aql::functions::extractFunctionParameterValue(parameters, 1);

  auto const isTimezoned = parameters.size() == 4 ||
                           (parameters.size() == 3 && parameter1.isString());
  auto const timezoneParameterIndex = parameters.size() == 4 ? 3 : 2;
  std::string inputTimezone = "UTC";

  if (isTimezoned) {
    AqlValue const timezoneParam =
        aql::functions::extractFunctionParameterValue(parameters,
                                                      timezoneParameterIndex);

    if (!timezoneParam.isString()) {  // timezone type must be string
      registerInvalidArgumentWarning(expressionContext, AFN);
      return AqlValue(AqlValueHintNull());
    }

    inputTimezone = timezoneParam.slice().copyString();

    localizeTimePoint(inputTimezone, tp);
  }

  if ((parameters.size() == 3 && !isTimezoned) || parameters.size() == 4) {
    AqlValue const& durationUnit =
        aql::functions::extractFunctionParameterValue(parameters, 1);
    if (!durationUnit.isNumber()) {  // unit must be number
      registerInvalidArgumentWarning(expressionContext, AFN);
      return AqlValue(AqlValueHintNull());
    }

    AqlValue const& durationType =
        aql::functions::extractFunctionParameterValue(parameters, 2);
    if (!durationType.isString()) {  // unit type must be string
      registerInvalidArgumentWarning(expressionContext, AFN);
      return AqlValue(AqlValueHintNull());
    }

    // Numbers and Strings can both be sliced
    return addOrSubtractUnitFromTimestamp(
        expressionContext, tp, durationUnit.slice(), durationType.slice(), AFN,
        true, inputTimezone);
  } else {  // iso duration
    AqlValue const& isoDuration =
        aql::functions::extractFunctionParameterValue(parameters, 1);
    if (!isoDuration.isString()) {
      registerInvalidArgumentWarning(expressionContext, AFN);
      return AqlValue(AqlValueHintNull());
    }

    return addOrSubtractIsoDurationFromTimestamp(
        expressionContext, tp, isoDuration.slice().stringView(), AFN, true,
        inputTimezone);
  }
}

/// @brief function DATE_DIFF
AqlValue functions::DateDiff(ExpressionContext* expressionContext,
                             AstNode const&,
                             VPackFunctionParametersView parameters) {
  static char const* AFN = "DATE_DIFF";
  // Extract first date
  tp_sys_clock_ms tp1;
  if (!parameterToTimePoint(expressionContext, parameters, tp1, AFN, 0)) {
    return AqlValue(AqlValueHintNull());
  }

  // Extract second date
  tp_sys_clock_ms tp2;
  if (!parameterToTimePoint(expressionContext, parameters, tp2, AFN, 1)) {
    return AqlValue(AqlValueHintNull());
  }

  double diff = 0.0;
  bool asFloat = false;

  AqlValue const& unitValue =
      aql::functions::extractFunctionParameterValue(parameters, 2);
  if (!unitValue.isString()) {
    registerInvalidArgumentWarning(expressionContext, AFN);
    return AqlValue(AqlValueHintNull());
  }

  DateSelectionModifier flag = parseDateModifierFlag(unitValue.slice());

  if (parameters.size() > 3) {
    AqlValue const& parameter4 =
        aql::functions::extractFunctionParameterValue(parameters, 3);
    if (parameter4.isBoolean()) {
      asFloat = parameter4.toBoolean();
    } else if (parameter4.isString()) {
      std::string inputTimezone = parameter4.slice().copyString();
      localizeTimePoint(inputTimezone, tp1);
      if (parameters.size() == 4) {
        localizeTimePoint(inputTimezone, tp2);
      }
    } else {
      registerInvalidArgumentWarning(expressionContext, AFN);
      return AqlValue(AqlValueHintNull());
    }

    if (parameters.size() > 4) {
      AqlValue const& parameter5 =
          aql::functions::extractFunctionParameterValue(parameters, 4);

      if (!parameter5.isString()) {  // timezone type must be string
        registerInvalidArgumentWarning(expressionContext, AFN);
        return AqlValue(AqlValueHintNull());
      }

      std::string inputTimezone = parameter5.slice().copyString();
      if (parameter4.isBoolean()) {
        localizeTimePoint(inputTimezone, tp1);
        if (parameters.size() == 5) {
          localizeTimePoint(inputTimezone, tp2);
        }
      } else {
        localizeTimePoint(inputTimezone, tp2);
      }

      if (parameters.size() == 6) {
        AqlValue const& parameter6 =
            aql::functions::extractFunctionParameterValue(parameters, 5);

        if (!parameter6.isString()) {  // timezone type must be string
          registerInvalidArgumentWarning(expressionContext, AFN);
          return AqlValue(AqlValueHintNull());
        }

        inputTimezone = parameter6.slice().copyString();
        localizeTimePoint(inputTimezone, tp2);
      }
    }
  }

  auto diffDuration = tp2 - tp1;

  switch (flag) {
    case YEAR:
      diff = duration_cast<
                 duration<double, std::ratio_multiply<std::ratio<146097, 400>,
                                                      date::days::period>>>(
                 diffDuration)
                 .count();
      break;
    case MONTH:
      diff = duration_cast<duration<
          double, std::ratio_divide<date::years::period, std::ratio<12>>>>(
                 diffDuration)
                 .count();
      break;
    case WEEK:
      diff = duration_cast<duration<
          double, std::ratio_multiply<std::ratio<7>, date::days::period>>>(
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
      diff = duration_cast<duration<double, std::ratio<3600>>>(diffDuration)
                 .count();
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
      registerWarning(expressionContext, AFN,
                      TRI_ERROR_QUERY_INVALID_DATE_VALUE);
      return AqlValue(AqlValueHintNull());
  }

  if (asFloat) {
    return AqlValue(AqlValueHintDouble(diff));
  }
  return AqlValue(AqlValueHintInt(static_cast<int64_t>(std::round(diff))));
}

/// @brief function DATE_COMPARE
AqlValue functions::DateCompare(ExpressionContext* expressionContext,
                                AstNode const&,
                                VPackFunctionParametersView parameters) {
  static char const* AFN = "DATE_COMPARE";
  tp_sys_clock_ms tp1;
  if (!parameterToTimePoint(expressionContext, parameters, tp1, AFN, 0)) {
    return AqlValue(AqlValueHintNull());
  }

  tp_sys_clock_ms tp2;
  if (!parameterToTimePoint(expressionContext, parameters, tp2, AFN, 1)) {
    return AqlValue(AqlValueHintNull());
  }

  AqlValue const& rangeStartValue =
      aql::functions::extractFunctionParameterValue(parameters, 2);

  DateSelectionModifier rangeStart =
      parseDateModifierFlag(rangeStartValue.slice());

  if (rangeStart == INVALID) {
    registerWarning(expressionContext, AFN,
                    TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH);
    return AqlValue(AqlValueHintNull());
  }

  DateSelectionModifier rangeEnd = rangeStart;
  if (parameters.size() > 3) {
    AqlValue const& rangeEndValue =
        aql::functions::extractFunctionParameterValue(parameters, 3);
    rangeEnd = parseDateModifierFlag(rangeEndValue.slice());

    auto startTimezoneIndex = 0;
    auto endTimezoneIndex = 0;

    if (rangeEnd == INVALID) {
      startTimezoneIndex = 3;
      rangeEnd = rangeStart;
      if (parameters.size() == 4) {
        endTimezoneIndex = 3;
      } else if (parameters.size() == 5) {
        endTimezoneIndex = 4;
      } else {
        registerWarning(expressionContext, AFN,
                        TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH);
        return AqlValue(AqlValueHintNull());
      }
    } else if (parameters.size() > 4) {
      startTimezoneIndex = 4;
      if (parameters.size() == 5) {
        endTimezoneIndex = 4;
      } else if (parameters.size() == 6) {
        endTimezoneIndex = 5;
      }
    }

    if (startTimezoneIndex > 0) {
      AqlValue const startTimezoneParam =
          aql::functions::extractFunctionParameterValue(parameters,
                                                        startTimezoneIndex);

      if (!startTimezoneParam.isString()) {  // timezone type must be string
        registerInvalidArgumentWarning(expressionContext, AFN);
        return AqlValue(AqlValueHintNull());
      }

      std::string startTimezone = startTimezoneParam.slice().copyString();

      localizeTimePoint(startTimezone, tp1);

      if (startTimezoneIndex != endTimezoneIndex) {
        AqlValue const endTimezoneParam =
            aql::functions::extractFunctionParameterValue(parameters,
                                                          endTimezoneIndex);

        if (!endTimezoneParam.isString()) {  // timezone type must be string
          registerInvalidArgumentWarning(expressionContext, AFN);
          return AqlValue(AqlValueHintNull());
        }

        std::string endTimezone = endTimezoneParam.slice().copyString();

        localizeTimePoint(endTimezone, tp2);
      } else {
        localizeTimePoint(startTimezone, tp2);
      }
    }
  }

  auto ymd1 = date::year_month_day{floor<date::days>(tp1)};
  auto ymd2 = date::year_month_day{floor<date::days>(tp2)};
  auto time1 = date::make_time(tp1 - floor<date::days>(tp1));
  auto time2 = date::make_time(tp2 - floor<date::days>(tp2));

  // This switch has the following feature:
  // It is ordered by the Highest value of
  // the Modifier (YEAR) and flows down to
  // lower values.
  // In each case if the value is significant
  // (above or equal the endRange) we compare it.
  // If this part is not equal we return false.
  // Otherwise, we fall down to the next part.
  // As soon as we are below the endRange
  // we bail out.
  // So all fall throughs here are intentional
  switch (rangeStart) {
    case YEAR:
      // Always check for the year
      if (ymd1.year() != ymd2.year()) {
        return AqlValue(AqlValueHintBool(false));
      }
      [[fallthrough]];
    case MONTH:
      if (rangeEnd > MONTH) {
        break;
      }
      if (ymd1.month() != ymd2.month()) {
        return AqlValue(AqlValueHintBool(false));
      }
      [[fallthrough]];
    case DAY:
      if (rangeEnd > DAY) {
        break;
      }
      if (ymd1.day() != ymd2.day()) {
        return AqlValue(AqlValueHintBool(false));
      }
      [[fallthrough]];
    case HOUR:
      if (rangeEnd > HOUR) {
        break;
      }
      if (time1.hours() != time2.hours()) {
        return AqlValue(AqlValueHintBool(false));
      }
      [[fallthrough]];
    case MINUTE:
      if (rangeEnd > MINUTE) {
        break;
      }
      if (time1.minutes() != time2.minutes()) {
        return AqlValue(AqlValueHintBool(false));
      }
      [[fallthrough]];
    case SECOND:
      if (rangeEnd > SECOND) {
        break;
      }
      if (time1.seconds() != time2.seconds()) {
        return AqlValue(AqlValueHintBool(false));
      }
      [[fallthrough]];
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

/// @brief function DATE_ROUND
AqlValue functions::DateRound(ExpressionContext* expressionContext,
                              AstNode const&,
                              VPackFunctionParametersView parameters) {
  static char const* AFN = "DATE_ROUND";
  tp_sys_clock_ms tp;

  if (!parameterToTimePoint(expressionContext, parameters, tp, AFN, 0)) {
    return AqlValue(AqlValueHintNull());
  }

  AqlValue const& durationUnit =
      aql::functions::extractFunctionParameterValue(parameters, 1);
  if (!durationUnit.isNumber()) {  // unit must be number
    registerInvalidArgumentWarning(expressionContext, AFN);
    return AqlValue(AqlValueHintNull());
  }

  AqlValue const& durationType =
      aql::functions::extractFunctionParameterValue(parameters, 2);
  if (!durationType.isString()) {  // unit type must be string
    registerInvalidArgumentWarning(expressionContext, AFN);
    return AqlValue(AqlValueHintNull());
  }

  std::string inputTimezone = "UTC";

  if (parameters.size() == 4) {
    AqlValue const timezoneParam =
        aql::functions::extractFunctionParameterValue(parameters, 3);

    if (!timezoneParam.isString()) {  // timezone type must be string
      registerInvalidArgumentWarning(expressionContext, AFN);
      return AqlValue(AqlValueHintNull());
    }

    inputTimezone = timezoneParam.slice().copyString();

    localizeTimePoint(inputTimezone, tp);
  }

  int64_t const m = durationUnit.toInt64();
  if (m <= 0) {
    registerInvalidArgumentWarning(expressionContext, AFN);
    return AqlValue(AqlValueHintNull());
  }

  std::string_view s = durationType.slice().stringView();

  int64_t factor = 1;
  if (s == "milliseconds" || s == "millisecond" || s == "f") {
    factor = 1;
  } else if (s == "seconds" || s == "second" || s == "s") {
    factor = 1000;
  } else if (s == "minutes" || s == "minute" || s == "i") {
    factor = 60 * 1000;
  } else if (s == "hours" || s == "hour" || s == "h") {
    factor = 60 * 60 * 1000;
  } else if (s == "days" || s == "day" || s == "d") {
    factor = 24 * 60 * 60 * 1000;
  } else {
    registerInvalidArgumentWarning(expressionContext, AFN);
    return AqlValue(AqlValueHintNull());
  }

  int64_t const multiplier = factor * m;

  duration<int64_t, std::milli> time = tp.time_since_epoch();
  int64_t t = time.count();
  // integer division!
  t /= multiplier;
  tp = tp_sys_clock_ms(milliseconds(t * multiplier));

  if (parameters.size() == 4) {
    unlocalizeTimePoint(inputTimezone, tp);
  }

  return timeAqlValue(expressionContext, AFN, tp);
}

/// @brief function DATE_FORMAT
AqlValue functions::DateFormat(ExpressionContext* expressionContext,
                               AstNode const&,
                               VPackFunctionParametersView parameters) {
  static char const* AFN = "DATE_FORMAT";
  tp_sys_clock_ms tp;

  if (!parameterToTimePoint(expressionContext, parameters, tp, AFN, 0)) {
    return AqlValue(AqlValueHintNull());
  }

  AqlValue const& aqlFormatString =
      aql::functions::extractFunctionParameterValue(parameters, 1);
  if (!aqlFormatString.isString()) {
    registerInvalidArgumentWarning(expressionContext, AFN);
    return AqlValue(AqlValueHintNull());
  }

  if (parameters.size() == 3) {
    AqlValue const timezoneParam =
        aql::functions::extractFunctionParameterValue(parameters, 2);

    if (!timezoneParam.isString()) {  // timezone type must be string
      registerInvalidArgumentWarning(expressionContext, AFN);
      return AqlValue(AqlValueHintNull());
    }

    std::string inputTimezone = timezoneParam.slice().copyString();

    localizeTimePoint(inputTimezone, tp);
  }

  return AqlValue(basics::formatDate(aqlFormatString.slice().copyString(), tp));
}

}  // namespace arangodb::aql
