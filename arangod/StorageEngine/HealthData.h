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

#ifndef ARANGOD_STORAGE_ENGINE_HEALTH_DATA_H
#define ARANGOD_STORAGE_ENGINE_HEALTH_DATA_H 1

#include "Basics/Result.h"

#include <chrono>

namespace arangodb {
namespace velocypack {
class Builder;
class Slice;
}
  
struct HealthData {
  Result res;
  /// @brief timestamp of full last health check execution. we only execute the
  /// full health checks every so often in order to reduce load
  std::chrono::steady_clock::time_point lastCheckTimestamp;
  bool backgroundError = false;
  uint64_t freeDiskSpaceBytes = 0;
  double freeDiskSpacePercent = 0.0;

  static HealthData fromVelocyPack(velocypack::Slice slice);
  void toVelocyPack(velocypack::Builder& builder) const;
};

}

#endif
