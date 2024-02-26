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
/// @author Michael Hackstein
////////////////////////////////////////////////////////////////////////////////

#pragma once

#ifdef ARANGODB_ENABLE_MAINTAINER_MODE

#include <any>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <optional>
#include <vector>

namespace arangodb {

namespace application_features {
class ApplicationServer;
}

namespace basics {

class DebugRaceController {
 public:
  static DebugRaceController& sharedInstance();

  DebugRaceController() = default;

  // Reset the stored state here, will free the stored data.
  void reset();

  // Caller is required to COPY the data to store here.
  // Otherwise, a concurrent thread might try to read it,
  // after the caller has freed the memory.
  auto waitForOthers(
      std::size_t numberOfThreadsToWaitFor, std::any myData,
      arangodb::application_features::ApplicationServer const& server)
      -> std::optional<std::vector<std::any>>;

 private:
  bool didTrigger(std::unique_lock<std::mutex> const& guard,
                  std::size_t numberOfThreadsToWaitFor) const;

 private:
  std::mutex mutable _mutex{};
  std::condition_variable _condVariable{};
  std::vector<std::any> _data{};
};
}  // namespace basics
}  // namespace arangodb
// This are used to synchronize parallel locking of two shards
// in our tests. Do NOT include in production.

#endif
