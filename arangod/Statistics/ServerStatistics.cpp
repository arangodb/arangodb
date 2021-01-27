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

static char const* arangodb_transactions_started = R"RRR(
**Metric**
- `arangodb_transactions_started`:

TO_BE_WRITTEN
)RRR";

static char const* arangodb_transactions_aborted = R"RRR(
**Metric**
- `arangodb_transactions_aborted`:

TO_BE_WRITTEN
)RRR";

static char const* arangodb_transactions_committed = R"RRR(
**Metric**
- `arangodb_transactions_committed`:

TO_BE_WRITTEN
)RRR";

static char const* arangodb_intermediate_commits = R"RRR(
**Metric**
- `arangodb_intermediate_commits`:

TO_BE_WRITTEN
)RRR";

static char const* arangodb_collection_lock_timeouts_exclusive = R"RRR(
**Metric**
- `arangodb_collection_lock_timeouts_exclusive`:

TO_BE_WRITTEN
)RRR";

static char const* arangodb_collection_lock_timeouts_write = R"RRR(
**Metric**
- `arangodb_collection_lock_timeouts_write`:

TO_BE_WRITTEN
)RRR";

static char const* arangodb_collection_lock_acquisition_micros = R"RRR(
**Metric**
- `arangodb_collection_lock_acquisition_micros`:

TO_BE_WRITTEN
)RRR";

static char const* arangodb_collection_lock_acquisition_time = R"RRR(
**Metric**
- `arangodb_collection_lock_acquisition_time`:

TO_BE_WRITTEN
)RRR";

static char const* arangodb_collection_lock_sequential_mode = R"RRR(
**Metric**
- `arangodb_collection_lock_sequential_mode`:

TO_BE_WRITTEN
)RRR";

static char const* arangodb_document_writes = R"RRR(
**Metric**
- `arangodb_document_writes`:

TO_BE_WRITTEN
)RRR";

static char const* arangodb_document_writes_replication = R"RRR(
**Metric**
- `arangodb_document_writes_replication`:

TO_BE_WRITTEN
)RRR";

static char const* arangodb_collection_truncates = R"RRR(
**Metric**
- `arangodb_collection_truncates`:

TO_BE_WRITTEN
)RRR";

static char const* arangodb_collection_truncates_replication = R"RRR(
**Metric**
- `arangodb_collection_truncates_replication`:

TO_BE_WRITTEN
)RRR";

static char const* arangodb_document_read_time = R"RRR(
**Metric**
- `arangodb_document_read_time`:

TO_BE_WRITTEN
)RRR";

static char const* arangodb_document_insert_time = R"RRR(
**Metric**
- `arangodb_document_insert_time`:

TO_BE_WRITTEN
)RRR";

static char const* arangodb_document_replace_time = R"RRR(
**Metric**
- `arangodb_document_replace_time`:

TO_BE_WRITTEN
)RRR";

static char const* arangodb_document_remove_time = R"RRR(
**Metric**
- `arangodb_document_remove_time`:

TO_BE_WRITTEN
)RRR";

static char const* arangodb_document_update_time = R"RRR(
**Metric**
- `arangodb_document_update_time`:

TO_BE_WRITTEN
)RRR";

static char const* arangodb_collection_truncate_time = R"RRR(
**Metric**
- `arangodb_collection_truncate_time`:

TO_BE_WRITTEN
)RRR";

TransactionStatistics::TransactionStatistics(MetricsFeature& metrics)
    : _metrics(metrics),
      _transactionsStarted(_metrics.counter("arangodb_transactions_started", 0,
                                            "Number of transactions started",
                                            arangodb_transactions_started)),
      _transactionsAborted(_metrics.counter("arangodb_transactions_aborted", 0,
                                            "Number of transactions aborted",
                                            arangodb_transactions_aborted)),
      _transactionsCommitted(
        _metrics.counter("arangodb_transactions_committed", 0,
                         "Number of transactions committed",
                         arangodb_transactions_committed)),
      _intermediateCommits(
        _metrics.counter("arangodb_intermediate_commits", 0,
                         "Number of intermediate commits performed in transactions",
                         arangodb_intermediate_commits)),
      _exclusiveLockTimeouts(
        _metrics.counter("arangodb_collection_lock_timeouts_exclusive", 0,
                         "Number of timeouts when trying to acquire "
                         "collection exclusive locks",
                         arangodb_collection_lock_timeouts_exclusive)),
      _writeLockTimeouts(
        _metrics.counter("arangodb_collection_lock_timeouts_write", 0,
                         "Number of timeouts when trying to acquire collection write locks",
                         arangodb_collection_lock_timeouts_write)),
      _lockTimeMicros(
        _metrics.counter("arangodb_collection_lock_acquisition_micros", 0,
                         "Total amount of collection lock acquisition time [μs]",
                         arangodb_collection_lock_acquisition_micros)),
      _lockTimes(
        _metrics.histogram("arangodb_collection_lock_acquisition_time",
                           log_scale_t(10., 0.0, 1000.0, 11),
                           "Collection lock acquisition time histogram [s]",
                           arangodb_collection_lock_acquisition_time)),
      _sequentialLocks(
        _metrics.counter("arangodb_collection_lock_sequential_mode", 0,
                         "Number of transactions using sequential locking of "
                         "collections to avoid deadlocking",
                         arangodb_collection_lock_sequential_mode)),
      _numWrites(
        _metrics.counter("arangodb_document_writes", 0,
                         "Total number of document write operations (excl. synchronous replication)",
                         arangodb_document_writes)),
      _numWritesReplication(
        _metrics.counter("arangodb_document_writes_replication", 0,
                         "Total number of document write oprations by synchronous replication",
                         arangodb_document_writes_replication)),
      _numTruncates(
        _metrics.counter("arangodb_collection_truncates", 0,
                         "Total number of collection truncate operations (excl. synchronous replication)",
                         arangodb_collection_truncates)),
      _numTruncatesReplication(
        _metrics.counter("arangodb_collection_truncates_replication", 0,
                         "Total number of collection truncate operations by synchronous replication",
                         arangodb_collection_truncates_replication)),
      _rocksdb_read_usec(
        _metrics.histogram("arangodb_document_read_time",
                           log_scale_t<float>(10., 0.0, 1000.0, 11),
                           "Total time spent in document read operations [s]",
                           arangodb_document_read_time)),
      _rocksdb_insert_usec(
        _metrics.histogram("arangodb_document_insert_time",
                           log_scale_t<float>(10., 0.0, 1000.0, 11),
                           "Total time spent in document insert operations [s]",
                           arangodb_document_insert_time)),
      _rocksdb_replace_usec(
        _metrics.histogram("arangodb_document_replace_time",
                           log_scale_t<float>(10., 0.0, 1000.0, 11),
                           "Total time spent in document replace operations [s]",
                           arangodb_document_replace_time)),
      _rocksdb_remove_usec(
        _metrics.histogram("arangodb_document_remove_time",
                           log_scale_t<float>(10., 0.0, 1000.0, 11),
                           "Total time spent in document remove operations [s]",
                           arangodb_document_remove_time)),
      _rocksdb_update_usec(
        _metrics.histogram("arangodb_document_update_time",
                           log_scale_t<float>(10., 0.0, 1000.0, 11),
                           "Total time spent in document update operations [s]",
                           arangodb_document_update_time)),
      _rocksdb_truncate_usec(
        _metrics.histogram("arangodb_collection_truncate_time",
                           log_scale_t<float>(10., 0.0, 1000.0, 11),
                           "Total time spent in collcection truncate operations [s]",
                           arangodb_collection_truncate_time)) {}

// -----------------------------------------------------------------------------
// --SECTION--                                             static public methods
// -----------------------------------------------------------------------------

double ServerStatistics::uptime() const noexcept {
  return StatisticsFeature::time() - _startTime;
}
