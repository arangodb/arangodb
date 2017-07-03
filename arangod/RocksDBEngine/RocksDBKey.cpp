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

RocksDBKey RocksDBKey::Database(TRI_voc_tick_t databaseId) {
  return RocksDBKey(RocksDBEntryType::Database, databaseId);
}

RocksDBKey RocksDBKey::Collection(TRI_voc_tick_t databaseId,
                                  TRI_voc_cid_t collectionId) {
  return RocksDBKey(RocksDBEntryType::Collection, databaseId, collectionId);
}

RocksDBKey RocksDBKey::Document(uint64_t collectionId,
                                TRI_voc_rid_t revisionId) {
  return RocksDBKey(RocksDBEntryType::Document, collectionId, revisionId);
}

RocksDBKey RocksDBKey::PrimaryIndexValue(
    uint64_t indexId, arangodb::StringRef const& primaryKey) {
  return RocksDBKey(RocksDBEntryType::PrimaryIndexValue, indexId, primaryKey);
}

RocksDBKey RocksDBKey::PrimaryIndexValue(uint64_t indexId,
                                         char const* primaryKey) {
  return RocksDBKey(RocksDBEntryType::PrimaryIndexValue, indexId,
                    StringRef(primaryKey));
}

RocksDBKey RocksDBKey::EdgeIndexValue(uint64_t indexId,
                                      arangodb::StringRef const& vertexId,
                                      TRI_voc_rid_t revisionId) {
  return RocksDBKey(RocksDBEntryType::EdgeIndexValue, indexId, vertexId,
                    revisionId);
}

RocksDBKey RocksDBKey::VPackIndexValue(uint64_t indexId,
                                       VPackSlice const& indexValues,
                                       TRI_voc_rid_t revisionId) {
  return RocksDBKey(RocksDBEntryType::VPackIndexValue, indexId, indexValues,
                    revisionId);
}

RocksDBKey RocksDBKey::UniqueVPackIndexValue(uint64_t indexId,
                                             VPackSlice const& indexValues) {
  return RocksDBKey(RocksDBEntryType::UniqueVPackIndexValue, indexId,
                    indexValues);
}

RocksDBKey RocksDBKey::FulltextIndexValue(uint64_t indexId,
                                          arangodb::StringRef const& word,
                                          TRI_voc_rid_t revisionId) {
  return RocksDBKey(RocksDBEntryType::FulltextIndexValue, indexId, word,
                    revisionId);
}

RocksDBKey RocksDBKey::GeoIndexValue(uint64_t indexId, int32_t offset,
                                     bool isSlot) {
  uint64_t norm = uint64_t(offset) << 32;
  norm |= isSlot ? 0xFFU : 0;  // encode slot|pot in lowest bit
  return RocksDBKey(RocksDBEntryType::GeoIndexValue, indexId, norm);
}

RocksDBKey RocksDBKey::View(TRI_voc_tick_t databaseId, TRI_voc_cid_t viewId) {
  return RocksDBKey(RocksDBEntryType::View, databaseId, viewId);
}

RocksDBKey RocksDBKey::CounterValue(uint64_t objectId) {
  return RocksDBKey(RocksDBEntryType::CounterValue, objectId);
}

RocksDBKey RocksDBKey::SettingsValue(RocksDBSettingsType st) {
  return RocksDBKey(RocksDBEntryType::SettingsValue, st);
}

RocksDBKey RocksDBKey::ReplicationApplierConfig(TRI_voc_tick_t databaseId) {
  return RocksDBKey(RocksDBEntryType::ReplicationApplierConfig, databaseId);
}

RocksDBKey RocksDBKey::IndexEstimateValue(uint64_t collectionObjectId) {
  return RocksDBKey(RocksDBEntryType::IndexEstimateValue, collectionObjectId);
}

RocksDBKey RocksDBKey::KeyGeneratorValue(uint64_t objectId) {
  return RocksDBKey(RocksDBEntryType::KeyGeneratorValue, objectId);
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

std::string const& RocksDBKey::string() const { return _buffer; }

RocksDBKey::RocksDBKey(RocksDBEntryType type,
                       RocksDBSettingsType st) : _type(type), _buffer() {
  switch (_type) {
    case RocksDBEntryType::SettingsValue: {
      _buffer.push_back(static_cast<char>(_type));
      _buffer.push_back(static_cast<char>(st));
      break;
    }

    default:
      THROW_ARANGO_EXCEPTION(TRI_ERROR_BAD_PARAMETER);
  }
}

RocksDBKey::RocksDBKey(RocksDBEntryType type, uint64_t first)
    : _type(type), _buffer() {
  switch (_type) {
    case RocksDBEntryType::Database:
    case RocksDBEntryType::CounterValue:
    case RocksDBEntryType::IndexEstimateValue:
    case RocksDBEntryType::KeyGeneratorValue:
    case RocksDBEntryType::ReplicationApplierConfig: {
      size_t length = sizeof(char) + sizeof(uint64_t);
      _buffer.reserve(length);
      _buffer.push_back(static_cast<char>(_type));
      uint64ToPersistent(_buffer, first);  // databaseId
      break;
    }

    default:
      THROW_ARANGO_EXCEPTION(TRI_ERROR_BAD_PARAMETER);
  }
}

RocksDBKey::RocksDBKey(RocksDBEntryType type, uint64_t first,
                       VPackSlice const& slice)
    : _type(type), _buffer() {
  switch (_type) {
    case RocksDBEntryType::UniqueVPackIndexValue: {
      size_t l = sizeof(uint64_t) + static_cast<size_t>(slice.byteSize());
      _buffer.reserve(l);
      uint64ToPersistent(_buffer, first);
      _buffer.append(reinterpret_cast<char const*>(slice.begin()),
                     static_cast<size_t>(slice.byteSize()));
      TRI_ASSERT(_buffer.size() == l);
      break;
    }

    default:
      THROW_ARANGO_EXCEPTION(TRI_ERROR_BAD_PARAMETER);
  }
}

RocksDBKey::RocksDBKey(RocksDBEntryType type, uint64_t first, uint64_t second)
    : _type(type), _buffer() {
  switch (_type) {
    case RocksDBEntryType::Document:
    case RocksDBEntryType::GeoIndexValue: {
      _buffer.reserve(2 * sizeof(uint64_t));
      uint64ToPersistent(_buffer, first);   // objectId
      uint64ToPersistent(_buffer, second);  // revisionId
      break;
    }
      
    case RocksDBEntryType::Collection:
    case RocksDBEntryType::View: {
      _buffer.reserve(sizeof(char) + (2 * sizeof(uint64_t)));
      _buffer.push_back(static_cast<char>(_type));
      uint64ToPersistent(_buffer, first);   // databaseId
      uint64ToPersistent(_buffer, second);  // collectionId
      break;
    }

    default:
      THROW_ARANGO_EXCEPTION(TRI_ERROR_BAD_PARAMETER);
  }
}

RocksDBKey::RocksDBKey(RocksDBEntryType type, uint64_t first,
                       VPackSlice const& second, uint64_t third)
    : _type(type), _buffer() {
  switch (_type) {
    case RocksDBEntryType::VPackIndexValue: {
      // Non-unique VPack index values are stored as follows:
      // - Key: 8-byte object ID of index + VPack array with index value(s)
      // + revisionID
      // - Value: empty
      _buffer.reserve(2 * sizeof(uint64_t) + second.byteSize());
      uint64ToPersistent(_buffer, first);
      _buffer.append(reinterpret_cast<char const*>(second.begin()),
                     static_cast<size_t>(second.byteSize()));
      uint64ToPersistent(_buffer, third);
      break;
    }

    default:
      THROW_ARANGO_EXCEPTION(TRI_ERROR_BAD_PARAMETER);
  }
}

RocksDBKey::RocksDBKey(RocksDBEntryType type, uint64_t first,
                       arangodb::StringRef const& second)
    : _type(type), _buffer() {
  switch (_type) {
    case RocksDBEntryType::PrimaryIndexValue: {
      _buffer.reserve(sizeof(uint64_t) + second.size());
      uint64ToPersistent(_buffer, first);
      _buffer.append(second.data(), second.size());
      break;
    }

    default:
      THROW_ARANGO_EXCEPTION(TRI_ERROR_BAD_PARAMETER);
  }
}

RocksDBKey::RocksDBKey(RocksDBEntryType type, uint64_t first,
                       arangodb::StringRef const& second, uint64_t third)
    : _type(type), _buffer() {
  switch (_type) {
    case RocksDBEntryType::FulltextIndexValue: {
      _buffer.reserve(sizeof(uint64_t) * 2 + second.size() + sizeof(char));
      uint64ToPersistent(_buffer, first);
      _buffer.append(second.data(), second.length());
      _buffer.push_back(_stringSeparator);
      uint64ToPersistent(_buffer, third);
      break;
    }

    case RocksDBEntryType::EdgeIndexValue: {
      _buffer.reserve((sizeof(uint64_t) + sizeof(char)) * 2 + second.size());
      uint64ToPersistent(_buffer, first);
      _buffer.append(second.data(), second.length());
      _buffer.push_back(_stringSeparator);
      uint64ToPersistent(_buffer, third);
      _buffer.push_back(0xFFU);
      break;
    }

    default:
      THROW_ARANGO_EXCEPTION(TRI_ERROR_BAD_PARAMETER);
  }
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
      break;
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
