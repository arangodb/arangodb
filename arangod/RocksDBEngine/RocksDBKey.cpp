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

#include "RocksDBEngine/RocksDBKey.h"
#include "Basics/Exceptions.h"
#include "RocksDBEngine/RocksDBCommon.h"
#include "RocksDBEngine/RocksDBTypes.h"

using namespace arangodb;
using namespace arangodb::rocksutils;
using namespace arangodb::velocypack;

const char RocksDBKey::_stringSeparator = '\0';

RocksDBKey RocksDBKey::Database(TRI_voc_tick_t databaseId) {
  return RocksDBKey(RocksDBEntryType::Database, databaseId);
}

RocksDBKey RocksDBKey::Collection(TRI_voc_tick_t databaseId,
                                  TRI_voc_cid_t collectionId) {
  return RocksDBKey(RocksDBEntryType::Collection, databaseId, collectionId);
}

RocksDBKey RocksDBKey::Index(TRI_voc_tick_t databaseId,
                             TRI_voc_cid_t collectionId,
                             TRI_idx_iid_t indexId) {
  return RocksDBKey(RocksDBEntryType::Index, databaseId, collectionId, indexId);
}

RocksDBKey RocksDBKey::Document(uint64_t collectionId,
                                TRI_voc_rid_t revisionId) {
  return RocksDBKey(RocksDBEntryType::Document, collectionId, revisionId);
}

RocksDBKey RocksDBKey::PrimaryIndexValue(uint64_t indexId,
                                         std::string const& primaryKey) {
  return RocksDBKey(RocksDBEntryType::PrimaryIndexValue, indexId, primaryKey);
}

RocksDBKey RocksDBKey::EdgeIndexValue(uint64_t indexId,
                                      std::string const& vertexId,
                                      std::string const& primaryKey) {
  return RocksDBKey(RocksDBEntryType::EdgeIndexValue, indexId, vertexId,
                    primaryKey);
}

RocksDBKey RocksDBKey::IndexValue(uint64_t indexId,
                                  std::string const& primaryKey,
                                  VPackSlice const& indexValues) {
  return RocksDBKey(RocksDBEntryType::IndexValue, indexId, primaryKey,
                    indexValues);
}

RocksDBKey RocksDBKey::UniqueIndexValue(uint64_t indexId,
                                        VPackSlice const& indexValues) {
  return RocksDBKey(RocksDBEntryType::UniqueIndexValue, indexId, indexValues);
}

RocksDBKey RocksDBKey::View(TRI_voc_tick_t databaseId, TRI_voc_cid_t viewId) {
  return RocksDBKey(RocksDBEntryType::View, databaseId, viewId);
}

RocksDBEntryType RocksDBKey::type(RocksDBKey const& key) {
  return type(key._buffer.data(), key._buffer.size());
}

RocksDBEntryType RocksDBKey::type(rocksdb::Slice const& slice) {
  return type(slice.data(), slice.size());
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

TRI_idx_iid_t RocksDBKey::indexId(RocksDBKey const& key) {
  return indexId(key._buffer.data(), key._buffer.size());
}

TRI_idx_iid_t RocksDBKey::indexId(rocksdb::Slice const& slice) {
  return indexId(slice.data(), slice.size());
}

TRI_voc_cid_t RocksDBKey::viewId(RocksDBKey const& key) {
  return viewId(key._buffer.data(), key._buffer.size());
}

TRI_voc_cid_t RocksDBKey::viewId(rocksdb::Slice const& slice) {
  return viewId(slice.data(), slice.size());
}

TRI_voc_rid_t RocksDBKey::revisionId(RocksDBKey const& key) {
  return revisionId(key._buffer.data(), key._buffer.size());
}

TRI_voc_rid_t RocksDBKey::revisionId(rocksdb::Slice const& slice) {
  return revisionId(slice.data(), slice.size());
}

std::string RocksDBKey::primaryKey(RocksDBKey const& key) {
  return primaryKey(key._buffer.data(), key._buffer.size());
}

std::string RocksDBKey::primaryKey(rocksdb::Slice const& slice) {
  return primaryKey(slice.data(), slice.size());
}

std::string RocksDBKey::vertexId(RocksDBKey const& key) {
  return vertexId(key._buffer.data(), key._buffer.size());
}

std::string RocksDBKey::vertexId(rocksdb::Slice const& slice) {
  return vertexId(slice.data(), slice.size());
}

VPackSlice RocksDBKey::indexedVPack(RocksDBKey const& key) {
  return indexedVPack(key._buffer.data(), key._buffer.size());
}

VPackSlice RocksDBKey::indexedVPack(rocksdb::Slice const& slice) {
  return indexedVPack(slice.data(), slice.size());
}

std::string const& RocksDBKey::key() const { return _buffer; }

bool RocksDBKey::isSameDatabase(RocksDBEntryType type, TRI_voc_tick_t id,
                                rocksdb::Slice const& slice) {
  switch (type) {
    case RocksDBEntryType::Collection:
    case RocksDBEntryType::View: {
      TRI_ASSERT(slice.size() ==
                 sizeof(char) + sizeof(uint64_t) + sizeof(uint64_t));
      return id == uint64FromPersistent(slice.data() + sizeof(char));
    }

    default:
      THROW_ARANGO_EXCEPTION(TRI_ERROR_BAD_PARAMETER);
  }
}

RocksDBKey::RocksDBKey(RocksDBEntryType type, uint64_t first)
    : _type(type), _buffer() {
  switch (_type) {
    case RocksDBEntryType::Database: {
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
    case RocksDBEntryType::UniqueIndexValue: {
      size_t length = sizeof(char) + sizeof(uint64_t) +
                      static_cast<size_t>(slice.byteSize());
      _buffer.reserve(length);
      _buffer.push_back(static_cast<char>(_type));
      uint64ToPersistent(_buffer, first);
      _buffer.append(reinterpret_cast<char const*>(slice.begin()),
                     static_cast<size_t>(slice.byteSize()));
      break;
    }

    default:
      THROW_ARANGO_EXCEPTION(TRI_ERROR_BAD_PARAMETER);
  }
}

RocksDBKey::RocksDBKey(RocksDBEntryType type, uint64_t first, uint64_t second)
    : _type(type), _buffer() {
  switch (_type) {
    case RocksDBEntryType::Collection:
    case RocksDBEntryType::Document:
    case RocksDBEntryType::View: {
      size_t length = sizeof(char) + (2 * sizeof(uint64_t));
      _buffer.reserve(length);
      _buffer.push_back(static_cast<char>(_type));
      uint64ToPersistent(_buffer,
                         first);  // databaseId / collectionId / databaseId
      uint64ToPersistent(_buffer,
                         second);  // collectionId / collectionId / viewId
      break;
    }

    default:
      THROW_ARANGO_EXCEPTION(TRI_ERROR_BAD_PARAMETER);
  }
}

RocksDBKey::RocksDBKey(RocksDBEntryType type, uint64_t first, uint64_t second,
                       uint64_t third)
    : _type(type), _buffer() {
  switch (_type) {
    case RocksDBEntryType::Index: {
      size_t length =
          sizeof(char) + sizeof(uint64_t) + sizeof(uint64_t) + sizeof(uint64_t);
      _buffer.reserve(length);
      _buffer.push_back(static_cast<char>(_type));
      uint64ToPersistent(_buffer, first);   // databaseId
      uint64ToPersistent(_buffer, second);  // collectionId
      uint64ToPersistent(_buffer, third);   // indexId
      break;
    }

    default:
      THROW_ARANGO_EXCEPTION(TRI_ERROR_BAD_PARAMETER);
  }
}

RocksDBKey::RocksDBKey(RocksDBEntryType type, uint64_t first,
                       std::string const& second, VPackSlice const& slice)
    : _type(type), _buffer() {
  switch (_type) {
    case RocksDBEntryType::IndexValue: {
      size_t length = sizeof(char) + sizeof(uint64_t) +
                      static_cast<size_t>(slice.byteSize()) + second.size();
      _buffer.reserve(length);
      _buffer.push_back(static_cast<char>(_type));
      uint64ToPersistent(_buffer, first);
      _buffer.append(reinterpret_cast<char const*>(slice.begin()),
                     static_cast<size_t>(slice.byteSize()));
      _buffer.append(second);
      break;
    }

    default:
      THROW_ARANGO_EXCEPTION(TRI_ERROR_BAD_PARAMETER);
  }
}

RocksDBKey::RocksDBKey(RocksDBEntryType type, uint64_t first,
                       std::string const& second)
    : _type(type), _buffer() {
  switch (_type) {
    case RocksDBEntryType::PrimaryIndexValue: {
      size_t length = sizeof(char) + sizeof(uint64_t) + second.size();
      _buffer.reserve(length);
      _buffer.push_back(static_cast<char>(_type));
      uint64ToPersistent(_buffer, first);
      _buffer.append(second);
      break;
    }

    default:
      THROW_ARANGO_EXCEPTION(TRI_ERROR_BAD_PARAMETER);
  }
}

RocksDBKey::RocksDBKey(RocksDBEntryType type, uint64_t first,
                       std::string const& second, std::string const& third)
    : _type(type), _buffer() {
  switch (_type) {
    case RocksDBEntryType::PrimaryIndexValue: {
      size_t length = sizeof(char) + sizeof(uint64_t) + second.size() +
                      sizeof(char) + third.size() + sizeof(uint8_t);
      _buffer.reserve(length);
      _buffer.push_back(static_cast<char>(_type));
      uint64ToPersistent(_buffer, first);
      _buffer.append(second);
      _buffer.push_back(_stringSeparator);
      _buffer.append(third);
      TRI_ASSERT(third.size() <= 254);
      _buffer.push_back(static_cast<char>(third.size() & 0xff));
      break;
    }

    default:
      THROW_ARANGO_EXCEPTION(TRI_ERROR_BAD_PARAMETER);
  }
}

RocksDBEntryType RocksDBKey::type(char const* data, size_t size) {
  TRI_ASSERT(data != nullptr);
  TRI_ASSERT(size >= sizeof(char));
  return static_cast<RocksDBEntryType>(data[0]);
}

TRI_voc_tick_t RocksDBKey::databaseId(char const* data, size_t size) {
  TRI_ASSERT(data != nullptr);
  TRI_ASSERT(size >= sizeof(char));
  RocksDBEntryType type = static_cast<RocksDBEntryType>(data[0]);
  switch (type) {
    case RocksDBEntryType::Database:
    case RocksDBEntryType::Collection:
    case RocksDBEntryType::Index:
    case RocksDBEntryType::View: {
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
  RocksDBEntryType type = static_cast<RocksDBEntryType>(data[0]);
  switch (type) {
    case RocksDBEntryType::Collection:
    case RocksDBEntryType::Index: {
      TRI_ASSERT(size >= (sizeof(char) + (2 * sizeof(uint64_t))));
      return uint64FromPersistent(data + sizeof(char) + sizeof(uint64_t));
    }

    default:
      THROW_ARANGO_EXCEPTION(TRI_ERROR_TYPE_ERROR);
  }
}

TRI_idx_iid_t RocksDBKey::indexId(char const* data, size_t size) {
  TRI_ASSERT(data != nullptr);
  TRI_ASSERT(size >= sizeof(char));
  RocksDBEntryType type = static_cast<RocksDBEntryType>(data[0]);
  switch (type) {
    case RocksDBEntryType::Index: {
      TRI_ASSERT(size >= (sizeof(char) + (3 * sizeof(uint64_t))));
      return uint64FromPersistent(data + sizeof(char) + (2 * sizeof(uint64_t)));
    }

    default:
      THROW_ARANGO_EXCEPTION(TRI_ERROR_TYPE_ERROR);
  }
}

TRI_voc_cid_t RocksDBKey::viewId(char const* data, size_t size) {
  TRI_ASSERT(data != nullptr);
  TRI_ASSERT(size >= sizeof(char));
  RocksDBEntryType type = static_cast<RocksDBEntryType>(data[0]);
  switch (type) {
    case RocksDBEntryType::View: {
      TRI_ASSERT(size >= (sizeof(char) + (2 * sizeof(uint64_t))));
      return uint64FromPersistent(data + sizeof(char) + sizeof(uint64_t));
    }

    default:
      THROW_ARANGO_EXCEPTION(TRI_ERROR_TYPE_ERROR);
  }
}

TRI_voc_rid_t RocksDBKey::revisionId(char const* data, size_t size) {
  TRI_ASSERT(data != nullptr);
  TRI_ASSERT(size >= sizeof(char));
  RocksDBEntryType type = static_cast<RocksDBEntryType>(data[0]);
  switch (type) {
    case RocksDBEntryType::Document: {
      TRI_ASSERT(size >= (sizeof(char) + (2 * sizeof(uint64_t))));
      return uint64FromPersistent(data + sizeof(char) + sizeof(uint64_t));
    }

    default:
      THROW_ARANGO_EXCEPTION(TRI_ERROR_TYPE_ERROR);
  }
}

std::string RocksDBKey::primaryKey(char const* data, size_t size) {
  TRI_ASSERT(data != nullptr);
  TRI_ASSERT(size >= sizeof(char));
  RocksDBEntryType type = static_cast<RocksDBEntryType>(data[0]);
  switch (type) {
    case RocksDBEntryType::PrimaryIndexValue: {
      TRI_ASSERT(size > (sizeof(char) + sizeof(uint64_t) + sizeof(uint8_t)));
      size_t keySize = size - (sizeof(char) + sizeof(uint64_t));
      return std::string(data + sizeof(char) + sizeof(uint64_t), keySize);
    }
    case RocksDBEntryType::EdgeIndexValue: {
      TRI_ASSERT(size > (sizeof(char) + sizeof(uint64_t) + sizeof(uint8_t)));
      size_t keySize = static_cast<size_t>(data[size - 1]);
      return std::string(data + (size - (keySize + sizeof(uint8_t))), keySize);
    }
    case RocksDBEntryType::IndexValue: {
      TRI_ASSERT(size > (sizeof(char) + sizeof(uint64_t)));
      VPackSlice slice(data + sizeof(char) + sizeof(uint64_t));
      size_t sliceSize = static_cast<size_t>(slice.byteSize());
      size_t keySize = size - (sizeof(char) + sizeof(uint64_t) + sliceSize);
      return std::string(data + (size - keySize), keySize);
    }

    default:
      THROW_ARANGO_EXCEPTION(TRI_ERROR_TYPE_ERROR);
  }
}

std::string RocksDBKey::vertexId(char const* data, size_t size) {
  TRI_ASSERT(data != nullptr);
  TRI_ASSERT(size >= sizeof(char));
  RocksDBEntryType type = static_cast<RocksDBEntryType>(data[0]);
  switch (type) {
    case RocksDBEntryType::EdgeIndexValue: {
      TRI_ASSERT(size > (sizeof(char) + sizeof(uint64_t) + sizeof(uint8_t)));
      size_t keySize = static_cast<size_t>(data[size - 1]);
      size_t idSize = size - (sizeof(char) + sizeof(uint64_t) + sizeof(char) +
                              keySize + sizeof(uint8_t));
      return std::string(data + sizeof(char) + sizeof(uint64_t), idSize);
    }

    default:
      THROW_ARANGO_EXCEPTION(TRI_ERROR_TYPE_ERROR);
  }
}

VPackSlice RocksDBKey::indexedVPack(char const* data, size_t size) {
  TRI_ASSERT(data != nullptr);
  TRI_ASSERT(size >= sizeof(char));
  RocksDBEntryType type = static_cast<RocksDBEntryType>(data[0]);
  switch (type) {
    case RocksDBEntryType::IndexValue:
    case RocksDBEntryType::UniqueIndexValue: {
      TRI_ASSERT(size > (sizeof(char) + sizeof(uint64_t)));
      return VPackSlice(data + sizeof(char) + sizeof(uint64_t));
    }

    default:
      THROW_ARANGO_EXCEPTION(TRI_ERROR_TYPE_ERROR);
  }
}
