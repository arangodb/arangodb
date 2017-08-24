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

#include "RocksDBKey.h"
#include "Basics/Exceptions.h"
#include "Logger/Logger.h"
#include "RocksDBEngine/RocksDBCommon.h"
#include "RocksDBEngine/RocksDBTypes.h"

using namespace arangodb;
using namespace arangodb::rocksutils;

const char RocksDBKey::_stringSeparator = '\0';

void RocksDBKey::constructDatabase(TRI_voc_tick_t databaseId) {
  _type = RocksDBEntryType::Database;
  size_t keyLength = sizeof(char) + sizeof(uint64_t);
  _buffer.clear();
  _buffer.reserve(keyLength);
  _buffer.push_back(static_cast<char>(_type));
  uint64ToPersistent(_buffer, databaseId);
  TRI_ASSERT(_buffer.size() == keyLength);
  _slice = rocksdb::Slice(_buffer.data(), keyLength);
}

void RocksDBKey::constructCollection(TRI_voc_tick_t databaseId,
                                    TRI_voc_cid_t collectionId) {
  _type = RocksDBEntryType::Collection;
  size_t keyLength = sizeof(char) + 2 * sizeof(uint64_t);
  _buffer.clear();
  _buffer.reserve(keyLength);
  _buffer.push_back(static_cast<char>(_type));
  uint64ToPersistent(_buffer, databaseId);
  uint64ToPersistent(_buffer, collectionId);
  TRI_ASSERT(_buffer.size() == keyLength);
  _slice = rocksdb::Slice(_buffer.data(), keyLength);
}


void RocksDBKey::constructDocument(uint64_t collectionId,
                                   TRI_voc_rid_t revisionId) {
  _type = RocksDBEntryType::Document;
  size_t keyLength = 2 * sizeof(uint64_t);
  _buffer.clear();
  _buffer.reserve(keyLength);
  uint64ToPersistent(_buffer, collectionId);
  uint64ToPersistent(_buffer, revisionId);
  TRI_ASSERT(_buffer.size() == keyLength);
  _slice = rocksdb::Slice(_buffer.data(), keyLength);
}

void RocksDBKey::constructPrimaryIndexValue(
    uint64_t indexId, arangodb::StringRef const& primaryKey) {
  _type = RocksDBEntryType::PrimaryIndexValue;
  size_t keyLength = sizeof(uint64_t) + primaryKey.size();
  _buffer.clear();
  _buffer.reserve(keyLength);
  uint64ToPersistent(_buffer, indexId);
  _buffer.append(primaryKey.data(), primaryKey.size());
  TRI_ASSERT(_buffer.size() == keyLength);
  _slice = rocksdb::Slice(_buffer.data(), keyLength);
}

void RocksDBKey::constructPrimaryIndexValue(uint64_t indexId,
                                            char const* primaryKey) {
  StringRef const keyRef(primaryKey);
  constructPrimaryIndexValue(indexId, keyRef);
}

void RocksDBKey::constructEdgeIndexValue(uint64_t indexId,
                                         arangodb::StringRef const& vertexId,
                                         TRI_voc_rid_t revisionId) {
  _type = RocksDBEntryType::EdgeIndexValue;
  size_t keyLength = (sizeof(uint64_t) + sizeof(char)) * 2 + vertexId.size();
  _buffer.clear();
  _buffer.reserve(keyLength);
  uint64ToPersistent(_buffer, indexId);
  _buffer.append(vertexId.data(), vertexId.length());
  _buffer.push_back(_stringSeparator);
  uint64ToPersistent(_buffer, revisionId);
  _buffer.push_back(0xFFU);
  TRI_ASSERT(_buffer.size() == keyLength);
  _slice = rocksdb::Slice(_buffer.data(), keyLength);
}

void RocksDBKey::constructVPackIndexValue(uint64_t indexId,
                                          VPackSlice const& indexValues,
                                          TRI_voc_rid_t revisionId) {
  _type = RocksDBEntryType::VPackIndexValue;
  size_t const byteSize = static_cast<size_t>(indexValues.byteSize());
  size_t keyLength = 2 * sizeof(uint64_t) + byteSize;
  _buffer.clear();
  _buffer.reserve(keyLength);
  uint64ToPersistent(_buffer, indexId);
  _buffer.append(reinterpret_cast<char const*>(indexValues.begin()), byteSize);
  uint64ToPersistent(_buffer, revisionId);
  TRI_ASSERT(_buffer.size() == keyLength);
  _slice = rocksdb::Slice(_buffer.data(), keyLength);
}

void RocksDBKey::constructUniqueVPackIndexValue(uint64_t indexId,
                                                VPackSlice const& indexValues) {
  _type = RocksDBEntryType::UniqueVPackIndexValue;
  size_t const byteSize = static_cast<size_t>(indexValues.byteSize());
  size_t keyLength = sizeof(uint64_t) + byteSize;
  _buffer.clear();
  _buffer.reserve(keyLength);
  uint64ToPersistent(_buffer, indexId);
  _buffer.append(reinterpret_cast<char const*>(indexValues.begin()), byteSize);
  TRI_ASSERT(_buffer.size() == keyLength);
  _slice = rocksdb::Slice(_buffer.data(), keyLength);
}

void RocksDBKey::constructFulltextIndexValue(uint64_t indexId,
                                             arangodb::StringRef const& word,
                                             TRI_voc_rid_t revisionId) {
  _type = RocksDBEntryType::FulltextIndexValue;
  size_t keyLength = sizeof(uint64_t) * 2 + word.size() + sizeof(char);
  _buffer.clear();
  _buffer.reserve(keyLength);
  uint64ToPersistent(_buffer, indexId);
  _buffer.append(word.data(), word.length());
  _buffer.push_back(_stringSeparator);
  uint64ToPersistent(_buffer, revisionId);
  TRI_ASSERT(_buffer.size() == keyLength);
  _slice = rocksdb::Slice(_buffer.data(), keyLength);
}

void RocksDBKey::constructGeoIndexValue(uint64_t indexId, int32_t offset,
                                        bool isSlot) {
  uint64_t norm = uint64_t(offset) << 32;
  norm |= isSlot ? 0xFFU : 0;  // encode slot|pot in lowest bit
  _type = RocksDBEntryType::GeoIndexValue;
  size_t keyLength = 2 * sizeof(uint64_t);
  _buffer.clear();
  _buffer.reserve(keyLength);
  uint64ToPersistent(_buffer, indexId);
  uint64ToPersistent(_buffer, norm);
  TRI_ASSERT(_buffer.size() == keyLength);
  _slice = rocksdb::Slice(_buffer.data(), keyLength);
}

void RocksDBKey::constructView(TRI_voc_tick_t databaseId, TRI_voc_cid_t viewId) {
  _type = RocksDBEntryType::View;
  size_t keyLength = sizeof(char) + 2 * sizeof(uint64_t);
  _buffer.clear();
  _buffer.reserve(keyLength);
  _buffer.push_back(static_cast<char>(_type));
  uint64ToPersistent(_buffer, databaseId);
  uint64ToPersistent(_buffer, viewId);
  TRI_ASSERT(_buffer.size() == keyLength);
  _slice = rocksdb::Slice(_buffer.data(), keyLength);
}

void RocksDBKey::constructCounterValue(uint64_t objectId) {
  _type = RocksDBEntryType::CounterValue;
  size_t keyLength = sizeof(char) + sizeof(uint64_t);
  _buffer.clear();
  _buffer.reserve(keyLength);
  _buffer.push_back(static_cast<char>(_type));
  uint64ToPersistent(_buffer, objectId);
  TRI_ASSERT(_buffer.size() == keyLength);
  _slice = rocksdb::Slice(_buffer.data(), keyLength);
}

void RocksDBKey::constructSettingsValue(RocksDBSettingsType st) {
  _type = RocksDBEntryType::SettingsValue;
  size_t keyLength = 2;
  _buffer.clear();
  _buffer.reserve(keyLength);
  _buffer.push_back(static_cast<char>(_type));
  _buffer.push_back(static_cast<char>(st));
  TRI_ASSERT(_buffer.size() == keyLength);
  _slice = rocksdb::Slice(_buffer.data(), keyLength);
}

void RocksDBKey::constructReplicationApplierConfig(TRI_voc_tick_t databaseId) {
  _type = RocksDBEntryType::ReplicationApplierConfig;
  size_t keyLength = sizeof(char) + sizeof(uint64_t);
  _buffer.clear();
  _buffer.reserve(keyLength);
  _buffer.push_back(static_cast<char>(_type));
  uint64ToPersistent(_buffer, databaseId);
  TRI_ASSERT(_buffer.size() == keyLength);
  _slice = rocksdb::Slice(_buffer.data(), keyLength);
}

void RocksDBKey::constructIndexEstimateValue(uint64_t collectionObjectId) {
  _type = RocksDBEntryType::IndexEstimateValue;
  size_t keyLength = sizeof(char) + sizeof(uint64_t);
  _buffer.clear();
  _buffer.reserve(keyLength);
  _buffer.push_back(static_cast<char>(_type));
  uint64ToPersistent(_buffer, collectionObjectId);
  TRI_ASSERT(_buffer.size() == keyLength);
  _slice = rocksdb::Slice(_buffer.data(), keyLength);
}

void RocksDBKey::constructKeyGeneratorValue(uint64_t objectId) {
  _type = RocksDBEntryType::KeyGeneratorValue;
  size_t keyLength = sizeof(char) + sizeof(uint64_t);
  _buffer.clear();
  _buffer.reserve(keyLength);
  _buffer.push_back(static_cast<char>(_type));
  uint64ToPersistent(_buffer, objectId);
  TRI_ASSERT(_buffer.size() == keyLength);
  _slice = rocksdb::Slice(_buffer.data(), keyLength);
}

// ========================= Member methods ===========================

RocksDBEntryType RocksDBKey::type(RocksDBKey const& key) {
  return type(key._buffer.data(), key._buffer.size());
}

uint64_t RocksDBKey::definitionsObjectId(rocksdb::Slice const& s) {
  TRI_ASSERT(s.size() >= (sizeof(char) + sizeof(uint64_t)));
  return uint64FromPersistent(s.data() + sizeof(char));
}

TRI_voc_tick_t RocksDBKey::databaseId(RocksDBKey const& key) {
  return databaseId(key._buffer.data(), key._buffer.size());
}

TRI_voc_tick_t RocksDBKey::databaseId(rocksdb::Slice const& slice) {
  return databaseId(slice.data(), slice.size());
}

TRI_voc_cid_t RocksDBKey::collectionId(RocksDBKey const& key) {
  return collectionId(key._buffer.data(), key._buffer.size());
}

TRI_voc_cid_t RocksDBKey::collectionId(rocksdb::Slice const& slice) {
  return collectionId(slice.data(), slice.size());
}

uint64_t RocksDBKey::objectId(RocksDBKey const& key) {
  return objectId(key._buffer.data(), key._buffer.size());
}

uint64_t RocksDBKey::objectId(rocksdb::Slice const& slice) {
  return objectId(slice.data(), slice.size());
}

TRI_voc_cid_t RocksDBKey::viewId(RocksDBKey const& key) {
  return viewId(key._buffer.data(), key._buffer.size());
}

TRI_voc_cid_t RocksDBKey::viewId(rocksdb::Slice const& slice) {
  return viewId(slice.data(), slice.size());
}

TRI_voc_rid_t RocksDBKey::revisionId(RocksDBKey const& key) {
  return revisionId(key._type, key._buffer.data(), key._buffer.size());
}

TRI_voc_rid_t RocksDBKey::revisionId(RocksDBEntryType type,
                                     rocksdb::Slice const& slice) {
  return revisionId(type, slice.data(), slice.size());
}

arangodb::StringRef RocksDBKey::primaryKey(RocksDBKey const& key) {
  return primaryKey(key._buffer.data(), key._buffer.size());
}

arangodb::StringRef RocksDBKey::primaryKey(rocksdb::Slice const& slice) {
  return primaryKey(slice.data(), slice.size());
}

StringRef RocksDBKey::vertexId(RocksDBKey const& key) {
  return vertexId(key._buffer.data(), key._buffer.size());
}

StringRef RocksDBKey::vertexId(rocksdb::Slice const& slice) {
  return vertexId(slice.data(), slice.size());
}

VPackSlice RocksDBKey::indexedVPack(RocksDBKey const& key) {
  return indexedVPack(key._buffer.data(), key._buffer.size());
}

VPackSlice RocksDBKey::indexedVPack(rocksdb::Slice const& slice) {
  return indexedVPack(slice.data(), slice.size());
}

std::pair<bool, int32_t> RocksDBKey::geoValues(rocksdb::Slice const& slice) {
  TRI_ASSERT(slice.size() == sizeof(uint64_t) * 2);
  uint64_t val = uint64FromPersistent(slice.data() + sizeof(uint64_t));
  bool isSlot = ((val & 0xFFULL) > 0);  // lowest byte is 0xFF if true
  return std::pair<bool, int32_t>(isSlot, static_cast<int32_t>(val >> 32));
}

// ====================== Private Methods ==========================

TRI_voc_tick_t RocksDBKey::databaseId(char const* data, size_t size) {
  TRI_ASSERT(data != nullptr);
  TRI_ASSERT(size >= sizeof(char));
  RocksDBEntryType type = RocksDBKey::type(data, size);
  switch (type) {
    case RocksDBEntryType::Database:
    case RocksDBEntryType::Collection:
    case RocksDBEntryType::View:
    case RocksDBEntryType::ReplicationApplierConfig: {
      TRI_ASSERT(size >= (sizeof(char) + sizeof(uint64_t)));
      return uint64FromPersistent(data + sizeof(char));
    }

    default:
      THROW_ARANGO_EXCEPTION(TRI_ERROR_TYPE_ERROR);
  }
}

TRI_voc_cid_t RocksDBKey::collectionId(char const* data, size_t size) {
  TRI_ASSERT(data != nullptr);
  TRI_ASSERT(size >= sizeof(char));
  RocksDBEntryType type = RocksDBKey::type(data, size);
  switch (type) {
    case RocksDBEntryType::Collection:
    case RocksDBEntryType::View: {
      TRI_ASSERT(size >= (sizeof(char) + (2 * sizeof(uint64_t))));
      return uint64FromPersistent(data + sizeof(char) + sizeof(uint64_t));
    }

    default:
      THROW_ARANGO_EXCEPTION(TRI_ERROR_TYPE_ERROR);
  }
}

TRI_voc_cid_t RocksDBKey::viewId(char const* data, size_t size) {
  TRI_ASSERT(data != nullptr);
  TRI_ASSERT(size >= sizeof(char));
  RocksDBEntryType type = RocksDBKey::type(data, size);
  switch (type) {
    case RocksDBEntryType::View: {
      TRI_ASSERT(size >= (sizeof(char) + (2 * sizeof(uint64_t))));
      return uint64FromPersistent(data + sizeof(char) + sizeof(uint64_t));
    }

    default:
      THROW_ARANGO_EXCEPTION(TRI_ERROR_TYPE_ERROR);
  }
}

TRI_voc_cid_t RocksDBKey::objectId(char const* data, size_t size) {
  TRI_ASSERT(data != nullptr);
  TRI_ASSERT(size > sizeof(uint64_t));
  return uint64FromPersistent(data);
}

TRI_voc_rid_t RocksDBKey::revisionId(RocksDBEntryType type, char const* data,
                                     size_t size) {
  TRI_ASSERT(data != nullptr);
  TRI_ASSERT(size >= sizeof(char));
  switch (type) {
    case RocksDBEntryType::Document:
    case RocksDBEntryType::VPackIndexValue:
    case RocksDBEntryType::FulltextIndexValue: {
      TRI_ASSERT(size >= (2 * sizeof(uint64_t)));
      // last 8 bytes should be the revision
      return uint64FromPersistent(data + size - sizeof(uint64_t));
    }
    case RocksDBEntryType::EdgeIndexValue: {
      TRI_ASSERT(size >= (sizeof(char) * 3 + 2 * sizeof(uint64_t)));
      // 1 byte prefix + 8 byte objectID + _from/_to + 1 byte \0
      // + 8 byte revision ID + 1-byte 0xff
      return uint64FromPersistent(data + size - sizeof(uint64_t) -
                                  sizeof(char));
    }

    default:
      THROW_ARANGO_EXCEPTION(TRI_ERROR_TYPE_ERROR);
  }
}

arangodb::StringRef RocksDBKey::primaryKey(char const* data, size_t size) {
  TRI_ASSERT(data != nullptr);
  TRI_ASSERT(size > sizeof(uint64_t));
  return StringRef(data + sizeof(uint64_t), size - sizeof(uint64_t));
}

StringRef RocksDBKey::vertexId(char const* data, size_t size) {
  TRI_ASSERT(data != nullptr);
  TRI_ASSERT(size > sizeof(uint64_t) * 2);
  // 8 byte objectID + _from/_to + 1 byte \0 +
  // 8 byte revision ID + 1-byte 0xff
  size_t keySize = size - (sizeof(char) + sizeof(uint64_t)) * 2;
  return StringRef(data + sizeof(uint64_t), keySize);
}

VPackSlice RocksDBKey::indexedVPack(char const* data, size_t size) {
  TRI_ASSERT(data != nullptr);
  TRI_ASSERT(size > sizeof(uint64_t));
  return VPackSlice(data + sizeof(uint64_t));
}

