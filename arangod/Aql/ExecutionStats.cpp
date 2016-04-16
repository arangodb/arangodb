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

#include "Aql/ExecutionStats.h"
#include "Basics/Exceptions.h"

#include <velocypack/Builder.h>
#include <velocypack/Value.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb::aql;

/// @brief convert the statistics to VelocyPack
void ExecutionStats::toVelocyPack(VPackBuilder& builder) const {
  builder.openObject();
  builder.add("writesExecuted", VPackValue(writesExecuted));
  builder.add("writesIgnored", VPackValue(writesIgnored));
  builder.add("scannedFull", VPackValue(scannedFull));
  builder.add("scannedIndex", VPackValue(scannedIndex));
  builder.add("filtered", VPackValue(filtered));

  if (fullCount > -1) {
    // fullCount is exceptional. it has a default value of -1 and is
    // not reported with this value
    builder.add("fullCount", VPackValue(fullCount));
  }
      
  builder.add("executionTime", VPackValue(executionTime));
  builder.close();
}

void ExecutionStats::toVelocyPackStatic(VPackBuilder& builder) {
  builder.openObject();
  builder.add("writesExecuted", VPackValue(0));
  builder.add("writesIgnored", VPackValue(0));
  builder.add("scannedFull", VPackValue(0));
  builder.add("scannedIndex", VPackValue(0));
  builder.add("filtered", VPackValue(0));
  builder.add("fullCount", VPackValue(-1));
  builder.add("executionTime", VPackValue(0.0));
  builder.close();
}

ExecutionStats::ExecutionStats()
    : writesExecuted(0),
      writesIgnored(0),
      scannedFull(0),
      scannedIndex(0),
      filtered(0),
      fullCount(-1),
      executionTime(0.0) {}

ExecutionStats::ExecutionStats(VPackSlice const& slice) {
  if (!slice.isObject()) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                   "stats is not an object");
  }

  writesExecuted = slice.get("writesExecuted").getNumber<int64_t>();
  writesIgnored = slice.get("writesIgnored").getNumber<int64_t>();
  scannedFull = slice.get("scannedFull").getNumber<int64_t>();
  scannedIndex = slice.get("scannedIndex").getNumber<int64_t>();
  filtered = slice.get("filtered").getNumber<int64_t>();

  // note: fullCount is an optional attribute!
  if (slice.hasKey("fullCount")) {
    fullCount = slice.get("fullCount").getNumber<int64_t>();
  }
  else {
    fullCount = -1;
  }

  // intentionally no modification of executionTime
}
