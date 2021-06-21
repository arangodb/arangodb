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
/// @author Dan Larkin-York
////////////////////////////////////////////////////////////////////////////////

#include "RocksDBKey.h"
#include "Basics/Exceptions.h"
#include "Logger/Logger.h"
#include "RocksDBEngine/RocksDBFormat.h"
#include "RocksDBEngine/RocksDBTypes.h"

using namespace arangodb;
using namespace arangodb::rocksutils;

const char RocksDBKey::_stringSeparator = '\0';
  
RocksDBKey::RocksDBKey(std::string* leased)
      : _type(RocksDBEntryType::Document),  // placeholder
        _local(),
        _buffer(leased != nullptr ? leased : &_local) {}

RocksDBKey::RocksDBKey(rocksdb::Slice slice)
      : _type(static_cast<RocksDBEntryType>(slice.data()[0])),
        _local(slice.data(), slice.size()),
        _buffer(&_local) {}
  
RocksDBKey::RocksDBKey(RocksDBKey&& other) noexcept
      : _type(other._type),
        _local(),
        _buffer(&_local) {
  _local.assign(std::move(*(other._buffer)));
  other._buffer = &(other._local);
}
  
/// @brief verify that a key actually contains the given local document id
bool RocksDBKey::containsLocalDocumentId(LocalDocumentId const& documentId) const {
  switch (_type) {
    case RocksDBEntryType::Document:
    case RocksDBEntryType::EdgeIndexValue:
    case RocksDBEntryType::VPackIndexValue:
    case RocksDBEntryType::FulltextIndexValue:
    case RocksDBEntryType::GeoIndexValue: {
      // create a temporary string containing the stringified local document id
      std::string buffer;
      uint64ToPersistent(buffer, documentId.id());
      // and now check if the key actually contains this local document id
      return _buffer->find(buffer) != std::string::npos;
    }

    default: {
      // we should never never get here
      TRI_ASSERT(false);
    }
  }

  return false;
}

void RocksDBKey::constructDatabase(TRI_voc_tick_t databaseId) {
  TRI_ASSERT(databaseId != 0);
  _type = RocksDBEntryType::Database;
  size_t keyLength = sizeof(char) + sizeof(uint64_t);
  _buffer->clear();
  _buffer->reserve(keyLength);
  _buffer->push_back(static_cast<char>(_type));
  uint64ToPersistent(*_buffer, databaseId);
  TRI_ASSERT(_buffer->size() == keyLength);
}

void RocksDBKey::constructCollection(TRI_voc_tick_t databaseId, DataSourceId collectionId) {
  TRI_ASSERT(databaseId != 0 && collectionId.isSet());
  _type = RocksDBEntryType::Collection;
  size_t keyLength = sizeof(char) + 2 * sizeof(uint64_t);
  _buffer->clear();
  _buffer->reserve(keyLength);
  _buffer->push_back(static_cast<char>(_type));
  uint64ToPersistent(*_buffer, databaseId);
  uint64ToPersistent(*_buffer, collectionId.id());
  TRI_ASSERT(_buffer->size() == keyLength);
}

void RocksDBKey::constructDocument(uint64_t objectId, LocalDocumentId documentId) {
  TRI_ASSERT(objectId != 0);
  _type = RocksDBEntryType::Document;
  size_t keyLength = 2 * sizeof(uint64_t);
  _buffer->clear();
  _buffer->reserve(keyLength);
  uint64ToPersistent(*_buffer, objectId);
  uint64ToPersistent(*_buffer, documentId.id());
  TRI_ASSERT(_buffer->size() == keyLength);
}

void RocksDBKey::constructPrimaryIndexValue(uint64_t indexId,
                                            arangodb::velocypack::StringRef const& primaryKey) {
  TRI_ASSERT(indexId != 0 && !primaryKey.empty());
  _type = RocksDBEntryType::PrimaryIndexValue;
  size_t keyLength = sizeof(uint64_t) + primaryKey.size();
  _buffer->clear();
  _buffer->reserve(keyLength);
  uint64ToPersistent(*_buffer, indexId);
  _buffer->append(primaryKey.data(), primaryKey.size());
  TRI_ASSERT(_buffer->size() == keyLength);
}

void RocksDBKey::constructPrimaryIndexValue(uint64_t indexId, char const* primaryKey) {
  TRI_ASSERT(indexId != 0);
  arangodb::velocypack::StringRef const keyRef(primaryKey);
  constructPrimaryIndexValue(indexId, keyRef);
}

void RocksDBKey::constructEdgeIndexValue(uint64_t indexId, arangodb::velocypack::StringRef const& vertexId,
                                         LocalDocumentId documentId) {
  TRI_ASSERT(indexId != 0 && !vertexId.empty());
  _type = RocksDBEntryType::EdgeIndexValue;
  size_t keyLength = (sizeof(uint64_t) + sizeof(char)) * 2 + vertexId.size();
  _buffer->clear();
  _buffer->reserve(keyLength);
  uint64ToPersistent(*_buffer, indexId);
  _buffer->append(vertexId.data(), vertexId.length());
  _buffer->push_back(_stringSeparator);
  uint64ToPersistent(*_buffer, documentId.id());
  _buffer->push_back(0xFFU);  // high-byte for prefix extractor
  TRI_ASSERT(_buffer->size() == keyLength);
}

void RocksDBKey::constructVPackIndexValue(uint64_t indexId, VPackSlice const& indexValues,
                                          LocalDocumentId documentId) {
  TRI_ASSERT(indexId != 0 && !indexValues.isNone());
  _type = RocksDBEntryType::VPackIndexValue;
  size_t const byteSize = static_cast<size_t>(indexValues.byteSize());
  size_t keyLength = 2 * sizeof(uint64_t) + byteSize;
  _buffer->clear();
  _buffer->reserve(keyLength);
  uint64ToPersistent(*_buffer, indexId);
  _buffer->append(reinterpret_cast<char const*>(indexValues.begin()), byteSize);
  uint64ToPersistent(*_buffer, documentId.id());
  TRI_ASSERT(_buffer->size() == keyLength);
}

void RocksDBKey::constructUniqueVPackIndexValue(uint64_t indexId, VPackSlice const& indexValues) {
  TRI_ASSERT(indexId != 0 && !indexValues.isNone());
  _type = RocksDBEntryType::UniqueVPackIndexValue;
  size_t const byteSize = static_cast<size_t>(indexValues.byteSize());
  size_t keyLength = sizeof(uint64_t) + byteSize;
  _buffer->clear();
  _buffer->reserve(keyLength);
  uint64ToPersistent(*_buffer, indexId);
  _buffer->append(reinterpret_cast<char const*>(indexValues.begin()), byteSize);
  TRI_ASSERT(_buffer->size() == keyLength);
}

void RocksDBKey::constructFulltextIndexValue(uint64_t indexId,
                                             arangodb::velocypack::StringRef const& word,
                                             LocalDocumentId documentId) {
  TRI_ASSERT(indexId != 0 && !word.empty());
  _type = RocksDBEntryType::FulltextIndexValue;
  size_t keyLength = sizeof(uint64_t) * 2 + word.size() + sizeof(char);
  _buffer->clear();
  _buffer->reserve(keyLength);
  uint64ToPersistent(*_buffer, indexId);
  _buffer->append(word.data(), word.length());
  _buffer->push_back(_stringSeparator);
  uint64ToPersistent(*_buffer, documentId.id());
  TRI_ASSERT(_buffer->size() == keyLength);
}

//////////////////////////////////////////////////////////////////////////////
/// @brief Create a fully-specified key for an S2CellId
//////////////////////////////////////////////////////////////////////////////
void RocksDBKey::constructGeoIndexValue(uint64_t indexId, uint64_t value,
                                        LocalDocumentId documentId) {
  TRI_ASSERT(indexId != 0);
  _type = RocksDBEntryType::GeoIndexValue;
  size_t keyLength = 3 * sizeof(uint64_t);
  _buffer->clear();
  _buffer->reserve(keyLength);
  uint64ToPersistent(*_buffer, indexId);
  uintToPersistentBigEndian<uint64_t>(*_buffer, value); // always big endian
  uint64ToPersistent(*_buffer, documentId.id());
  TRI_ASSERT(_buffer->size() == keyLength);
}

void RocksDBKey::constructView(TRI_voc_tick_t databaseId, DataSourceId viewId) {
  TRI_ASSERT(databaseId != 0 && viewId.isSet());
  _type = RocksDBEntryType::View;
  size_t keyLength = sizeof(char) + 2 * sizeof(uint64_t);
  _buffer->clear();
  _buffer->reserve(keyLength);
  _buffer->push_back(static_cast<char>(_type));
  uint64ToPersistent(*_buffer, databaseId);
  uint64ToPersistent(*_buffer, viewId.id());
  TRI_ASSERT(_buffer->size() == keyLength);
}

void RocksDBKey::constructCounterValue(uint64_t objectId) {
  TRI_ASSERT(objectId != 0);
  _type = RocksDBEntryType::CounterValue;
  size_t keyLength = sizeof(char) + sizeof(uint64_t);
  _buffer->clear();
  _buffer->reserve(keyLength);
  _buffer->push_back(static_cast<char>(_type));
  uint64ToPersistent(*_buffer, objectId);
  TRI_ASSERT(_buffer->size() == keyLength);
}

void RocksDBKey::constructSettingsValue(RocksDBSettingsType st) {
  TRI_ASSERT(st != RocksDBSettingsType::Invalid);
  _type = RocksDBEntryType::SettingsValue;
  size_t keyLength = 2;
  _buffer->clear();
  _buffer->reserve(keyLength);
  _buffer->push_back(static_cast<char>(_type));
  _buffer->push_back(static_cast<char>(st));
  TRI_ASSERT(_buffer->size() == keyLength);
}

void RocksDBKey::constructReplicationApplierConfig(TRI_voc_tick_t databaseId) {
  // databaseId may be 0 for global applier config
  _type = RocksDBEntryType::ReplicationApplierConfig;
  size_t keyLength = sizeof(char) + sizeof(uint64_t);
  _buffer->clear();
  _buffer->reserve(keyLength);
  _buffer->push_back(static_cast<char>(_type));
  uint64ToPersistent(*_buffer, databaseId);
  TRI_ASSERT(_buffer->size() == keyLength);
}

void RocksDBKey::constructIndexEstimateValue(uint64_t collectionObjectId) {
  TRI_ASSERT(collectionObjectId != 0);
  _type = RocksDBEntryType::IndexEstimateValue;
  size_t keyLength = sizeof(char) + sizeof(uint64_t);
  _buffer->clear();
  _buffer->reserve(keyLength);
  _buffer->push_back(static_cast<char>(_type));
  uint64ToPersistent(*_buffer, collectionObjectId);
  TRI_ASSERT(_buffer->size() == keyLength);
}

void RocksDBKey::constructKeyGeneratorValue(uint64_t objectId) {
  TRI_ASSERT(objectId != 0);
  _type = RocksDBEntryType::KeyGeneratorValue;
  size_t keyLength = sizeof(char) + sizeof(uint64_t);
  _buffer->clear();
  _buffer->reserve(keyLength);
  _buffer->push_back(static_cast<char>(_type));
  uint64ToPersistent(*_buffer, objectId);
  TRI_ASSERT(_buffer->size() == keyLength);
}

void RocksDBKey::constructRevisionTreeValue(uint64_t collectionObjectId) {
  TRI_ASSERT(collectionObjectId != 0);
  _type = RocksDBEntryType::RevisionTreeValue;
  size_t keyLength = sizeof(char) + sizeof(uint64_t);
  _buffer->clear();
  _buffer->reserve(keyLength);
  _buffer->push_back(static_cast<char>(_type));
  uint64ToPersistent(*_buffer, collectionObjectId);
  TRI_ASSERT(_buffer->size() == keyLength);
}

// ========================= Member methods ===========================

RocksDBEntryType RocksDBKey::type(RocksDBKey const& key) {
  return type(key._buffer->data(), key._buffer->size());
}

uint64_t RocksDBKey::definitionsObjectId(rocksdb::Slice const& s) {
  TRI_ASSERT(s.size() >= (sizeof(char) + sizeof(uint64_t)));
  return uint64FromPersistent(s.data() + sizeof(char));
}

TRI_voc_tick_t RocksDBKey::databaseId(RocksDBKey const& key) {
  return databaseId(key._buffer->data(), key._buffer->size());
}

TRI_voc_tick_t RocksDBKey::databaseId(rocksdb::Slice const& slice) {
  return databaseId(slice.data(), slice.size());
}

DataSourceId RocksDBKey::collectionId(RocksDBKey const& key) {
  return collectionId(key._buffer->data(), key._buffer->size());
}

DataSourceId RocksDBKey::collectionId(rocksdb::Slice const& slice) {
  return collectionId(slice.data(), slice.size());
}

uint64_t RocksDBKey::objectId(RocksDBKey const& key) {
  return objectId(key._buffer->data(), key._buffer->size());
}

uint64_t RocksDBKey::objectId(rocksdb::Slice const& slice) {
  return objectId(slice.data(), slice.size());
}

DataSourceId RocksDBKey::viewId(RocksDBKey const& key) {
  return viewId(key._buffer->data(), key._buffer->size());
}

DataSourceId RocksDBKey::viewId(rocksdb::Slice const& slice) {
  return viewId(slice.data(), slice.size());
}

LocalDocumentId RocksDBKey::documentId(rocksdb::Slice const& slice) {
  TRI_ASSERT(slice.size() == 2 * sizeof(uint64_t));
  // last 8 bytes should be the LocalDocumentId
  return LocalDocumentId(uint64FromPersistent(slice.data() + sizeof(uint64_t)));
}

LocalDocumentId RocksDBKey::indexDocumentId(rocksdb::Slice const slice) {
  char const* data = slice.data();
  size_t const size = slice.size();
  TRI_ASSERT(size >= (2 * sizeof(uint64_t)));
  // last 8 bytes should be the LocalDocumentId
  return LocalDocumentId(uint64FromPersistent(data + size - sizeof(uint64_t)));
}

LocalDocumentId RocksDBKey::edgeDocumentId(rocksdb::Slice const slice) {
  char const* data = slice.data();
  size_t const size = slice.size();
  TRI_ASSERT(size >= (sizeof(char) * 3 + 2 * sizeof(uint64_t)));
  // 1 byte prefix + 8 byte objectID + _from/_to + 1 byte \0
  // + 8 byte revision ID + 1-byte 0xff
  return LocalDocumentId(uint64FromPersistent(data + size - sizeof(uint64_t) - sizeof(char)));
}

arangodb::velocypack::StringRef RocksDBKey::primaryKey(RocksDBKey const& key) {
  return primaryKey(key._buffer->data(), key._buffer->size());
}

arangodb::velocypack::StringRef RocksDBKey::primaryKey(rocksdb::Slice const& slice) {
  return primaryKey(slice.data(), slice.size());
}

arangodb::velocypack::StringRef RocksDBKey::vertexId(RocksDBKey const& key) {
  return vertexId(key._buffer->data(), key._buffer->size());
}

arangodb::velocypack::StringRef RocksDBKey::vertexId(rocksdb::Slice const& slice) {
  return vertexId(slice.data(), slice.size());
}

VPackSlice RocksDBKey::indexedVPack(RocksDBKey const& key) {
  return indexedVPack(key._buffer->data(), key._buffer->size());
}

VPackSlice RocksDBKey::indexedVPack(rocksdb::Slice const& slice) {
  return indexedVPack(slice.data(), slice.size());
}

uint64_t RocksDBKey::geoValue(rocksdb::Slice const& slice) {
  TRI_ASSERT(slice.size() == sizeof(uint64_t) * 3);
  return uintFromPersistentBigEndian<uint64_t>(slice.data() + sizeof(uint64_t));
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

DataSourceId RocksDBKey::collectionId(char const* data, size_t size) {
  TRI_ASSERT(data != nullptr);
  TRI_ASSERT(size >= sizeof(char));
  RocksDBEntryType type = RocksDBKey::type(data, size);
  switch (type) {
    case RocksDBEntryType::Collection:
    case RocksDBEntryType::View: {
      TRI_ASSERT(size >= (sizeof(char) + (2 * sizeof(uint64_t))));
      return DataSourceId{uint64FromPersistent(data + sizeof(char) + sizeof(uint64_t))};
    }

    default:
      THROW_ARANGO_EXCEPTION(TRI_ERROR_TYPE_ERROR);
  }
}

DataSourceId RocksDBKey::viewId(char const* data, size_t size) {
  TRI_ASSERT(data != nullptr);
  TRI_ASSERT(size >= sizeof(char));
  RocksDBEntryType type = RocksDBKey::type(data, size);
  switch (type) {
    case RocksDBEntryType::View: {
      TRI_ASSERT(size >= (sizeof(char) + (2 * sizeof(uint64_t))));
      return DataSourceId{uint64FromPersistent(data + sizeof(char) + sizeof(uint64_t))};
    }

    default:
      THROW_ARANGO_EXCEPTION(TRI_ERROR_TYPE_ERROR);
  }
}

uint64_t RocksDBKey::objectId(char const* data, size_t size) {
  TRI_ASSERT(data != nullptr);
  TRI_ASSERT(size >= sizeof(uint64_t));
  return uint64FromPersistent(data);
}

arangodb::velocypack::StringRef RocksDBKey::primaryKey(char const* data, size_t size) {
  TRI_ASSERT(data != nullptr);
  TRI_ASSERT(size > sizeof(uint64_t));
  return arangodb::velocypack::StringRef(data + sizeof(uint64_t), size - sizeof(uint64_t));
}

arangodb::velocypack::StringRef RocksDBKey::vertexId(char const* data, size_t size) {
  TRI_ASSERT(data != nullptr);
  TRI_ASSERT(size > sizeof(uint64_t) * 2);
  // 8 byte objectID + _from/_to + 1 byte \0 +
  // 8 byte revision ID + 1-byte 0xff
  size_t keySize = size - (sizeof(char) + sizeof(uint64_t)) * 2;
  return arangodb::velocypack::StringRef(data + sizeof(uint64_t), keySize);
}

VPackSlice RocksDBKey::indexedVPack(char const* data, size_t size) {
  TRI_ASSERT(data != nullptr);
  TRI_ASSERT(size > sizeof(uint64_t));
  return VPackSlice(reinterpret_cast<uint8_t const*>(data) + sizeof(uint64_t));
}

namespace arangodb {

std::ostream& operator<<(std::ostream& stream, RocksDBKey const& key) {
  stream << "[key type: " << arangodb::rocksDBEntryTypeName(RocksDBKey::type(key)) << " ";

  auto dump = [&stream](rocksdb::Slice const& slice) {
    size_t const n = slice.size();

    for (size_t i = 0; i < n; ++i) {
      stream << "0x";

      uint8_t const value = static_cast<uint8_t>(slice[i]);
      uint8_t x = value / 16;
      stream << static_cast<char>((x < 10 ? ('0' + x) : ('a' + x - 10)));
      x = value % 16;
      stream << static_cast<char>(x < 10 ? ('0' + x) : ('a' + x - 10));

      if (i + 1 != n) {
        stream << " ";
      }
    }
  };

  dump(key.string());
  stream << "]";

  return stream;
}
}  // namespace arangodb
