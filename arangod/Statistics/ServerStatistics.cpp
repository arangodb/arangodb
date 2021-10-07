////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2017 ArangoDB GmbH, Cologne, Germany
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

TransactionStatistics::TransactionStatistics(MetricsFeature& metrics)
    : _metrics(metrics),
      _transactionsStarted(_metrics.counter("arangodb_transactions_started", 0,
                                            "Number of transactions started")),
      _transactionsAborted(_metrics.counter("arangodb_transactions_aborted", 0,
                                            "Number of transactions aborted")),
      _transactionsCommitted(
          _metrics.counter("arangodb_transactions_committed", 0,
                           "Number of transactions committed")),
      _intermediateCommits(
        _metrics.counter("arangodb_intermediate_commits", 0,
                         "Number of intermediate commits performed in transactions")),
      _readTransactions(
        _metrics.counter("arangodb_read_ransactions", 0,
                         "Number of read transactions")),
      _exclusiveLockTimeouts(
        _metrics.counter("arangodb_collection_lock_timeouts_exclusive", 0,
                         "Number of timeouts when trying to acquire "
                         "collection exclusive locks")),
      _writeLockTimeouts(
        _metrics.counter("arangodb_collection_lock_timeouts_write", 0,
                         "Number of timeouts when trying to acquire collection write locks")),
      _lockTimeMicros(
        _metrics.counter("arangodb_collection_lock_acquisition_micros", 0,
                         "Total amount of collection lock acquisition time [μs]")),
      _lockTimes(
        _metrics.histogram("arangodb_collection_lock_acquisition_time",
                           log_scale_t(10., 0.0, 1000.0, 11),
                           "Collection lock acquisition time histogram [s]")),
      _sequentialLocks(
        _metrics.counter("arangodb_collection_lock_sequential_mode", 0,
                         "Number of transactions using sequential locking of "
                         "collections to avoid deadlocking")),
      _exportReadWriteMetrics(/*may be updated later*/ false) {}

void TransactionStatistics::setupDocumentMetrics() {
  // the following metrics are conditional, so we don't initialize them in the constructor
  _exportReadWriteMetrics = true;

  _numWrites =
    _metrics.counter("arangodb_document_writes", 0,
                     "Total number of document write operations (excl. synchronous replication)");
  _numWritesReplication =
    _metrics.counter("arangodb_document_writes_replication", 0,
                     "Total number of document write oprations by synchronous replication");
  _numTruncates =
    _metrics.counter("arangodb_collection_truncates", 0,
                     "Total number of collection truncate operations (excl. synchronous replication)");
  _numTruncatesReplication =
    _metrics.counter("arangodb_collection_truncates_replication", 0,
                     "Total number of collection truncate operations by synchronous replication");
  _rocksdb_read_sec =
    _metrics.histogram("arangodb_document_read_time",
                       log_scale_t<float>(10., 0.0, 1000.0, 11),
                       "Total time spent in document read operations [s]");
  _rocksdb_insert_sec =
    _metrics.histogram("arangodb_document_insert_time",
                       log_scale_t<float>(10., 0.0, 1000.0, 11),
                       "Total time spent in document insert operations [s]");
  _rocksdb_replace_sec =
    _metrics.histogram("arangodb_document_replace_time",
                       log_scale_t<float>(10., 0.0, 1000.0, 11),
                       "Total time spent in document replace operations [s]");
  _rocksdb_remove_sec =
    _metrics.histogram("arangodb_document_remove_time",
                       log_scale_t<float>(10., 0.0, 1000.0, 11),
                       "Total time spent in document remove operations [s]");
  _rocksdb_update_sec =
    _metrics.histogram("arangodb_document_update_time",
                       log_scale_t<float>(10., 0.0, 1000.0, 11),
                       "Total time spent in document update operations [s]");
  _rocksdb_truncate_sec =
    _metrics.histogram("arangodb_collection_truncate_time",
                       log_scale_t<float>(10., 0.0, 1000.0, 11),
                       "Total time spent in collcection truncate operations [s]");
}

void ServerStatistics::setupDocumentMetrics() {
  _transactionsStatistics.setupDocumentMetrics();
}
// -----------------------------------------------------------------------------
// --SECTION--                                             static public methods
// -----------------------------------------------------------------------------

double ServerStatistics::uptime() const noexcept {
  return StatisticsFeature::time() - _startTime;
}
