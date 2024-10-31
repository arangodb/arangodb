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

#include "Metrics/CounterBuilder.h"
#include "Metrics/GaugeBuilder.h"
#include "Metrics/MetricsFeature.h"
#include "ProgramOptions/Parameters.h"

using namespace arangodb::async_registry;

DECLARE_COUNTER(
    arangodb_async_promises_total,
    "Total number of registered asynchronous promises since database creation");

DECLARE_GAUGE(arangodb_async_registered_promises, std::uint64_t,
              "Number of currently registered asynchronous promises");
DECLARE_GAUGE(arangodb_async_ready_for_deletion_promises, std::uint64_t,
              "Number of currently registered asynchronous promises that wait "
              "for their garbage "
              "collection");

DECLARE_COUNTER(arangodb_async_threads_total,
                "Total number of threads that registered asynchronous promises "
                "since database creation");

DECLARE_GAUGE(arangodb_async_running_threads, std::uint64_t,
              "Number of currently running threads that registered "
              "asynchronous promises");
DECLARE_GAUGE(arangodb_async_registered_threads, std::uint64_t,
              "Number of threads the asynchronous registry iterates over to "
              "list all asynchronous promises");

Feature::Feature(Server& server) : ArangodFeature{server, *this} {
  startsAfter<arangodb::metrics::MetricsFeature>();
}

auto Feature::create_metrics(arangodb::metrics::MetricsFeature& metrics_feature)
    -> std::shared_ptr<const Metrics> {
  return std::make_shared<Metrics>(
      metrics_feature.addShared(arangodb_async_promises_total{}),
      metrics_feature.addShared(arangodb_async_registered_promises{}),
      metrics_feature.addShared(arangodb_async_ready_for_deletion_promises{}),
      metrics_feature.addShared(arangodb_async_threads_total{}),
      metrics_feature.addShared(arangodb_async_running_threads{}),
      metrics_feature.addShared(arangodb_async_registered_threads{}));
}

struct Feature::PromiseCleanupThread {
  PromiseCleanupThread(size_t gc_timeout)
      : _thread([gc_timeout, this](std::stop_token stoken) {
          while (not stoken.stop_requested()) {
            std::unique_lock guard(_mutex);
            auto status = _cv.wait_for(guard, std::chrono::seconds{gc_timeout});
            if (status == std::cv_status::timeout) {
              async_registry::registry.run_external_cleanup();
            }
          }
        }) {}

  ~PromiseCleanupThread() {
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
  _cleanupThread = std::make_shared<PromiseCleanupThread>(_options.gc_timeout);
}

void Feature::stop() { _cleanupThread.reset(); }

void Feature::collectOptions(std::shared_ptr<options::ProgramOptions> options) {
  options->addSection("async-registry", "Options for the async-registry");

  options
      ->addOption(
          "--async-registry.cleanup-timeout",
          "Timeout in seconds between async-registry garbage collection "
          "swipes.",
          new options::SizeTParameter(&_options.gc_timeout, /*base*/ 1,
                                      /*minValue*/ 1))
      .setLongDescription(
          R"(Each thread that is involved in the async-registry needs to garbage collect its finished async function calls regularly. This option controls how often this is done in seconds. This can possibly be performance relevant because each involved thread aquires a lock.)");
}

Feature::~Feature() { registry.set_metrics(std::make_shared<Metrics>()); }
