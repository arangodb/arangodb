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
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Metrics/Fwd.h"

#include <cstdint>

namespace arangodb {

struct TransactionStatistics {
  explicit TransactionStatistics(metrics::IRegistry&);
  TransactionStatistics(TransactionStatistics const&) = delete;
  TransactionStatistics(TransactionStatistics&&) = delete;
  TransactionStatistics& operator=(TransactionStatistics const&) = delete;
  TransactionStatistics& operator=(TransactionStatistics&&) = delete;

  metrics::IRegistry& _metrics;

  metrics::Gauge<uint64_t>& _restTransactionsMemoryUsage;
  metrics::Gauge<uint64_t>& _internalTransactionsMemoryUsage;

  metrics::Counter& _transactionsStarted;
  metrics::Counter& _transactionsAborted;
  metrics::Counter& _transactionsCommitted;
  metrics::Counter& _intermediateCommits;
  metrics::Counter& _readTransactions;
  metrics::Counter& _dirtyReadTransactions;

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
};

}  // namespace arangodb
