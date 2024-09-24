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
/// @author Max Neunhoeffer
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Basics/Thread.h"
#include "Metrics/Fwd.h"
#include "Metrics/LogScale.h"
#include "RestServer/arangod.h"

#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <mutex>

namespace arangodb {

class IOHeartbeatThread final : public ServerThread<ArangodServer> {
 public:
  IOHeartbeatThread(IOHeartbeatThread const&) = delete;
  IOHeartbeatThread& operator=(IOHeartbeatThread const&) = delete;

  explicit IOHeartbeatThread(Server&, metrics::MetricsFeature& metricsFeature,
                             DatabasePathFeature& databasePathFeature);
  ~IOHeartbeatThread();

  void run() override;
  void wakeup() {
    std::lock_guard<std::mutex> guard(_mutex);
    _cv.notify_one();
  }

 private:
  // how long will the thread pause between iterations, in case of trouble:
  static constexpr std::chrono::duration<int64_t> checkIntervalTrouble =
      std::chrono::seconds(1);
  // how long will the thread pause between iterations:
  static constexpr std::chrono::duration<int64_t> checkIntervalNormal =
      std::chrono::seconds(15);

  std::mutex _mutex;
  std::condition_variable _cv;  // for waiting with wakeup

  DatabasePathFeature& _databasePathFeature;
  metrics::Histogram<metrics::LogScale<double>>& _exeTimeHistogram;
  metrics::Counter& _failures;
  metrics::Counter& _delays;
};

}  // namespace arangodb
