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

#pragma once

#include "Metrics/Fwd.h"

namespace arangodb {

struct RocksDBReadWriteMetrics {
  explicit RocksDBReadWriteMetrics(metrics::IRegistry& registry);

  // Total number of write operations in storage engine (excl. sync replication)
  metrics::Counter& numWrites;
  // Total number of write operations in storage engine by sync replication
  metrics::Counter& numWritesReplication;
  // Total number of truncate operations (excl. sync replication)
  metrics::Counter& numTruncates;
  // Total number of truncate operations by sync replication
  metrics::Counter& numTruncatesReplication;

  metrics::Histogram<metrics::LogScale<float>>& rocksdb_read_sec;
  metrics::Histogram<metrics::LogScale<float>>& rocksdb_insert_sec;
  metrics::Histogram<metrics::LogScale<float>>& rocksdb_replace_sec;
  metrics::Histogram<metrics::LogScale<float>>& rocksdb_remove_sec;
  metrics::Histogram<metrics::LogScale<float>>& rocksdb_update_sec;
  metrics::Histogram<metrics::LogScale<float>>& rocksdb_truncate_sec;
};

}  // namespace arangodb
