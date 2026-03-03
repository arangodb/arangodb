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
#include "Basics/Exceptions.h"
#include "Basics/FutureSharedLock.h"
#include "Metrics/CounterBuilder.h"
#include "Metrics/GaugeBuilder.h"
#include "Metrics/MetricsFeature.h"
#include "ProgramOptions/Parameters.h"
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
    : application_features::ApplicationFeature{server, *this},
      crash_handler::CrashHandlerDataSource(std::move(dataSourceRegistry)) {
  startsAfter<metrics::MetricsFeature>();
}

auto Feature::create_metrics(metrics::MetricsFeature& metrics_feature)
    -> std::shared_ptr<RegistryMetrics> {
  return std::make_shared<RegistryMetrics>(
      metrics_feature.addShared(arangodb_activities_total{}),
      metrics_feature.addShared(arangodb_activities_existing{}));
}
struct Feature::CleanupThread {
  CleanupThread(size_t gc_timeout)
      : _thread([gc_timeout](std::stop_token stoken) {
          while (not stoken.stop_requested()) {
            std::this_thread::sleep_for(std::chrono::seconds{gc_timeout});
            activities::registry.garbageCollect();
          }
        }) {}

  ~CleanupThread() { _thread.request_stop(); }

  std::jthread _thread;
};

void Feature::prepare() {
  _metrics = create_metrics(server().getFeature<metrics::MetricsFeature>());
  registry.setMetrics(_metrics);
}

void Feature::start() {
  _cleanupThread = std::make_shared<CleanupThread>(_options.gc_timeout);
}

void Feature::stop() { _cleanupThread.reset(); }

void Feature::collectOptions(std::shared_ptr<options::ProgramOptions> options) {
  options->addSection("activites", "Options for activities");

  options
      ->addOption(
          "--activities.registry-cleanup-timeout",
          "Timeout in seconds between activity registry garbage collections.",
          new options::SizeTParameter(&_options.gc_timeout, /*base*/ 1,
                                      /*minValue*/ 1))
      .setLongDescription(
          R"(Each thread that is involved in the activity-registry needs to garbage collect its finished activities regularly. This option controls how often this is done in seconds. This can possibly be performance relevant because each involved thread aquires a lock.)");

  options
      ->addOption(
          "--activities.only-superuser-enabled",
          "Whether only superusers can request the API or all admin users",
          new options::BooleanParameter(&_options.isOnlySuperUserEnabled),
          options::makeDefaultFlags(arangodb::options::Flags::Uncommon))
      .setLongDescription(
          R"(Switched on, only superuser is allowed to query this endpoint.
      Default is that admin users are allowed to query the endpoint.)");
}

velocypack::SharedSlice Feature::getData() const {
  auto res = registry.snapshot();
  if (res.ok()) {
    return res.get();
  } else {
    THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_INTERNAL,
        std::format("Failed to serialize snapshot with error: {}",
                    res.error().error()));
  }
}

velocypack::SharedSlice Feature::getCrashData() const {
  auto res = registry.snapshot();
  if (res.ok()) {
    return getData();
  } else {
    // YOLO.  If an exception is thrown here it happens inside crash
    // data collection and the synthetic text extrusion machine
    // complains.
    return velocypack::SharedSlice();
  }
}
