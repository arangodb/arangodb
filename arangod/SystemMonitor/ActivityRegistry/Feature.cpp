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

#include "ActivityRegistry/activity_registry_variable.h"
#include "Basics/FutureSharedLock.h"
#include "Metrics/CounterBuilder.h"
#include "Metrics/GaugeBuilder.h"
#include "Metrics/MetricsFeature.h"
#include "ProgramOptions/Parameters.h"

using namespace arangodb::activity_registry;

DECLARE_COUNTER(arangodb_activity_activities_total,
                "Total number of created activities since database creation");

DECLARE_GAUGE(arangodb_activity_existing_activities, std::uint64_t,
              "Number of currently existing activities");

DECLARE_GAUGE(arangodb_activity_ready_for_deletion_activities, std::uint64_t,
              "Number of currently existing activities that wait "
              "for their garbage collection");

DECLARE_COUNTER(arangodb_activity_thread_registries_total,
                "Total number of threads that started actities "
                "since database creation");

DECLARE_GAUGE(arangodb_activity_existing_thread_registries, std::uint64_t,
              "Number of currently existing activity thread registries");

Feature::Feature(application_features::ApplicationServer& server)
    : application_features::ApplicationFeature{server, *this} {
  startsAfter<metrics::MetricsFeature>();
  startsAfter<SchedulerFeature>();
}

auto Feature::create_metrics(arangodb::metrics::MetricsFeature& metrics_feature)
    -> std::shared_ptr<RegistryMetrics> {
  return std::make_shared<RegistryMetrics>(
      metrics_feature.addShared(arangodb_activity_activities_total{}),
      metrics_feature.addShared(arangodb_activity_existing_activities{}),
      metrics_feature.addShared(
          arangodb_activity_ready_for_deletion_activities{}),
      metrics_feature.addShared(arangodb_activity_thread_registries_total{}),
      metrics_feature.addShared(
          arangodb_activity_existing_thread_registries{}));
}
struct Feature::CleanupThread {
  CleanupThread(size_t gc_timeout)
      : _thread([gc_timeout, this](std::stop_token stoken) {
          while (not stoken.stop_requested()) {
            std::unique_lock guard(_mutex);
            auto status = _cv.wait_for(guard, std::chrono::seconds{gc_timeout});
            if (status == std::cv_status::timeout) {
              activity_registry::registry.run_external_cleanup();
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
  _metrics =
      create_metrics(server().getFeature<arangodb::metrics::MetricsFeature>());
  registry.set_metrics(_metrics);
}

void Feature::start() {
  _cleanupThread = std::make_shared<CleanupThread>(_options.gc_timeout);
}

void Feature::stop() { _cleanupThread.reset(); }

void Feature::collectOptions(std::shared_ptr<options::ProgramOptions> options) {
  options->addSection("activity-registry", "Options for the activity-registry");

  options
      ->addOption(
          "--activity-registry.cleanup-timeout",
          "Timeout in seconds between activity-registry garbage collection "
          "swipes.",
          new options::SizeTParameter(&_options.gc_timeout, /*base*/ 1,
                                      /*minValue*/ 1))
      .setLongDescription(
          R"(Each thread that is involved in the activity-registry needs to garbage collect its finished activities regularly. This option controls how often this is done in seconds. This can possibly be performance relevant because each involved thread aquires a lock.)");
}
