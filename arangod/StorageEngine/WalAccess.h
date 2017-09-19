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

#ifndef ARANGOD_STORAGE_ENGINE_WAL_ACCESS_H
#define ARANGOD_STORAGE_ENGINE_WAL_ACCESS_H 1

#include "Basics/Result.h"
#include "VocBase/voc-types.h"

#include <velocypack/Builder.h>

namespace arangodb {
  
struct WalTailingResult : public Result {
  WalTailingResult()
    : Result(TRI_ERROR_NO_ERROR), _minTick(0), _maxTick(0) {}
  WalTailingResult(int code, TRI_voc_tick_t min, TRI_voc_tick_t max)
    : Result(code), _minTick(min), _maxTick(max) {}

  TRI_voc_tick_t minTick() const { return _minTick; }
  TRI_voc_tick_t maxTick() const { return _maxTick; }
  
 private:
  TRI_voc_tick_t _minTick;
  TRI_voc_tick_t _maxTick;
};

/// @brief StorageEngine agnostic wal access interface.
/// TODO: add methods for _admin/wal/ and get rid of engine specific handlers
class WalAccess {
  WalAccess(WalAccess const&) = delete;
  WalAccess& operator=(WalAccess const&) = delete;

 protected:
  WalAccess() {}
  virtual ~WalAccess() {};

 public:
  typedef std::unordered_map<TRI_voc_tick_t, std::set<TRI_voc_cid_t>> WalFilter;
  typedef std::function<void(TRI_vocbase_t*,
                        velocypack::Slice const&)> MarkerCallback;

  /// {"tickMin":"123", "tickMax":"456",
  ///  "server":{"version":"3.2", "serverId":"abc"}}
  virtual Result tickRange(std::pair<TRI_voc_tick_t,
                                     TRI_voc_tick_t>& minMax) const = 0;

  /// {"lastTick":"123",
  ///  "server":{"version":"3.2",
  ///  "serverId":"abc"},
  ///  "clients": {
  ///    "serverId": "ass", "lastTick":"123", ...
  ///  }}
  ///
  virtual TRI_voc_tick_t lastTick() const = 0;

  virtual WalTailingResult tail(uint64_t tickStart, uint64_t tickEnd,
                                size_t chunkSize,
                                bool includeSystem, WalFilter const& filter,
                                MarkerCallback const&) const = 0;
};
}

#endif
