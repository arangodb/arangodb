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

#pragma once

#include <cstdint>
#include <optional>
#include <functional>

#include "Metrics/Fwd.h"

namespace arangodb {

struct TransactionStatistics {
  explicit TransactionStatistics(metrics::MetricsFeature&);
  TransactionStatistics(TransactionStatistics const&) = delete;
  TransactionStatistics(TransactionStatistics &&) = delete;
  TransactionStatistics& operator=(TransactionStatistics const&) = delete;
  TransactionStatistics& operator=(TransactionStatistics &&) = delete;

  void setupDocumentMetrics();

  metrics::MetricsFeature& _metrics;

  metrics::Counter& _transactionsStarted;
  metrics::Counter& _transactionsAborted;
  metrics::Counter& _transactionsCommitted;
  metrics::Counter& _intermediateCommits;
  metrics::Counter& _readTransactions;

  // total number of lock timeouts for exclusive locks
  metrics::Counter& _exclusiveLockTimeouts;
  // total number of lock timeouts for write locks
  metrics::Counter& _writeLockTimeouts;
  // total duration of lock acquisition (in microseconds)
  metrics::Counter& _lockTimeMicros;
  // histogram for lock acquisition (in seconds)
  metrics::Histogram<metrics::LogScale<double>>& _lockTimes;
  // Total number of times we used a fallback to sequential locking
  metrics::Counter& _sequentialLocks;

  struct ReadWriteMetrics {
    // Total number of write operations in storage engine (excl. sync replication)
    metrics::Counter& numWrites;
    // Total number of write operations in storage engine by sync replication
    metrics::Counter& numWritesReplication;
    // Total number of truncate operations (not number of documents truncated!) (excl. sync replication)
    metrics::Counter& numTruncates;
    // Total number of truncate operations (not number of documents truncated!) by sync replication
    metrics::Counter& numTruncatesReplication;

    /// @brief the following metrics are conditional and only initialized if
    /// startup option `--server.export-read-write-metrics` is set
    metrics::Histogram<metrics::LogScale<float>>& rocksdb_read_sec;
    metrics::Histogram<metrics::LogScale<float>>& rocksdb_insert_sec;
    metrics::Histogram<metrics::LogScale<float>>& rocksdb_replace_sec;
    metrics::Histogram<metrics::LogScale<float>>& rocksdb_remove_sec;
    metrics::Histogram<metrics::LogScale<float>>& rocksdb_update_sec;
    metrics::Histogram<metrics::LogScale<float>>& rocksdb_truncate_sec;
  };

  std::optional<ReadWriteMetrics> _readWriteMetrics;
};

struct ServerStatistics {
  ServerStatistics(ServerStatistics const&) = delete;
  ServerStatistics(ServerStatistics &&) = delete;
  ServerStatistics& operator=(ServerStatistics const&) = delete;
  ServerStatistics& operator=(ServerStatistics &&) = delete;

  void setupDocumentMetrics();

  TransactionStatistics _transactionsStatistics;
  double const _startTime;

  double uptime() const noexcept;

  explicit ServerStatistics(metrics::MetricsFeature& metrics, double start)
      : _transactionsStatistics(metrics), _startTime(start) {}
};

} // namespace
