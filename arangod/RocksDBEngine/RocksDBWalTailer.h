////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2023 ArangoDB GmbH, Cologne, Germany
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
////////////////////////////////////////////////////////////////////////////////
///
/// This WalTailer was primarily written to enable HotBackup consistency in
/// conjunction with ArangoSearch.
///
/// In the HotBackup code, the RocksDB WAL between a point where an ArangoSearch
/// snapshot was taken and a point between a RocksDB snapshot was taken is
/// applied to the ArangoSearch snapshot.
///
/// This application process did not require any of the complications
/// implemented in `WalAccess`, and in particular it seemed counter-productive
/// to use WalAccess seeing as it did not have some information needed, such as
/// LocalDocumentIds
///
/// Nothing speaks against extending this WalTailer to become more sophisticated
/// (with the caveat that it should not become too complicated).
///
/// One detail that took some time to establish was the role of ticks/rocksdb
/// sequence numbers;
///
/// The rocksdb::WriteBatch::Handler only gets the initial sequence number for
/// a batch, and we have to count every operation; this is done using incTick()
///
#pragma once

#include "RocksDBEngine/RocksDBEngine.h"
#include "Basics/ErrorT.h"

namespace arangodb {

class RocksDBWalTailer {
 public:
  explicit RocksDBWalTailer(RocksDBEngine& engine, TRI_voc_tick_t startTick,
                            TRI_voc_tick_t endTick)
      : _engine(engine), _startTick(startTick), _endTick(endTick) {}
  virtual ~RocksDBWalTailer() = default;

  struct PutMarker {
    TRI_voc_tick_t tick;
    DataSourceId datasourceId;
    LocalDocumentId documentId;
    VPackSlice document;
  };
  struct DeleteMarker {
    TRI_voc_tick_t tick;
    DataSourceId datasourceId;
    LocalDocumentId documentId;
  };
  using Marker = std::variant<PutMarker, DeleteMarker>;

  using TailingResult = errors::ErrorT<rocksdb::Status, std::monostate>;
  auto tail(std::function<void(Marker const&)> const&) const -> TailingResult;

  RocksDBEngine& _engine;
  TRI_voc_tick_t _startTick{0};
  TRI_voc_tick_t _endTick{0};
};
}  // namespace arangodb
