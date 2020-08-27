////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2020 ArangoDB GmbH, Cologne, Germany
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
/// @author Heiko Kernbach
/// @author Lars Maier
/// @author Markus Pfeiffer
////////////////////////////////////////////////////////////////////////////////

#include <Basics/VelocyPackHelper.h>
#include <Basics/datetime.h>
#include <velocypack/Collection.h>
#include <velocypack/Iterator.h>
#include <velocypack/velocypack-aliases.h>

#include <iostream>

#include "../Extractor.h"
#include "../Interpreter.h"
#include "../Primitives.h"
#include "DateTime.h"


/*  WARNING! HACK HACK HACK HACK

    This is a *horrendous* hack to provide a demo prototype of how to
    provide date functions inside greenspun (a purported universal extension
    language to arangodb); These function should then be used (and usable)
    inside AQL (and elsewhere).

    They could (and SHOULD!) really be implemented in the greenspun langauge (or
    one of its better syntactic representations), as its better modifiable and
    easier to read. Some extremists might want to implement some backend functions
    in C++ and they can, but should really provide better error messages than
    "well this doesn't work, user, try again (which is what happens at the moment)"

    add({"DATE_TIMESTAMP", ".|.,.,.,.,.,.", flags, &Functions::DateTimestamp});
    add({"DATE_ISO8601", ".|.,.,.,.,.,.", flags, &Functions::DateIso8601});
    add({"DATE_DAYOFWEEK", ".", flags, &Functions::DateDayOfWeek});
    add({"DATE_YEAR", ".", flags, &Functions::DateYear});
    add({"DATE_MONTH", ".", flags, &Functions::DateMonth});
    add({"DATE_DAY", ".", flags, &Functions::DateDay});
    add({"DATE_HOUR", ".", flags, &Functions::DateHour});
    add({"DATE_MINUTE", ".", flags, &Functions::DateMinute});
    add({"DATE_SECOND", ".", flags, &Functions::DateSecond});
    add({"DATE_MILLISECOND", ".", flags, &Functions::DateMillisecond});
    add({"DATE_DAYOFYEAR", ".", flags, &Functions::DateDayOfYear});
    add({"DATE_ISOWEEK", ".", flags, &Functions::DateIsoWeek});
    add({"DATE_LEAPYEAR", ".", flags, &Functions::DateLeapYear});
    add({"DATE_QUARTER", ".", flags, &Functions::DateQuarter});
    add({"DATE_DAYS_IN_MONTH", ".", flags, &Functions::DateDaysInMonth});
    add({"DATE_ADD", ".,.|.", flags, &Functions::DateAdd});
    add({"DATE_SUBTRACT", ".,.|.", flags, &Functions::DateSubtract});
    add({"DATE_DIFF", ".,.,.|.", flags, &Functions::DateDiff});
    add({"DATE_COMPARE", ".,.,.|.", flags, &Functions::DateCompare});
    add({"DATE_FORMAT", ".,.", flags, &Functions::DateFormat});
    add({"DATE_TRUNC", ".,.", flags, &Functions::DateTrunc});
    add({"DATE_ROUND", ".,.,.", flags, &Functions::DateRound});

    // special flags:
    add({"DATE_NOW", "", Function::makeFlags(FF::Deterministic, FF::CanRunOnDBServer),
    &Functions::DateNow});  // deterministic, but not cacheable!
}
*/

namespace arangodb::greenspun {

EvalResult DateTime_dateStringToUnix(Machine& ctx, VPackSlice const paramsList, VPackBuilder& result) {
  if (!paramsList.isArray() || paramsList.length() != 1) {
      return EvalError("expected exactly one string as parameter, found: " + paramsList.toJson());
  }

  auto dateString = paramsList.at(0);
  if (!dateString.isString()) {
    return EvalError("expected exactly one string as parameter, found: " + dateString.toJson());
  }

  tp_sys_clock_ms tp;
  // Here it shows that implementing this parser ourselves would
  // allow for better error messages from the datetime parser...
  if(!basics::parseDateTime(dateString.stringRef(), tp)) {
    return EvalError("string did not parse as date");
  }

  result.add(VPackValue(tp.time_since_epoch().count()));
  return {};
}

void RegisterAllDateTimeFunctions(Machine& ctx) {
  ctx.setFunction("datestring->unix", DateTime_dateStringToUnix);
}
}
