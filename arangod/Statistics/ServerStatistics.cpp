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
/// @author Dr. Frank Celler
////////////////////////////////////////////////////////////////////////////////

#include "ServerStatistics.h"
#include "ApplicationFeatures/ApplicationFeature.h"
#include "Statistics/StatisticsFeature.h"
#include "Metrics/CounterBuilder.h"
#include "Metrics/GaugeBuilder.h"
#include "Metrics/HistogramBuilder.h"
#include "Metrics/IRegistry.h"
#include "Metrics/TimeScale.h"

using namespace arangodb;

DECLARE_COUNTER(arangodb_collection_lock_acquisition_micros_total,
                "Total amount of collection lock acquisition time [μs]");
DECLARE_HISTOGRAM(arangodb_collection_lock_acquisition_time, TimeScale<double>,
                  "Collection lock acquisition time histogram [s]");
DECLARE_COUNTER(arangodb_collection_lock_sequential_mode_total,
                "Number of transactions using sequential locking of "
                "collections to avoid deadlocking");
DECLARE_COUNTER(
    arangodb_collection_lock_timeouts_exclusive_total,
    "Number of timeouts when trying to acquire collection exclusive locks");
DECLARE_COUNTER(
    arangodb_collection_lock_timeouts_write_total,
    "Number of timeouts when trying to acquire collection write locks");
DECLARE_GAUGE(arangodb_transactions_rest_memory_usage, uint64_t,
              "Memory usage of transactions (excl. AQL queries)");
DECLARE_GAUGE(arangodb_transactions_internal_memory_usage, uint64_t,
              "Memory usage of internal transactions");
DECLARE_COUNTER(arangodb_transactions_aborted_total,
                "Number of transactions aborted");
DECLARE_COUNTER(arangodb_transactions_committed_total,
                "Number of transactions committed");
DECLARE_COUNTER(arangodb_transactions_started_total,
                "Number of transactions started");
DECLARE_COUNTER(arangodb_intermediate_commits_total,
                "Number of intermediate commits performed in transactions");
DECLARE_COUNTER(arangodb_read_transactions_total,
                "Number of read transactions");
DECLARE_COUNTER(arangodb_dirty_read_transactions_total,
                "Number of read transactions which can do dirty reads");

TransactionStatistics::TransactionStatistics(metrics::IRegistry& metrics)
    : _metrics(metrics),
      _restTransactionsMemoryUsage(
          _metrics.add(arangodb_transactions_rest_memory_usage{})),
      _internalTransactionsMemoryUsage(
          _metrics.add(arangodb_transactions_internal_memory_usage{})),
      _transactionsStarted(_metrics.add(arangodb_transactions_started_total{})),
      _transactionsAborted(_metrics.add(arangodb_transactions_aborted_total{})),
      _transactionsCommitted(
          _metrics.add(arangodb_transactions_committed_total{})),
      _intermediateCommits(_metrics.add(arangodb_intermediate_commits_total{})),
      _readTransactions(_metrics.add(arangodb_read_transactions_total{})),
      _dirtyReadTransactions(
          _metrics.add(arangodb_dirty_read_transactions_total{})),
      _exclusiveLockTimeouts(
          _metrics.add(arangodb_collection_lock_timeouts_exclusive_total{})),
      _writeLockTimeouts(
          _metrics.add(arangodb_collection_lock_timeouts_write_total{})),
      _lockTimeMicros(
          _metrics.add(arangodb_collection_lock_acquisition_micros_total{})),
      _lockTimes(_metrics.add(arangodb_collection_lock_acquisition_time{})),
      _sequentialLocks(
          _metrics.add(arangodb_collection_lock_sequential_mode_total{})) {}

double ServerStatistics::uptime() const noexcept {
  return StatisticsFeature::time() - _startTime;
}
