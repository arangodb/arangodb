////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2022 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Apache License, Version 2.0 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     http://www.apache.org/licenses/LICENSE-2.0
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

#include "ApplicationFeatures/ApplicationServer.h"
#include "ApplicationFeatures/GreetingsFeaturePhase.h"
#include "Basics/application-exit.h"
#include "Basics/debugging.h"
#include "Cluster/ServerState.h"
#include "Logger/LoggerFeature.h"
#include "Metrics/Metric.h"
#include "ProgramOptions/ProgramOptions.h"
#include "ProgramOptions/Section.h"
#include "RestServer/QueryRegistryFeature.h"
#include "RocksDBEngine/RocksDBEngine.h"
#include "Statistics/StatisticsFeature.h"
#include "StorageEngine/EngineSelectorFeature.h"

#include <frozen/string.h>
#include <frozen/unordered_map.h>
#include <frozen/unordered_set.h>
#include <chrono>

namespace arangodb::metrics {
namespace {

constexpr std::string_view kArangoDbConnection = "arangodb_connection_";
constexpr std::string_view kPoolAgencyComm = "pool=\"AgencyComm\"";
constexpr std::string_view kPoolClusterComm = "pool=\"ClusterComm\"";

constexpr auto kNameVersionTable = frozen::make_unordered_map<frozen::string,
                                                              frozen::string>({
    {
        "arangodb_agency_cache_callback_number",
        "arangodb_agency_cache_callback_count",
    },
    {
        "arangodb_agency_callback_number",
        "arangodb_agency_callback_count",
    },
    {
        "arangodb_agency_supervision_failed_server_total",
        "arangodb_agency_supervision_failed_server_count",
    },
    {
        "arangodb_refused_followers_total",
        "arangodb_refused_followers_count",
    },
    {
        "arangodb_dropped_followers_total",
        "arangodb_dropped_followers_count",
    },
    {
        "arangodb_rocksdb_write_stalls_total",
        "rocksdb_write_stalls",
    },
    {
        "arangodb_rocksdb_write_stops_total",
        "rocksdb_write_stops",
    },
    {
        "arangodb_shards_leader_number",
        "arangodb_shards_leader_count",
    },
    {
        "arangodb_shards_number",
        "arangodb_shards_total_count",
    },
    {
        "arangodb_replication_cluster_inventory_requests_total",
        "arangodb_replication_cluster_inventory_requests",
    },
    {
        "arangodb_replication_dump_requests_total",
        "arangodb_replication_dump_requests",
    },
    {
        "arangodb_replication_dump_bytes_received_total",
        "arangodb_replication_dump_bytes_received",
    },
    {
        "arangodb_replication_dump_documents_total",
        "arangodb_replication_dump_documents",
    },
    {
        "arangodb_replication_dump_request_time_total",
        "arangodb_replication_dump_request_time",
    },
    {
        "arangodb_replication_dump_apply_time_total",
        "arangodb_replication_dump_apply_time",
    },
    {
        "arangodb_replication_initial_sync_keys_requests_total",
        "arangodb_replication_initial_sync_keys_requests",
    },
    {
        "arangodb_replication_initial_sync_docs_requests_total",
        "arangodb_replication_initial_sync_docs_requests",
    },
    {
        "arangodb_replication_initial_sync_docs_requested_total",
        "arangodb_replication_initial_sync_docs_requested",
    },
    {
        "arangodb_replication_initial_sync_docs_inserted_total",
        "arangodb_replication_initial_sync_docs_inserted",
    },
    {
        "arangodb_replication_initial_sync_docs_removed_total",
        "arangodb_replication_initial_sync_docs_removed",
    },
    {
        "arangodb_replication_initial_sync_bytes_received_total",
        "arangodb_replication_initial_sync_bytes_received",
    },
    {
        "arangodb_replication_initial_chunks_requests_time_total",
        "arangodb_replication_initial_chunks_requests_time",
    },
    {
        "arangodb_replication_initial_keys_requests_time_total",
        "arangodb_replication_initial_keys_requests_time",
    },
    {
        "arangodb_replication_initial_docs_requests_time_total",
        "arangodb_replication_initial_docs_requests_time",
    },
    {
        "arangodb_replication_initial_insert_apply_time_total",
        "arangodb_replication_initial_insert_apply_time",
    },
    {
        "arangodb_replication_initial_remove_apply_time_total",
        "arangodb_replication_initial_remove_apply_time",
    },
    {
        "arangodb_replication_tailing_requests_total",
        "arangodb_replication_tailing_requests",
    },
    {
        "arangodb_replication_tailing_follow_tick_failures_total",
        "arangodb_replication_tailing_follow_tick_failures",
    },
    {
        "arangodb_replication_tailing_markers_total",
        "arangodb_replication_tailing_markers",
    },
    {
        "arangodb_replication_tailing_documents_total",
        "arangodb_replication_tailing_documents",
    },
    {
        "arangodb_replication_tailing_removals_total",
        "arangodb_replication_tailing_removals",
    },
    {
        "arangodb_replication_tailing_bytes_received_total",
        "arangodb_replication_tailing_bytes_received",
    },
    {
        "arangodb_replication_failed_connects_total",
        "arangodb_replication_failed_connects",
    },
    {
        "arangodb_replication_tailing_request_time_total",
        "arangodb_replication_tailing_request_time",
    },
    {
        "arangodb_replication_tailing_apply_time_total",
        "arangodb_replication_tailing_apply_time",
    },
    {
        "arangodb_replication_synchronous_requests_total_time_total",
        "arangodb_replication_synchronous_requests_total_time",
    },
    {
        "arangodb_replication_synchronous_requests_total_number_total",
        "arangodb_replication_synchronous_requests_total_number",
    },
    {
        "arangodb_aql_all_query_total",
        "arangodb_aql_all_query",
    },
    {
        "arangodb_aql_slow_query_total",
        "arangodb_aql_slow_query",
    },
    {
        "arangodb_aql_total_query_time_msec_total",
        "arangodb_aql_total_query_time_msec",
    },
    {
        "arangodb_collection_lock_acquisition_micros_total",
        "arangodb_collection_lock_acquisition_micros",
    },
    {
        "arangodb_collection_lock_sequential_mode_total",
        "arangodb_collection_lock_sequential_mode",
    },
    {
        "arangodb_collection_lock_timeouts_exclusive_total",
        "arangodb_collection_lock_timeouts_exclusive",
    },
    {
        "arangodb_collection_lock_timeouts_write_total",
        "arangodb_collection_lock_timeouts_write",
    },
    {
        "arangodb_transactions_aborted_total",
        "arangodb_transactions_aborted",
    },
    {
        "arangodb_transactions_committed_total",
        "arangodb_transactions_committed",
    },
    {
        "arangodb_transactions_started_total",
        "arangodb_transactions_started",
    },
    {
        "arangodb_intermediate_commits_total",
        "arangodb_intermediate_commits",
    },
    {
        "arangodb_collection_truncates_total",
        "arangodb_collection_truncates",
    },
    {
        "arangodb_collection_truncates_replication_total",
        "arangodb_collection_truncates_replication",
    },
    {
        "arangodb_document_writes_total",
        "arangodb_document_writes",
    },
    {
        "arangodb_document_writes_replication_total",
        "arangodb_document_writes_replication",
    },
    {
        "arangodb_agency_callback_registered_total",
        "arangodb_agency_callback_registered",
    },
    {
        "arangodb_heartbeat_failures_total",
        "arangodb_heartbeat_failures",
    },
    {
        "arangodb_sync_wrong_checksum_total",
        "arangodb_sync_wrong_checksum",
    },
    {
        "arangodb_maintenance_phase1_accum_runtime_msec_total",
        "arangodb_maintenance_phase1_accum_runtime_msec",
    },
    {
        "arangodb_maintenance_phase2_accum_runtime_msec_total",
        "arangodb_maintenance_phase2_accum_runtime_msec",
    },
    {
        "arangodb_maintenance_agency_sync_accum_runtime_msec_total",
        "arangodb_maintenance_agency_sync_accum_runtime_msec",
    },
    {
        "arangodb_maintenance_action_duplicate_total",
        "arangodb_maintenance_action_duplicate_counter",
    },
    {
        "arangodb_maintenance_action_registered_total",
        "arangodb_maintenance_action_registered_counter",
    },
    {
        "arangodb_maintenance_action_accum_runtime_msec_total",
        "arangodb_maintenance_action_accum_runtime_msec",
    },
    {
        "arangodb_maintenance_action_accum_queue_time_msec_total",
        "arangodb_maintenance_action_accum_queue_time_msec",
    },
    {
        "arangodb_maintenance_action_failure_total",
        "arangodb_maintenance_action_failure_counter",
    },
    {
        "arangodb_maintenance_action_done_total",
        "arangodb_maintenance_action_done_counter",
    },
    {
        "arangodb_load_current_accum_runtime_msec_total",
        "arangodb_load_current_accum_runtime_msec",
    },
    {
        "arangodb_load_plan_accum_runtime_msec_total",
        "arangodb_load_plan_accum_runtime_msec",
    },
    {
        "arangodb_network_forwarded_requests_total",
        "arangodb_network_forwarded_requests",
    },
    {
        "arangodb_network_request_timeouts_total",
        "arangodb_network_request_timeouts",
    },
    {
        "arangodb_connection_pool_leases_successful_total",
        "arangodb_connection_leases_successful",
    },
    {
        "arangodb_connection_pool_leases_failed_total",
        "arangodb_connection_pool_leases_failed",
    },
    {
        "arangodb_connection_pool_connections_created_total",
        "arangodb_connection_pool_connections_created",
    },
    {
        "arangodb_connection_pool_connections_current",
        "arangodb_connection_connections_current",
    },
    {
        "arangodb_agency_supervision_accum_runtime_msec_total",
        "arangodb_agency_supervision_accum_runtime_msec",
    },
    {
        // clang-format off
          "arangodb_agency_supervision_accum_runtime_wait_for_replication_msec_total",  // clang-format on
        "arangodb_agency_supervision_accum_runtime_wait_for_replication_msec",
    },
    {
        "arangodb_transactions_expired_total",
        "arangodb_transactions_expired",
    },
    {
        "arangodb_agency_read_no_leader_total",
        "arangodb_agency_read_no_leader",
    },
    {
        "arangodb_agency_read_ok_total",
        "arangodb_agency_read_ok",
    },
    {
        "arangodb_agency_write_no_leader_total",
        "arangodb_agency_write_no_leader",
    },
    {
        "arangodb_agency_write_ok_total",
        "arangodb_agency_write_ok",
    },
    {
        "arangodb_v8_context_created_total",
        "arangodb_v8_context_created",
    },
    {
        "arangodb_v8_context_creation_time_msec_total",
        "arangodb_v8_context_creation_time_msec",
    },
    {
        "arangodb_v8_context_destroyed_total",
        "arangodb_v8_context_destroyed",
    },
    {
        "arangodb_v8_context_entered_total",
        "arangodb_v8_context_entered",
    },
    {
        "arangodb_v8_context_enter_failures_total",
        "arangodb_v8_context_enter_failures",
    },
    {
        "arangodb_v8_context_exited_total",
        "arangodb_v8_context_exited",
    },
    {
        "arangodb_scheduler_queue_full_failures_total",
        "arangodb_scheduler_queue_full_failures",
    },
    {
        "arangodb_scheduler_threads_started_total",
        "arangodb_scheduler_threads_started",
    },
    {
        "arangodb_scheduler_threads_stopped_total",
        "arangodb_scheduler_threads_stopped",
    },
    {
        "arangodb_scheduler_num_awake_threads",
        "arangodb_scheduler_awake_threads",
    },
    // For the sake of completeness, we add the renamings from v1 to v2 from
    // the statistics feature:
    {
        "arangodb_process_statistics_minor_page_faults_total",
        "arangodb_process_statistics_minor_page_faults",
    },
    {
        "arangodb_process_statistics_major_page_faults_total",
        "arangodb_process_statistics_major_page_faults",
    },
    {
        "arangodb_http_request_statistics_total_requests_total",
        "arangodb_http_request_statistics_total_requests",
    },
    {
        "arangodb_http_request_statistics_superuser_requests_total",
        "arangodb_http_request_statistics_superuser_requests",
    },
    {
        "arangodb_http_request_statistics_user_requests_total",
        "arangodb_http_request_statistics_user_requests",
    },
    {
        "arangodb_http_request_statistics_async_requests_total",
        "arangodb_http_request_statistics_async_requests",
    },
    {
        "arangodb_http_request_statistics_http_delete_requests_total",
        "arangodb_http_request_statistics_http_delete_requests",
    },
    {
        "arangodb_http_request_statistics_http_get_requests_total",
        "arangodb_http_request_statistics_http_get_requests",
    },
    {
        "arangodb_http_request_statistics_http_head_requests_total",
        "arangodb_http_request_statistics_http_head_requests",
    },
    {
        "arangodb_http_request_statistics_http_options_requests_total",
        "arangodb_http_request_statistics_http_options_requests",
    },
    {
        "arangodb_http_request_statistics_http_patch_requests_total",
        "arangodb_http_request_statistics_http_patch_requests",
    },
    {
        "arangodb_http_request_statistics_http_post_requests_total",
        "arangodb_http_request_statistics_http_post_requests",
    },
    {
        "arangodb_http_request_statistics_http_put_requests_total",
        "arangodb_http_request_statistics_http_put_requests",
    },
    {
        "arangodb_http_request_statistics_other_http_requests_total",
        "arangodb_http_request_statistics_other_http_requests",
    },
    {
        "arangodb_server_statistics_server_uptime_total",
        "arangodb_server_statistics_server_uptime",
    },
    // And one for rocksdb:
    {
        "rocksdb_engine_throttle_bps",
        "rocksdbengine_throttle_bps",
    },
});

constexpr auto kSuppressionsV2 = frozen::make_unordered_set<frozen::string>({
    // Note that if we ever need to suppress a metric which is
    // coming from the statistics feature further code is needed
    // there. This list is only considered for the normally registered
    // metrics in the MetricsFeature.
    "arangodb_maintenance_phase1_accum_runtime_msec_total",
    "arangodb_maintenance_phase2_accum_runtime_msec_total",
    "arangodb_maintenance_agency_sync_accum_runtime_msec_total",
    "arangodb_maintenance_action_accum_runtime_msec_total",
    "arangodb_maintenance_action_accum_queue_time_msec_total",
    "arangodb_agency_supervision_accum_runtime_msec_total",
    "arangodb_agency_supervision_accum_runtime_wait_for_replication_msec_total",
    "arangodb_load_current_accum_runtime_msec_total",
    "arangodb_load_plan_accum_runtime_msec_total",
    "arangodb_aql_slow_query_total",
    "arangodb_scheduler_jobs_dequeued",
    "arangodb_scheduler_jobs_submitted",
    "arangodb_scheduler_jobs_done",
});

constexpr auto kSuppressionsV1 = frozen::make_unordered_set<frozen::string>({
    "arangodb_scheduler_jobs_dequeued_total",
    "arangodb_scheduler_jobs_submitted_total",
    "arangodb_scheduler_jobs_done_total",
});

}  // namespace

MetricsFeature::MetricsFeature(application_features::ApplicationServer& server)
    : ApplicationFeature{server, "Metrics"},
      _export{true},
      _exportReadWriteMetrics{false} {
  setOptional(false);
  startsAfter<LoggerFeature>();
  startsBefore<application_features::GreetingsFeaturePhase>();
}

void MetricsFeature::collectOptions(
    std::shared_ptr<options::ProgramOptions> options) {
  _serverStatistics =
      std::make_unique<ServerStatistics>(*this, StatisticsFeature::time());

  options
      ->addOption(
          "--server.export-metrics-api", "turn metrics API on or off",
          new options::BooleanParameter(&_export),
          arangodb::options::makeDefaultFlags(arangodb::options::Flags::Hidden))
      .setIntroducedIn(30600);

  options
      ->addOption(
          "--server.export-read-write-metrics",
          "turn metrics for document read/write metrics on or off",
          new options::BooleanParameter(&_exportReadWriteMetrics),
          arangodb::options::makeDefaultFlags(arangodb::options::Flags::Hidden))
      .setIntroducedIn(30707);
}

std::shared_ptr<Metric> MetricsFeature::doAdd(Builder& builder) {
  auto metric = builder.build();
  MetricKey key{metric->name(), metric->labels()};
  bool success = false;
  {
    std::lock_guard guard(_lock);
    success = _registry.try_emplace(key, metric).second;
  }
  if (!success) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                   std::string{builder.type()} +
                                       std::string{builder.name()} +
                                       " already exists");
  }
  return metric;
}

Metric* MetricsFeature::get(MetricKey const& key) {
  std::lock_guard guard{_lock};
  auto it = _registry.find(key);
  if (it == _registry.end()) {
    return nullptr;
  }
  return it->second.get();
}

bool MetricsFeature::remove(Builder const& builder) {
  MetricKey key{builder.name(), builder.labels()};
  std::lock_guard guard{_lock};
  return _registry.erase(key) != 0;
}

bool MetricsFeature::exportAPI() const { return _export; }

bool MetricsFeature::exportReadWriteMetrics() const {
  return _exportReadWriteMetrics;
}

void MetricsFeature::validateOptions(std::shared_ptr<options::ProgramOptions>) {
  if (_exportReadWriteMetrics) {
    serverStatistics().setupDocumentMetrics();
  }
}

void MetricsFeature::toPrometheus(std::string& result, bool v2) const {
  // minimize reallocs
  result.reserve(32768);

  // QueryRegistryFeature
  auto& q = server().getFeature<QueryRegistryFeature>();
  q.updateMetrics();
  {
    bool changed = false;

    std::lock_guard guard(_lock);
    if (!_globalLabels.contains("shortname")) {
      std::string shortName = ServerState::instance()->getShortName();
      // Very early after a server start it is possible that the
      // short name is not yet known. This check here is to prevent
      // that the label is permanently empty if metrics are requested
      // too early.
      if (!shortName.empty()) {
        _globalLabels.try_emplace("shortname", std::move(shortName));
        changed = true;
      }
    }
    if (!_globalLabels.contains("role") && ServerState::instance() != nullptr &&
        ServerState::instance()->getRole() != ServerState::ROLE_UNDEFINED) {
      _globalLabels.try_emplace(
          "role",
          ServerState::roleToString(ServerState::instance()->getRole()));
      changed = true;
    }
    if (changed) {
      bool first = true;
      for (auto const& i : _globalLabels) {
        if (!first) {
          _globalLabelsStr += ",";
        } else {
          first = false;
        }
        _globalLabelsStr += i.first + "=\"" + i.second + "\"";
      }
    }

    std::string lastType;
    std::string name;
    for (auto const& i : _registry) {
      name.clear();
      name.append(i.second->name());
      bool empty = true;
      if (!v2) {
        if (kSuppressionsV1.count({name.data(), name.size()})) {
          continue;
        }
        // In v1 we do a name conversion. Note that we set
        // alternativeName == name in the end, in v2 though,
        // alternativeName is empty and no conversion happens.
        auto it = kNameVersionTable.find({name.data(), name.size()});
        if (it != kNameVersionTable.end()) {
          name.clear();
          name.append(it->second.data(), it->second.size());
        }
        if (name.compare(0, 20, kArangoDbConnection) == 0) {
          auto const labels = i.second->labels();
          if (labels == kPoolAgencyComm) {
            name.append("_AgencyComm");
          } else if (labels == kPoolClusterComm) {
            name.append("_ClusterComm");
          } else {
            // Avoid someone sneaking in an other connection
            // pool without dedicated metric for v1
            TRI_ASSERT(false);
          }
        }
        empty = false;
      } else if (kSuppressionsV2.count({name.data(), name.size()})) {
        continue;
      }
      if (lastType != name) {
        i.second->toPrometheusBegin(result, name);
        lastType = name;
      }
      i.second->toPrometheus(result, _globalLabelsStr, empty ? "" : name);
      result.push_back('\n');
    }
  }

  // StatisticsFeature
  auto& sf = server().getFeature<StatisticsFeature>();
  sf.toPrometheus(result,
                  std::chrono::duration<double, std::milli>(
                      std::chrono::system_clock::now().time_since_epoch())
                      .count(),
                  v2);

  // RocksDBEngine
  auto& es = server().getFeature<EngineSelectorFeature>().engine();
  std::string const& engineName = es.typeName();
  if (engineName == RocksDBEngine::EngineName) {
    es.getStatistics(result, v2);
  }
}

ServerStatistics& MetricsFeature::serverStatistics() noexcept {
  return *_serverStatistics;
}

}  // namespace arangodb::metrics
