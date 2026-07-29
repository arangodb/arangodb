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
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "ApplicationFeatures/ApplicationFeature.h"
#include "Metrics/Fwd.h"
#include "Transaction/ManagerFeatureOptions.h"
#include "Scheduler/Scheduler.h"

#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>

namespace arangodb::metrics {
struct IRegistry;
}  // namespace arangodb::metrics

namespace arangodb::transaction {
class Manager;

class ManagerFeature final : public application_features::ApplicationFeature {
 public:
  static constexpr std::string_view name() noexcept {
    return "TransactionManager";
  }

  ManagerFeature(application_features::ApplicationServer& server,
                 metrics::IRegistry& metricsRegistry,
                 ManagerFeatureOptions options);
  ManagerFeature(application_features::ApplicationServer& server,
                 metrics::IRegistry& metricsRegistry);
  ~ManagerFeature();

  void collectOptions(
      std::shared_ptr<arangodb::options::ProgramOptions> options) override;
  void prepare() override;
  void start() override;
  void stop() override;
  void initiateSoftShutdown() override;
  void beginShutdown() override;
  void unprepare() override;

  static transaction::Manager* manager() noexcept;

 private:
  void queueGarbageCollection();

  static std::shared_ptr<transaction::Manager> MANAGER;

  ManagerFeatureOptions _options;

  std::mutex _workItemMutex;
  Scheduler::WorkHandle _workItem;

  // garbage collection function, scheduled regularly in the
  // scheduler
  std::function<void(bool)> _gcfunc;

  /// @brief number of expired transactions that were aborted by
  /// transaction garbage collection
  metrics::Counter& _numExpiredTransactions;
};

}  // namespace arangodb::transaction
