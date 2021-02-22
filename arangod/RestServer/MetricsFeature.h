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

#ifndef ARANGODB_REST_SERVER_METRICS_FEATURE_H
#define ARANGODB_REST_SERVER_METRICS_FEATURE_H 1

#include "ApplicationFeatures/ApplicationFeature.h"
#include "Cluster/ServerState.h"
#include "Logger/LogMacros.h"
#include "Logger/LoggerFeature.h"
#include "ProgramOptions/ProgramOptions.h"
#include "RestServer/Metrics.h"
#include "Statistics/ServerStatistics.h"

inline extern constexpr char arangodb_agency_write_ok[] = "arangodb_agency_write_ok";
inline extern constexpr char arangodb_agency_write_no_leader[] =
  "arangodb_agency_write_no_leader";
inline extern constexpr char arangodb_agency_read_ok[] =
  "arangodb_agency_read_ok";
inline extern constexpr char arangodb_agency_read_no_leader[] =
  "arangodb_agency_read_no_leader";
inline extern constexpr char arangodb_agency_write_hist[] =
  "arangodb_agency_write_hist";
inline extern constexpr char arangodb_agency_commit_hist[] =
  "arangodb_agency_commit_hist";
inline extern constexpr char arangodb_agency_append_hist[] =
  "arangodb_agency_append_hist";
inline extern constexpr char arangodb_agency_compaction_hist[] =
  "arangodb_agency_compaction_hist";
inline extern constexpr char arangodb_agency_local_commit_index[] =
  "arangodb_agency_local_commit_index";
inline extern constexpr char arangodb_agency_supervision_accum_runtime_msec[] =
  "arangodb_agency_supervision_accum_runtime_msec";
inline extern constexpr char arangodb_agency_supervision_accum_runtime_wait_for_replication_msec[] =
  "arangodb_agency_supervision_accum_runtime_wait_for_replication_msec";
inline extern constexpr char arangodb_agency_supervision_failed_server_count[] =
  "arangodb_agency_supervision_failed_server_count";
inline extern constexpr char arangodb_agencycomm_request_time_msec[] =
  "arangodb_agencycomm_request_time_msec";
inline extern constexpr char arangodb_agency_term[] =
  "arangodb_agency_term";
inline extern constexpr char arangodb_agency_log_size_bytes[] =
  "arangodb_agency_log_size_bytes";
inline extern constexpr char arangodb_agency_client_lookup_table_size[] =
  "arangodb_agency_client_lookup_table_size";
inline extern constexpr char arangodb_agency_supervision_runtime_msec[] =
  "arangodb_agency_supervision_runtime_msec";
inline extern constexpr char arangodb_agency_supervision_runtime_wait_for_replication_msec[] =
  "arangodb_agency_supervision_runtime_wait_for_replication_msec";
inline extern constexpr char arangodb_v8_context_creation_time_msec[] =
  "arangodb_v8_context_creation_time_msec";
inline extern constexpr char arangodb_v8_context_created[] =
  "arangodb_v8_context_created";
inline extern constexpr char arangodb_v8_context_destroyed[] =
  "arangodb_v8_context_destroyed";
inline extern constexpr char arangodb_v8_context_entered[] =
  "arangodb_v8_context_entered";
inline extern constexpr char arangodb_v8_context_exited[] =
  "arangodb_v8_context_exited";
inline extern constexpr char arangodb_v8_context_enter_failures[] =
  "arangodb_v8_context_enter_failures";
inline extern constexpr char arangodb_connection_leases_successful[] =
  "arangodb_connection_leases_successful_";
inline extern constexpr char arangodb_connection_pool_leases_failed[] =
  "arangodb_connection_pool_leases_failed_";
inline extern constexpr char arangodb_connection_pool_connections_created[] =
  "arangodb_connection_pool_connections_created_";
inline extern constexpr char arangodb_rocksdb_write_stalls[] =
  "arangodb_rocksdb_write_stalls";
inline extern constexpr char arangodb_rocksdb_write_stops[] =
  "arangodb_rocksdb_write_stops";
inline extern constexpr char arangodb_replication_dump_requests[] =
  "arangodb_replication_dump_requests";
inline extern constexpr char arangodb_replication_dump_bytes_received[] =
  "arangodb_replication_dump_bytes_received";
inline extern constexpr char arangodb_replication_dump_documents[] =
  "arangodb_replication_dump_documents";
inline extern constexpr char arangodb_replication_dump_request_time[] =
  "arangodb_replication_dump_request_time";
inline extern constexpr char arangodb_replication_dump_apply_time[] =
  "arangodb_replication_dump_apply_time";
inline extern constexpr char arangodb_replication_initial_sync_keys_requests[] =
  "arangodb_replication_initial_sync_keys_requests";
inline extern constexpr char arangodb_replication_initial_sync_docs_requests[] =
  "arangodb_replication_initial_sync_docs_requests";
inline extern constexpr char arangodb_replication_initial_sync_docs_requested[] =
  "arangodb_replication_initial_sync_docs_requested";
inline extern constexpr char arangodb_replication_initial_sync_docs_inserted[] =
  "arangodb_replication_initial_sync_docs_inserted";
inline extern constexpr char arangodb_replication_initial_sync_docs_removed[] =
  "arangodb_replication_initial_sync_docs_removed";
inline extern constexpr char arangodb_replication_initial_sync_bytes_received[] =
  "arangodb_replication_initial_sync_bytes_received";
inline extern constexpr char arangodb_replication_initial_chunks_requests_time[] =
  "arangodb_replication_initial_chunks_requests_time";
inline extern constexpr char arangodb_replication_initial_keys_requests_time[] =
  "arangodb_replication_initial_keys_requests_time";
inline extern constexpr char arangodb_replication_initial_docs_requests_time[] =
  "arangodb_replication_initial_docs_requests_time";
inline extern constexpr char arangodb_replication_initial_insert_apply_time[] =
  "arangodb_replication_initial_insert_apply_time";
inline extern constexpr char arangodb_replication_initial_remove_apply_time[] =
  "arangodb_replication_initial_remove_apply_time";
inline extern constexpr char arangodb_replication_initial_lookup_time[] =
  "arangodb_replication_initial_lookup_time";
inline extern constexpr char arangodb_replication_tailing_requests[] =
  "arangodb_replication_tailing_requests";
inline extern constexpr char arangodb_replication_tailing_follow_tick_failures[] =
  "arangodb_replication_tailing_follow_tick_failures";
inline extern constexpr char arangodb_replication_tailing_markers[] =
  "arangodb_replication_tailing_markers";
inline extern constexpr char arangodb_replication_tailing_documents[] =
  "arangodb_replication_tailing_documents";
inline extern constexpr char arangodb_replication_tailing_removals[] =
  "arangodb_replication_tailing_removals";
inline extern constexpr char arangodb_replication_tailing_bytes_received[] =
  "arangodb_replication_tailing_bytes_received";
inline extern constexpr char arangodb_replication_failed_connects[] =
  "arangodb_replication_failed_connects";
inline extern constexpr char arangodb_replication_tailing_request_time[] =
  "arangodb_replication_tailing_request_time";
inline extern constexpr char arangodb_replication_tailing_apply_time[]=
  "arangodb_replication_tailing_apply_time";
inline extern constexpr char arangodb_replication_synchronous_requests_total_time[] =
  "arangodb_replication_synchronous_requests_total_time";
inline extern constexpr char arangodb_dropped_followers_count[] =
  "arangodb_dropped_followers_count";
inline extern constexpr char arangodb_refused_followers_count[] =
  "arangodb_refused_followers_count";
inline extern constexpr char arangodb_sync_wrong_checksum[] =
  "arangodb_sync_wrong_checksum";
inline extern constexpr char arangodb_load_plan_accum_runtime_msec[] =
  "arangodb_load_plan_accum_runtime_msec";
inline extern constexpr char arangodb_load_current_accum_runtime_msec[] =
  "arangodb_load_current_accum_runtime_msec";
inline extern constexpr char arangodb_heartbeat_failures[] =
  "arangodb_heartbeat_failures";
inline extern constexpr char arangodb_agency_callback_registered[] =
  "arangodb_agency_callback_registered";
inline extern constexpr char arangodb_transactions_expired[] =
  "arangodb_transactions_expired";
inline extern constexpr char arangodb_collection_lock_acquisition_time[] =
  "arangodb_collection_lock_acquisition_time";
inline extern constexpr char arangodb_document_read_time[] =
  "arangodb_document_read_time";
inline extern constexpr char arangodb_document_insert_time[] =
  "arangodb_document_insert_time";
inline extern constexpr char arangodb_document_replace_time[] =
  "arangodb_document_replace_time";
inline extern constexpr char arangodb_document_remove_time[] =
  "arangodb_document_remove_time";
inline extern constexpr char arangodb_document_update_time[] =
  "arangodb_document_update_time";
inline extern constexpr char arangodb_collection_truncate_time[] =
  "arangodb_collection_truncate_time";
inline extern constexpr char arangodb_aql_query_time[] =
  "arangodb_aql_query_time";
inline extern constexpr char arangodb_aql_slow_query_time[] =
  "arangodb_aql_slow_query_time";
inline extern constexpr char arangodb_aql_all_query[] =
  "arangodb_aql_all_query";
inline extern constexpr char arangodb_aql_slow_query[] =
  "arangodb_aql_slow_query";
inline extern constexpr char arangodb_aql_total_query_time_msec[] =
  "arangodb_aql_total_query_time_msec";
inline extern constexpr char arangodb_network_forwarded_requests[] =
  "arangodb_network_forwarded_requests";
inline extern constexpr char arangodb_network_request_timeouts[] =
  "arangodb_network_request_timeouts";
inline extern constexpr char arangodb_network_request_duration_as_percentage_of_timeout[] =
  "arangodb_network_request_duration_as_percentage_of_timeout";
inline extern constexpr char arangodb_maintenance_phase1_accum_runtime_msec[] =
  "arangodb_maintenance_phase1_accum_runtime_msec";
inline extern constexpr char arangodb_maintenance_phase2_accum_runtime_msec[] =
  "arangodb_maintenance_phase2_accum_runtime_msec";
inline extern constexpr char arangodb_maintenance_agency_sync_accum_runtime_msec[] =
  "arangodb_maintenance_agency_sync_accum_runtime_msec";
inline extern constexpr char arangodb_maintenance_action_duplicate_counter[] =
  "arangodb_maintenance_action_duplicate_counter";
inline extern constexpr char arangodb_maintenance_action_registered_counter[] =
  "arangodb_maintenance_action_registered_counter";
inline extern constexpr char arangodb_maintenance_action_accum_runtime_msec[] =
  "arangodb_maintenance_action_accum_runtime_msec";
inline extern constexpr char arangodb_maintenance_action_accum_queue_time_msec[] =
  "arangodb_maintenance_action_accum_queue_time_msec";
inline extern constexpr char arangodb_maintenance_action_failure_counter[] =
  "arangodb_maintenance_action_failure_counter";
inline extern constexpr char arangodb_maintenance_phase1_runtime_msec[] =
  "arangodb_maintenance_phase1_runtime_msec";
inline extern constexpr char arangodb_maintenance_phase2_runtime_msec[] =
  "arangodb_maintenance_phase2_runtime_msec";
inline extern constexpr char arangodb_maintenance_agency_sync_runtime_msec[] =
  "arangodb_maintenance_agency_sync_runtime_msec";
inline extern constexpr char arangodb_maintenance_action_runtime_msec[] =
  "arangodb_maintenance_action_runtime_msec";
inline extern constexpr char arangodb_maintenance_action_queue_time_msec[] =
  "arangodb_maintenance_action_queue_time_msec";
inline extern constexpr char arangodb_heartbeat_send_time_msec[] =
  "arangodb_heartbeat_send_time_msec";
inline extern constexpr char arangodb_load_current_runtime[] =
  "arangodb_load_current_runtime";
inline extern constexpr char arangodb_connection_pool_lease_time_hist[] =
  "arangodb_connection_pool_lease_time_hist";
inline extern constexpr char arangodb_scheduler_queue_length[] =
  "arangodb_scheduler_queue_length";
inline extern constexpr char arangodb_scheduler_jobs_done[] =
  "arangodb_scheduler_jobs_done";
inline extern constexpr char arangodb_scheduler_jobs_submitted[] =
  "arangodb_scheduler_jobs_submitted";
inline extern constexpr char arangodb_scheduler_jobs_dequeued[] =
  "arangodb_scheduler_jobs_dequeued";
inline extern constexpr char arangodb_scheduler_awake_threads[] =
  "arangodb_scheduler_awake_threads";
inline extern constexpr char arangodb_scheduler_num_working_threads[] =
  "arangodb_scheduler_num_working_threads";
inline extern constexpr char arangodb_scheduler_ongoing_low_prio[] =
  "arangodb_scheduler_ongoing_low_prio";
inline extern constexpr char arangodb_scheduler_low_prio_queue_last_dequeue_time[] =
  "arangodb_scheduler_low_prio_queue_last_dequeue_time";
inline extern constexpr char arangodb_scheduler_maintenance_prio_queue_length[] =
  "arangodb_scheduler_maintenance_prio_queue_length";
inline extern constexpr char arangodb_scheduler_high_prio_queue_length[] =
  "arangodb_scheduler_high_prio_queue_length";
inline extern constexpr char arangodb_scheduler_medium_prio_queue_length[] =
  "arangodb_scheduler_medium_prio_queue_length";
inline extern constexpr char arangodb_scheduler_low_prio_queue_length[] =
  "arangodb_scheduler_low_prio_queue_length";
inline extern constexpr char arangodb_aql_current_query[] =
  "arangodb_aql_current_query";
inline extern constexpr char arangodb_network_requests_in_flight[] =
  "arangodb_network_requests_in_flight";
inline extern constexpr char arangodb_connection_connections_current[] =
  "arangodb_connection_connections_current";
inline extern constexpr char arangodb_agency_callback_count[] =
  "arangodb_agency_callback_count";
inline extern constexpr char arangodb_shards_out_of_sync[] =
  "arangodb_shards_out_of_sync";
inline extern constexpr char arangodb_shards_not_replicated[] =
  "arangodb_shards_not_replicated";
inline extern constexpr char arangodb_agency_cache_callback_count[] =
  "arangodb_agency_cache_callback_count";
//inline extern constexpr char [] = "";

namespace arangodb {
struct metrics_key;
}

namespace std {
template <>
struct hash<arangodb::metrics_key> {
  typedef arangodb::metrics_key argument_t;
  typedef std::size_t result_t;
  result_t operator()(argument_t const& r) const noexcept;
};
}  // namespace std

namespace arangodb {

struct metrics_key {
  std::string name;
  std::string labels;
  std::size_t _hash;
  metrics_key() {}
  metrics_key(std::string const& name);
  metrics_key(std::string const& name, std::string const& labels);
  metrics_key(std::initializer_list<std::string> const& il);
  metrics_key(std::string const& name, std::initializer_list<std::string> const& il);
  std::size_t hash() const noexcept;
  bool operator==(metrics_key const& other) const;
};

class MetricsFeature final : public application_features::ApplicationFeature {
 public:
  using registry_type = std::unordered_map<metrics_key, std::shared_ptr<Metric>>;

  static double time();

  explicit MetricsFeature(application_features::ApplicationServer& server);

  bool exportAPI() const;
  bool exportReadWriteMetrics() const;

  void collectOptions(std::shared_ptr<options::ProgramOptions>) override final;
  void validateOptions(std::shared_ptr<options::ProgramOptions>) override final;

  template <char const* name, typename Scale>
  Histogram<Scale>& histogram(Scale const& scale, std::string const& help = std::string()) {
    return histogram<name, Scale>(metrics_key(name), scale, help);
  }

  template <char const* name, typename Scale>
  Histogram<Scale>& histogram(std::initializer_list<std::string> const& il,
                              Scale const& scale,
                              std::string const& help = std::string()) {
    return histogram<name, Scale>(metrics_key(name, il), scale, help);
  }

  template <char const* name, typename Scale>
  Histogram<Scale>& histogram(metrics_key const& mk, Scale const& scale,
                              std::string const& help = std::string()) {
    std::string labels = mk.labels;
    if (ServerState::instance() != nullptr &&
        ServerState::instance()->getRole() != ServerState::ROLE_UNDEFINED) {
      if (!labels.empty()) {
        labels += ",";
      }
      labels +=
          "role=\"" + ServerState::roleToString(ServerState::instance()->getRole()) +
          "\",shortname=\"" + ServerState::instance()->getShortName() + "\"";
    }

    auto metric = std::make_shared<Histogram<Scale>>(scale, name, help, labels);
    bool success = false;
    {
      std::lock_guard<std::recursive_mutex> guard(_lock);
      success =
          _registry.try_emplace(mk, std::dynamic_pointer_cast<Metric>(metric)).second;
    }
    if (!success) {
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                     std::string("histogram ") + name +
                                         " alredy exists");
    }
    return *metric;
  };

  template<const char* name>
  Counter& counter(
    std::initializer_list<std::string> const& key, uint64_t const& val,
    std::string const& help) {
    return counter<name>(metrics_key(name, key), val, help);
  }

  template<const char* name>
  Counter& counter(
    metrics_key const& mk, uint64_t const& val, std::string const& help) {

    std::string labels = mk.labels;
    if (ServerState::instance() != nullptr &&
        ServerState::instance()->getRole() != ServerState::ROLE_UNDEFINED) {
      if (!labels.empty()) {
        labels += ",";
      }
      labels += "role=\"" + ServerState::roleToString(ServerState::instance()->getRole()) +
        "\",shortname=\"" + ServerState::instance()->getShortName() + "\"";
    }
    auto metric = std::make_shared<Counter>(val, name, help, labels);
    bool success = false;
    {
      std::lock_guard<std::recursive_mutex> guard(_lock);
      success = _registry.emplace(mk, std::dynamic_pointer_cast<Metric>(metric)).second;
    }
    if (!success) {
      THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_INTERNAL, std::string("counter ") + name + " already exists");
    }
    return *metric;
  }

  template<const char* name>
  Counter& counter(uint64_t const& val, std::string const& help) {
    return counter<name>(metrics_key(name), val, help);
  }


  template <const char* name, typename T>
  Gauge<T>& gauge(T const& t, std::string const& help) {
    return gauge<name>(metrics_key(name), t, help);
  }

  template <const char* name, typename T>
  Gauge<T>& gauge(std::initializer_list<std::string> const& il, T const& t,
                  std::string const& help) {
    return gauge<name>(metrics_key(name, il), t, help);
  }

  template <const char* name, typename T>
  Gauge<T>& gauge(metrics_key const& key, T const& t,
                  std::string const& help = std::string()) {
    metrics_key mk(key);
    std::string labels = mk.labels;
    if (ServerState::instance() != nullptr &&
        ServerState::instance()->getRole() != ServerState::ROLE_UNDEFINED) {
      if (!labels.empty()) {
        labels += ",";
      }
      labels +=
          "role=\"" + ServerState::roleToString(ServerState::instance()->getRole()) +
          "\",shortname=\"" + ServerState::instance()->getShortName() + "\"";
    }
    auto metric = std::make_shared<Gauge<T>>(t, name, help, labels);
    bool success = false;
    {
      std::lock_guard<std::recursive_mutex> guard(_lock);
      success =
          _registry.try_emplace(mk, std::dynamic_pointer_cast<Metric>(metric)).second;
    }
    if (!success) {
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, std::string("gauge ") + name +
                                                             " alredy exists");
    }
    return *metric;
  }


  void toPrometheus(std::string& result) const;

  ServerStatistics& serverStatistics();

 private:
  registry_type _registry;

  mutable std::recursive_mutex _lock;

  std::unique_ptr<ServerStatistics> _serverStatistics;

  bool _export;
  bool _exportReadWriteMetrics;
};

}  // namespace arangodb

#endif
