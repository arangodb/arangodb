////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2026 ArangoDB GmbH, Cologne, Germany
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
/// @author Julia CP
////////////////////////////////////////////////////////////////////////////////

#include "RocksDBReadWriteMetrics.h"

#include "Metrics/CounterBuilder.h"
#include "Metrics/IRegistry.h"
#include "Metrics/HistogramBuilder.h"
#include "Metrics/TimeScale.h"

namespace arangodb {

DECLARE_COUNTER(arangodb_document_writes_total,
                "Total number of document write operations (excl. synchronous "
                "replication)");
DECLARE_COUNTER(
    arangodb_document_writes_replication_total,
    "Total number of document write operations by synchronous replication");
DECLARE_COUNTER(arangodb_collection_truncates_total,
                "Total number of collection truncate operations (excl. "
                "synchronous replication)");
DECLARE_COUNTER(arangodb_collection_truncates_replication_total,
                "Total number of collection truncate operations by synchronous "
                "replication");
DECLARE_HISTOGRAM(arangodb_document_read_time, TimeScale<>,
                  "Total time spent in document read operations [s]");
DECLARE_HISTOGRAM(arangodb_document_insert_time, TimeScale<>,
                  "Total time spent in document insert operations [s]");
DECLARE_HISTOGRAM(arangodb_document_replace_time, TimeScale<>,
                  "Total time spent in document replace operations [s]");
DECLARE_HISTOGRAM(arangodb_document_remove_time, TimeScale<>,
                  "Total time spent in document remove operations [s]");
DECLARE_HISTOGRAM(arangodb_document_update_time, TimeScale<>,
                  "Total time spent in document update operations [s]");
DECLARE_HISTOGRAM(arangodb_collection_truncate_time, TimeScale<>,
                  "Total time spent in collection truncate operations [s]");

RocksDBReadWriteMetrics::RocksDBReadWriteMetrics(metrics::IRegistry& registry)
    : numWrites(registry.add(arangodb_document_writes_total{})),
      numWritesReplication(
          registry.add(arangodb_document_writes_replication_total{})),
      numTruncates(registry.add(arangodb_collection_truncates_total{})),
      numTruncatesReplication(
          registry.add(arangodb_collection_truncates_replication_total{})),
      rocksdb_read_sec(registry.add(arangodb_document_read_time{})),
      rocksdb_insert_sec(registry.add(arangodb_document_insert_time{})),
      rocksdb_replace_sec(registry.add(arangodb_document_replace_time{})),
      rocksdb_remove_sec(registry.add(arangodb_document_remove_time{})),
      rocksdb_update_sec(registry.add(arangodb_document_update_time{})),
      rocksdb_truncate_sec(registry.add(arangodb_collection_truncate_time{})) {}

}  // namespace arangodb
