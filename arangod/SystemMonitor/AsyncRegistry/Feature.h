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
/// @author Julia Volmer
////////////////////////////////////////////////////////////////////////////////
#pragma once

#include "SystemMonitor/AsyncRegistry/Metrics.h"
#include "RestServer/arangod.h"
#include "Scheduler/AsyncLockWithScheduler.h"
#include "Async/Registry/promise.h"
#include "Containers/Forest/forest.h"
#include "CrashHandler/DataSource.h"
#include "CrashHandler/DataSourceRegistry.h"

namespace arangodb::async_registry {

auto all_undeleted_promises() -> containers::ForestWithRoots<PromiseSnapshot>;

VPackBuilder serialize(
    containers::IndexedForestWithRoots<PromiseSnapshot> const& promises);

class Feature final : public ArangodFeature,
                      public crash_handler::ICrashHandlerDataSource {
 private:
  static auto create_metrics(arangodb::metrics::MetricsFeature& metrics_feature)
      -> std::shared_ptr<RegistryMetrics>;

 public:
  static constexpr std::string_view name() { return "AsyncRegistry"; }
  auto asyncLock() -> futures::Future<AsyncLockWithScheduler::Lock> {
    return _asyncLock.lock();
  };

  Feature(Server& server,
          crash_handler::DataSourceRegistry* crashHandlerRegistry);

  void prepare() override final;
  void start() override final;
  void stop() override final;
  void collectOptions(std::shared_ptr<options::ProgramOptions>) override final;

  velocypack::SharedSlice getCrashData() const override;

  std::string_view getDataSourceName() const override;

 private:
  struct Options {
    size_t gc_timeout{1};
  };
  Options _options;

  std::shared_ptr<RegistryMetrics> _metrics;

  struct PromiseCleanupThread;
  std::shared_ptr<PromiseCleanupThread> _cleanupThread;

  AsyncLockWithScheduler _asyncLock{std::string{name()}};
};

}  // namespace arangodb::async_registry
