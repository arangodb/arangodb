////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2020 ArangoDB GmbH, Cologne, Germany
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
      _transactionsCommitted(_metrics.counter("arangodb_transactions_committed",
                                              0, "Number of transactions committed")),
      _intermediateCommits(_metrics.counter("arangodb_intermediate_commits", 0,
                                            "Number of intermediate commits performed in transactions")),
      _exclusiveLockTimeouts(_metrics.counter("arangodb_collection_lock_timeouts_exclusive", 0,
                                              "Number of timeouts when trying to acquire collection exclusive locks")),
      _writeLockTimeouts(_metrics.counter("arangodb_collection_lock_timeouts_write", 0,
                                          "Number of timeouts when trying to acquire collection write locks")),
      _lockTimeMicros(_metrics.counter("arangodb_collection_lock_acquisition_micros", 0,
                                       "Total amount of collection lock acquisition time [μs]")),
      _lockTimes(
        _metrics.histogram("arangodb_collection_lock_acquisition_time", log_scale_t(10., 0.0, 1000.0, 11),
                           "Collection lock acquisition time histogram [s]")) {}

// -----------------------------------------------------------------------------
// --SECTION--                                             static public methods
// -----------------------------------------------------------------------------

double ServerStatistics::uptime() const noexcept {
  return StatisticsFeature::time() - _startTime;
}
