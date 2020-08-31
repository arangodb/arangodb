////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2020 ArangoDB GmbH, Cologne, Germany
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

#include <map>

#include "Basics/Result.h"
#include "Utils/CollectionGuard.h"
#include "Utils/DatabaseGuard.h"
#include "VocBase/voc-types.h"

#include <velocypack/Builder.h>

namespace arangodb {

struct WalAccessResult {
  WalAccessResult() : WalAccessResult(TRI_ERROR_NO_ERROR, false, 0, 0, 0) {}

  WalAccessResult(int code, bool ft, TRI_voc_tick_t included,
                  TRI_voc_tick_t lastScannedTick, TRI_voc_tick_t latest)
      : _result(code),
        _fromTickIncluded(ft),
        _lastIncludedTick(included),
        _lastScannedTick(lastScannedTick),
        _latestTick(latest) {}

  /*
    WalAccessResult(WalAccessResult const& other) = default;
    WalAccessResult& operator=(WalAccessResult const& other)  = default;
  */
  bool fromTickIncluded() const { return _fromTickIncluded; }
  TRI_voc_tick_t lastIncludedTick() const { return _lastIncludedTick; }
  TRI_voc_tick_t lastScannedTick() const { return _lastScannedTick; }
  void lastScannedTick(TRI_voc_tick_t tick) { _lastScannedTick = tick; }
  TRI_voc_tick_t latestTick() const { return _latestTick; }

  WalAccessResult& reset(int errorNumber, bool ft, TRI_voc_tick_t included,
                         TRI_voc_tick_t lastScannedTick, TRI_voc_tick_t latest) {
    _result.reset(errorNumber);
    _fromTickIncluded = ft;
    _lastIncludedTick = included;
    _lastScannedTick = lastScannedTick;
    _latestTick = latest;
    return *this;
  }

  // forwarded methods
  bool ok() const { return _result.ok(); }
  bool fail() const { return _result.fail(); }
  int errorNumber() const { return _result.errorNumber(); }
  std::string errorMessage() const { return _result.errorMessage(); }
  void reset(Result const& other) { _result.reset(); }

  // access methods
  Result const& result() const& { return _result; }
  Result result() && { return std::move(_result); }

 private:
  Result _result;
  bool _fromTickIncluded;
  TRI_voc_tick_t _lastIncludedTick;
  TRI_voc_tick_t _lastScannedTick;
  TRI_voc_tick_t _latestTick;
};

/// @brief StorageEngine agnostic wal access interface.
/// TODO: add methods for _admin/wal/ and get rid of engine specific handlers
class WalAccess {
  WalAccess(WalAccess const&) = delete;
  WalAccess& operator=(WalAccess const&) = delete;

 protected:
  WalAccess() {}
  virtual ~WalAccess() = default;

 public:
  struct Filter {
    Filter() {}

    /// tick last scanned by the last iteration
    /// is used to find batches in rocksdb
    uint64_t tickLastScanned = 0;

    /// first tick to use
    uint64_t tickStart = 0;

    /// last tick to include
    uint64_t tickEnd = UINT64_MAX;

    /// In case collection is == 0,
    bool includeSystem = false;

    /// export _queues and _jobs collection
    bool includeFoxxQueues = false;

    /// only output markers from this database
    TRI_voc_tick_t vocbase = 0;
    /// Only output data from this collection
    /// FIXME: make a set of collections
    DataSourceId collection = DataSourceId::none();

    /// only include these transactions, up to
    /// (not including) firstRegularTick
    std::unordered_set<TransactionId> transactionIds;
    /// @brief starting from this tick ignore transactionIds
    TRI_voc_tick_t firstRegularTick = 0;
  };

  typedef std::function<void(TRI_vocbase_t*, velocypack::Slice const&)> MarkerCallback;
  typedef std::function<void(TransactionId, TransactionId)> TransactionCallback;

  /// {"tickMin":"123", "tickMax":"456",
  ///  "server":{"version":"3.2", "serverId":"abc"}}
  virtual Result tickRange(std::pair<TRI_voc_tick_t, TRI_voc_tick_t>& minMax) const = 0;

  /// {"lastTick":"123",
  ///  "server":{"version":"3.2",
  ///  "serverId":"abc"},
  ///  "clients": {
  ///    "serverId": "ass", "lastTick":"123", ...
  ///  }}
  ///
  virtual TRI_voc_tick_t lastTick() const = 0;

  /// should return the list of transactions started, but not committed in that
  /// range (range can be adjusted)
  virtual WalAccessResult openTransactions(Filter const& filter,
                                           TransactionCallback const&) const = 0;

  virtual WalAccessResult tail(Filter const& filter, size_t chunkSize,
                               MarkerCallback const&) const = 0;
};

/// @brief helper class used to resolve vocbases
///        and collections from wal markers in an efficient way
struct WalAccessContext {
  WalAccessContext(WalAccess::Filter const& filter, WalAccess::MarkerCallback const& c)
      : _filter(filter), _callback(c), _responseSize(0) {}

  ~WalAccessContext() = default;

  /// @brief check if db should be handled, might already be deleted
  bool shouldHandleDB(TRI_voc_tick_t dbid) const;

  /// @brief check if view should be handled, might already be deleted
  bool shouldHandleView(TRI_voc_tick_t dbid, DataSourceId vid) const;

  /// @brief Check if collection is in filter, will load collection
  /// and prevent deletion
  bool shouldHandleCollection(TRI_voc_tick_t dbid, DataSourceId cid);

  /// @brief try to get collection, may return null
  TRI_vocbase_t* loadVocbase(TRI_voc_tick_t dbid);

  LogicalCollection* loadCollection(TRI_voc_tick_t dbid, DataSourceId cid);

 public:
  /// @brief arbitrary collection filter (inclusive)
  const WalAccess::Filter _filter;
  /// @brief callback for marker output
  WalAccess::MarkerCallback _callback;

  /// @brief current response size
  size_t _responseSize;

  /// @brief result builder
  velocypack::Builder _builder;

  /// @brief cache the vocbases
  std::map<TRI_voc_tick_t, DatabaseGuard> _vocbases;

  // @brief collection replication UUID cache
  std::map<DataSourceId, CollectionGuard> _collectionCache;
};

}  // namespace arangodb

#endif
