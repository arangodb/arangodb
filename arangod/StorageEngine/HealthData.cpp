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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#include "Basics/voc-errors.h"
#include "StorageEngine/HealthData.h"

#include <velocypack/Builder.h>
#include <velocypack/Value.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;

/*static*/ arangodb::HealthData arangodb::HealthData::fromVelocyPack(velocypack::Slice slice) {
  HealthData result;
  if (slice.isObject()) {
    auto code = TRI_ERROR_NO_ERROR;
    std::string message;

    if (VPackSlice status = slice.get("status"); status.isString() && status.isEqualString("BAD")) {
      code = TRI_ERROR_FAILED;
    }
    if (VPackSlice msg = slice.get("message"); msg.isString()) {
      message = msg.copyString();
    }
    result.res.reset(code, message);
  
    if (VPackSlice bg = slice.get("backgroundError"); bg.isBoolean()) {
      result.backgroundError = bg.getBoolean();
    }

    if (VPackSlice fdsb = slice.get("freeDiskSpaceBytes"); fdsb.isNumber()) {
      result.freeDiskSpaceBytes = fdsb.getNumber<decltype(freeDiskSpaceBytes)>();
    }
    
    if (VPackSlice fdsp = slice.get("freeDiskSpacePercent"); fdsp.isNumber()) {
      result.freeDiskSpacePercent = fdsp.getNumber<decltype(freeDiskSpacePercent)>();
    }
  }

  return result;
}

void arangodb::HealthData::toVelocyPack(velocypack::Builder& builder) const {
  builder.add("health", VPackValue(VPackValueType::Object));
  builder.add("status", VPackValue(res.ok() ? "GOOD" : "BAD"));
  if (res.fail()) {
    builder.add("message", VPackValue(res.errorMessage()));
  }
  builder.add("backgroundError", VPackValue(backgroundError));
  builder.add("freeDiskSpaceBytes", VPackValue(freeDiskSpaceBytes));
  builder.add("freeDiskSpacePercent", VPackValue(freeDiskSpacePercent));
  builder.close();
}
