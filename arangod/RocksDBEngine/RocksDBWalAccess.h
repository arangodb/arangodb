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

namespace arangodb {

/// @brief StorageEngine agnostic wal access interface.
/// TODO: add methods for _admin/wal/ and get rid of engine specific handlers
class RocksDBWalAccess final : public WalAccess {
 public:
  RocksDBWalAccess() {}
  virtual ~RocksDBWalAccess() = default;

  /// {"tickMin":"123", "tickMax":"456", "version":"3.2", "serverId":"abc"}
  Result tickRange(std::pair<TRI_voc_tick_t, TRI_voc_tick_t>& minMax) const override;

  /// {"lastTick":"123",
  ///  "version":"3.2",
  ///  "serverId":"abc",
  ///  "clients": {
  ///    "serverId": "ass", "lastTick":"123", ...
  ///  }}
  ///
  TRI_voc_tick_t lastTick() const override;

  /// should return the list of transactions started, but not committed in that
  /// range (range can be adjusted)
  WalAccessResult openTransactions(WalAccess::Filter const& filter,
                                   TransactionCallback const&) const override;

  /// Tails the wall, this will already sanitize the
  WalAccessResult tail(WalAccess::Filter const& filter, size_t chunkSize,
                       TRI_voc_tick_t barrierId, 
                       MarkerCallback const&) const override;
};
}  // namespace arangodb

#endif
