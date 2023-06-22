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

#pragma once

#include "RocksDBEngine/RocksDBEngine.h"

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
    DataSourceId datasourceId;
    LocalDocumentId documentId;
  };
  using Marker = std::variant<PutMarker, DeleteMarker>;

  void tail(std::function<void(Marker const&)> const&) const;

  RocksDBEngine& _engine;
  TRI_voc_tick_t _startTick{0};
  TRI_voc_tick_t _endTick{0};
};
}  // namespace arangodb
