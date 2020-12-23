////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2020-2020 ArangoDB GmbH, Cologne, Germany
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
/// @author Michael Hackstein
////////////////////////////////////////////////////////////////////////////////

#ifndef BASICS_DEBUG_RACE_CONTROLLER_H
#define BASICS_DEBUG_RACE_CONTROLLER_H 1

#ifdef ARANGODB_ENABLE_MAINTAINER_MODE

#include <any>
#include <condition_variable>
#include <mutex>
#include <vector>

namespace arangodb {

namespace application_features {
class ApplicationServer;
}

namespace basics {

class DebugRaceController {
 public:
  static DebugRaceController& sharedInstance();

  DebugRaceController();

  // Reset the sotread state here, will free the stored data
  // and remove the didTrigger flag
  void reset();

  // Test if this RaceController did trigger once.
  // It can be de-activated after the first waiting
  bool didTrigger() const;

  // Access the data stored by waiting threads.
  std::vector<std::any> data() const;

  // Caller is required to COPY the data to store here.
  // Otherwise a concurrent thread might try to read it,
  // after the caller has freed the memory.
  void waitForOthers(size_t numberOfThreadsToWaitFor, std::any myData,
                     arangodb::application_features::ApplicationServer const& server);

 private:
  bool _didTrigger{false};
  mutable std::mutex _mutex{};
  std::vector<std::any> _data{};
  std::condition_variable _condVariable{};
};
}  // namespace basics
}  // namespace arangodb
// This are used to synchronize parallel locking of two shards
// in our tests. Do NOT include in production.

#endif

#endif
