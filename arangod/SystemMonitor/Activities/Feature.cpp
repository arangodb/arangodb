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

#include "Activities/activity_registry_variable.h"
#include "Basics/FutureSharedLock.h"
#include "Metrics/CounterBuilder.h"
#include "Metrics/GaugeBuilder.h"
#include "Metrics/MetricsFeature.h"
#include "ProgramOptions/Parameters.h"
#include "velocypack/SharedSlice.h"
#include "Inspection/VPack.h"

#include <thread>

using namespace arangodb::activities;
using namespace arangodb;

DECLARE_COUNTER(
    arangodb_activities_total,
    "Total number of created activities since database process start");

DECLARE_GAUGE(arangodb_activities_existing, std::uint64_t,
              "Number of currently existing activities");

DECLARE_GAUGE(arangodb_activities_ready_for_deletion, std::uint64_t,
              "Number of currently existing activities that wait "
              "for their garbage collection");

DECLARE_COUNTER(arangodb_activities_thread_registries_total,
                "Total number of threads that started actities "
                "since database process start");

DECLARE_GAUGE(arangodb_activities_existing_thread_registries, std::uint64_t,
              "Number of currently existing activity thread registries");

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
      metrics_feature.addShared(arangodb_activities_existing{}),
      metrics_feature.addShared(arangodb_activities_ready_for_deletion{}),
      metrics_feature.addShared(arangodb_activities_thread_registries_total{}),
      metrics_feature.addShared(
          arangodb_activities_existing_thread_registries{}));
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
  options->addSection("activites", "Options for activities");

  options
      ->addOption(
          "--activities.registry-cleanup-timeout",
          "Timeout in seconds between activity registry garbage collections.",
          new options::SizeTParameter(&_options.gc_timeout, /*base*/ 1,
                                      /*minValue*/ 1))
      .setLongDescription(
          R"(Each thread that is involved in the activity-registry needs to garbage collect its finished activities regularly. This option controls how often this is done in seconds. This can possibly be performance relevant because each involved thread aquires a lock.)");
}

struct ActivityOutput {
  std::string type;
  ActivityId id;
  ActivityId parent;
  Metadata metadata;
};
template<typename Inspector>
auto inspect(Inspector& f, ActivityOutput& x) {
  return f.object(x).fields(f.embedFields(x.id), f.field("type", x.type),
                            f.field("parent", x.parent),
                            f.field("metadata", x.metadata));
}

velocypack::Builder Feature::getData() const {
  VPackBuilder builder;
  builder.openArray();
  registry.for_node([&](ActivityInRegistrySnapshot activity) {
    if (activity.state != activities::State::Deleted) {
      auto a = ActivityOutput{.type = std::move(activity.type),
                              .id = std::move(activity.id),
                              .parent = std::move(activity.parent),
                              .metadata = std::move(activity.metadata)};
      velocypack::serialize(builder, a);
    }
  });
  builder.close();
  return builder;
}

velocypack::SharedSlice Feature::getCrashData() const {
  return getData().sharedSlice();
}
