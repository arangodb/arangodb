////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2023 ArangoDB GmbH, Cologne, Germany
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

#pragma once

#include "Metrics/Fwd.h"
#include "Scheduler/Scheduler.h"
#include "RestServer/arangod.h"

#include <mutex>

namespace arangodb::transaction {

class Manager;

class ManagerFeature final : public ArangodFeature {
 public:
  static constexpr std::string_view name() noexcept {
    return "TransactionManager";
  }

  explicit ManagerFeature(Server& server);

  void collectOptions(
      std::shared_ptr<arangodb::options::ProgramOptions> options) override;
  void prepare() override;
  void start() override;
  void stop() override;
  void initiateSoftShutdown() override;
  void beginShutdown() override;
  void unprepare() override;

  double streamingLockTimeout() const noexcept { return _streamingLockTimeout; }

  double streamingIdleTimeout() const noexcept { return _streamingIdleTimeout; }

  static transaction::Manager* manager() noexcept { return MANAGER.get(); }

  /// @brief track number of aborted managed transactions
  void trackExpired(uint64_t numExpired) noexcept;

 private:
  void queueGarbageCollection();

  static constexpr double defaultStreamingIdleTimeout = 60.0;
  static constexpr double maxStreamingIdleTimeout = 120.0;

  static std::unique_ptr<transaction::Manager> MANAGER;

  std::mutex _workItemMutex;
  Scheduler::WorkHandle _workItem;

  // where rhythm is life, and life is rhythm :)
  std::function<void(bool)> _gcfunc;

  // lock time in seconds
  double _streamingLockTimeout;

  /// @brief idle timeout for streaming transactions, in seconds
  double _streamingIdleTimeout;

  /// @brief number of expired transactions that were aborted by
  /// transaction garbage collection
  metrics::Counter& _numExpiredTransactions;
};

}  // namespace arangodb::transaction
