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
/// @author Kaveh Vahedipour
////////////////////////////////////////////////////////////////////////////////
#include "Metrics/MetricsFeature.h"

#include <frozen/string.h>
#include <frozen/unordered_set.h>
#include <velocypack/Builder.h>

#include <chrono>
#include <unordered_set>

#include "ApplicationFeatures/ApplicationServer.h"
#include "ApplicationFeatures/GreetingsFeaturePhase.h"
#include "Agency/Node.h"
#include "Basics/application-exit.h"
#include "Basics/debugging.h"
#include "Cluster/ClusterFeature.h"
#include "Cluster/ServerState.h"
#include "Logger/LogMacros.h"
#include "Logger/Logger.h"
#include "Logger/LoggerFeature.h"
#include "Metrics/ClusterMetricsFeature.h"
#include "Metrics/Metric.h"
#include "ProgramOptions/Parameters.h"
#include "ProgramOptions/ProgramOptions.h"
#include "Basics/CGroupDetection.h"
#include "Basics/NumberOfCores.h"
#include "Basics/PhysicalMemory.h"
#include "Basics/process-utils.h"
#include "Basics/system-functions.h"
#include "GeneralServer/RequestStatisticsMetrics.h"
#include "Metrics/Builder.h"
#include "Metrics/CounterBuilder.h"
#include "Metrics/FixScale.h"
#include "Metrics/GaugeBuilder.h"
#include "Metrics/HistogramBuilder.h"
#include "RestServer/CpuUsageFeature.h"
#include "RestServer/QueryRegistryFeature.h"
#include "RocksDBEngine/RocksDBEngine.h"
#include "StorageEngine/EngineSelectorFeature.h"

namespace arangodb::metrics {

MetricsFeature::MetricsFeature(
    application_features::ApplicationServer& server,
    LazyApplicationFeatureReference<QueryRegistryFeature>
        lazyQueryRegistryFeatureRef,
    LazyApplicationFeatureReference<EngineSelectorFeature>
        lazyEngineSelectorFeatureRef,
    LazyApplicationFeatureReference<ClusterMetricsFeature>
        lazyClusterMetricsFeatureRef,
    LazyApplicationFeatureReference<ClusterFeature> lazyClusterFeatureRef)
    : ApplicationFeature{server, *this},
      _lazyQueryRegistryFeatureRef(std::move(lazyQueryRegistryFeatureRef)),
      _lazyEngineSelectorFeatureRef(std::move(lazyEngineSelectorFeatureRef)),
      _lazyClusterMetricsFeatureRef(std::move(lazyClusterMetricsFeatureRef)),
      _lazyClusterFeatureRef(std::move(lazyClusterFeatureRef)),
      _transactionStatistics(std::make_unique<TransactionStatistics>(*this)) {
  setOptional(false);
  startsAfter<LoggerFeature>();
  startsBefore<application_features::GreetingsFeaturePhase>();
}

void MetricsFeature::collectOptions(
    std::shared_ptr<options::ProgramOptions> options) {
  _startTime = TRI_microtime();

  options->addOption(
      "--server.export-metrics-api", "Whether to enable the metrics API.",
      new options::BooleanParameter(&_options.exportAPI),
      arangodb::options::makeDefaultFlags(arangodb::options::Flags::Uncommon));

  options
      ->addOption(
          "--server.export-read-write-metrics",
          "Whether to enable metrics for document reads and writes.",
          new options::BooleanParameter(&_options.exportReadWriteMetrics),
          arangodb::options::makeDefaultFlags(
              arangodb::options::Flags::Uncommon))
      .setLongDescription(R"(Enabling this option exposes the following
additional metrics via the `GET /_admin/metrics` endpoint:

- `arangodb_document_writes_total`
- `arangodb_document_writes_replication_total`
- `arangodb_document_insert_time`
- `arangodb_document_read_time`
- `arangodb_document_update_time`
- `arangodb_document_replace_time`
- `arangodb_document_remove_time`
- `arangodb_collection_truncates_total`
- `arangodb_collection_truncates_replication_total`
- `arangodb_collection_truncate_time`
)");

  options
      ->addOption(
          "--server.ensure-whitespace-metrics-format",
          "Set to `true` to ensure whitespace between the exported metric "
          "value and the preceding token (metric name or labels) in the "
          "metrics output.",
          new options::BooleanParameter(&_options.ensureWhitespace),
          arangodb::options::makeDefaultFlags(
              arangodb::options::Flags::Uncommon))
      .setIntroducedIn(31006)
      .setLongDescription(R"(Using the whitespace characters in the output may
be required to make the metrics output compatible with some processing tools,
although Prometheus itself doesn't need it.)");

  std::unordered_set<std::string> modes = {"disabled", "enabled-per-shard",
                                           "enabled-per-shard-per-user"};
  options
      ->addOption(
          "--server.export-shard-usage-metrics",
          "Whether or not to export shard usage metrics.",
          new options::DiscreteValuesParameter<options::StringParameter>(
              &_options.usageTrackingModeString, modes),
          arangodb::options::makeFlags(
              arangodb::options::Flags::DefaultNoComponents,
              arangodb::options::Flags::OnDBServer))
      .setIntroducedIn(31200)
      .setLongDescription(R"(This option can be used to make DB-Servers export
detailed shard usage metrics.

- By default, this option is set to `disabled` so that no shard usage metrics
  are exported.

- Set the option to `enabled-per-shard` to make DB-Servers collect per-shard
  usage metrics whenever a shard is accessed.

- Set this option to `enabled-per-shard-per-user` to make DB-Servers collect
  usage metrics per shard and per user whenever a shard is accessed.

Note that enabling shard usage metrics can produce a lot of metrics if there
are many shards and/or users in the system.)");
}

std::shared_ptr<Metric> MetricsFeature::doAdd(Builder& builder) {
  auto metric = builder.build();
  TRI_ASSERT(metric != nullptr);
  MetricKeyView key{metric->name(), metric->labels()};
  std::lock_guard lock{_mutex};
  auto [it, inserted] = _registry.try_emplace(key, metric);
  if (!inserted) {
    THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_INTERNAL,
        absl::StrCat(builder.type(), " ", metric->name(), ":", metric->labels(),
                     " already exists"));
  }
  return (*it).second;
}

std::shared_ptr<Metric> MetricsFeature::doEnsureMetric(Builder& builder) {
  auto metric = builder.build();
  TRI_ASSERT(metric != nullptr);
  MetricKeyView key{metric->name(), metric->labels()};
  {
    // happy path: check if metric already exists and if so, return it
    std::shared_lock lock{_mutex};
    if (auto it = _registry.find(key); it != _registry.end()) {
      return (*it).second;
    }
  }
  // slow path: create new metric under exclusive lock
  std::lock_guard lock{_mutex};
  // insertion can fail here because someone else concurrently inserted the
  // metric. this is fine, because in that case we simply return that
  // version.
  auto [it, inserted] = _registry.try_emplace(key, metric);
  return (*it).second;
}

std::shared_ptr<Metric> MetricsFeature::doAddDynamic(Builder& builder) {
  auto metric = doEnsureMetric(builder);
  metric->setDynamic();
  return metric;
}

Metric* MetricsFeature::get(MetricKeyView const& key) const {
  std::shared_lock lock{_mutex};
  auto it = _registry.find(key);
  if (it == _registry.end()) {
    return nullptr;
  }
  return it->second.get();
}

bool MetricsFeature::remove(Builder const& builder) {
  MetricKeyView key{builder.name(), builder.labels()};
  std::lock_guard guard{_mutex};
  return _registry.erase(key) != 0;
}

bool MetricsFeature::remove(Metric const& m) {
  MetricKeyView key{m.name(), m.labels()};
  std::lock_guard guard{_mutex};
  return _registry.erase(key) != 0;
}

bool MetricsFeature::exportAPI() const noexcept { return _options.exportAPI; }

bool MetricsFeature::ensureWhitespace() const noexcept {
  return _options.ensureWhitespace;
}

MetricsFeature::UsageTrackingMode MetricsFeature::usageTrackingMode()
    const noexcept {
  return _options.usageTrackingMode;
}

void MetricsFeature::validateOptions(
    std::shared_ptr<options::ProgramOptions> options) {
  // translate usage tracking mode string to enum value
  if (_options.usageTrackingModeString == "enabled-per-shard") {
    _options.usageTrackingMode = UsageTrackingMode::kEnabledPerShard;
  } else if (_options.usageTrackingModeString == "enabled-per-shard-per-user") {
    _options.usageTrackingMode = UsageTrackingMode::kEnabledPerShardPerUser;
  } else {
    _options.usageTrackingMode = UsageTrackingMode::kDisabled;
  }

  if (_options.exportReadWriteMetrics) {
    transactionStatistics().setupDocumentMetrics();
  }
}

namespace {

DECLARE_COUNTER(arangodb_process_statistics_minor_page_faults_total,
                "The number of minor faults the process has made which have "
                "not required loading a memory page from disk");
DECLARE_COUNTER(arangodb_process_statistics_major_page_faults_total,
                "This figure contains the number of major faults the process "
                "has made which have required loading a memory page from disk");
DECLARE_GAUGE(arangodb_process_statistics_user_time, double,
              "Amount of time that this process has been scheduled in user "
              "mode, measured in seconds");
DECLARE_GAUGE(arangodb_process_statistics_system_time, double,
              "Amount of time that this process has been scheduled in kernel "
              "mode, measured in seconds");
DECLARE_GAUGE(arangodb_process_statistics_number_of_threads, double,
              "Number of threads in the arangod process");
DECLARE_GAUGE(arangodb_process_statistics_resident_set_size, double,
              "The total size of the number of pages the process has in real "
              "memory. This is just the pages which count toward text, data, "
              "or stack space. This does not include pages which have not been "
              "demand-loaded in, or which are swapped out. The resident set "
              "size is reported in bytes");
DECLARE_GAUGE(arangodb_process_statistics_resident_set_size_percent, double,
              "The relative size of the number of pages the process has in "
              "real memory compared to system memory. This is just the pages "
              "which count toward text, data, or stack space. This does not "
              "include pages which have not been demand-loaded in, or which "
              "are swapped out. The value is a ratio between 0.00 and 1.00");
DECLARE_GAUGE(arangodb_process_statistics_virtual_memory_size, double,
              "This figure contains The size of the virtual memory the process "
              "is using");
DECLARE_COUNTER(arangodb_server_statistics_server_uptime_total,
                "Number of seconds elapsed since server start");
DECLARE_GAUGE(arangodb_server_statistics_physical_memory, double,
              "Physical memory in bytes");
DECLARE_GAUGE(arangodb_server_statistics_effective_physical_memory, double,
              "Effective physical memory in bytes");
DECLARE_GAUGE(arangodb_server_statistics_cpu_cores, double,
              "Number of CPU cores visible to the arangod process");
DECLARE_GAUGE(arangodb_server_statistics_effective_cpu_cores, double,
              "Number of effective CPU cores set for the arangod process");
DECLARE_GAUGE(arangodb_server_statistics_cpu_cgroup_version, uint64_t,
              "CGroup version detected (0=none, 1=v1, 2=v2)");
DECLARE_GAUGE(arangodb_server_statistics_user_percent, double,
              "Percentage of time that the system CPUs have spent in user "
              "mode");
DECLARE_GAUGE(arangodb_server_statistics_system_percent, double,
              "Percentage of time that the system CPUs have spent in kernel "
              "mode");
DECLARE_GAUGE(arangodb_server_statistics_idle_percent, double,
              "Percentage of time that the system CPUs have been idle");
DECLARE_GAUGE(arangodb_server_statistics_iowait_percent, double,
              "Percentage of time that the system CPUs have been waiting for "
              "I/O");

auto const statStrings =
    std::map<std::string_view, std::vector<std::string_view>>{
        {"bytesReceived",
         {"arangodb_client_connection_statistics_bytes_received", "histogram",
          "Bytes received for requests"}},
        {"bytesSent",
         {"arangodb_client_connection_statistics_bytes_sent", "histogram",
          "Bytes sent for responses"}},
        {"bytesReceivedUser",
         {"arangodb_client_user_connection_statistics_bytes_received",
          "histogram", "Bytes received for requests, only user traffic"}},
        {"bytesSentUser",
         {"arangodb_client_user_connection_statistics_bytes_sent", "histogram",
          "Bytes sent for responses, only user traffic"}},
        {"minorPageFaults",
         {"arangodb_process_statistics_minor_page_faults_total", "counter",
          "The number of minor faults the process has made which have not "
          "required loading a memory page from disk"}},
        {"majorPageFaults",
         {"arangodb_process_statistics_major_page_faults_total", "counter",
          "This figure contains the number of major faults the process has "
          "made which have required loading a memory page from disk"}},
        {"userTime",
         {"arangodb_process_statistics_user_time", "gauge",
          "Amount of time that this process has been scheduled in user mode, "
          "measured in seconds"}},
        {"systemTime",
         {"arangodb_process_statistics_system_time", "gauge",
          "Amount of time that this process has been scheduled in kernel mode, "
          "measured in seconds"}},
        {"numberOfThreads",
         {"arangodb_process_statistics_number_of_threads", "gauge",
          "Number of threads in the arangod process"}},
        {"residentSize",
         {"arangodb_process_statistics_resident_set_size", "gauge",
          "The total size of the number of pages the process has in real "
          "memory. This is just the pages which count toward text, data, or "
          "stack space. This does not include pages which have not been "
          "demand-loaded in, or which are swapped out. The resident set size "
          "is reported in bytes"}},
        {"residentSizePercent",
         {"arangodb_process_statistics_resident_set_size_percent", "gauge",
          "The relative size of the number of pages the process has in real "
          "memory compared to system memory. This is just the pages which "
          "count toward text, data, or stack space. This does not include "
          "pages which have not been demand-loaded in, or which are swapped "
          "out. The value is a ratio between 0.00 and 1.00"}},
        {"virtualSize",
         {"arangodb_process_statistics_virtual_memory_size", "gauge",
          "This figure contains The size of the virtual memory the process is "
          "using"}},
        {"totalTime",
         {"arangodb_client_connection_statistics_total_time", "histogram",
          "Total time needed to answer a request"}},
        {"totalTimeCount",
         {"arangodb_client_connection_statistics_total_time_count", "gauge",
          "Total time needed to answer a request"}},
        {"totalTimeSum",
         {"arangodb_client_connection_statistics_total_time_sum", "gauge",
          "Total time needed to answer a request"}},
        {"requestTime",
         {"arangodb_client_connection_statistics_request_time", "histogram",
          "Request time needed to answer a request"}},
        {"requestTimeCount",
         {"arangodb_client_connection_statistics_request_time_count", "gauge",
          "Request time needed to answer a request"}},
        {"requestTimeSum",
         {"arangodb_client_connection_statistics_request_time_sum", "gauge",
          "Request time needed to answer a request"}},
        {"queueTime",
         {"arangodb_client_connection_statistics_queue_time", "histogram",
          "Request time needed to answer a request"}},
        {"queueTimeCount",
         {"arangodb_client_connection_statistics_queue_time_count", "gauge",
          "Request time needed to answer a request"}},
        {"queueTimeSum",
         {"arangodb_client_connection_statistics_queue_time_sum", "gauge",
          "Request time needed to answer a request"}},
        {"ioTime",
         {"arangodb_client_connection_statistics_io_time", "histogram",
          "Request time needed to answer a request"}},
        {"ioTimeCount",
         {"arangodb_client_connection_statistics_io_time_count", "gauge",
          "Queue time needed to answer a request"}},
        {"ioTimeSum",
         {"arangodb_client_connection_statistics_io_time_sum", "gauge",
          "IO time needed to answer a request"}},
        {"httpReqsTotal",
         {"arangodb_http_request_statistics_total_requests_total", "counter",
          "Total number of HTTP requests"}},
        {"httpReqsSuperuser",
         {"arangodb_http_request_statistics_superuser_requests_total",
          "counter",
          "Total number of HTTP requests executed by superuser/JWT"}},
        {"httpReqsUser",
         {"arangodb_http_request_statistics_user_requests_total", "counter",
          "Total number of HTTP requests executed by clients"}},
        {"httpReqsAsync",
         {"arangodb_http_request_statistics_async_requests_total", "counter",
          "Number of asynchronously executed HTTP requests"}},
        {"httpReqsDelete",
         {"arangodb_http_request_statistics_http_delete_requests_total",
          "counter", "Number of HTTP DELETE requests"}},
        {"httpReqsGet",
         {"arangodb_http_request_statistics_http_get_requests_total", "counter",
          "Number of HTTP GET requests"}},
        {"httpReqsHead",
         {"arangodb_http_request_statistics_http_head_requests_total",
          "counter", "Number of HTTP HEAD requests"}},
        {"httpReqsOptions",
         {"arangodb_http_request_statistics_http_options_requests_total",
          "counter", "Number of HTTP OPTIONS requests"}},
        {"httpReqsPatch",
         {"arangodb_http_request_statistics_http_patch_requests_total",
          "counter", "Number of HTTP PATCH requests"}},
        {"httpReqsPost",
         {"arangodb_http_request_statistics_http_post_requests_total",
          "counter", "Number of HTTP POST requests"}},
        {"httpReqsPut",
         {"arangodb_http_request_statistics_http_put_requests_total", "counter",
          "Number of HTTP PUT requests"}},
        {"httpReqsOther",
         {"arangodb_http_request_statistics_other_http_requests_total",
          "counter", "Number of other HTTP requests"}},
        {"uptime",
         {"arangodb_server_statistics_server_uptime_total", "counter",
          "Number of seconds elapsed since server start"}},
        {"physicalSize",
         {"arangodb_server_statistics_physical_memory", "gauge",
          "Physical memory in bytes"}},
        {"effectivePhysicalSize",
         {"arangodb_server_statistics_effective_physical_memory", "gauge",
          "Effective physical memory in bytes"}},
        {"cores",
         {"arangodb_server_statistics_cpu_cores", "gauge",
          "Number of CPU cores visible to the arangod process"}},
        {"cgroupVersion",
         {"arangodb_server_statistics_cpu_cgroup_version", "gauge",
          "CGroup version detected (0=none, 1=v1, 2=v2)"}},
        {"userPercent",
         {"arangodb_server_statistics_user_percent", "gauge",
          "Percentage of time that the system CPUs have spent in user mode"}},
        {"systemPercent",
         {"arangodb_server_statistics_system_percent", "gauge",
          "Percentage of time that the system CPUs have spent in kernel "
          "mode"}},
        {"idlePercent",
         {"arangodb_server_statistics_idle_percent", "gauge",
          "Percentage of time that the system CPUs have been idle"}},
        {"iowaitPercent",
         {"arangodb_server_statistics_iowait_percent", "gauge",
          "Percentage of time that the system CPUs have been waiting for "
          "I/O"}},
        {"effectiveCores",
         {"arangodb_server_statistics_effective_cpu_cores", "gauge",
          "Number of effective CPU cores set for the arangod process"}},
    };

#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
using StatBuilder =
    std::unordered_map<std::string_view,
                       std::unique_ptr<arangodb::metrics::Builder const> const>;
auto makeStatBuilder(
    std::initializer_list<
        std::pair<std::string_view, arangodb::metrics::Builder const* const>>
        initList) -> StatBuilder {
  auto unomap = StatBuilder{};
  unomap.reserve(initList.size());
  for (auto const& it : initList) {
    unomap.emplace(it.first, it.second);
  }
  return unomap;
}
auto const statBuilder = makeStatBuilder({
    {"bytesReceived",
     new arangodb_client_connection_statistics_bytes_received()},
    {"bytesSent", new arangodb_client_connection_statistics_bytes_sent()},
    {"bytesReceivedUser",
     new arangodb_client_user_connection_statistics_bytes_received()},
    {"bytesSentUser",
     new arangodb_client_user_connection_statistics_bytes_sent()},
    {"minorPageFaults",
     new arangodb_process_statistics_minor_page_faults_total()},
    {"majorPageFaults",
     new arangodb_process_statistics_major_page_faults_total()},
    {"userTime", new arangodb_process_statistics_user_time()},
    {"systemTime", new arangodb_process_statistics_system_time()},
    {"numberOfThreads", new arangodb_process_statistics_number_of_threads()},
    {"residentSize", new arangodb_process_statistics_resident_set_size()},
    {"residentSizePercent",
     new arangodb_process_statistics_resident_set_size_percent()},
    {"virtualSize", new arangodb_process_statistics_virtual_memory_size()},
    {"totalTime", new arangodb_client_connection_statistics_total_time()},
    {"totalTimeCount", nullptr},
    {"totalTimeSum", nullptr},
    {"requestTime", new arangodb_client_connection_statistics_request_time()},
    {"requestTimeCount", nullptr},
    {"requestTimeSum", nullptr},
    {"queueTime", new arangodb_client_connection_statistics_queue_time()},
    {"queueTimeCount", nullptr},
    {"queueTimeSum", nullptr},
    {"ioTime", new arangodb_client_connection_statistics_io_time()},
    {"ioTimeCount", nullptr},
    {"ioTimeSum", nullptr},
    {"httpReqsTotal",
     new arangodb_http_request_statistics_total_requests_total()},
    {"httpReqsSuperuser",
     new arangodb_http_request_statistics_superuser_requests_total()},
    {"httpReqsUser",
     new arangodb_http_request_statistics_user_requests_total()},
    {"httpReqsAsync",
     new arangodb_http_request_statistics_async_requests_total()},
    {"httpReqsDelete",
     new arangodb_http_request_statistics_http_delete_requests_total()},
    {"httpReqsGet",
     new arangodb_http_request_statistics_http_get_requests_total()},
    {"httpReqsHead",
     new arangodb_http_request_statistics_http_head_requests_total()},
    {"httpReqsOptions",
     new arangodb_http_request_statistics_http_options_requests_total()},
    {"httpReqsPatch",
     new arangodb_http_request_statistics_http_patch_requests_total()},
    {"httpReqsPost",
     new arangodb_http_request_statistics_http_post_requests_total()},
    {"httpReqsPut",
     new arangodb_http_request_statistics_http_put_requests_total()},
    {"httpReqsOther",
     new arangodb_http_request_statistics_other_http_requests_total()},
    {"uptime", new arangodb_server_statistics_server_uptime_total()},
    {"physicalSize", new arangodb_server_statistics_physical_memory()},
    {"effectivePhysicalSize",
     new arangodb_server_statistics_effective_physical_memory()},
    {"cores", new arangodb_server_statistics_cpu_cores()},
    {"effectiveCores", new arangodb_server_statistics_effective_cpu_cores()},
    {"cgroupVersion", new arangodb_server_statistics_cpu_cgroup_version()},
    {"userPercent", new arangodb_server_statistics_user_percent()},
    {"systemPercent", new arangodb_server_statistics_system_percent()},
    {"idlePercent", new arangodb_server_statistics_idle_percent()},
    {"iowaitPercent", new arangodb_server_statistics_iowait_percent()},
});
#endif

void appendMetric(std::string& result, std::string const& val,
                  std::string const& label, std::string_view globals,
                  bool ensureWhitespace) {
  auto const& stat = statStrings.at(label);
  auto const name = stat[0];
  auto const type = stat[1];
  auto const help = stat[2];
  arangodb::metrics::Metric::addInfo(result, name, help, type);
  arangodb::metrics::Metric::addMark(result, name, globals, "");
  absl::StrAppend(&result, ensureWhitespace ? " " : "", val, "\n");
}

void appendMetricWithMachineId(std::string& result, std::string const& val,
                               std::string const& label,
                               std::string_view machineId,
                               std::string_view globals,
                               bool ensureWhitespace) {
  auto const& stat = statStrings.at(label);
  auto const name = stat[0];
  auto const type = stat[1];
  auto const help = stat[2];
  arangodb::metrics::Metric::addInfo(result, name, help, type);
  std::string labels = absl::StrCat("machine_id=\"", machineId, "\"");
  if (!globals.empty()) {
    labels = absl::StrCat(labels, ",", globals);
  }
  absl::StrAppend(&result, name, "{", labels, "}",
                  (ensureWhitespace ? " " : ""), val, "\n");
}

}  // namespace

void MetricsFeature::toPrometheus(std::string& result,
                                  MetricsParts metricsParts,
                                  CollectMode mode) const {
  // minimize reallocs
  result.reserve(64 * 1024);

  if (metricsParts.includeStandardMetrics()) {
    // QueryRegistryFeature only provides standard metrics.
    // update only necessary if these metrics should be included
    // in the output
    _queryRegistryFeature->updateMetrics();
  }

  bool hasGlobals = false;
  {
    auto lock = initGlobalLabels();
    hasGlobals = hasShortname && hasRole;
    std::string_view last;
    std::string_view curr;
    for (auto const& i : _registry) {
      TRI_ASSERT(i.second);
      if (i.second->isDynamic()) {
        if (!metricsParts.includeDynamicMetrics()) {
          continue;
        }
      } else {
        if (!metricsParts.includeStandardMetrics()) {
          continue;
        }
      }
      curr = i.second->name();
      if (last != curr) {
        last = curr;
        Metric::addInfo(result, curr, i.second->help(), i.second->type());
      }
      i.second->toPrometheus(result, _globals, _options.ensureWhitespace);
    }
    for (auto const& [_, batch] : _batch) {
      TRI_ASSERT(batch);
      // TODO(MBkkt) merge vector::reserve's between IBatch::toPrometheus
      batch->toPrometheus(result, _globals, _options.ensureWhitespace);
    }
  }

  if (metricsParts.includeStandardMetrics()) {
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
    bool foundError = false;
    for (auto const& it : statBuilder) {
      if (auto const& statIt = statStrings.find(it.first);
          statIt != statStrings.end()) {
        if (it.second != nullptr) {
          auto const& builder = *it.second;
          auto const& stat = statIt->second;
          auto const name = stat[0];
          auto const type = stat[1];
          if (builder.name() != name) {
            foundError = true;
            LOG_TOPIC("f66dd", ERR, arangodb::Logger::STATISTICS)
                << "Statistic '" << it.first << "' has mismatching names: '"
                << builder.name() << "' in statBuilder but '" << name
                << "' in statStrings";
          }
          if (builder.type() != type) {
            foundError = true;
            LOG_TOPIC("9fe22", ERR, arangodb::Logger::STATISTICS)
                << "Statistic '" << it.first
                << "' has mismatching types (for API v2): '" << builder.type()
                << "' in statBuilder but '" << type << "' in statStrings";
          }
        }
      } else {
        foundError = true;
        LOG_TOPIC("015da", ERR, arangodb::Logger::STATISTICS)
            << "Statistic '" << it.first
            << "' defined in statBuilder, but not in statStrings";
      }
    }
    for (auto const& it : statStrings) {
      if (statBuilder.find(it.first) == statBuilder.end()) {
        foundError = true;
        LOG_TOPIC("eedac", ERR, arangodb::Logger::STATISTICS)
            << "Statistic '" << it.first
            << "' defined in statStrings, but not in statBuilder";
      }
    }
    if (foundError) {
      FATAL_ERROR_EXIT();
    }
#endif

    ProcessInfo info = TRI_ProcessInfoSelf();
    uint64_t rss = static_cast<uint64_t>(info._residentSize);
    double rssp = 0;
    if (PhysicalMemory::getValue() != 0) {
      rssp = static_cast<double>(rss) /
             static_cast<double>(PhysicalMemory::getValue());
    }

    appendMetric(result, std::to_string(info._minorPageFaults),
                 "minorPageFaults", _globals, _options.ensureWhitespace);
    appendMetric(result, std::to_string(info._majorPageFaults),
                 "majorPageFaults", _globals, _options.ensureWhitespace);
    if (info._scClkTck != 0) {
      appendMetric(result,
                   std::to_string(static_cast<double>(info._userTime) /
                                  static_cast<double>(info._scClkTck)),
                   "userTime", _globals, _options.ensureWhitespace);
      appendMetric(result,
                   std::to_string(static_cast<double>(info._systemTime) /
                                  static_cast<double>(info._scClkTck)),
                   "systemTime", _globals, _options.ensureWhitespace);
    }
    appendMetric(result, std::to_string(info._numberThreads), "numberOfThreads",
                 _globals, _options.ensureWhitespace);
    appendMetric(result, std::to_string(rss), "residentSize", _globals,
                 _options.ensureWhitespace);
    appendMetric(result, std::to_string(rssp), "residentSizePercent", _globals,
                 _options.ensureWhitespace);
    appendMetric(result, std::to_string(info._virtualSize), "virtualSize",
                 _globals, _options.ensureWhitespace);
    appendMetric(result, std::to_string(PhysicalMemory::getValue()),
                 "physicalSize", _globals, _options.ensureWhitespace);
    appendMetric(result, std::to_string(uptime()), "uptime", _globals,
                 _options.ensureWhitespace);
    appendMetric(result, std::to_string(NumberOfCores::getValue()), "cores",
                 _globals, _options.ensureWhitespace);

    {
      auto instance = ServerState::instance();
      std::string machineId;
      if (instance) {
        machineId = instance->getHost();
      }
      appendMetricWithMachineId(
          result, std::to_string(NumberOfCores::getEffectiveValue()),
          "effectiveCores", machineId, _globals, _options.ensureWhitespace);
      appendMetricWithMachineId(
          result, std::to_string(PhysicalMemory::getEffectiveValue()),
          "effectivePhysicalSize", machineId, _globals,
          _options.ensureWhitespace);
    }
    appendMetric(
        result,
        std::to_string(static_cast<std::underlying_type_t<cgroup::Version>>(
            cgroup::getVersion())),
        "cgroupVersion", _globals, _options.ensureWhitespace);

    CpuUsageFeature& cpuUsage = server().getFeature<CpuUsageFeature>();
    if (cpuUsage.isEnabled()) {
      auto snapshot = cpuUsage.snapshot();
      appendMetric(result, std::to_string(snapshot.userPercent()),
                   "userPercent", _globals, _options.ensureWhitespace);
      appendMetric(result, std::to_string(snapshot.systemPercent()),
                   "systemPercent", _globals, _options.ensureWhitespace);
      appendMetric(result, std::to_string(snapshot.idlePercent()),
                   "idlePercent", _globals, _options.ensureWhitespace);
      appendMetric(result, std::to_string(snapshot.iowaitPercent()),
                   "iowaitPercent", _globals, _options.ensureWhitespace);
    }

    // Storage engine only provides standard metrics
    auto& es = _engineSelectorFeature->engine();
    if (es.typeName() == RocksDBEngine::kEngineName) {
      es.toPrometheus(result, _globals, _options.ensureWhitespace);
    }

    // ClusterMetricsFeature only provides standard metrics
    if (hasGlobals && _clusterMetricsFeature->isEnabled() &&
        mode != CollectMode::Local) {
      _clusterMetricsFeature->toPrometheus(result, _globals,
                                           _options.ensureWhitespace);
    }

    // agency node metrics only provide standard metrics
    consensus::Node::toPrometheus(result, _globals, _options.ensureWhitespace);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// Sets metrics that can be collected by ClusterMetricsFeature
////////////////////////////////////////////////////////////////////////////////
constexpr auto kCoordinatorBatch = frozen::make_unordered_set<frozen::string>({
    "arangodb_search_link_stats",
});

constexpr auto kCoordinatorMetrics =
    frozen::make_unordered_set<frozen::string>({
        "arangodb_search_num_failed_commits",
        "arangodb_search_num_failed_cleanups",
        "arangodb_search_num_failed_consolidations",
        "arangodb_search_commit_time",
        "arangodb_search_cleanup_time",
        "arangodb_search_consolidation_time",
    });

void MetricsFeature::toVPack(velocypack::Builder& builder,
                             MetricsParts metricsParts) const {
  builder.openArray(true);
  std::shared_lock lock{_mutex};
  for (auto const& i : _registry) {
    TRI_ASSERT(i.second);
    auto const name = i.second->name();
    if (kCoordinatorMetrics.count(name)) {
      i.second->toVPack(builder);
    }
  }
  auto& ci = _clusterFeature->clusterInfo();
  for (auto const& [name, batch] : _batch) {
    TRI_ASSERT(batch);
    if (kCoordinatorBatch.count(name)) {
      batch->toVPack(builder, ci);
    }
  }
  lock.unlock();
  builder.close();
}

TransactionStatistics& MetricsFeature::transactionStatistics() noexcept {
  return *_transactionStatistics;
}

double MetricsFeature::uptime() const noexcept {
  return TRI_microtime() - _startTime;
}

std::shared_lock<std::shared_mutex> MetricsFeature::initGlobalLabels() const {
  std::shared_lock sharedLock{_mutex};
  auto instance = ServerState::instance();
  if (!instance || (hasShortname && hasRole)) {
    return sharedLock;
  }
  sharedLock.unlock();
  std::unique_lock uniqueLock{_mutex};
  if (!hasShortname) {
    // Very early after a server start it is possible that the short name
    // isn't yet known. This check here is to prevent that the label is
    // permanently empty if metrics are requested too early.
    if (auto shortname = instance->getShortName(); !shortname.empty()) {
      _globals = absl::StrCat("shortname=\"", shortname, "\"",
                              (_globals.empty() ? "" : ","), _globals);
      hasShortname = true;
    }
  }
  if (!hasRole) {
    if (auto role = instance->getRole(); role != ServerState::ROLE_UNDEFINED) {
      absl::StrAppend(&_globals, (_globals.empty() ? "" : ","), "role=\"",
                      ServerState::roleToString(role), "\"");
      hasRole = true;
    }
  }
  uniqueLock.unlock();
  sharedLock.lock();
  return sharedLock;
}

std::pair<std::shared_lock<std::shared_mutex>, metrics::IBatch*>
MetricsFeature::getBatch(std::string_view name) const {
  std::shared_lock lock{_mutex};
  metrics::IBatch* batch = nullptr;
  if (auto it = _batch.find(name); it != _batch.end()) {
    batch = it->second.get();
  } else {
    lock.unlock();
    lock.release();
  }
  return {std::move(lock), batch};
}

void MetricsFeature::batchRemove(std::string_view name,
                                 std::string_view labels) {
  std::unique_lock lock{_mutex};
  auto it = _batch.find(name);
  if (it == _batch.end()) {
    return;
  }
  TRI_ASSERT(it->second);
  if (it->second->remove(labels) == 0) {
    _batch.erase(name);
  }
}

void MetricsFeature::prepare() {
  _queryRegistryFeature = std::move(_lazyQueryRegistryFeatureRef).get();
  _engineSelectorFeature = std::move(_lazyEngineSelectorFeatureRef).get();
  _clusterMetricsFeature = std::move(_lazyClusterMetricsFeatureRef).get();
  _clusterFeature = std::move(_lazyClusterFeatureRef).get();
}

}  // namespace arangodb::metrics
