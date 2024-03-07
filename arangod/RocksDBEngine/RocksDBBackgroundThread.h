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
/// @author Simon Grätzer
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Basics/Common.h"
#include "Basics/ConditionVariable.h"
#include "Basics/Thread.h"
#include "Metrics/Fwd.h"

namespace arangodb {

class RocksDBEngine;

class RocksDBBackgroundThread final : public Thread {
 public:
  RocksDBBackgroundThread(RocksDBEngine& eng, double interval);
  ~RocksDBBackgroundThread();

  void beginShutdown() override;

 protected:
  void run() override;

 private:
  /// @brief engine pointer
  RocksDBEngine& _engine;

  /// @brief interval in which we will run
  double const _interval;

  /// @brief condition variable for heartbeat
  arangodb::basics::ConditionVariable _condition;

  metrics::Gauge<uint64_t>& _metricsWalReleasedTickReplication;
};
}  // namespace arangodb
