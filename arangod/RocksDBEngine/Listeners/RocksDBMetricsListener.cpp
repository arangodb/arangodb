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
/// @author Matthew Von-Maszewski
////////////////////////////////////////////////////////////////////////////////

#include "RocksDBMetricsListener.h"

#include "ApplicationFeatures/ApplicationServer.h"
#include "RestServer/MetricsFeature.h"

DECLARE_COUNTER(arangodb_rocksdb_write_stalls_total, "Number of times RocksDB has entered a stalled (slowed) write state");
DECLARE_COUNTER(arangodb_rocksdb_write_stops_total, "Number of times RocksDB has entered a stopped write state");

namespace arangodb {

/// @brief Setup the object, clearing variables, but do no real work
RocksDBMetricsListener::RocksDBMetricsListener(application_features::ApplicationServer& server)
    : _writeStalls(server.getFeature<arangodb::MetricsFeature>().add(arangodb_rocksdb_write_stalls_total{})),
      _writeStops(server.getFeature<arangodb::MetricsFeature>().add(arangodb_rocksdb_write_stops_total{})) {}

void RocksDBMetricsListener::OnStallConditionsChanged(const rocksdb::WriteStallInfo& info) {
  // we should only get here if there's an actual change
  TRI_ASSERT(info.condition.cur != info.condition.prev);

  // in the case that we go from normal to stalled or stopped, we count it;
  // we also count a stall if we go from stopped to stall since it's a distinct
  // state

  if (info.condition.cur == rocksdb::WriteStallCondition::kDelayed) {
    _writeStalls.count();
    LOG_TOPIC("9123c", WARN, Logger::ENGINES)
        << "rocksdb is slowing incoming writes to column family '"
        << info.cf_name << "' to let background writes catch up";
  } else if (info.condition.cur == rocksdb::WriteStallCondition::kStopped) {
    _writeStops.count();
    LOG_TOPIC("9123d", WARN, Logger::ENGINES)
        << "rocksdb has stopped incoming writes to column family '"
        << info.cf_name << "' to let background writes catch up";
  } else {
    TRI_ASSERT(info.condition.cur == rocksdb::WriteStallCondition::kNormal);
    LOG_TOPIC("9123e", INFO, Logger::ENGINES)
        << "rocksdb is resuming normal writes for column family '"
        << info.cf_name << "'";
  }
}

}  // namespace arangodb
