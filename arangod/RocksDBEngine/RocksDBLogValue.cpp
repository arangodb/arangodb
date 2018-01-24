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

using namespace arangodb;
using namespace arangodb::rocksutils;

RocksDBLogValue RocksDBLogValue::DatabaseCreate(TRI_voc_tick_t id) {
  return RocksDBLogValue(RocksDBLogType::DatabaseCreate, id);
}

RocksDBLogValue RocksDBLogValue::DatabaseDrop(TRI_voc_tick_t id) {
  return RocksDBLogValue(RocksDBLogType::DatabaseDrop, id);
}

RocksDBLogValue RocksDBLogValue::CollectionCreate(TRI_voc_tick_t dbid,
                                                  TRI_voc_cid_t cid) {
  return RocksDBLogValue(RocksDBLogType::CollectionCreate, dbid, cid);
}

RocksDBLogValue RocksDBLogValue::CollectionDrop(TRI_voc_tick_t dbid,
                                                TRI_voc_cid_t cid,
                                                StringRef const& uuid) {
  return RocksDBLogValue(RocksDBLogType::CollectionDrop, dbid, cid, uuid);
}

RocksDBLogValue RocksDBLogValue::CollectionRename(TRI_voc_tick_t dbid,
                                                  TRI_voc_cid_t cid,
                                                  StringRef const& oldName) {
  return RocksDBLogValue(RocksDBLogType::CollectionRename, dbid, cid, oldName);
}

RocksDBLogValue RocksDBLogValue::CollectionChange(TRI_voc_tick_t dbid,
                                                  TRI_voc_cid_t cid) {
  return RocksDBLogValue(RocksDBLogType::CollectionChange, dbid, cid);
}

RocksDBLogValue RocksDBLogValue::IndexCreate(TRI_voc_tick_t dbid,
                                             TRI_voc_cid_t cid,
                                             VPackSlice const& indexInfo) {
  return RocksDBLogValue(RocksDBLogType::IndexCreate, dbid, cid, indexInfo);
}

RocksDBLogValue RocksDBLogValue::IndexDrop(TRI_voc_tick_t dbid,
                                           TRI_voc_cid_t cid,
                                           TRI_idx_iid_t iid) {
  return RocksDBLogValue(RocksDBLogType::IndexDrop, dbid, cid, iid);
}

RocksDBLogValue RocksDBLogValue::ViewCreate(TRI_voc_tick_t dbid,
                                            TRI_voc_cid_t vid) {
  return RocksDBLogValue(RocksDBLogType::ViewCreate, dbid, vid);
}

RocksDBLogValue RocksDBLogValue::ViewDrop(TRI_voc_tick_t dbid,
                                          TRI_voc_cid_t vid,
                                          VPackSlice const& viewInfo) {
  return RocksDBLogValue(RocksDBLogType::ViewDrop, dbid, vid, viewInfo);
}

RocksDBLogValue RocksDBLogValue::ViewChange(TRI_voc_tick_t dbid,
                                            TRI_voc_cid_t vid) {
  return RocksDBLogValue(RocksDBLogType::ViewChange, dbid, vid);
}

RocksDBLogValue RocksDBLogValue::ViewRename(TRI_voc_cid_t cid,
                                            TRI_idx_iid_t iid) {
  return RocksDBLogValue(RocksDBLogType::ViewRename, cid, iid);
}

#ifdef USE_IRESEARCH
RocksDBLogValue RocksDBLogValue::IResearchLinkDrop(TRI_voc_tick_t dbid,
                                                   TRI_voc_cid_t cid,
                                                   TRI_voc_cid_t vid,
                                                   TRI_idx_iid_t iid) {
  return RocksDBLogValue(RocksDBLogType::IResearchLinkDrop, dbid, cid, vid,
                         iid);
}
#endif

RocksDBLogValue RocksDBLogValue::BeginTransaction(TRI_voc_tick_t dbid,
                                                  TRI_voc_tid_t tid) {
  return RocksDBLogValue(RocksDBLogType::BeginTransaction, dbid, tid);
}

RocksDBLogValue RocksDBLogValue::DocumentOpsPrologue(TRI_voc_cid_t cid) {
  return RocksDBLogValue(RocksDBLogType::DocumentOperationsPrologue, cid);
}

RocksDBLogValue RocksDBLogValue::DocumentRemove(
    arangodb::StringRef const& key) {
  return RocksDBLogValue(RocksDBLogType::DocumentRemove, key);
}

RocksDBLogValue RocksDBLogValue::DocumentRemoveAsPartOfUpdate(
    arangodb::StringRef const& key) {
  return RocksDBLogValue(RocksDBLogType::DocumentRemoveAsPartOfUpdate, key);
}

RocksDBLogValue RocksDBLogValue::SinglePut(TRI_voc_tick_t vocbaseId,
                                           TRI_voc_cid_t cid) {
  return RocksDBLogValue(RocksDBLogType::SinglePut, vocbaseId, cid);
}
RocksDBLogValue RocksDBLogValue::SingleRemove(TRI_voc_tick_t vocbaseId,
                                              TRI_voc_cid_t cid,
                                              arangodb::StringRef const& key) {
  return RocksDBLogValue(RocksDBLogType::SingleRemove, vocbaseId, cid, key);
}

RocksDBLogValue::RocksDBLogValue(RocksDBLogType type, uint64_t val)
    : _buffer() {
  switch (type) {
    case RocksDBLogType::DatabaseCreate:
    case RocksDBLogType::DatabaseDrop:
    case RocksDBLogType::DocumentOperationsPrologue: {
      _buffer.reserve(sizeof(RocksDBLogType) + sizeof(uint64_t));
      _buffer.push_back(static_cast<char>(type));
      uint64ToPersistent(_buffer, val);  // database or collection ID
      break;
    }

    default:
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_BAD_PARAMETER,
                                     "invalid type for log value");
  }
}

RocksDBLogValue::RocksDBLogValue(RocksDBLogType type, uint64_t dbId,
                                 uint64_t val2)
    : _buffer() {
  switch (type) {
    case RocksDBLogType::CollectionCreate:
    case RocksDBLogType::CollectionChange:
    case RocksDBLogType::CollectionDrop:
    case RocksDBLogType::ViewCreate:
    case RocksDBLogType::BeginTransaction:
    case RocksDBLogType::SinglePut: {
      _buffer.reserve(sizeof(RocksDBLogType) + sizeof(uint64_t) * 2);
      _buffer.push_back(static_cast<char>(type));
      uint64ToPersistent(_buffer, dbId);
      uint64ToPersistent(_buffer, val2);
      break;
    }

    default:
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_BAD_PARAMETER,
                                     "invalid type for log value");
  }
}

RocksDBLogValue::RocksDBLogValue(RocksDBLogType type, uint64_t dbId,
                                 uint64_t cid, uint64_t iid)
    : _buffer() {
  switch (type) {
    case RocksDBLogType::IndexDrop: {
      _buffer.reserve(sizeof(RocksDBLogType) + sizeof(uint64_t) * 3);
      _buffer.push_back(static_cast<char>(type));
      uint64ToPersistent(_buffer, dbId);
      uint64ToPersistent(_buffer, cid);
      uint64ToPersistent(_buffer, iid);
      break;
    }
    default:
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_BAD_PARAMETER,
                                     "invalid type for log value");
  }
}

#ifdef USE_IRESEARCH
RocksDBLogValue::RocksDBLogValue(RocksDBLogType type, uint64_t dbId,
                                 uint64_t cid, uint64_t vid, uint64_t iid)
    : _buffer() {
  switch (type) {
    case RocksDBLogType::IResearchLinkDrop: {
      _buffer.reserve(sizeof(RocksDBLogType) + sizeof(uint64_t) * 4);
      _buffer.push_back(static_cast<char>(type));
      uint64ToPersistent(_buffer, dbId);
      uint64ToPersistent(_buffer, cid);
      uint64ToPersistent(_buffer, vid);
      uint64ToPersistent(_buffer, iid);
      break;
    }
    default:
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_BAD_PARAMETER,
                                     "invalid type for log value");
  }
}
#endif

RocksDBLogValue::RocksDBLogValue(RocksDBLogType type, uint64_t dbId,
                                 uint64_t cid, VPackSlice const& info)
    : _buffer() {
  switch (type) {
    case RocksDBLogType::IndexCreate:
    case RocksDBLogType::ViewDrop: {
      _buffer.reserve(sizeof(RocksDBLogType) + (sizeof(uint64_t) * 2) +
                      info.byteSize());
      _buffer.push_back(static_cast<char>(type));
      uint64ToPersistent(_buffer, dbId);
      uint64ToPersistent(_buffer, cid);
      _buffer.append(reinterpret_cast<char const*>(info.begin()),
                     info.byteSize());
      break;
    }
    default:
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_BAD_PARAMETER,
                                     "invalid type for log value");
  }
}

RocksDBLogValue::RocksDBLogValue(RocksDBLogType type, uint64_t dbId,
                                 uint64_t cid, StringRef const& data)
    : _buffer() {
  switch (type) {
    case RocksDBLogType::SingleRemove:
    case RocksDBLogType::CollectionDrop:
    case RocksDBLogType::CollectionRename: {
      _buffer.reserve(sizeof(RocksDBLogType) + sizeof(uint64_t) * 2 +
                      data.length());
      _buffer.push_back(static_cast<char>(type));
      uint64ToPersistent(_buffer, dbId);
      uint64ToPersistent(_buffer, cid);
      // append primary key for SingleRemove, or
      // collection name for CollectionRename, or
      // collection uuid for CollectionDrop
      _buffer.append(data.data(), data.length());
      break;
    }
    default:
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_BAD_PARAMETER,
                                     "invalid type for log value");
  }
}

RocksDBLogValue::RocksDBLogValue(RocksDBLogType type, StringRef const& data)
    : _buffer() {
  switch (type) {
    case RocksDBLogType::DocumentRemove: 
    case RocksDBLogType::DocumentRemoveAsPartOfUpdate: {
      _buffer.reserve(data.length() + sizeof(RocksDBLogType));
      _buffer.push_back(static_cast<char>(type));
      _buffer.append(data.data(), data.length());  // primary key
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
  if (type == RocksDBLogType::DocumentOperationsPrologue) {  // only exception
    return uint64FromPersistent(slice.data() + sizeof(RocksDBLogType));
  } else {
    TRI_ASSERT(slice.size() >= sizeof(RocksDBLogType) + sizeof(uint64_t) * 2);
    return uint64FromPersistent(slice.data() + sizeof(RocksDBLogType) +
                                sizeof(uint64_t));
  }
}

TRI_voc_cid_t RocksDBLogValue::viewId(rocksdb::Slice const& slice) {
  TRI_ASSERT(slice.size() >= sizeof(RocksDBLogType) + sizeof(uint64_t));
  RocksDBLogType type = static_cast<RocksDBLogType>(slice.data()[0]);
  TRI_ASSERT(RocksDBLogValue::containsViewId(type));

#ifdef USE_IRESEARCH
  if (type == RocksDBLogType::IResearchLinkDrop) {
    TRI_ASSERT(slice.size() >= sizeof(RocksDBLogType) + sizeof(uint64_t) * 3);
    return uint64FromPersistent(slice.data() + sizeof(RocksDBLogType) +
                                (sizeof(uint64_t) * 2));
  }
#endif

  TRI_ASSERT(slice.size() >= sizeof(RocksDBLogType) + sizeof(uint64_t) * 2);
  return uint64FromPersistent(slice.data() + sizeof(RocksDBLogType) +
                                sizeof(uint64_t));
}

TRI_voc_tid_t RocksDBLogValue::transactionId(rocksdb::Slice const& slice) {
  TRI_ASSERT(slice.size() >= sizeof(RocksDBLogType) + sizeof(uint64_t));
  RocksDBLogType type = static_cast<RocksDBLogType>(slice.data()[0]);
  TRI_ASSERT(type == RocksDBLogType::BeginTransaction);
  // <type> + 8-byte <dbId> + 8-byte <trxId>
  return uint64FromPersistent(slice.data() + sizeof(RocksDBLogType) +
                              sizeof(TRI_voc_tick_t));
}

TRI_idx_iid_t RocksDBLogValue::indexId(rocksdb::Slice const& slice) {
  TRI_ASSERT(slice.size() >= sizeof(RocksDBLogType) + (3 * sizeof(uint64_t)));
  RocksDBLogType type = static_cast<RocksDBLogType>(slice.data()[0]);

#ifdef USE_IRESEARCH
  if (type == RocksDBLogType::IResearchLinkDrop) {
    TRI_ASSERT(slice.size() >= sizeof(RocksDBLogType) + sizeof(uint64_t) * 4);
    return uint64FromPersistent(slice.data() + sizeof(RocksDBLogType) +
                                (sizeof(uint64_t) * 3));
  }
#endif

  TRI_ASSERT(type == RocksDBLogType::IndexDrop);
  return uint64FromPersistent(slice.data() + sizeof(RocksDBLogType) +
                              (2 * sizeof(uint64_t)));
}

VPackSlice RocksDBLogValue::indexSlice(rocksdb::Slice const& slice) {
  TRI_ASSERT(slice.size() >= sizeof(RocksDBLogType) + sizeof(uint64_t) * 2);
  RocksDBLogType type = static_cast<RocksDBLogType>(slice.data()[0]);
  TRI_ASSERT(type == RocksDBLogType::IndexCreate);
  return VPackSlice(slice.data() + sizeof(RocksDBLogType) +
                    sizeof(uint64_t) * 2);
}

VPackSlice RocksDBLogValue::viewSlice(rocksdb::Slice const& slice) {
  TRI_ASSERT(slice.size() >= sizeof(RocksDBLogType) + sizeof(uint64_t) * 2);
  RocksDBLogType type = static_cast<RocksDBLogType>(slice.data()[0]);
  TRI_ASSERT(type == RocksDBLogType::ViewDrop);
  return VPackSlice(slice.data() + sizeof(RocksDBLogType) +
                    sizeof(uint64_t) * 2);
}

StringRef RocksDBLogValue::collectionUUID(
    rocksdb::Slice const& slice) {
  size_t off = sizeof(RocksDBLogType) + sizeof(uint64_t) * 2;
  TRI_ASSERT(slice.size() >= off);
  RocksDBLogType type = static_cast<RocksDBLogType>(slice.data()[0]);
  TRI_ASSERT(type == RocksDBLogType::CollectionDrop);
  if (slice.size() > off) {
    // have a UUID
    return StringRef(slice.data() + off, slice.size() - off);
  }
  // do not have a UUID
  return StringRef();
}

StringRef RocksDBLogValue::oldCollectionName(
    rocksdb::Slice const& slice) {
  size_t off = sizeof(RocksDBLogType) + sizeof(uint64_t) * 2;
  TRI_ASSERT(slice.size() >= off);
  RocksDBLogType type = static_cast<RocksDBLogType>(slice.data()[0]);
  TRI_ASSERT(type == RocksDBLogType::CollectionRename);
  return StringRef(slice.data() + off, slice.size() - off);
}

StringRef RocksDBLogValue::documentKey(rocksdb::Slice const& slice) {
  RocksDBLogType type = static_cast<RocksDBLogType>(slice.data()[0]);
  TRI_ASSERT(type == RocksDBLogType::SingleRemove ||
             type == RocksDBLogType::DocumentRemove ||
             type == RocksDBLogType::DocumentRemoveAsPartOfUpdate);
  size_t off = sizeof(RocksDBLogType);
  // only single remove contains vocbase id and cid
  if (type == RocksDBLogType::SingleRemove) {
    off += sizeof(uint64_t) * 2;
  }
  TRI_ASSERT(slice.size() >= off);
  return StringRef(slice.data() + off, slice.size() - off);
}

bool RocksDBLogValue::containsDatabaseId(RocksDBLogType type) {
  return type == RocksDBLogType::DatabaseCreate ||
  type == RocksDBLogType::DatabaseDrop ||
  type == RocksDBLogType::CollectionCreate ||
  type == RocksDBLogType::CollectionDrop ||
  type == RocksDBLogType::CollectionRename ||
  type == RocksDBLogType::CollectionChange ||
  type == RocksDBLogType::ViewCreate ||
  type == RocksDBLogType::ViewDrop ||
  type == RocksDBLogType::ViewRename ||
  type == RocksDBLogType::ViewChange ||
#ifdef USE_IRESEARCH
  type == RocksDBLogType::IResearchLinkDrop ||
#endif
  type == RocksDBLogType::IndexCreate ||
  type == RocksDBLogType::IndexDrop ||
  type == RocksDBLogType::BeginTransaction ||
  type == RocksDBLogType::SinglePut ||
  type == RocksDBLogType::SingleRemove;
}

bool RocksDBLogValue::containsCollectionId(RocksDBLogType type) {
  return type == RocksDBLogType::CollectionCreate ||
  type == RocksDBLogType::CollectionDrop ||
  type == RocksDBLogType::CollectionRename ||
  type == RocksDBLogType::CollectionChange ||
  #ifdef USE_IRESEARCH
    type == RocksDBLogType::IResearchLinkDrop ||
  #endif
  type == RocksDBLogType::IndexCreate ||
  type == RocksDBLogType::IndexDrop ||
  type == RocksDBLogType::DocumentOperationsPrologue ||
  type == RocksDBLogType::SinglePut ||
  type == RocksDBLogType::SingleRemove;
}

bool RocksDBLogValue::containsViewId(RocksDBLogType type) {
  return type == RocksDBLogType::ViewCreate ||
  type == RocksDBLogType::ViewDrop ||
  type == RocksDBLogType::ViewRename ||
  #ifdef USE_IRESEARCH
    type == RocksDBLogType::IResearchLinkDrop ||
  #endif
  type == RocksDBLogType::ViewChange;
}
