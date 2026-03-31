////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
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
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Rest/CommonDefines.h"
#include "Statistics/StatisticsFeature.h"

#include <cstdint>

namespace arangodb {

// Plain timing data struct carried out on CommTask (per message id)
// and handed to RestHander during execution.
struct RequestTimingData {
  double readStart = 0.0;
  double readEnd = 0.0;
  double queueStart = 0.0;
  double queueEnd = 0.0;
  int64_t queueSize = 0;
  double requestStart = 0.0;
  double requestEnd = 0.0;
  double writeStart = 0.0;
  double writeEnd = 0.0;
  double receivedBytes = 0.0;
  double sentBytes = 0.0;
  rest::RequestType requestType = rest::RequestType::ILLEGAL;
  bool async = false;
  bool superuser = false;

  // When false, all timing is skipped (--server.statistics=false)
  bool active = false;

  double elapsedSinceReadStart() const {
    if (active && readStart != 0.0) {
      return StatisticsFeature::time() - readStart;
    }
    return 0.0;
  }

  double elapsedWhileQueued() const noexcept {
    if (active) {
      return queueEnd - queueStart;
    }
    return 0.0;
  }
};

}  // namespace arangodb