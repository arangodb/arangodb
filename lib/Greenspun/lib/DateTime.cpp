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

#include "DateTime.h"
#include "Greenspun/Extractor.h"
#include "Greenspun/Interpreter.h"
#include "Greenspun/Primitives.h"

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
