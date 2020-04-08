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
/// @author Jan Steemann
/// @author Daniel H. Larkin
////////////////////////////////////////////////////////////////////////////////

#include "RocksDBLogValue.h"
#include "Basics/Common.h"
#include "Basics/Exceptions.h"
#include "RocksDBEngine/RocksDBCommon.h"
#include "RocksDBEngine/RocksDBFormat.h"

using namespace arangodb;
using namespace arangodb::rocksutils;

RocksDBLogValue RocksDBLogValue::DatabaseCreate(TRI_voc_tick_t id) {
  return RocksDBLogValue(RocksDBLogType::DatabaseCreate, id);
}

RocksDBLogValue RocksDBLogValue::DatabaseDrop(TRI_voc_tick_t id) {
  return RocksDBLogValue(RocksDBLogType::DatabaseDrop, id);
}

RocksDBLogValue RocksDBLogValue::CollectionCreate(TRI_voc_tick_t dbid, TRI_voc_cid_t cid) {
  return RocksDBLogValue(RocksDBLogType::CollectionCreate, dbid, cid);
}

RocksDBLogValue RocksDBLogValue::CollectionDrop(TRI_voc_tick_t dbid, TRI_voc_cid_t cid,
                                                arangodb::velocypack::StringRef const& uuid) {
  return RocksDBLogValue(RocksDBLogType::CollectionDrop, dbid, cid, uuid);
}

RocksDBLogValue RocksDBLogValue::CollectionRename(TRI_voc_tick_t dbid, TRI_voc_cid_t cid,
                                                  arangodb::velocypack::StringRef const& oldName) {
  return RocksDBLogValue(RocksDBLogType::CollectionRename, dbid, cid, oldName);
}

RocksDBLogValue RocksDBLogValue::CollectionChange(TRI_voc_tick_t dbid, TRI_voc_cid_t cid) {
  return RocksDBLogValue(RocksDBLogType::CollectionChange, dbid, cid);
}

RocksDBLogValue RocksDBLogValue::CollectionTruncate(TRI_voc_tick_t dbid, TRI_voc_cid_t cid,
                                                    uint64_t objectId) {
  return RocksDBLogValue(RocksDBLogType::CollectionTruncate, dbid, cid, objectId);
}

RocksDBLogValue RocksDBLogValue::IndexCreate(TRI_voc_tick_t dbid, TRI_voc_cid_t cid,
                                             VPackSlice const& indexInfo) {
  return RocksDBLogValue(RocksDBLogType::IndexCreate, dbid, cid, indexInfo);
}

RocksDBLogValue RocksDBLogValue::IndexDrop(TRI_voc_tick_t dbid,
                                           TRI_voc_cid_t cid, IndexId iid) {
  return RocksDBLogValue(RocksDBLogType::IndexDrop, dbid, cid, iid.id());
}

RocksDBLogValue RocksDBLogValue::ViewCreate(TRI_voc_tick_t dbid, TRI_voc_cid_t vid) {
  return RocksDBLogValue(RocksDBLogType::ViewCreate, dbid, vid);
}

RocksDBLogValue RocksDBLogValue::ViewDrop(TRI_voc_tick_t dbid, TRI_voc_cid_t vid,
                                          arangodb::velocypack::StringRef const& uuid) {
  return RocksDBLogValue(RocksDBLogType::ViewDrop, dbid, vid, uuid);
}

RocksDBLogValue RocksDBLogValue::ViewChange(TRI_voc_tick_t dbid, TRI_voc_cid_t vid) {
  return RocksDBLogValue(RocksDBLogType::ViewChange, dbid, vid);
}

RocksDBLogValue RocksDBLogValue::BeginTransaction(TRI_voc_tick_t dbid, TRI_voc_tid_t tid) {
  return RocksDBLogValue(RocksDBLogType::BeginTransaction, dbid, tid);
}

RocksDBLogValue RocksDBLogValue::CommitTransaction(TRI_voc_tick_t dbid, TRI_voc_tid_t tid) {
  return RocksDBLogValue(RocksDBLogType::CommitTransaction, dbid, tid);
}

RocksDBLogValue RocksDBLogValue::DocumentRemoveV2(TRI_voc_rid_t rid) {
  return RocksDBLogValue(RocksDBLogType::DocumentRemoveV2, rid);
}

RocksDBLogValue RocksDBLogValue::SinglePut(TRI_voc_tick_t vocbaseId, TRI_voc_cid_t cid) {
  return RocksDBLogValue(RocksDBLogType::SinglePut, vocbaseId, cid);
}

RocksDBLogValue RocksDBLogValue::SingleRemoveV2(TRI_voc_tick_t vocbaseId,
                                                TRI_voc_cid_t cid, TRI_voc_rid_t rid) {
  return RocksDBLogValue(RocksDBLogType::SingleRemoveV2, vocbaseId, cid, rid);
}

RocksDBLogValue RocksDBLogValue::TrackedDocumentInsert(LocalDocumentId docId,
                                                       VPackSlice const& slice) {
  RocksDBLogValue val{};
  val._buffer.reserve(sizeof(RocksDBLogType) + sizeof(LocalDocumentId::BaseType) + slice.byteSize());
  val._buffer.push_back(static_cast<char>(RocksDBLogType::TrackedDocumentInsert));
  uintToPersistentLittleEndian(val._buffer, docId.id());
  val._buffer.append(slice.startAs<char>(), slice.byteSize());
  return val;
}

RocksDBLogValue RocksDBLogValue::TrackedDocumentRemove(LocalDocumentId docId,
                                                       VPackSlice const& slice) {
  RocksDBLogValue val{};
  val._buffer.reserve(sizeof(RocksDBLogType) + sizeof(LocalDocumentId::BaseType) + slice.byteSize());
  val._buffer.push_back(static_cast<char>(RocksDBLogType::TrackedDocumentRemove));
  uintToPersistentLittleEndian(val._buffer, docId.id());
  val._buffer.append(reinterpret_cast<char const*>(slice.begin()), slice.byteSize());
  return val;
}

RocksDBLogValue RocksDBLogValue::Empty() {
  return RocksDBLogValue();
}

RocksDBLogValue::RocksDBLogValue(RocksDBLogType type, uint64_t val)
    : _buffer() {
  switch (type) {
    case RocksDBLogType::DatabaseCreate:
    case RocksDBLogType::DatabaseDrop:
    case RocksDBLogType::DocumentRemoveV2: {
      _buffer.reserve(sizeof(RocksDBLogType) + sizeof(uint64_t));
      _buffer.push_back(static_cast<char>(type));
      uint64ToPersistent(_buffer, val);  // database or collection ID
      break;
    }

    default:
      TRI_ASSERT(false);
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_BAD_PARAMETER,
                                     "invalid type for log value");
  }
}

RocksDBLogValue::RocksDBLogValue(RocksDBLogType type, uint64_t dbId, uint64_t val2)
    : _buffer() {
  switch (type) {
    case RocksDBLogType::CollectionCreate:
    case RocksDBLogType::CollectionChange:
    case RocksDBLogType::ViewCreate:
    case RocksDBLogType::ViewChange:
    case RocksDBLogType::BeginTransaction:
    case RocksDBLogType::SinglePut:
    case RocksDBLogType::CommitTransaction: {
      _buffer.reserve(sizeof(RocksDBLogType) + sizeof(uint64_t) * 2);
      _buffer.push_back(static_cast<char>(type));
      uint64ToPersistent(_buffer, dbId);
      uint64ToPersistent(_buffer, val2);
      break;
    }

    default:
      TRI_ASSERT(false);
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_BAD_PARAMETER,
                                     "invalid type for log value");
  }
}

RocksDBLogValue::RocksDBLogValue(RocksDBLogType type, uint64_t dbId, uint64_t cid, uint64_t third)
    : _buffer() {
  switch (type) {
    case RocksDBLogType::CollectionTruncate:
    case RocksDBLogType::IndexDrop:
    case RocksDBLogType::SingleRemoveV2: {
      _buffer.reserve(sizeof(RocksDBLogType) + sizeof(uint64_t) * 3);
      _buffer.push_back(static_cast<char>(type));
      uint64ToPersistent(_buffer, dbId);
      uint64ToPersistent(_buffer, cid);
      uint64ToPersistent(_buffer, third);
      break;
    }
    default:
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_BAD_PARAMETER,
                                     "invalid type for log value");
  }
}

RocksDBLogValue::RocksDBLogValue(RocksDBLogType type, uint64_t dbId,
                                 uint64_t cid, VPackSlice const& info)
    : _buffer() {
  switch (type) {
    case RocksDBLogType::IndexCreate: {
      _buffer.reserve(sizeof(RocksDBLogType) + (sizeof(uint64_t) * 2) + info.byteSize());
      _buffer.push_back(static_cast<char>(type));
      uint64ToPersistent(_buffer, dbId);
      uint64ToPersistent(_buffer, cid);
      _buffer.append(reinterpret_cast<char const*>(info.begin()), info.byteSize());
      break;
    }
    default:
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_BAD_PARAMETER,
                                     "invalid type for log value");
  }
}

RocksDBLogValue::RocksDBLogValue(RocksDBLogType type, uint64_t dbId,
                                 uint64_t cid, arangodb::velocypack::StringRef const& data)
    : _buffer() {
  switch (type) {
    case RocksDBLogType::CollectionDrop:
    case RocksDBLogType::CollectionRename:
    case RocksDBLogType::ViewDrop: {
      _buffer.reserve(sizeof(RocksDBLogType) + sizeof(uint64_t) * 2 + data.length());
      _buffer.push_back(static_cast<char>(type));
      uint64ToPersistent(_buffer, dbId);
      uint64ToPersistent(_buffer, cid);
      // collection uuid for CollectionDrop
      _buffer.append(data.data(), data.length());
      break;
    }
    default:
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_BAD_PARAMETER,
                                     "invalid type for log value");
  }
}

// ================= Instance Methods ===================

RocksDBLogType RocksDBLogValue::type(rocksdb::Slice const& slice) {
  TRI_ASSERT(slice.size() >= sizeof(RocksDBLogType));
  return static_cast<RocksDBLogType>(slice.data()[0]);
}

TRI_voc_tick_t RocksDBLogValue::databaseId(rocksdb::Slice const& slice) {
  TRI_ASSERT(slice.size() >= sizeof(RocksDBLogType) + sizeof(uint64_t));
  RocksDBLogType type = static_cast<RocksDBLogType>(slice.data()[0]);
  TRI_ASSERT(RocksDBLogValue::containsDatabaseId(type));
  return uint64FromPersistent(slice.data() + sizeof(RocksDBLogType));
}

TRI_voc_cid_t RocksDBLogValue::collectionId(rocksdb::Slice const& slice) {
  TRI_ASSERT(slice.size() >= sizeof(RocksDBLogType) + sizeof(uint64_t));
  RocksDBLogType type = static_cast<RocksDBLogType>(slice.data()[0]);
  TRI_ASSERT(RocksDBLogValue::containsCollectionId(type));
  TRI_ASSERT(slice.size() >= sizeof(RocksDBLogType) + sizeof(uint64_t) * 2);
  return uint64FromPersistent(slice.data() + sizeof(RocksDBLogType) + sizeof(uint64_t));
}

TRI_voc_cid_t RocksDBLogValue::viewId(rocksdb::Slice const& slice) {
  TRI_ASSERT(slice.size() >= sizeof(RocksDBLogType) + sizeof(uint64_t));
  RocksDBLogType type = static_cast<RocksDBLogType>(slice.data()[0]);
  TRI_ASSERT(RocksDBLogValue::containsViewId(type));
  TRI_ASSERT(slice.size() >= sizeof(RocksDBLogType) + sizeof(uint64_t) * 2);
  return uint64FromPersistent(slice.data() + sizeof(RocksDBLogType) + sizeof(uint64_t));
}

TRI_voc_tid_t RocksDBLogValue::transactionId(rocksdb::Slice const& slice) {
  TRI_ASSERT(slice.size() >= sizeof(RocksDBLogType) + sizeof(uint64_t));
  RocksDBLogType type = static_cast<RocksDBLogType>(slice.data()[0]);
  TRI_ASSERT(type == RocksDBLogType::BeginTransaction || type == RocksDBLogType::CommitTransaction);
  // <type> + 8-byte <dbId> + 8-byte <trxId>
  return uint64FromPersistent(slice.data() + sizeof(RocksDBLogType) + sizeof(TRI_voc_tick_t));
}

IndexId RocksDBLogValue::indexId(rocksdb::Slice const& slice) {
  TRI_ASSERT(slice.size() >= sizeof(RocksDBLogType) + (3 * sizeof(uint64_t)));
  RocksDBLogType type = static_cast<RocksDBLogType>(slice.data()[0]);
  TRI_ASSERT(type == RocksDBLogType::IndexDrop);
  return IndexId{uint64FromPersistent(slice.data() + sizeof(RocksDBLogType) +
                                      (2 * sizeof(uint64_t)))};
}

/// CollectionTruncate contains an object id
uint64_t RocksDBLogValue::objectId(rocksdb::Slice const& slice) {
  TRI_ASSERT(slice.size() >= sizeof(RocksDBLogType) + (sizeof(uint64_t)) * 3);
  RocksDBLogType type = static_cast<RocksDBLogType>(slice.data()[0]);
  TRI_ASSERT(type == RocksDBLogType::CollectionTruncate);
  return uint64FromPersistent(slice.data() + sizeof(RocksDBLogType) + 2 * sizeof(uint64_t));
}

/// For DocumentRemoveV2 and SingleRemoveV2
TRI_voc_rid_t RocksDBLogValue::revisionId(rocksdb::Slice const& slice) {
  TRI_ASSERT(slice.size() >= sizeof(RocksDBLogType) + (sizeof(uint64_t)));
  RocksDBLogType type = static_cast<RocksDBLogType>(slice.data()[0]);
  if (type == RocksDBLogType::DocumentRemoveV2) {
    return uint64FromPersistent(slice.data() + sizeof(RocksDBLogType));
  } else if (type == RocksDBLogType::SingleRemoveV2) {
    TRI_ASSERT(slice.size() >= sizeof(RocksDBLogType) + (3 * sizeof(uint64_t)));
    return uint64FromPersistent(slice.data() + sizeof(RocksDBLogType) + 2 * sizeof(uint64_t));
  }
  TRI_ASSERT(false);  // invalid type
  return 0;
}

VPackSlice RocksDBLogValue::indexSlice(rocksdb::Slice const& slice) {
  TRI_ASSERT(slice.size() >= sizeof(RocksDBLogType) + sizeof(uint64_t) * 2);
  RocksDBLogType type = static_cast<RocksDBLogType>(slice.data()[0]);
  TRI_ASSERT(type == RocksDBLogType::IndexCreate);
  return VPackSlice(reinterpret_cast<uint8_t const*>(slice.data() + sizeof(RocksDBLogType) + sizeof(uint64_t) * 2));
}

VPackSlice RocksDBLogValue::viewSlice(rocksdb::Slice const& slice) {
  TRI_ASSERT(slice.size() >= sizeof(RocksDBLogType) + sizeof(uint64_t) * 2);
  RocksDBLogType type = static_cast<RocksDBLogType>(slice.data()[0]);
  TRI_ASSERT(type == RocksDBLogType::ViewDrop);
  return VPackSlice(reinterpret_cast<uint8_t const*>(slice.data() + sizeof(RocksDBLogType) + sizeof(uint64_t) * 2));
}

namespace {
arangodb::velocypack::StringRef dropMarkerUUID(rocksdb::Slice const& slice) {
  size_t off = sizeof(RocksDBLogType) + sizeof(uint64_t) * 2;
  TRI_ASSERT(slice.size() >= off);
  RocksDBLogType type = static_cast<RocksDBLogType>(slice.data()[0]);
  TRI_ASSERT(type == RocksDBLogType::CollectionDrop || type == RocksDBLogType::ViewDrop);
  if (slice.size() > off) {
    // have a UUID
    return arangodb::velocypack::StringRef(slice.data() + off, slice.size() - off);
  }
  // do not have a UUID
  return arangodb::velocypack::StringRef();
}
}  // namespace

arangodb::velocypack::StringRef RocksDBLogValue::collectionUUID(rocksdb::Slice const& slice) {
  return ::dropMarkerUUID(slice);
}

arangodb::velocypack::StringRef RocksDBLogValue::viewUUID(rocksdb::Slice const& slice) {
  return ::dropMarkerUUID(slice);
}

arangodb::velocypack::StringRef RocksDBLogValue::oldCollectionName(rocksdb::Slice const& slice) {
  size_t off = sizeof(RocksDBLogType) + sizeof(uint64_t) * 2;
  TRI_ASSERT(slice.size() >= off);
  RocksDBLogType type = static_cast<RocksDBLogType>(slice.data()[0]);
  TRI_ASSERT(type == RocksDBLogType::CollectionRename);
  return arangodb::velocypack::StringRef(slice.data() + off, slice.size() - off);
}

/// @brief get slice from tracked document
std::pair<LocalDocumentId, VPackSlice> RocksDBLogValue::trackedDocument(rocksdb::Slice const& slice) {
  TRI_ASSERT(slice.size() >= 2);
  RocksDBLogType type = static_cast<RocksDBLogType>(slice.data()[0]);
  TRI_ASSERT(type == RocksDBLogType::TrackedDocumentInsert ||
             type == RocksDBLogType::TrackedDocumentRemove);
  
  LocalDocumentId id(uintFromPersistentLittleEndian<LocalDocumentId::BaseType>(slice.data() + sizeof(RocksDBLogType)));
  VPackSlice data(reinterpret_cast<uint8_t const*>(slice.data() + sizeof(RocksDBLogType) + sizeof(LocalDocumentId::BaseType)));
  return std::make_pair(id, data);
}

bool RocksDBLogValue::containsDatabaseId(RocksDBLogType type) {
  return type == RocksDBLogType::DatabaseCreate || type == RocksDBLogType::DatabaseDrop ||
         type == RocksDBLogType::CollectionCreate ||
         type == RocksDBLogType::CollectionDrop ||
         type == RocksDBLogType::CollectionRename ||
         type == RocksDBLogType::CollectionChange ||
         type == RocksDBLogType::CollectionTruncate || type == RocksDBLogType::ViewCreate ||
         type == RocksDBLogType::ViewDrop || type == RocksDBLogType::ViewChange ||
         type == RocksDBLogType::IndexCreate || type == RocksDBLogType::IndexDrop ||
         type == RocksDBLogType::BeginTransaction ||
         type == RocksDBLogType::CommitTransaction ||
         type == RocksDBLogType::SinglePut || type == RocksDBLogType::SingleRemoveV2;
}

bool RocksDBLogValue::containsCollectionId(RocksDBLogType type) {
  return type == RocksDBLogType::CollectionCreate ||
         type == RocksDBLogType::CollectionDrop ||
         type == RocksDBLogType::CollectionRename ||
         type == RocksDBLogType::CollectionChange ||
         type == RocksDBLogType::CollectionTruncate ||
         type == RocksDBLogType::IndexCreate || type == RocksDBLogType::IndexDrop ||
         type == RocksDBLogType::SinglePut || type == RocksDBLogType::SingleRemoveV2;
}

bool RocksDBLogValue::containsViewId(RocksDBLogType type) {
  return type == RocksDBLogType::ViewCreate || type == RocksDBLogType::ViewDrop ||
         type == RocksDBLogType::ViewChange;
}
