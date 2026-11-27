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

#include "Aql/ExecutionNode/WindowBounds.h"

#include "Aql/QueryWarnings.h"
#include "Basics/Exceptions.h"
#include "Basics/ScopeGuard.h"

#include "velocypack/Builder.h"

namespace arangodb::aql {

namespace {
bool parameterToTimePoint(AqlValue const& value, QueryWarnings& warnings,
                          tp_sys_clock_ms& tp) {
  if (value.isNumber()) {
    int64_t v = value.toInt64();
    if (ADB_UNLIKELY(v < -62167219200000 || v > 253402300799999)) {
      // check if value is between "0000-01-01T00:00:00.000Z" and
      // "9999-12-31T23:59:59.999Z" -62167219200000: "0000-01-01T00:00:00.000Z"
      // 253402300799999: "9999-12-31T23:59:59.999Z"
      warnings.registerWarning(
          TRI_ERROR_QUERY_INVALID_DATE_VALUE,
          "range value is not a valid timepoint (out of range)");
      return false;
    }
    tp = tp_sys_clock_ms(std::chrono::milliseconds(v));
    return true;
  }
  // TODO is there a way to properly support ISO datestrings ?
  /*else if (value.isString()) {
    if (!basics::parseDateTime(value.slice().stringView(), tp)) {
      q.warnings().registerWarning(TRI_ERROR_QUERY_INVALID_DATE_VALUE,
                                   "range value is not a valid ISO 8601 date
  time string"); return false;
    }
    return true;
  }*/
  warnings.registerWarning(TRI_ERROR_QUERY_INVALID_DATE_VALUE);
  return false;
}

tp_sys_clock_ms addOrSubtractDate(
    tp_sys_clock_ms tp, arangodb::basics::ParsedDuration const& parsed,
    bool isSubtract) {
  std::chrono::year_month_day ymd{std::chrono::floor<std::chrono::days>(tp)};
  auto day_time = std::chrono::hh_mm_ss(tp - std::chrono::sys_days(ymd));

  if (isSubtract) {
    ymd -= std::chrono::years{parsed.years};
  } else {
    ymd += std::chrono::years{parsed.years};
  }

  if (isSubtract) {
    ymd -= std::chrono::months{parsed.months};
  } else {
    ymd += std::chrono::months{parsed.months};
  }

  std::chrono::milliseconds ms{0};
  ms += std::chrono::weeks{parsed.weeks};
  ms += std::chrono::days{parsed.days};
  ms += std::chrono::hours{parsed.hours};
  ms += std::chrono::minutes{parsed.minutes};
  ms += std::chrono::seconds{parsed.seconds};
  ms += std::chrono::milliseconds{parsed.milliseconds};

  if (isSubtract) {
    return tp_sys_clock_ms{std::chrono::sys_days(ymd) + day_time.to_duration() -
                           ms};
  } else {
    return tp_sys_clock_ms{std::chrono::sys_days(ymd) + day_time.to_duration() +
                           ms};
  }
}
}  // namespace

WindowBounds::WindowBounds(Type type, AqlValue&& preceding,
                           AqlValue&& following)
    : _type(type) {
  auto g = scopeGuard([&]() noexcept {
    preceding.destroy();
    following.destroy();
  });

  if (Type::Row == type) {
    auto validate = [&](AqlValue& val) -> int64_t {
      if (val.isNumber()) {
        int64_t v = val.toInt64();
        if (v >= 0) {
          return v;
        }
      } else if (val.isString() && (val.slice().isEqualString("unbounded") ||
                                    val.slice().isEqualString("inf"))) {
        return std::numeric_limits<int64_t>::max();
      } else if (val.isNone()) {
        return 0;
      }
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_BAD_PARAMETER,
                                     "WINDOW row spec is invalid; bounds must "
                                     "be positive integers or \"unbounded\"");
    };
    _numPrecedingRows = validate(preceding);
    _numFollowingRows = validate(following);
    return;
  }

  if (Type::Range == type) {
    auto checkType = [](AqlValue const& val) {
      if (!val.isString() && !val.isNumber() && !val.isNone()) {
        THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_BAD_PARAMETER,
                                       "WINDOW range spec is invalid");
      }
    };
    checkType(preceding);
    checkType(following);

    if ((preceding.isNone() == following.isNone()) &&
        (preceding.isString() != following.isString())) {
      THROW_ARANGO_EXCEPTION_MESSAGE(
          TRI_ERROR_BAD_PARAMETER,
          "WINDOW range spec is invalid; bounds must be of the same type - "
          "either both are numeric values, or both are ISO 8601 duration "
          "strings");
    }

    if (preceding.isString() || following.isString()) {
      _rangeType = RangeType::Date;
      if (preceding.isString()) {
        if (!basics::parseIsoDuration(preceding.slice().stringView(),
                                      _precedingDuration)) {
          THROW_ARANGO_EXCEPTION_MESSAGE(
              TRI_ERROR_BAD_PARAMETER,
              "WINDOW range spec is invalid; 'preceding' is not a "
              "valid ISO 8601 duration string");
        }
      } else {
        TRI_ASSERT(preceding.isNone());
      }

      if (following.isString()) {
        if (!basics::parseIsoDuration(following.slice().stringView(),
                                      _followingDuration)) {
          THROW_ARANGO_EXCEPTION_MESSAGE(
              TRI_ERROR_BAD_PARAMETER,
              "WINDOW range spec is invalid; 'following' is not a "
              "valid ISO 8601 duration string");
        }
      } else {
        TRI_ASSERT(following.isNone());
      }
      return;
    }

    _rangeType = RangeType::Numeric;
    _precedingNumber = preceding.toDouble();
    _followingNumber = following.toDouble();
  }
}

WindowBounds::WindowBounds(Type t, VPackSlice slice)
    : WindowBounds(t, AqlValue(slice.get("preceding")),
                   AqlValue(slice.get("following"))) {}

WindowBounds::~WindowBounds() = default;

int64_t WindowBounds::numPrecedingRows() const {
  TRI_ASSERT(_type == Type::Row);
  return _numPrecedingRows;
}

int64_t WindowBounds::numFollowingRows() const {
  TRI_ASSERT(_type == Type::Row);
  return _numFollowingRows;
}

bool WindowBounds::unboundedPreceding() const {
  return _type == Type::Row &&
         _numPrecedingRows == std::numeric_limits<int64_t>::max() &&
         _numFollowingRows == 0;
}

bool WindowBounds::needsFollowingRows() const {
  if (_type == Type::Row) {
    return _numPrecedingRows == std::numeric_limits<int64_t>::max() &&
           _numFollowingRows == 0;
  }
  TRI_ASSERT(_type == Type::Range);
  if (_rangeType == RangeType::Date) {
    return _followingDuration.years > 0 || _followingDuration.months > 0 ||
           _followingDuration.days > 0 || _followingDuration.hours > 0 ||
           _followingDuration.minutes > 0 || _followingDuration.seconds > 0 ||
           _followingDuration.milliseconds > 0;
  }
  TRI_ASSERT(_rangeType == RangeType::Numeric);
  return _followingNumber > 0.0;
}

WindowBounds::Row WindowBounds::calcRow(AqlValue const& input,
                                        QueryWarnings& w) const {
  TRI_ASSERT(_type == Type::Range);

  if (_rangeType == RangeType::Date) {
    tp_sys_clock_ms tp;
    if (!parameterToTimePoint(input, w, tp)) {
      return {0, 0, 0, /*valid*/ false};
    }

    auto lowerTP = addOrSubtractDate(tp, _precedingDuration, /*subtract*/ true);
    auto upperTP =
        addOrSubtractDate(tp, _followingDuration, /*subtract*/ false);

    auto val = std::chrono::duration<double>(tp.time_since_epoch()).count();
    auto low =
        std::chrono::duration<double>(lowerTP.time_since_epoch()).count();
    auto upper =
        std::chrono::duration<double>(upperTP.time_since_epoch()).count();

    return {val, low, upper, /*valid*/ true};
  }

  TRI_ASSERT(_rangeType == RangeType::Numeric);

  bool failed;
  double val = input.toDouble(failed);
  if (failed) {
    w.registerWarning(TRI_ERROR_QUERY_INVALID_ARITHMETIC_VALUE);
    return {0, 0, 0, /*valid*/ false};
  }

  return {val, val - _precedingNumber, val + _followingNumber, /*valid*/ true};
}

void WindowBounds::toVelocyPack(VPackBuilder& b) const {
  switch (_type) {
    case Type::Row: {
      auto translate = [](int64_t v) {
        if (v == std::numeric_limits<int64_t>::max()) {
          return VPackValue("unbounded");
        }
        return VPackValue(v);
      };
      b.add("preceding", translate(_numPrecedingRows));
      b.add("following", translate(_numFollowingRows));
      break;
    }
    case Type::Range:
      if (_rangeType == RangeType::Numeric) {
        b.add("preceding", VPackValue(_precedingNumber));
        b.add("following", VPackValue(_followingNumber));
      } else {
        auto makeDuration = [](basics::ParsedDuration const& duration) {
          std::string result = "P";
          auto append = [&result](int v, char suffix) {
            if (v != 0) {
              result.append(std::to_string(v)).push_back(suffix);
            }
          };
          append(duration.years, 'Y');
          append(duration.months, 'M');
          append(duration.weeks, 'W');
          append(duration.days, 'D');
          if (duration.hours != 0 || duration.minutes != 0 ||
              duration.seconds != 0 || duration.milliseconds != 0) {
            result.push_back('T');
            append(duration.hours, 'H');
            append(duration.minutes, 'M');
            if (duration.milliseconds == 0) {
              append(duration.seconds, 'S');
            } else {
              result.append(std::to_string(duration.seconds)).push_back('.');
              auto ms = std::to_string(duration.milliseconds);
              // parseIsoDuration already limits the number of decimals in
              // milliseconds
              TRI_ASSERT(ms.size() <= 3);
              result.append(3 - ms.size(), '0').append(ms).push_back('S');
            }
          }
          if (result == "P") {
            // we have an "empty" duration
            // -> create the shortest ISO string representing such
            result.append("0D");
          }
          return result;
        };
        b.add("preceding", VPackValue(makeDuration(_precedingDuration)));
        b.add("following", VPackValue(makeDuration(_followingDuration)));
      }
      break;
  }
}

}  // namespace arangodb::aql
