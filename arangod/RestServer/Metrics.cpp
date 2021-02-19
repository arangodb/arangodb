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

#include "Metrics.h"
#include "Logger/LogMacros.h"
#include "Logger/Logger.h"
#include "Basics/application-exit.h"
#include "Basics/debugging.h"
#include "Basics/exitcodes.h"
#include <type_traits>

using namespace arangodb;

char const* arangodb::metricsNameList[] = {
  "arangodb_agency_append_hist",
  "arangodb_agency_cache_callback_count",
  "arangodb_agency_callback_count",
  "arangodb_agency_callback_registered",
  "arangodb_agency_client_lookup_table_size",
  "arangodb_agency_commit_hist",
  "arangodb_agencycomm_request_time_msec",
  "arangodb_agency_compaction_hist",
  "arangodb_agency_local_commit_index",
  "arangodb_agency_log_size_bytes",
  "arangodb_agency_read_no_leader",
  "arangodb_agency_read_ok",
  "arangodb_agency_supervision_accum_runtime_msec",
  "arangodb_agency_supervision_accum_runtime_wait_for_replication_msec",
  "arangodb_agency_supervision_failed_server_count",
  "arangodb_agency_supervision_runtime_msec",
  "arangodb_agency_supervision_runtime_wait_for_replication_msec",
  "arangodb_agency_term",
  "arangodb_agency_write_hist",
  "arangodb_agency_write_no_leader",
  "arangodb_agency_write_ok",
  "arangodb_aql_all_query",
  "arangodb_aql_current_query",
  "arangodb_aql_query_time",
  "arangodb_aql_slow_query",
  "arangodb_aql_slow_query_time",
  "arangodb_aql_total_query_time_msec",
  "arangodb_collection_lock_acquisition_micros",
  "arangodb_collection_lock_acquisition_time",
  "arangodb_collection_lock_sequential_mode",
  "arangodb_collection_lock_timeouts_exclusive",
  "arangodb_collection_lock_timeouts_write",
  "arangodb_collection_truncates",
  "arangodb_collection_truncates_replication",
  "arangodb_collection_truncate_time",
  "arangodb_connection_connections_current_AgencyComm",
  "arangodb_connection_connections_current_ClusterComm",
  "arangodb_connection_leases_successful_AgencyComm",
  "arangodb_connection_leases_successful_ClusterComm",
  "arangodb_connection_pool_connections_created_AgencyComm",
  "arangodb_connection_pool_connections_created_ClusterComm",
  "arangodb_connection_pool_leases_failed_AgencyComm",
  "arangodb_connection_pool_leases_failed_ClusterComm",
  "arangodb_connection_pool_lease_time_hist_AgencyComm",
  "arangodb_connection_pool_lease_time_hist_ClusterComm",
  "arangodb_document_insert_time",
  "arangodb_document_read_time",
  "arangodb_document_remove_time",
  "arangodb_document_replace_time",
  "arangodb_document_update_time",
  "arangodb_document_writes",
  "arangodb_document_writes_replication",
  "arangodb_dropped_followers_count",
  "arangodb_heartbeat_failures",
  "arangodb_heartbeat_send_time_msec",
  "arangodb_intermediate_commits",
  "arangodb_load_current_accum_runtime_msec",
  "arangodb_load_current_runtime",
  "arangodb_load_plan_accum_runtime_msec",
  "arangodb_load_plan_runtime",
  "arangodb_maintenance_action_accum_queue_time_msec",
  "arangodb_maintenance_action_accum_runtime_msec",
  "arangodb_maintenance_action_done_counter",
  "arangodb_maintenance_action_duplicate_counter",
  "arangodb_maintenance_action_failure_counter",
  "arangodb_maintenance_action_queue_time_msec",
  "arangodb_maintenance_action_registered_counter",
  "arangodb_maintenance_action_runtime_msec",
  "arangodb_maintenance_agency_sync_accum_runtime_msec",
  "arangodb_maintenance_agency_sync_runtime_msec",
  "arangodb_maintenance_phase1_accum_runtime_msec",
  "arangodb_maintenance_phase1_runtime_msec",
  "arangodb_maintenance_phase2_accum_runtime_msec",
  "arangodb_maintenance_phase2_runtime_msec",
  "arangodb_network_forwarded_requests",
  "arangodb_network_request_duration_as_percentage_of_timeout",
  "arangodb_network_requests_in_flight",
  "arangodb_network_request_timeouts",
  "arangodb_refused_followers_count",
  "arangodb_replication_cluster_inventory_requests",
  "arangodb_replication_dump_apply_time",
  "arangodb_replication_dump_bytes_received",
  "arangodb_replication_dump_documents",
  "arangodb_replication_dump_requests",
  "arangodb_replication_dump_request_time",
  "arangodb_replication_failed_connects",
  "arangodb_replication_initial_chunks_requests_time",
  "arangodb_replication_initial_docs_requests_time",
  "arangodb_replication_initial_insert_apply_time",
  "arangodb_replication_initial_keys_requests_time",
  "arangodb_replication_initial_lookup_time",
  "arangodb_replication_initial_remove_apply_time",
  "arangodb_replication_initial_sync_bytes_received",
  "arangodb_replication_initial_sync_docs_inserted",
  "arangodb_replication_initial_sync_docs_removed",
  "arangodb_replication_initial_sync_docs_requested",
  "arangodb_replication_initial_sync_docs_requests",
  "arangodb_replication_initial_sync_keys_requests",
  "arangodb_replication_synchronous_requests_total_number",
  "arangodb_replication_synchronous_requests_total_time",
  "arangodb_replication_tailing_apply_time",
  "arangodb_replication_tailing_bytes_received",
  "arangodb_replication_tailing_documents",
  "arangodb_replication_tailing_follow_tick_failures",
  "arangodb_replication_tailing_markers",
  "arangodb_replication_tailing_removals",
  "arangodb_replication_tailing_requests",
  "arangodb_replication_tailing_request_time",
  "arangodb_rocksdb_write_stalls",
  "arangodb_rocksdb_write_stops",
  "arangodb_scheduler_awake_threads",
  "arangodb_scheduler_high_prio_queue_length",
  "arangodb_scheduler_jobs_dequeued",
  "arangodb_scheduler_jobs_done",
  "arangodb_scheduler_jobs_submitted",
  "arangodb_scheduler_low_prio_queue_last_dequeue_time",
  "arangodb_scheduler_low_prio_queue_length",
  "arangodb_scheduler_maintenance_prio_queue_length",
  "arangodb_scheduler_medium_prio_queue_length",
  "arangodb_scheduler_num_worker_threads",
  "arangodb_scheduler_num_working_threads",
  "arangodb_scheduler_ongoing_low_prio",
  "arangodb_scheduler_queue_full_failures",
  "arangodb_scheduler_queue_length",
  "arangodb_scheduler_threads_started",
  "arangodb_scheduler_threads_stopped",
  "arangodb_shards_leader_count",
  "arangodb_shards_not_replicated",
  "arangodb_shards_out_of_sync",
  "arangodb_shards_total_count",
  "arangodb_sync_wrong_checksum",
  "arangodb_transactions_aborted",
  "arangodb_transactions_committed",
  "arangodb_transactions_expired",
  "arangodb_transactions_started",
  "arangodb_v8_context_created",
  "arangodb_v8_context_creation_time_msec",
  "arangodb_v8_context_destroyed",
  "arangodb_v8_context_entered",
  "arangodb_v8_context_enter_failures",
  "arangodb_v8_context_exited",
  nullptr
};

std::ostream& operator<< (std::ostream& o, Metrics::counter_type const& s) {
  o << s.load();
  return o;
}

std::ostream& operator<< (std::ostream& o, Counter const& s) {
  o << s.load();
  return o;
}

std::ostream& operator<< (std::ostream& o, Metrics::hist_type const& v) {
  o << "[";
  for (size_t i = 0; i < v.size(); ++i) {
    if (i > 0) { o << ", "; }
    o << v.load(i);
  }
  o << "]";
  return o;
}

Metric::Metric(std::string const& name, std::string const& help, std::string const& labels)
  : _name(name), _help(help), _labels(labels) {
  // We look up the name at runtime in the list of metrics
  // arangodb::metricsNameList. This ensures that each metric is listed
  // in that list. External scripts then parse the source code of the
  // list and ensure that all metrics have documentation snippets in
  // `Documentation/Metrics` and that these are well-formed.
  char const** p = metricsNameList;
  bool found = false;
  while (*p != nullptr) {
    if (name.compare(*p) == 0) {
      found = true;
      break;
    }
    ++p;
  }
  if (!found) {
    LOG_TOPIC("77777", FATAL, Logger::STATISTICS)
      << "Tried to register a metric whose name is not listed in "
         "`arangodb::metricsNameList`, please add the name " << name
      << " to the list in source file `arangod/RestServer/Metrics.cpp` "
         "and create a corresponding documentation snippet in "
         "`Documentation/Metrics`.";
    FATAL_ERROR_EXIT_CODE(TRI_EXIT_FAILED);
  }
};

Metric::~Metric() = default;

std::string const& Metric::help() const { return _help; }
std::string const& Metric::name() const { return _name; }
std::string const& Metric::labels() const { return _labels; }

Counter& Counter::operator++() {
  count();
  return *this;
}

Counter& Counter::operator++(int n) {
  count(1);
  return *this;
}

Counter& Counter::operator+=(uint64_t const& n) {
  count(n);
  return *this;
}

Counter& Counter::operator=(uint64_t const& n) {
  store(n);
  return *this;
}

void Counter::count() {
  ++_b;
}

void Counter::count(uint64_t n) {
  _b += n;
}

std::ostream& Counter::print(std::ostream& o) const {
  o << _c;
  return o;
}

uint64_t Counter::load() const {
  _b.push();
  return _c.load();
}

void Counter::store(uint64_t const& n) {
  _c.exchange(n);
}

void Counter::toPrometheus(std::string& result) const {
  _b.push();
  result += "\n#TYPE " + name() + " counter\n";
  result += "#HELP " + name() + " " + help() + "\n";
  result += name();
  if (!labels().empty()) {
    result += "{" + labels() + "}";
  }
  result += " " + std::to_string(load()) + "\n";
}

Counter::Counter(
  uint64_t const& val, std::string const& name, std::string const& help,
  std::string const& labels) :
  Metric(name, help, labels), _c(val), _b(_c) {}

Counter::~Counter() { _b.push(); }

