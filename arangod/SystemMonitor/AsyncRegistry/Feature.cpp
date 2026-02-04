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
#include "SystemMonitor/AsyncRegistry/Feature.h"

#include "Containers/Forest/depth_first.h"
#include "Containers/Forest/forest.h"
#include "CrashHandler/DataSource.h"
#include "Metrics/CounterBuilder.h"
#include "Metrics/GaugeBuilder.h"
#include "Metrics/MetricsFeature.h"
#include "ProgramOptions/Parameters.h"
#include "Inspection/VPack.h"

DECLARE_COUNTER(
    arangodb_async_promises_total,
    "Total number of created asynchronous operations since database creation");

DECLARE_GAUGE(arangodb_async_existing_promises, std::uint64_t,
              "Number of currently existing asynchronous operations");

DECLARE_GAUGE(arangodb_async_ready_for_deletion_promises, std::uint64_t,
              "Number of currently existing asynchronous operations that wait "
              "for their garbage collection");

DECLARE_COUNTER(arangodb_async_thread_registries_total,
                "Total number of threads that started asynchronous operations "
                "since database creation");

DECLARE_GAUGE(arangodb_async_existing_thread_registries, std::uint64_t,
              "Number of currently existing async thread registries");

using namespace arangodb::containers;

namespace arangodb::async_registry {

struct Entry {
  TreeHierarchy hierarchy;
  PromiseSnapshot data;
};

template<typename Inspector>
auto inspect(Inspector& f, Entry& x) {
  return f.object(x).fields(f.field("hierarchy", x.hierarchy),
                            f.field("data", x.data));
}
/**
   Creates a forest of all promises in the async registry

   An edge between two promises means that the lower hierarchy promise waits for
 the larger hierarchy promise.
 **/
auto all_undeleted_promises() -> ForestWithRoots<PromiseSnapshot> {
  Forest<PromiseSnapshot> forest;
  std::vector<Id> roots;
  registry.for_node([&](PromiseSnapshot promise) {
    if (promise.state != State::Deleted) {
      std::visit(overloaded{
                     [&](PromiseId const& async_waiter) {
                       forest.insert(promise.id.id, async_waiter.id, promise);
                     },
                     [&](basics::ThreadInfo const& sync_waiter_thread) {
                       forest.insert(promise.id.id, nullptr, promise);
                       roots.emplace_back(promise.id.id);
                     },
                 },
                 promise.requester);
    }
  });
  return ForestWithRoots{forest, roots};
}

/**
   Converts a forest of promises into a list of stacktraces inside a
 velocypack.

   The list of stacktraces include one stacktrace per tree in the forest. To
 create one stacktrace, it uses a depth first search to traverse the forest in
 post order, such that promises with the highest hierarchy in a tree are given
 first and the root promise is given last.
 **/
VPackBuilder serialize(
    IndexedForestWithRoots<PromiseSnapshot> const& promises) {
  VPackBuilder builder;
  builder.openObject();
  builder.add(VPackValue("promise_stacktraces"));
  builder.openArray();
  for (auto const& root : promises.roots()) {
    builder.openArray();
    auto dfs = DFS_PostOrder{promises, root};
    do {
      auto next = dfs.next();
      if (next == std::nullopt) {
        break;
      }
      auto [id, hierarchy] = next.value();
      auto data = promises.node(id);
      if (data != std::nullopt) {
        auto entry = Entry{.hierarchy = hierarchy, .data = data.value()};
        serialize(builder, entry);
      }
    } while (true);
    builder.close();
  }
  builder.close();
  builder.close();
  return builder;
}

Feature::Feature(
    application_features::ApplicationServer& server,
    std::shared_ptr<crash_handler::DataSourceRegistry> dataSourceRegistry)
    : application_features::ApplicationFeature{server, *this},
      crash_handler::CrashHandlerDataSource(std::move(dataSourceRegistry)) {
  startsAfter<metrics::MetricsFeature>();
  startsAfter<SchedulerFeature>();
}

auto Feature::create_metrics(metrics::MetricsFeature& metrics_feature)
    -> std::shared_ptr<RegistryMetrics> {
  return std::make_shared<RegistryMetrics>(
      metrics_feature.addShared(arangodb_async_promises_total{}),
      metrics_feature.addShared(arangodb_async_existing_promises{}),
      metrics_feature.addShared(arangodb_async_ready_for_deletion_promises{}),
      metrics_feature.addShared(arangodb_async_thread_registries_total{}),
      metrics_feature.addShared(arangodb_async_existing_thread_registries{}));
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

void Feature::prepare() {
  _metrics = create_metrics(server().getFeature<metrics::MetricsFeature>());
  registry.set_metrics(_metrics);
}

void Feature::start() {
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

velocypack::Builder Feature::getData() const {
  auto promises = all_undeleted_promises().index_by_parent();
  return serialize(promises);
}

velocypack::SharedSlice Feature::getCrashData() const {
  return getData().sharedSlice();
}

std::string_view Feature::getDataSourceName() const { return name(); }

}  // namespace arangodb::async_registry
