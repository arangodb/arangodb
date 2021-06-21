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
/// @author Dr. Frank Celler
////////////////////////////////////////////////////////////////////////////////

#include "ServerStatistics.h"
#include "ApplicationFeatures/ApplicationFeature.h"
#include "Statistics/StatisticsFeature.h"
#include "RestServer/MetricsFeature.h"

using namespace arangodb;

template <typename T = float>
struct TimeScale {
  static log_scale_t<T> scale() { return {10., 0.0, 1000.0, 11}; }
};

DECLARE_COUNTER(arangodb_collection_lock_acquisition_micros_total, "Total amount of collection lock acquisition time [μs]");
DECLARE_HISTOGRAM(arangodb_collection_lock_acquisition_time, TimeScale<double>, "Collection lock acquisition time histogram [s]");
DECLARE_COUNTER(arangodb_collection_lock_sequential_mode_total, "Number of transactions using sequential locking of collections to avoid deadlocking");
DECLARE_COUNTER(arangodb_collection_lock_timeouts_exclusive_total, "Number of timeouts when trying to acquire collection exclusive locks");
DECLARE_COUNTER(arangodb_collection_lock_timeouts_write_total, "Number of timeouts when trying to acquire collection write locks");
DECLARE_COUNTER(arangodb_transactions_aborted_total, "Number of transactions aborted");
DECLARE_COUNTER(arangodb_transactions_committed_total, "Number of transactions committed");
DECLARE_COUNTER(arangodb_transactions_started_total, "Number of transactions started");
DECLARE_COUNTER(arangodb_intermediate_commits_total, "Number of intermediate commits performed in transactions");

DECLARE_COUNTER(arangodb_collection_truncates_total, "Total number of collection truncate operations (excl. synchronous replication)");
DECLARE_COUNTER(arangodb_collection_truncates_replication_total, "Total number of collection truncate operations by synchronous replication");
DECLARE_COUNTER(arangodb_document_writes_total, "Total number of document write operations (excl. synchronous replication)");
DECLARE_COUNTER(arangodb_document_writes_replication_total, "Total number of document write operations by synchronous replication");
DECLARE_HISTOGRAM(arangodb_document_read_time, TimeScale<>, "Total time spent in document read operations [s]");
DECLARE_HISTOGRAM(arangodb_document_insert_time, TimeScale<>, "Total time spent in document insert operations [s]");
DECLARE_HISTOGRAM(arangodb_document_replace_time, TimeScale<>, "Total time spent in document replace operations [s]");
DECLARE_HISTOGRAM(arangodb_document_remove_time, TimeScale<>, "Total time spent in document remove operations [s]");
DECLARE_HISTOGRAM(arangodb_document_update_time, TimeScale<>, "Total time spent in document update operations [s]");
DECLARE_HISTOGRAM(arangodb_collection_truncate_time, TimeScale<>, "Total time spent in collection truncate operations [s]");

TransactionStatistics::TransactionStatistics(MetricsFeature& metrics)
    : _metrics(metrics),
      _transactionsStarted(_metrics.add(arangodb_transactions_started_total{})),
      _transactionsAborted(_metrics.add(arangodb_transactions_aborted_total{})),
      _transactionsCommitted(_metrics.add(arangodb_transactions_committed_total{})),
      _intermediateCommits(_metrics.add(arangodb_intermediate_commits_total{})),
      _exclusiveLockTimeouts(_metrics.add(arangodb_collection_lock_timeouts_exclusive_total{})),
      _writeLockTimeouts(_metrics.add(arangodb_collection_lock_timeouts_write_total{})),
      _lockTimeMicros(_metrics.add(arangodb_collection_lock_acquisition_micros_total{})),
      _lockTimes(_metrics.add(arangodb_collection_lock_acquisition_time{})),
      _sequentialLocks(_metrics.add(arangodb_collection_lock_sequential_mode_total{})),
      _exportReadWriteMetrics(/*may be updated later*/ false) {}

void TransactionStatistics::setupDocumentMetrics() {
  // the following metrics are conditional, so we don't initialize them in the constructor
  _exportReadWriteMetrics = true;
  
  _numWrites = _metrics.add(arangodb_document_writes_total{});
  _numWritesReplication = _metrics.add(arangodb_document_writes_replication_total{});
  _numTruncates = _metrics.add(arangodb_collection_truncates_total{});
  _numTruncatesReplication = _metrics.add(arangodb_collection_truncates_replication_total{});
  _rocksdb_read_sec = _metrics.add(arangodb_document_read_time{});
  _rocksdb_insert_sec = _metrics.add(arangodb_document_insert_time{});
  _rocksdb_replace_sec = _metrics.add(arangodb_document_replace_time{});
  _rocksdb_remove_sec = _metrics.add(arangodb_document_remove_time{});
  _rocksdb_update_sec = _metrics.add(arangodb_document_update_time{});
  _rocksdb_truncate_sec = _metrics.add(arangodb_collection_truncate_time{});
}

void ServerStatistics::setupDocumentMetrics() {
  _transactionsStatistics.setupDocumentMetrics();
}

double ServerStatistics::uptime() const noexcept {
  return StatisticsFeature::time() - _startTime;
}
