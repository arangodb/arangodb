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

#include "MetricsFeature.h"

#include "ApplicationFeatures/ApplicationServer.h"
#include "ApplicationFeatures/GreetingsFeaturePhase.h"
#include "Basics/application-exit.h"
#include "Logger/LogMacros.h"
#include "Logger/Logger.h"
#include "Logger/LoggerStream.h"
#include "ProgramOptions/ProgramOptions.h"
#include "ProgramOptions/Section.h"
#include "RestServer/Metrics.h"
#include "RocksDBEngine/RocksDBEngine.h"
#include "Statistics/StatisticsFeature.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "StorageEngine/StorageEngine.h"
#include "StorageEngine/StorageEngineFeature.h"

#include <chrono>
#include <thread>

using namespace arangodb;
using namespace arangodb::application_features;
using namespace arangodb::basics;
using namespace arangodb::options;

// -----------------------------------------------------------------------------
// --SECTION--                                                 MetricsFeature
// -----------------------------------------------------------------------------


MetricsFeature::MetricsFeature(application_features::ApplicationServer& server)
  : ApplicationFeature(server, "Metrics"),
    _export(true) ,
    _exportReadWriteMetrics(false) {
  setOptional(false);
  startsAfter<LoggerFeature>();
  startsBefore<GreetingsFeaturePhase>();

  try {
    nameVersionTable.reserve(64);
    nameVersionTable.try_emplace(arangodb_agency_supervision_accum_runtime_msec, "");
    nameVersionTable.try_emplace(arangodb_agency_supervision_accum_runtime_wait_for_replication_msec, "");
    nameVersionTable.try_emplace(arangodb_agency_supervision_failed_server_total, "");
    nameVersionTable.try_emplace(arangodb_agency_supervision_runtime_msec, "");
    nameVersionTable.try_emplace(arangodb_agency_supervision_runtime_wait_for_replication_msec, "");
    nameVersionTable.try_emplace(arangodb_agency_append_hist, "");
    nameVersionTable.try_emplace(arangodb_agency_commit_hist, "");
    nameVersionTable.try_emplace(arangodb_agency_compaction_hist, "");
    nameVersionTable.try_emplace(arangodb_agency_local_commit_index, "");
    nameVersionTable.try_emplace(arangodb_agency_read_no_leader, "");
    nameVersionTable.try_emplace(arangodb_agency_read_ok, "");
    nameVersionTable.try_emplace(arangodb_agency_write_hist, "");
    nameVersionTable.try_emplace(arangodb_agency_write_no_leader, "");
    nameVersionTable.try_emplace(arangodb_agency_write_ok, "");
    nameVersionTable.try_emplace(arangodb_agency_term, "");
    nameVersionTable.try_emplace(arangodb_agency_log_size_bytes, "");
    nameVersionTable.try_emplace(arangodb_agency_client_lookup_table_size, "");
    nameVersionTable.try_emplace(arangodb_agency_cache_callback_number, "");
    nameVersionTable.try_emplace(arangodb_load_current_accum_runtime_msec, "");
    nameVersionTable.try_emplace(arangodb_load_current_runtime, "");
    nameVersionTable.try_emplace(arangodb_load_plan_accum_runtime_msec, "");
    nameVersionTable.try_emplace(arangodb_load_plan_runtime, "");
    nameVersionTable.try_emplace(arangodb_agencycomm_request_time_msec, "");
    nameVersionTable.try_emplace(arangodb_dropped_followers_total, "");
    nameVersionTable.try_emplace(arangodb_refused_followers_total, "");
    nameVersionTable.try_emplace(arangodb_sync_wrong_checksum, "");
    nameVersionTable.try_emplace(arangodb_agency_callback_registered, "");
    nameVersionTable.try_emplace(arangodb_agency_callback_number, "");
    nameVersionTable.try_emplace(arangodb_maintenance_phase1_accum_runtime_msec, "");
    nameVersionTable.try_emplace(arangodb_maintenance_phase2_accum_runtime_msec, "");
    nameVersionTable.try_emplace(arangodb_maintenance_agency_sync_accum_runtime_msec, "");
    nameVersionTable.try_emplace(arangodb_maintenance_action_duplicate_counter, "");
    nameVersionTable.try_emplace(arangodb_maintenance_action_registered_counter, "");
    nameVersionTable.try_emplace(arangodb_maintenance_action_accum_runtime_msec, "");
    nameVersionTable.try_emplace(arangodb_maintenance_action_accum_queue_time_msec, "");
    nameVersionTable.try_emplace(arangodb_maintenance_action_failure_counter, "");
    nameVersionTable.try_emplace(arangodb_maintenance_phase1_runtime_msec, "");
    nameVersionTable.try_emplace(arangodb_maintenance_phase2_runtime_msec, "");
    nameVersionTable.try_emplace(arangodb_maintenance_agency_sync_runtime_msec, "");
    nameVersionTable.try_emplace(arangodb_maintenance_action_runtime_msec, "");
    nameVersionTable.try_emplace(arangodb_maintenance_action_queue_time_msec, "");
    nameVersionTable.try_emplace(arangodb_maintenance_action_done_counter, "");
    nameVersionTable.try_emplace(arangodb_shards_out_of_sync, "");
    nameVersionTable.try_emplace(arangodb_shards_total, "");
    nameVersionTable.try_emplace(arangodb_shards_leader_total, "");
    nameVersionTable.try_emplace(arangodb_shards_not_replicated, "");
    nameVersionTable.try_emplace(arangodb_heartbeat_failures, "");
    nameVersionTable.try_emplace(arangodb_heartbeat_send_time_msec, "");
    nameVersionTable.try_emplace(arangodb_connection_pool_connections_current, "");
    nameVersionTable.try_emplace(arangodb_connection_pool_leases_successful, "");
    nameVersionTable.try_emplace(arangodb_connection_pool_leases_failed, "");
    nameVersionTable.try_emplace(arangodb_connection_pool_connections_created, "");
    nameVersionTable.try_emplace(arangodb_network_forwarded_requests, "");
    nameVersionTable.try_emplace(arangodb_network_request_timeouts, "");
    nameVersionTable.try_emplace(arangodb_network_request_duration_as_percentage_of_timeout, "");
    nameVersionTable.try_emplace(arangodb_network_requests_in_flight, "");
    nameVersionTable.try_emplace(arangodb_replication_cluster_inventory_requests, "");
    nameVersionTable.try_emplace(arangodb_replication_dump_requests, "");
    nameVersionTable.try_emplace(arangodb_replication_dump_bytes_received, "");
    nameVersionTable.try_emplace(arangodb_replication_dump_documents, "");
    nameVersionTable.try_emplace(arangodb_replication_dump_request_time, "");
    nameVersionTable.try_emplace(arangodb_replication_dump_apply_time, "");
    nameVersionTable.try_emplace(arangodb_replication_initial_sync_keys_requests, "");
    nameVersionTable.try_emplace(arangodb_replication_initial_sync_docs_requests, "");
    nameVersionTable.try_emplace(arangodb_replication_initial_sync_docs_requested, "");
    nameVersionTable.try_emplace(arangodb_replication_initial_sync_docs_inserted, "");
    nameVersionTable.try_emplace(arangodb_replication_initial_sync_docs_removed, "");
    nameVersionTable.try_emplace(arangodb_replication_initial_sync_bytes_received, "");
    nameVersionTable.try_emplace(arangodb_replication_initial_keys_requests_time, "");
    nameVersionTable.try_emplace(arangodb_replication_initial_docs_requests_time, "");
    nameVersionTable.try_emplace(arangodb_replication_initial_remove_apply_time, "");
    nameVersionTable.try_emplace(arangodb_replication_initial_lookup_time, "");
    nameVersionTable.try_emplace(arangodb_replication_tailing_requests, "");
    nameVersionTable.try_emplace(arangodb_replication_tailing_markers, "");
    nameVersionTable.try_emplace(arangodb_replication_tailing_removals, "");
    nameVersionTable.try_emplace(arangodb_replication_tailing_bytes_received, "");
    nameVersionTable.try_emplace(arangodb_replication_failed_connects, "");
    nameVersionTable.try_emplace(arangodb_replication_tailing_request_time, "");
    nameVersionTable.try_emplace(arangodb_replication_tailing_apply_time, "");
    nameVersionTable.try_emplace(arangodb_replication_synchronous_requests_total_number, "");
    nameVersionTable.try_emplace(arangodb_aql_all_query, "");
    nameVersionTable.try_emplace(arangodb_aql_query_time, "");
    nameVersionTable.try_emplace(arangodb_aql_slow_query, "");
    nameVersionTable.try_emplace(arangodb_aql_slow_query_time, "");
    nameVersionTable.try_emplace(arangodb_aql_total_query_time_msec, "");
    nameVersionTable.try_emplace(arangodb_aql_current_query, "");
    nameVersionTable.try_emplace(arangodb_rocksdb_write_stalls, "");
    nameVersionTable.try_emplace(arangodb_rocksdb_write_stops, "");
    nameVersionTable.try_emplace(arangodb_scheduler_awake_threads, "");
    nameVersionTable.try_emplace(arangodb_scheduler_jobs_dequeued, "");
    nameVersionTable.try_emplace(arangodb_scheduler_jobs_done, "");
    nameVersionTable.try_emplace(arangodb_scheduler_jobs_submitted, "");
    nameVersionTable.try_emplace(arangodb_scheduler_low_prio_queue_last_dequeue_time, "");
    nameVersionTable.try_emplace(arangodb_scheduler_num_working_threads, "");
    nameVersionTable.try_emplace(arangodb_scheduler_num_worker_threads, "");
    nameVersionTable.try_emplace(arangodb_scheduler_queue_full_failures, "");
    nameVersionTable.try_emplace(arangodb_scheduler_queue_length, "");
    nameVersionTable.try_emplace(arangodb_scheduler_threads_started, "");
    nameVersionTable.try_emplace(arangodb_scheduler_threads_stopped, "");
    nameVersionTable.try_emplace(arangodb_collection_lock_acquisition_micros, "");
    nameVersionTable.try_emplace(arangodb_collection_lock_acquisition_time, "");
    nameVersionTable.try_emplace(arangodb_collection_lock_sequential_mode, "");
    nameVersionTable.try_emplace(arangodb_collection_lock_timeouts_exclusive, "");
    nameVersionTable.try_emplace(arangodb_collection_lock_timeouts_write, "");
    nameVersionTable.try_emplace(arangodb_transactions_aborted, "");
    nameVersionTable.try_emplace(arangodb_transactions_committed, "");
    nameVersionTable.try_emplace(arangodb_transactions_started, "");
    nameVersionTable.try_emplace(arangodb_intermediate_commits, "");
    nameVersionTable.try_emplace(arangodb_collection_truncates, "");
    nameVersionTable.try_emplace(arangodb_collection_truncates_replication, "");
    nameVersionTable.try_emplace(arangodb_document_writes, "");
    nameVersionTable.try_emplace(arangodb_document_writes_replication, "");
    nameVersionTable.try_emplace(arangodb_document_read_time, "");
    nameVersionTable.try_emplace(arangodb_document_insert_time, "");
    nameVersionTable.try_emplace(arangodb_document_replace_time, "");
    nameVersionTable.try_emplace(arangodb_document_remove_time, "");
    nameVersionTable.try_emplace(arangodb_document_update_time, "");
    nameVersionTable.try_emplace(arangodb_collection_truncate_time, "");
    nameVersionTable.try_emplace(arangodb_transactions_expired, "");
    nameVersionTable.try_emplace(arangodb_v8_context_created, "");
    nameVersionTable.try_emplace(arangodb_v8_context_creation_time_msec, "");
    nameVersionTable.try_emplace(arangodb_v8_context_destroyed, "");
    nameVersionTable.try_emplace(arangodb_v8_context_enter_failures, "");
    nameVersionTable.try_emplace(arangodb_v8_context_entered, "");
    nameVersionTable.try_emplace(arangodb_v8_context_exited, "");
  } catch (std::exception const& e) {
    LOG_TOPIC("efd51", ERR, Logger::METRICS) <<
      "failed to allocate and populate the metrics v1/v2 translation table " << e.what();
    EXIT_FATAL_ERROR();
  }
}

void MetricsFeature::collectOptions(std::shared_ptr<ProgramOptions> options) {
  _serverStatistics = std::make_unique<ServerStatistics>(
      *this, StatisticsFeature::time());

  options->addOption("--server.export-metrics-api",
                     "turn metrics API on or off",
                     new BooleanParameter(&_export),
                     arangodb::options::makeDefaultFlags(arangodb::options::Flags::Hidden))
                     .setIntroducedIn(30600);

  options->addOption("--server.export-read-write-metrics",
                     "turn metrics for document read/write metrics on or off",
                     new BooleanParameter(&_exportReadWriteMetrics),
                     arangodb::options::makeDefaultFlags(arangodb::options::Flags::Hidden))
                     .setIntroducedIn(30707);
}

::Metric& MetricsFeature::doAdd(metrics::Builder& builder) {
  auto metric = builder.build();
  auto key = builder.key();
  bool success = false;
  {
    std::lock_guard<std::recursive_mutex> guard(_lock);
    success = _registry
      .try_emplace(std::move(key), std::dynamic_pointer_cast<::Metric>(metric))
      .second;
  }
  if (!success) {
    THROW_ARANGO_EXCEPTION_MESSAGE(
      TRI_ERROR_INTERNAL, builder.type() + builder.name() + " already exists");
  }
  return *metric;
}


bool MetricsFeature::exportAPI() const {
  return _export;
}

bool MetricsFeature::exportReadWriteMetrics() const {
  return _exportReadWriteMetrics;
}

void MetricsFeature::validateOptions(std::shared_ptr<ProgramOptions>) {
  if (_exportReadWriteMetrics) {
    serverStatistics().setupDocumentMetrics();
  }
}

void MetricsFeature::toPrometheus(std::string& result) const {

  // minimize reallocs
  result.reserve(32768);
  {
    bool changed = false;
    std::lock_guard<std::recursive_mutex> guard(_lock);
    if (_globalLabels.find("shortname") == _globalLabels.end()) {
      _globalLabels.try_emplace("shortname", ServerState::instance()->getShortName());
      changed = true;
    }
    if (_globalLabels.find("role") == _globalLabels.end() &&
        ServerState::instance() != nullptr &&
        ServerState::instance()->getRole() != ServerState::ROLE_UNDEFINED) {
      _globalLabels.try_emplace(
        "role", ServerState::roleToString(ServerState::instance()->getRole()));
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
      if (lastType != i.second->name()) {
        result +=   "# HELP " + i.second->name() + " " + i.second->help() + "\n";
        result += "\n# TYPE " + i.second->name() + " " + i.second->type() + "\n";
        lastType = i.second->name();
      }
      i.second->toPrometheus(result, _globalLabelsStr);
    }
  }

  // StatisticsFeature
  auto& sf = server().getFeature<StatisticsFeature>();
  sf.toPrometheus(result, std::chrono::duration<double, std::milli>(
                    std::chrono::system_clock::now().time_since_epoch()).count());

  // RocksDBEngine
  auto& es = server().getFeature<EngineSelectorFeature>().engine();
  std::string const& engineName = es.typeName();
  if (engineName == RocksDBEngine::EngineName) {
    es.getStatistics(result);
  }
}

ServerStatistics& MetricsFeature::serverStatistics() {
  return *_serverStatistics;
}

metrics_key::metrics_key(std::string const& name, std::initializer_list<std::string> const& il) : name(name) {
  TRI_ASSERT(il.size() < 2);
  if (il.size() == 1) {
    labels = *(il.begin());
  }
}

metrics_key::metrics_key(std::initializer_list<std::string> const& il) {
  TRI_ASSERT(il.size() > 0);
  TRI_ASSERT(il.size() < 3);
  name = *(il.begin());
  if (il.size() == 2) {
    labels = *(il.begin()+1);
  }
}

metrics_key::metrics_key(std::string const& name) : name(name) {
  // the metric name should not include any spaces
  TRI_ASSERT(name.find(' ') == std::string::npos);
}

metrics_key::metrics_key(std::string const& name, std::string const& labels) :
  name(name), labels(labels) {
  // the metric name should not include any spaces
  TRI_ASSERT(name.find(' ') == std::string::npos);
}

bool metrics_key::operator< (metrics_key const& other) const {
  return name+labels < other.name+other.labels;
}
