////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2022 ArangoDB GmbH, Cologne, Germany
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
/// @author Dan Larkin-York
////////////////////////////////////////////////////////////////////////////////

#pragma once

// public rocksdb headers
#include <rocksdb/listener.h>

#include "Metrics/Fwd.h"
#include "RestServer/arangod.h"

#include "ApplicationFeatures/ApplicationServer.h"
#include "Metrics/CounterBuilder.h"
#include "Metrics/MetricsFeature.h"
DECLARE_COUNTER(
    arangodb_rocksdb_write_stalls_total,
    "Number of times RocksDB has entered a stalled (slowed) write state");
DECLARE_COUNTER(arangodb_rocksdb_write_stops_total,
                "Number of times RocksDB has entered a stopped write state");
DECLARE_COUNTER(
    arangodb_rocksdb_temp_write_stalls_total,
    "Number of times RocksDB has entered a stalled (slowed) write state");
DECLARE_COUNTER(arangodb_rocksdb_temp_write_stops_total,
                "Number of times RocksDB has entered a stopped write state");
namespace arangodb {
namespace application_features {
class ApplicationServer;
}

/// @brief Gathers better metrics from RocksDB than we can get by scraping
/// alone.
template <typename T=arangodb_rocksdb_write_stalls_total, typename S=arangodb_rocksdb_write_stops_total>
class RocksDBMetricsListener : public rocksdb::EventListener {
 public:
  explicit RocksDBMetricsListener(ArangodServer&);

  void OnStallConditionsChanged(const rocksdb::WriteStallInfo& info) override;

 protected:
  metrics::Counter& _writeStalls;
  metrics::Counter& _writeStops;
};

}  // namespace arangodb
