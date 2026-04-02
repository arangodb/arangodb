////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2026 ArangoDB GmbH, Cologne, Germany
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
/// @author Koichi Nakata
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Rest/CommonDefines.h"

#include <cstdint>
#include <chrono>

namespace arangodb {
namespace application_features {
class ApplicationServer;
}

// Timing data struct carried out on CommTask (per message id)
// and handed to RestHander during execution.
struct RequestTimingData {
  using clock = std::chrono::steady_clock;
  using time_point = std::chrono::time_point<clock>;

  time_point readStart{};
  time_point readEnd{};
  time_point queueStart{};
  time_point queueEnd{};
  int64_t queueSize = 0;
  time_point requestStart{};
  time_point requestEnd{};
  time_point writeStart{};
  time_point writeEnd{};
  double receivedBytes = 0.0;
  double sentBytes = 0.0;
  rest::RequestType requestType = rest::RequestType::ILLEGAL;
  bool async = false;
  bool superuser = false;

  double elapsedSinceReadStart() const noexcept {
    if (readStart != time_point{}) {
      return toSeconds(clock::now() - readStart);
    }
    return 0.0;
  }

  double elapsedWhileQueued() const noexcept {
    return toSeconds(queueEnd - queueStart);
  }

  static double toSeconds(clock::duration d) noexcept {
    return std::chrono::duration<double>(d).count();
  }

  static time_point now() noexcept { return clock::now(); }
};

// @brief finalize timing data
// We need this free function so both RestHanlder and CommTask can call it.
void finalizeTimingData(application_features::ApplicationServer& server,
                        RequestTimingData& data);

}  // namespace arangodb