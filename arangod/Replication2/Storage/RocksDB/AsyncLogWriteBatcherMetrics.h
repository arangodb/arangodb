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
/// @author Manuel Pöter
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <cstdint>

#include "Metrics/Fwd.h"

namespace arangodb::replication2::storage::rocksdb {

struct AsyncLogWriteBatcherMetrics {
  metrics::Gauge<std::size_t>* numWorkerThreadsWaitForSync;
  metrics::Gauge<std::size_t>* numWorkerThreadsNoWaitForSync;
  metrics::Gauge<std::size_t>* queueLength;

  metrics::Histogram<metrics::LogScale<std::uint64_t>>* writeBatchSize;
  // metrics::Histogram<metrics::LogScale<std::uint64_t>>* writeBatchCount; TODO
  // required?

  metrics::Histogram<metrics::LogScale<std::uint64_t>>* rocksdbWriteTimeInUs;
  metrics::Histogram<metrics::LogScale<std::uint64_t>>* rocksdbSyncTimeInUs;

  metrics::Histogram<metrics::LogScale<std::uint64_t>>* operationLatencyInsert;
  metrics::Histogram<metrics::LogScale<std::uint64_t>>*
      operationLatencyRemoveFront;
  metrics::Histogram<metrics::LogScale<std::uint64_t>>*
      operationLatencyRemoveBack;
};

}  // namespace arangodb::replication2::storage::rocksdb
