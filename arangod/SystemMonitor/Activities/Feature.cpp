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
#include "Feature.h"

#include "Activities/RegistryGlobalVariable.h"
#include "SystemMonitor/Activities/OptionsProvider.h"
#include "Basics/Exceptions.h"
#include "Basics/FutureSharedLock.h"
#include "Metrics/CounterBuilder.h"
#include "Metrics/GaugeBuilder.h"
#include "Metrics/MetricsFeature.h"
#include "Metrics/IRegistry.h"
#include "velocypack/SharedSlice.h"
#include "Inspection/VPack.h"

#include <chrono>
#include <thread>
#include <unordered_map>

using namespace arangodb::activities;
using namespace arangodb;

DECLARE_COUNTER(
    arangodb_activities_total,
    "Total number of created activities since database process start");

DECLARE_GAUGE(arangodb_activities_existing, std::uint64_t,
              "Number of currently registered activities");

Feature::Feature(
    application_features::ApplicationServer& server,
    std::shared_ptr<crash_handler::DataSourceRegistry> dataSourceRegistry)
    : Feature(server, std::move(dataSourceRegistry), FeatureOptions{}) {}

Feature::Feature(
    application_features::ApplicationServer& server,
    std::shared_ptr<crash_handler::DataSourceRegistry> dataSourceRegistry,
    FeatureOptions options)
    : application_features::ApplicationFeature{server, *this},
      crash_handler::CrashHandlerDataSource(std::move(dataSourceRegistry)),
      _options(std::move(options)) {
  startsAfter<metrics::MetricsFeature>();
}

auto Feature::create_metrics(metrics::IRegistry& registry)
    -> std::shared_ptr<RegistryMetrics> {
  return std::make_shared<RegistryMetrics>(
      registry.addShared(arangodb_activities_total{}),
      registry.addShared(arangodb_activities_existing{}));
}
struct Feature::CleanupThread {
  CleanupThread(size_t gc_timeout)
      : _thread([gc_timeout, this](std::stop_token stoken) {
          while (not stoken.stop_requested()) {
            std::unique_lock guard(_mutex);
            auto status = _cv.wait_for(guard, std::chrono::seconds{gc_timeout});
            if (status == std::cv_status::timeout) {
              activities::registry.run_external_cleanup();
            }
          }
        }) {}

  ~CleanupThread() {
    _thread.request_stop();
    _cv.notify_one();
  }

  std::mutex _mutex;
  std::condition_variable _cv;
  std::jthread _thread;
};

void Feature::prepare() {
  _metrics = create_metrics(server().getFeature<metrics::MetricsFeature>());
  registry.set_metrics(_metrics);
}

void Feature::start() {
  _cleanupThread = std::make_shared<CleanupThread>(_options.gc_timeout);
}

void Feature::stop() { _cleanupThread.reset(); }

void Feature::collectOptions(std::shared_ptr<options::ProgramOptions> options) {
  activities::OptionsProvider provider;
  provider.declareOptions(options, _options);
}

velocypack::SharedSlice Feature::getData() const {
  auto snap = registry.snapshot();
  if (not snap.ok()) {
    THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_INTERNAL,
        std::string{"Error while serializing to VelocyPack: "} +
            snap.error().error() + "\nPath: " + snap.error().path());
  }
  return snap.get();
}

velocypack::SharedSlice Feature::getCrashData() const { return getData(); }
