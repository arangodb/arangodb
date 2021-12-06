////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2021 ArangoDB GmbH, Cologne, Germany
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
#include "Logger/LogMacros.h"
#include "Logger/Logger.h"
#include "Logger/LoggerFeature.h"
#include "Logger/LoggerStream.h"
#include "Metrics/Metric.h"
#include "ProgramOptions/ProgramOptions.h"
#include "ProgramOptions/Section.h"
#include "RestServer/QueryRegistryFeature.h"
#include "RocksDBEngine/RocksDBEngine.h"
#include "Statistics/StatisticsFeature.h"
#include "StorageEngine/EngineSelectorFeature.h"

#include <chrono>

namespace arangodb::metrics {
namespace {

constexpr std::string_view kArangoDbConnection = "arangodb_connection_";
constexpr std::string_view kPoolAgencyComm = "pool=\"AgencyComm\"";
constexpr std::string_view kPoolClusterComm = "pool=\"ClusterComm\"";

}  // namespace

MetricsFeature::MetricsFeature(application_features::ApplicationServer& server)
    : ApplicationFeature{server, "Metrics"}, _export{true}, _exportReadWriteMetrics{false} {
  setOptional(false);
  startsAfter<LoggerFeature>();
  startsBefore<application_features::GreetingsFeaturePhase>();

  nameVersionTable.try_emplace("arangodb_agency_cache_callback_number",
                               "arangodb_agency_cache_callback_count");
  nameVersionTable.try_emplace("arangodb_agency_callback_number",
                               "arangodb_agency_callback_count");
  nameVersionTable.try_emplace(
      "arangodb_agency_supervision_failed_server_total",
      "arangodb_agency_supervision_failed_server_count");
  nameVersionTable.try_emplace("arangodb_refused_followers_total",
                               "arangodb_refused_followers_count");
  nameVersionTable.try_emplace("arangodb_dropped_followers_total",
                               "arangodb_dropped_followers_count");
  nameVersionTable.try_emplace("arangodb_rocksdb_write_stalls_total",
                               "rocksdb_write_stalls");
  nameVersionTable.try_emplace("arangodb_rocksdb_write_stops_total",
                               "rocksdb_write_stops");
  nameVersionTable.try_emplace("arangodb_shards_leader_number",
                               "arangodb_shards_leader_count");
  nameVersionTable.try_emplace("arangodb_shards_number",
                               "arangodb_shards_total_count");
  nameVersionTable.try_emplace(
      "arangodb_replication_cluster_inventory_requests_total",
      "arangodb_replication_cluster_inventory_requests");
  nameVersionTable.try_emplace("arangodb_replication_dump_requests_total",
                               "arangodb_replication_dump_requests");
  nameVersionTable.try_emplace("arangodb_replication_dump_bytes_received_total",
                               "arangodb_replication_dump_bytes_received");
  nameVersionTable.try_emplace("arangodb_replication_dump_documents_total",
                               "arangodb_replication_dump_documents");
  nameVersionTable.try_emplace("arangodb_replication_dump_request_time_total",
                               "arangodb_replication_dump_request_time");
  nameVersionTable.try_emplace("arangodb_replication_dump_apply_time_total",
                               "arangodb_replication_dump_apply_time");
  nameVersionTable.try_emplace(
      "arangodb_replication_initial_sync_keys_requests_total",
      "arangodb_replication_initial_sync_keys_requests");
  nameVersionTable.try_emplace(
      "arangodb_replication_initial_sync_docs_requests_total",
      "arangodb_replication_initial_sync_docs_requests");
  nameVersionTable.try_emplace(
      "arangodb_replication_initial_sync_docs_requested_total",
      "arangodb_replication_initial_sync_docs_requested");
  nameVersionTable.try_emplace(
      "arangodb_replication_initial_sync_docs_inserted_total",
      "arangodb_replication_initial_sync_docs_inserted");
  nameVersionTable.try_emplace(
      "arangodb_replication_initial_sync_docs_removed_total",
      "arangodb_replication_initial_sync_docs_removed");
  nameVersionTable.try_emplace(
      "arangodb_replication_initial_sync_bytes_received_total",
      "arangodb_replication_initial_sync_bytes_received");
  nameVersionTable.try_emplace(
      "arangodb_replication_initial_chunks_requests_time_total",
      "arangodb_replication_initial_chunks_requests_time");
  nameVersionTable.try_emplace(
      "arangodb_replication_initial_keys_requests_time_total",
      "arangodb_replication_initial_keys_requests_time");
  nameVersionTable.try_emplace(
      "arangodb_replication_initial_docs_requests_time_total",
      "arangodb_replication_initial_docs_requests_time");
  nameVersionTable.try_emplace(
      "arangodb_replication_initial_insert_apply_time_total",
      "arangodb_replication_initial_insert_apply_time");
  nameVersionTable.try_emplace(
      "arangodb_replication_initial_remove_apply_time_total",
      "arangodb_replication_initial_remove_apply_time");
  nameVersionTable.try_emplace("arangodb_replication_tailing_requests_total",
                               "arangodb_replication_tailing_requests");
  nameVersionTable.try_emplace(
      "arangodb_replication_tailing_follow_tick_failures_total",
      "arangodb_replication_tailing_follow_tick_failures");
  nameVersionTable.try_emplace("arangodb_replication_tailing_markers_total",
                               "arangodb_replication_tailing_markers");
  nameVersionTable.try_emplace("arangodb_replication_tailing_documents_total",
                               "arangodb_replication_tailing_documents");
  nameVersionTable.try_emplace("arangodb_replication_tailing_removals_total",
                               "arangodb_replication_tailing_removals");
  nameVersionTable.try_emplace(
      "arangodb_replication_tailing_bytes_received_total",
      "arangodb_replication_tailing_bytes_received");
  nameVersionTable.try_emplace("arangodb_replication_failed_connects_total",
                               "arangodb_replication_failed_connects");
  nameVersionTable.try_emplace(
      "arangodb_replication_tailing_request_time_total",
      "arangodb_replication_tailing_request_time");
  nameVersionTable.try_emplace("arangodb_replication_tailing_apply_time_total",
                               "arangodb_replication_tailing_apply_time");
  nameVersionTable.try_emplace(
      "arangodb_replication_synchronous_requests_total_time_total",
      "arangodb_replication_synchronous_requests_total_time");
  nameVersionTable.try_emplace(
      "arangodb_replication_synchronous_requests_total_number_total",
      "arangodb_replication_synchronous_requests_total_number");
  nameVersionTable.try_emplace("arangodb_aql_all_query_total",
                               "arangodb_aql_all_query");
  nameVersionTable.try_emplace("arangodb_aql_slow_query_total",
                               "arangodb_aql_slow_query");
  nameVersionTable.try_emplace("arangodb_aql_total_query_time_msec_total",
                               "arangodb_aql_total_query_time_msec");
  nameVersionTable.try_emplace(
      "arangodb_collection_lock_acquisition_micros_total",
      "arangodb_collection_lock_acquisition_micros");
  nameVersionTable.try_emplace("arangodb_collection_lock_sequential_mode_total",
                               "arangodb_collection_lock_sequential_mode");
  nameVersionTable.try_emplace(
      "arangodb_collection_lock_timeouts_exclusive_total",
      "arangodb_collection_lock_timeouts_exclusive");
  nameVersionTable.try_emplace("arangodb_collection_lock_timeouts_write_total",
                               "arangodb_collection_lock_timeouts_write");
  nameVersionTable.try_emplace("arangodb_transactions_aborted_total",
                               "arangodb_transactions_aborted");
  nameVersionTable.try_emplace("arangodb_transactions_committed_total",
                               "arangodb_transactions_committed");
  nameVersionTable.try_emplace("arangodb_transactions_started_total",
                               "arangodb_transactions_started");
  nameVersionTable.try_emplace("arangodb_intermediate_commits_total",
                               "arangodb_intermediate_commits");
  nameVersionTable.try_emplace("arangodb_collection_truncates_total",
                               "arangodb_collection_truncates");
  nameVersionTable.try_emplace(
      "arangodb_collection_truncates_replication_total",
      "arangodb_collection_truncates_replication");
  nameVersionTable.try_emplace("arangodb_document_writes_total",
                               "arangodb_document_writes");
  nameVersionTable.try_emplace("arangodb_document_writes_replication_total",
                               "arangodb_document_writes_replication");
  nameVersionTable.try_emplace("arangodb_agency_callback_registered_total",
                               "arangodb_agency_callback_registered");
  nameVersionTable.try_emplace("arangodb_heartbeat_failures_total",
                               "arangodb_heartbeat_failures");
  nameVersionTable.try_emplace("arangodb_sync_wrong_checksum_total",
                               "arangodb_sync_wrong_checksum");
  nameVersionTable.try_emplace(
      "arangodb_maintenance_phase1_accum_runtime_msec_total",
      "arangodb_maintenance_phase1_accum_runtime_msec");
  nameVersionTable.try_emplace(
      "arangodb_maintenance_phase2_accum_runtime_msec_total",
      "arangodb_maintenance_phase2_accum_runtime_msec");
  nameVersionTable.try_emplace(
      "arangodb_maintenance_agency_sync_accum_runtime_msec_total",
      "arangodb_maintenance_agency_sync_accum_runtime_msec");
  nameVersionTable.try_emplace("arangodb_maintenance_action_duplicate_total",
                               "arangodb_maintenance_action_duplicate_counter");
  nameVersionTable.try_emplace(
      "arangodb_maintenance_action_registered_total",
      "arangodb_maintenance_action_registered_counter");
  nameVersionTable.try_emplace(
      "arangodb_maintenance_action_accum_runtime_msec_total",
      "arangodb_maintenance_action_accum_runtime_msec");
  nameVersionTable.try_emplace(
      "arangodb_maintenance_action_accum_queue_time_msec_total",
      "arangodb_maintenance_action_accum_queue_time_msec");
  nameVersionTable.try_emplace("arangodb_maintenance_action_failure_total",
                               "arangodb_maintenance_action_failure_counter");
  nameVersionTable.try_emplace("arangodb_maintenance_action_done_total",
                               "arangodb_maintenance_action_done_counter");
  nameVersionTable.try_emplace("arangodb_load_current_accum_runtime_msec_total",
                               "arangodb_load_current_accum_runtime_msec");
  nameVersionTable.try_emplace("arangodb_load_plan_accum_runtime_msec_total",
                               "arangodb_load_plan_accum_runtime_msec");
  nameVersionTable.try_emplace("arangodb_network_forwarded_requests_total",
                               "arangodb_network_forwarded_requests");
  nameVersionTable.try_emplace("arangodb_network_request_timeouts_total",
                               "arangodb_network_request_timeouts");
  nameVersionTable.try_emplace(
      "arangodb_connection_pool_leases_successful_total",
      "arangodb_connection_leases_successful");
  nameVersionTable.try_emplace("arangodb_connection_pool_leases_failed_total",
                               "arangodb_connection_pool_leases_failed");
  nameVersionTable.try_emplace(
      "arangodb_connection_pool_connections_created_total",
      "arangodb_connection_pool_connections_created");
  nameVersionTable.try_emplace("arangodb_connection_pool_connections_current",
                               "arangodb_connection_connections_current");
  nameVersionTable.try_emplace(
      "arangodb_agency_supervision_accum_runtime_msec_total",
      "arangodb_agency_supervision_accum_runtime_msec");
  nameVersionTable.try_emplace(
      "arangodb_agency_supervision_accum_runtime_wait_for_replication_msec_"
      "total",
      "arangodb_agency_supervision_accum_runtime_wait_for_replication_msec");
  nameVersionTable.try_emplace("arangodb_transactions_expired_total",
                               "arangodb_transactions_expired");
  nameVersionTable.try_emplace("arangodb_agency_read_no_leader_total",
                               "arangodb_agency_read_no_leader");
  nameVersionTable.try_emplace("arangodb_agency_read_ok_total",
                               "arangodb_agency_read_ok");
  nameVersionTable.try_emplace("arangodb_agency_write_no_leader_total",
                               "arangodb_agency_write_no_leader");
  nameVersionTable.try_emplace("arangodb_agency_write_ok_total",
                               "arangodb_agency_write_ok");
  nameVersionTable.try_emplace("arangodb_v8_context_created_total",
                               "arangodb_v8_context_created");
  nameVersionTable.try_emplace("arangodb_v8_context_creation_time_msec_total",
                               "arangodb_v8_context_creation_time_msec");
  nameVersionTable.try_emplace("arangodb_v8_context_destroyed_total",
                               "arangodb_v8_context_destroyed");
  nameVersionTable.try_emplace("arangodb_v8_context_entered_total",
                               "arangodb_v8_context_entered");
  nameVersionTable.try_emplace("arangodb_v8_context_enter_failures_total",
                               "arangodb_v8_context_enter_failures");
  nameVersionTable.try_emplace("arangodb_v8_context_exited_total",
                               "arangodb_v8_context_exited");
  nameVersionTable.try_emplace("arangodb_scheduler_queue_full_failures_total",
                               "arangodb_scheduler_queue_full_failures");
  nameVersionTable.try_emplace("arangodb_scheduler_threads_started_total",
                               "arangodb_scheduler_threads_started");
  nameVersionTable.try_emplace("arangodb_scheduler_threads_stopped_total",
                               "arangodb_scheduler_threads_stopped");
  nameVersionTable.try_emplace("arangodb_scheduler_num_awake_threads",
                               "arangodb_scheduler_awake_threads");
  // For the sake of completeness, we add the renamings from v1 to v2 from the
  // statistics feature:
  nameVersionTable.try_emplace(
      "arangodb_process_statistics_minor_page_faults_total",
      "arangodb_process_statistics_minor_page_faults");
  nameVersionTable.try_emplace(
      "arangodb_process_statistics_major_page_faults_total",
      "arangodb_process_statistics_major_page_faults");
  nameVersionTable.try_emplace(
      "arangodb_http_request_statistics_total_requests_total",
      "arangodb_http_request_statistics_total_requests");
  nameVersionTable.try_emplace(
      "arangodb_http_request_statistics_superuser_requests_total",
      "arangodb_http_request_statistics_superuser_requests");
  nameVersionTable.try_emplace(
      "arangodb_http_request_statistics_user_requests_total",
      "arangodb_http_request_statistics_user_requests");
  nameVersionTable.try_emplace(
      "arangodb_http_request_statistics_async_requests_total",
      "arangodb_http_request_statistics_async_requests");
  nameVersionTable.try_emplace(
      "arangodb_http_request_statistics_http_delete_requests_total",
      "arangodb_http_request_statistics_http_delete_requests");
  nameVersionTable.try_emplace(
      "arangodb_http_request_statistics_http_get_requests_total",
      "arangodb_http_request_statistics_http_get_requests");
  nameVersionTable.try_emplace(
      "arangodb_http_request_statistics_http_head_requests_total",
      "arangodb_http_request_statistics_http_head_requests");
  nameVersionTable.try_emplace(
      "arangodb_http_request_statistics_http_options_requests_total",
      "arangodb_http_request_statistics_http_options_requests");
  nameVersionTable.try_emplace(
      "arangodb_http_request_statistics_http_patch_requests_total",
      "arangodb_http_request_statistics_http_patch_requests");
  nameVersionTable.try_emplace(
      "arangodb_http_request_statistics_http_post_requests_total",
      "arangodb_http_request_statistics_http_post_requests");
  nameVersionTable.try_emplace(
      "arangodb_http_request_statistics_http_put_requests_total",
      "arangodb_http_request_statistics_http_put_requests");
  nameVersionTable.try_emplace(
      "arangodb_http_request_statistics_other_http_requests_total",
      "arangodb_http_request_statistics_other_http_requests");
  nameVersionTable.try_emplace("arangodb_server_statistics_server_uptime_total",
                               "arangodb_server_statistics_server_uptime");
  // And one for rocksdb:
  nameVersionTable.try_emplace("rocksdb_engine_throttle_bps",
                               "rocksdbengine_throttle_bps");

  v2suppressions = {
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
      "arangodb_agency_supervision_accum_runtime_wait_for_replication_msec_"
      "total",
      "arangodb_load_current_accum_runtime_msec_total",
      "arangodb_load_plan_accum_runtime_msec_total",
      "arangodb_aql_slow_query_total",
      "arangodb_scheduler_jobs_dequeued",
      "arangodb_scheduler_jobs_submitted",
      "arangodb_scheduler_jobs_done",
  };

  v1suppressions = {
      "arangodb_scheduler_jobs_dequeued_total",
      "arangodb_scheduler_jobs_submitted_total",
      "arangodb_scheduler_jobs_done_total",
  };
}

void MetricsFeature::collectOptions(std::shared_ptr<options::ProgramOptions> options) {
  _serverStatistics =
      std::make_unique<ServerStatistics>(*this, StatisticsFeature::time());

  options
      ->addOption("--server.export-metrics-api", "turn metrics API on or off",
                  new options::BooleanParameter(&_export),
                  arangodb::options::makeDefaultFlags(arangodb::options::Flags::Hidden))
      .setIntroducedIn(30600);

  options
      ->addOption("--server.export-read-write-metrics",
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
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, std::string{builder.type()} +
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
      _globalLabels.try_emplace("role", ServerState::roleToString(
                                            ServerState::instance()->getRole()));
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

    std::string lastType{};
    for (auto const& i : _registry) {
      std::string name{i.second->name()};
      std::string alternativeName;
      if (!v2) {
        if (v1suppressions.contains(name)) {
          continue;
        }
        // In v1 we do a name conversion. Note that we set
        // alternativeName == name in the end, in v2 though,
        // alternativeName is empty and no conversion happens.
        auto it = nameVersionTable.find(name);
        if (it != nameVersionTable.end()) {
          name = it->second;
        }
        if (name.compare(0, 20, kArangoDbConnection) == 0) {
          auto const labels = i.second->labels();
          if (labels == kPoolAgencyComm) {
            name += "_AgencyComm";
          } else if (labels == kPoolClusterComm) {
            name += "_ClusterComm";
          } else {
            // Avoid someone sneaking in an other connection
            // pool without dedicated metric for v1
            TRI_ASSERT(false);
          }
        }
        alternativeName = name;
      } else if (v2suppressions.contains(name)) {
        continue;
      }
      if (lastType != name) {
        i.second->toPrometheusBegin(result, name);
        lastType = name;
      }
      i.second->toPrometheus(result, _globalLabelsStr, alternativeName);
      result += "\n";
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
