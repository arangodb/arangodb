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

#include "Basics/FutureSharedLock.h"
#include "Metrics/CounterBuilder.h"
#include "Metrics/GaugeBuilder.h"
#include "Metrics/MetricsFeature.h"
#include "ProgramOptions/Parameters.h"

using namespace arangodb::task_monitoring;

DECLARE_COUNTER(
    arangodb_monitoring_tasks_total,
    "Total number of created monitoring tasks since database creation");

DECLARE_GAUGE(arangodb_monitoring_tasks_existing, std::uint64_t,
              "Number of currently existing monitoring tasks");

DECLARE_GAUGE(arangodb_monitoring_tasks_ready_for_deletion, std::uint64_t,
              "Number of currently existing monitoring tasks that wait "
              "for their garbage collection");

DECLARE_COUNTER(arangodb_monitoring_tasks_thread_registries_total,
                "Total number of threads that started monitoring tasks "
                "since database creation");

DECLARE_GAUGE(
    arangodb_monitoring_tasks_existing_thread_registries, std::uint64_t,
    "Number of threads that started currently existing monitoring tasks");

Feature::Feature(Server& server)
    : ArangodFeature{server, *this}, _async_mutex{_schedulerWrapper} {
  startsAfter<metrics::MetricsFeature>();
  startsAfter<SchedulerFeature>();
}

auto Feature::create_metrics(arangodb::metrics::MetricsFeature& metrics_feature)
    -> std::shared_ptr<RegistryMetrics> {
  return std::make_shared<RegistryMetrics>(
      metrics_feature.addShared(arangodb_monitoring_tasks_total{}),
      metrics_feature.addShared(arangodb_monitoring_tasks_existing{}),
      metrics_feature.addShared(arangodb_monitoring_tasks_ready_for_deletion{}),
      metrics_feature.addShared(
          arangodb_monitoring_tasks_thread_registries_total{}),
      metrics_feature.addShared(
          arangodb_monitoring_tasks_existing_thread_registries{}));
}
auto Feature::asyncLock()
    -> futures::Future<futures::FutureSharedLock<SchedulerWrapper>::LockGuard> {
  return _async_mutex.asyncLockExclusive();
}

struct Feature::CleanupThread {
  CleanupThread(size_t gc_timeout)
      : _thread([gc_timeout, this](std::stop_token stoken) {
          while (not stoken.stop_requested()) {
            std::unique_lock guard(_mutex);
            auto status = _cv.wait_for(guard, std::chrono::seconds{gc_timeout});
            if (status == std::cv_status::timeout) {
              async_registry::registry.run_external_cleanup();
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

void Feature::start() {
  metrics = create_metrics(
      server().template getFeature<arangodb::metrics::MetricsFeature>());
  registry.set_metrics(metrics);
  _cleanupThread = std::make_shared<CleanupThread>(_options.gc_timeout);
}

void Feature::stop() { _cleanupThread.reset(); }

void Feature::collectOptions(std::shared_ptr<options::ProgramOptions> options) {
  options->addSection("task-registry", "Options for the task-registry");

  options
      ->addOption("--task-registry.cleanup-timeout",
                  "Timeout in seconds between task-registry garbage collection "
                  "swipes.",
                  new options::SizeTParameter(&_options.gc_timeout, /*base*/ 1,
                                              /*minValue*/ 1))
      .setLongDescription(
          R"(Each thread that is involved in the task-registry needs to garbage collect its finished tasks regularly. This option controls how often this is done in seconds. This can possibly be performance relevant because each involved thread aquires a lock.)");
}

Feature::~Feature() { registry.set_metrics(nullptr); }
