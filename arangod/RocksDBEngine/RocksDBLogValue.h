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
/// @author Jan Steemann
/// @author Daniel H. Larkin
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGO_ROCKSDB_ROCKSDB_LOG_VALUE_H
#define ARANGO_ROCKSDB_ROCKSDB_LOG_VALUE_H 1

#include <rocksdb/slice.h>

#include <velocypack/Slice.h>
#include <velocypack/StringRef.h>
#include <velocypack/velocypack-aliases.h>

#include "Basics/Common.h"
#include "RocksDBEngine/RocksDBTypes.h"
#include "VocBase/Identifiers/DataSourceId.h"
#include "VocBase/Identifiers/IndexId.h"
#include "VocBase/Identifiers/LocalDocumentId.h"
#include "VocBase/Identifiers/RevisionId.h"
#include "VocBase/Identifiers/TransactionId.h"
#include "VocBase/voc-types.h"

namespace arangodb {

class RocksDBLogValue {
 public:
  //----------------------------------------------------------------------------
  // SECTION Constructors
  // Each of these simply specifies the correct type and copies the input
  // parameter in an appropriate format into the underlying string buffer.
  //----------------------------------------------------------------------------

  static RocksDBLogValue DatabaseCreate(TRI_voc_tick_t id);
  static RocksDBLogValue DatabaseDrop(TRI_voc_tick_t id);

  static RocksDBLogValue CollectionCreate(TRI_voc_tick_t dbid, DataSourceId cid);
  static RocksDBLogValue CollectionDrop(TRI_voc_tick_t dbid, DataSourceId cid,
                                        arangodb::velocypack::StringRef const& uuid);
  static RocksDBLogValue CollectionRename(TRI_voc_tick_t dbid, DataSourceId cid,
                                          arangodb::velocypack::StringRef const& oldName);
  static RocksDBLogValue CollectionChange(TRI_voc_tick_t dbid, DataSourceId cid);
  static RocksDBLogValue CollectionTruncate(TRI_voc_tick_t dbid,
                                            DataSourceId cid, uint64_t objectId);

  static RocksDBLogValue IndexCreate(TRI_voc_tick_t dbid, DataSourceId cid,
                                     VPackSlice const& indexInfo);
  static RocksDBLogValue IndexDrop(TRI_voc_tick_t dbid, DataSourceId cid, IndexId indexId);

  static RocksDBLogValue ViewCreate(TRI_voc_tick_t, DataSourceId);
  static RocksDBLogValue ViewDrop(TRI_voc_tick_t, DataSourceId,
                                  arangodb::velocypack::StringRef const& uuid);
  static RocksDBLogValue ViewChange(TRI_voc_tick_t, DataSourceId);

  static RocksDBLogValue BeginTransaction(TRI_voc_tick_t vocbaseId, TransactionId tid);
  static RocksDBLogValue CommitTransaction(TRI_voc_tick_t vocbaseId, TransactionId tid);
  static RocksDBLogValue DocumentRemoveV2(RevisionId rid);

  static RocksDBLogValue SinglePut(TRI_voc_tick_t vocbaseId, DataSourceId cid);
  static RocksDBLogValue SingleRemoveV2(TRI_voc_tick_t vocbaseId,
                                        DataSourceId cid, RevisionId rid);

  static RocksDBLogValue TrackedDocumentInsert(LocalDocumentId, velocypack::Slice const&);
  static RocksDBLogValue TrackedDocumentRemove(LocalDocumentId, velocypack::Slice const&);

  // empty log value
  static RocksDBLogValue Empty();

 public:
  static RocksDBLogType type(rocksdb::Slice const&);
  static TRI_voc_tick_t databaseId(rocksdb::Slice const&);
  static TransactionId transactionId(rocksdb::Slice const&);
  static DataSourceId collectionId(rocksdb::Slice const&);
  static DataSourceId viewId(rocksdb::Slice const&);
  static IndexId indexId(rocksdb::Slice const&);

  /// CollectionTruncate contains an object id
  static uint64_t objectId(rocksdb::Slice const&);

  /// For DocumentRemoveV2 and SingleRemoveV2
  static RevisionId revisionId(rocksdb::Slice const&);

  static velocypack::Slice indexSlice(rocksdb::Slice const&);
  static velocypack::Slice viewSlice(rocksdb::Slice const&);
  /// @brief get UUID from collection drop marker
  static arangodb::velocypack::StringRef collectionUUID(rocksdb::Slice const&);
  /// @brief get UUID from view drop marker
  static arangodb::velocypack::StringRef viewUUID(rocksdb::Slice const&);

  /// @deprecated method for old collection drop marker
  static arangodb::velocypack::StringRef oldCollectionName(rocksdb::Slice const&);
  
  /// @brief get slice from tracked document
  static std::pair<LocalDocumentId, velocypack::Slice> trackedDocument(rocksdb::Slice const&);

  static bool containsDatabaseId(RocksDBLogType type);
  static bool containsDataSourceId(RocksDBLogType type);
  static bool containsViewId(RocksDBLogType type);

 public:
  //////////////////////////////////////////////////////////////////////////////
  /// @brief Returns a reference to the underlying string buffer.
  //////////////////////////////////////////////////////////////////////////////
  std::string const& string() const { return _buffer; }  // to be used with put
  RocksDBLogType type() const {
    return static_cast<RocksDBLogType>(*(_buffer.data()));
  }
  rocksdb::Slice slice() const { return rocksdb::Slice(_buffer); }

 private:
  explicit RocksDBLogValue() = default;
  RocksDBLogValue(RocksDBLogType, uint64_t);
  RocksDBLogValue(RocksDBLogType, uint64_t, uint64_t);
  RocksDBLogValue(RocksDBLogType, uint64_t, uint64_t, uint64_t);
  RocksDBLogValue(RocksDBLogType, uint64_t, uint64_t, VPackSlice const&);
  RocksDBLogValue(RocksDBLogType, uint64_t, uint64_t, arangodb::velocypack::StringRef const& data);

 private:
  std::string _buffer;
};

}  // namespace arangodb

#endif
