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
/// @author Dan Larkin-York
////////////////////////////////////////////////////////////////////////////////

#pragma once

// public rocksdb headers
#include <rocksdb/listener.h>

#include "Metrics/Fwd.h"
#include "RestServer/arangod.h"

#include <string_view>

namespace rocksdb {
struct CompactionJobInfo;
class DB;
struct FlushJobInfo;
}  // namespace rocksdb

namespace arangodb {
namespace application_features {
class ApplicationServer;
}

/// @brief Gathers better metrics from RocksDB than we can get by scraping
/// alone.
class RocksDBMetricsListener : public rocksdb::EventListener {
 public:
  explicit RocksDBMetricsListener(ArangodServer&);

  void OnFlushBegin(rocksdb::DB*, rocksdb::FlushJobInfo const& info) override;
  void OnFlushCompleted(rocksdb::DB*,
                        rocksdb::FlushJobInfo const& info) override;

  void OnCompactionBegin(rocksdb::DB*,
                         rocksdb::CompactionJobInfo const&) override;
  void OnCompactionCompleted(rocksdb::DB*,
                             rocksdb::CompactionJobInfo const&) override;
  void OnStallConditionsChanged(rocksdb::WriteStallInfo const& info) override;

 private:
  void handleFlush(std::string_view phase,
                   rocksdb::FlushJobInfo const& info) const;

  void handleCompaction(std::string_view phase,
                        rocksdb::CompactionJobInfo const& info) const;

 protected:
  metrics::Counter& _writeStalls;
  metrics::Counter& _writeStops;
};

}  // namespace arangodb
