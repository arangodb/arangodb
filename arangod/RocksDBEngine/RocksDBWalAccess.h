////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2017 ArangoDB GmbH, Cologne, Germany
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
/// @author Simon Grätzer
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_ROCKSDB_ENGINE_WAL_ACCESS_H
#define ARANGOD_ROCKSDB_ENGINE_WAL_ACCESS_H 1

#include "StorageEngine/WalAccess.h"
#include "VocBase/voc-types.h"

#include <velocypack/Builder.h>

namespace arangodb {

/// @brief StorageEngine agnostic wal access interface.
/// TODO: add methods for _admin/wal/ and get rid of engine specific handlers
class RocksDBWalAccess : public WalAccess {
 public:
  RocksDBWalAccess() {}
  virtual ~RocksDBWalAccess() {}

  /// {"tickMin":"123", "tickMax":"456", "version":"3.2", "serverId":"abc"}
  Result tickRange(std::pair<TRI_voc_tick_t,
                             TRI_voc_tick_t>& minMax) const override;

  /// {"lastTick":"123",
  ///  "version":"3.2",
  ///  "serverId":"abc",
  ///  "clients": {
  ///    "serverId": "ass", "lastTick":"123", ...
  ///  }}
  ///
  TRI_voc_tick_t lastTick() const override;

  WalTailingResult tail(uint64_t tickStart, uint64_t tickEnd, size_t chunkSize,
              bool includeSystem, WalFilter const& filter,
              velocypack::Builder&) const override;
};
}

#endif
