////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2017 ArangoDB GmbH, Cologne, Germany
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

#include "Basics/Common.h"
#include "Basics/StringRef.h"
#include "RocksDBEngine/RocksDBTypes.h"
#include "VocBase/voc-types.h"

#include <rocksdb/slice.h>
#include <velocypack/Slice.h>
#include <velocypack/velocypack-aliases.h>

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

  static RocksDBLogValue CollectionCreate(TRI_voc_tick_t dbid, TRI_voc_cid_t cid);
  static RocksDBLogValue CollectionDrop(TRI_voc_tick_t dbid, TRI_voc_cid_t cid,
                                        StringRef const& uuid);
  static RocksDBLogValue CollectionRename(TRI_voc_tick_t dbid, TRI_voc_cid_t cid,
                                          StringRef const& oldName);
  static RocksDBLogValue CollectionChange(TRI_voc_tick_t dbid, TRI_voc_cid_t cid);
  static RocksDBLogValue CollectionTruncate(TRI_voc_tick_t dbid,
                                            TRI_voc_cid_t cid, uint64_t objectId);

  static RocksDBLogValue IndexCreate(TRI_voc_tick_t dbid, TRI_voc_cid_t cid,
                                     VPackSlice const& indexInfo);
  static RocksDBLogValue IndexDrop(TRI_voc_tick_t dbid, TRI_voc_cid_t cid,
                                   TRI_idx_iid_t indexId);

  static RocksDBLogValue ViewCreate(TRI_voc_tick_t, TRI_voc_cid_t);
  static RocksDBLogValue ViewDrop(TRI_voc_tick_t, TRI_voc_cid_t, StringRef const& uuid);
  static RocksDBLogValue ViewChange(TRI_voc_tick_t, TRI_voc_cid_t);

#ifdef USE_IRESEARCH
  static RocksDBLogValue IResearchLinkDrop(TRI_voc_tick_t, TRI_voc_cid_t,
                                           TRI_voc_cid_t, TRI_idx_iid_t);
#endif

  static RocksDBLogValue BeginTransaction(TRI_voc_tick_t vocbaseId, TRI_voc_tid_t tid);
  static RocksDBLogValue CommitTransaction(TRI_voc_tick_t vocbaseId, TRI_voc_tid_t tid);
  static RocksDBLogValue DocumentRemoveV2(TRI_voc_rid_t rid);

  static RocksDBLogValue SinglePut(TRI_voc_tick_t vocbaseId, TRI_voc_cid_t cid);
  static RocksDBLogValue SingleRemoveV2(TRI_voc_tick_t vocbaseId,
                                        TRI_voc_cid_t cid, TRI_voc_rid_t rid);

  // empty log value
  static RocksDBLogValue Empty();

 public:
  static RocksDBLogType type(rocksdb::Slice const&);
  static TRI_voc_tick_t databaseId(rocksdb::Slice const&);
  static TRI_voc_tid_t transactionId(rocksdb::Slice const&);
  static TRI_voc_cid_t collectionId(rocksdb::Slice const&);
  static TRI_voc_cid_t viewId(rocksdb::Slice const&);
  static TRI_idx_iid_t indexId(rocksdb::Slice const&);

  /// CollectionTruncate contains an object id
  static uint64_t objectId(rocksdb::Slice const&);

  /// For DocumentRemoveV2 and SingleRemoveV2
  static TRI_voc_rid_t revisionId(rocksdb::Slice const&);

  static velocypack::Slice indexSlice(rocksdb::Slice const&);
  static velocypack::Slice viewSlice(rocksdb::Slice const&);
  /// @brief get UUID from collection drop marker
  static arangodb::StringRef collectionUUID(rocksdb::Slice const&);
  /// @brief get UUID from view drop marker
  static arangodb::StringRef viewUUID(rocksdb::Slice const&);

  // deprecated method for old collection drop marker
  static arangodb::StringRef oldCollectionName(rocksdb::Slice const&);

  static bool containsDatabaseId(RocksDBLogType type);
  static bool containsCollectionId(RocksDBLogType type);
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
  explicit RocksDBLogValue() {}
  RocksDBLogValue(RocksDBLogType, uint64_t);
  RocksDBLogValue(RocksDBLogType, uint64_t, uint64_t);
  RocksDBLogValue(RocksDBLogType, uint64_t, uint64_t, uint64_t);
#ifdef USE_IRESEARCH
  RocksDBLogValue(RocksDBLogType, uint64_t, uint64_t, uint64_t, uint64_t);
#endif
  RocksDBLogValue(RocksDBLogType, uint64_t, uint64_t, VPackSlice const&);
  RocksDBLogValue(RocksDBLogType, uint64_t, uint64_t, StringRef const& data);

 private:
  std::string _buffer;
};

}  // namespace arangodb

#endif
