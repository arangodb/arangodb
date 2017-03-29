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

#include "RocksDBEngine/RocksDBKeyBounds.h"
#include "Basics/Exceptions.h"
#include "RocksDBEngine/RocksDBCommon.h"
#include "RocksDBEngine/RocksDBTypes.h"

using namespace arangodb;
using namespace arangodb::rocksutils;
using namespace arangodb::velocypack;

const char RocksDBKeyBounds::_stringSeparator = '\0';

RocksDBKeyBounds RocksDBKeyBounds::Databases() {
  return RocksDBKeyBounds(RocksDBEntryType::Database);
}

RocksDBKeyBounds RocksDBKeyBounds::DatabaseCollections(
    TRI_voc_tick_t databaseId) {
  return RocksDBKeyBounds(RocksDBEntryType::Collection, databaseId);
}

RocksDBKeyBounds RocksDBKeyBounds::CollectionIndexes(
    TRI_voc_tick_t databaseId, TRI_voc_cid_t collectionId) {
  return RocksDBKeyBounds(RocksDBEntryType::Index, databaseId, collectionId);
}

RocksDBKeyBounds RocksDBKeyBounds::CollectionDocuments(uint64_t collectionId) {
  return RocksDBKeyBounds(RocksDBEntryType::Document, collectionId);
}

RocksDBKeyBounds RocksDBKeyBounds::PrimaryIndex(uint64_t indexId) {
  return RocksDBKeyBounds(RocksDBEntryType::PrimaryIndexValue, indexId);
}

RocksDBKeyBounds RocksDBKeyBounds::EdgeIndex(uint64_t indexId) {
  return RocksDBKeyBounds(RocksDBEntryType::EdgeIndexValue, indexId);
}

RocksDBKeyBounds RocksDBKeyBounds::EdgeIndexVertex(
    uint64_t indexId, std::string const& vertexId) {
  return RocksDBKeyBounds(RocksDBEntryType::EdgeIndexValue, indexId, vertexId);
}

RocksDBKeyBounds RocksDBKeyBounds::Index(uint64_t indexId) {
  return RocksDBKeyBounds(RocksDBEntryType::IndexValue, indexId);
}

RocksDBKeyBounds RocksDBKeyBounds::UniqueIndex(uint64_t indexId) {
  return RocksDBKeyBounds(RocksDBEntryType::UniqueIndexValue, indexId);
}

RocksDBKeyBounds RocksDBKeyBounds::DatabaseViews(TRI_voc_tick_t databaseId) {
  return RocksDBKeyBounds(RocksDBEntryType::View, databaseId);
}

rocksdb::Slice const RocksDBKeyBounds::start() const {
  return rocksdb::Slice(_startBuffer);
}

rocksdb::Slice const RocksDBKeyBounds::end() const {
  return rocksdb::Slice(_endBuffer);
}

RocksDBKeyBounds::RocksDBKeyBounds(RocksDBEntryType type)
    : _type(type), _startBuffer(), _endBuffer() {
  switch (_type) {
    case RocksDBEntryType::Database: {
      size_t length = sizeof(char);
      _startBuffer.reserve(length);
      _startBuffer.push_back(static_cast<char>(_type));

      generateEndFromStart();

      break;
    }

    default:
      THROW_ARANGO_EXCEPTION(TRI_ERROR_BAD_PARAMETER);
  }
}

RocksDBKeyBounds::RocksDBKeyBounds(RocksDBEntryType type, uint64_t first)
    : _type(type), _startBuffer(), _endBuffer() {
  switch (_type) {
    case RocksDBEntryType::Collection:
    case RocksDBEntryType::Document:
    case RocksDBEntryType::PrimaryIndexValue:
    case RocksDBEntryType::EdgeIndexValue:
    case RocksDBEntryType::IndexValue:
    case RocksDBEntryType::UniqueIndexValue:
    case RocksDBEntryType::View: {
      size_t length = sizeof(char) + sizeof(uint64_t);
      _startBuffer.reserve(length);
      _startBuffer.push_back(static_cast<char>(_type));
      uint64ToPersistent(_startBuffer, first);

      generateEndFromStart();

      break;
    }

    default:
      THROW_ARANGO_EXCEPTION(TRI_ERROR_BAD_PARAMETER);
  }
}

RocksDBKeyBounds::RocksDBKeyBounds(RocksDBEntryType type, uint64_t first,
                                   uint64_t second)
    : _type(type), _startBuffer(), _endBuffer() {
  switch (_type) {
    case RocksDBEntryType::Index: {
      size_t length = sizeof(char) + (2 * sizeof(uint64_t));
      _startBuffer.reserve(length);
      _startBuffer.push_back(static_cast<char>(_type));
      uint64ToPersistent(_startBuffer, first);
      uint64ToPersistent(_startBuffer, second);

      generateEndFromStart();

      break;
    }

    default:
      THROW_ARANGO_EXCEPTION(TRI_ERROR_BAD_PARAMETER);
  }
}

RocksDBKeyBounds::RocksDBKeyBounds(RocksDBEntryType type, uint64_t first,
                                   std::string const& second)
    : _type(type), _startBuffer(), _endBuffer() {
  switch (_type) {
    case RocksDBEntryType::EdgeIndexValue: {
      size_t length =
          sizeof(char) + sizeof(uint64_t) + second.size() + sizeof(char);
      _startBuffer.reserve(length);
      _startBuffer.push_back(static_cast<char>(_type));
      uint64ToPersistent(_startBuffer, first);
      _startBuffer.append(second);
      // leave separator byte off for now

      generateEndFromStart();

      // now push separator byte to both strings
      _startBuffer.push_back(_stringSeparator);
      _endBuffer.push_back(_stringSeparator);

      break;
    }

    default:
      THROW_ARANGO_EXCEPTION(TRI_ERROR_BAD_PARAMETER);
  }
}

void RocksDBKeyBounds::generateEndFromStart() {
  TRI_ASSERT(_startBuffer.size() >= 1);

  _endBuffer.clear();
  _endBuffer.append(_startBuffer);

  size_t i = _endBuffer.size() - 1;
  while (_endBuffer[i] == static_cast<char>(0xff)) {
    _endBuffer[i] = static_cast<char>(0x00);
    if (i == 0) {
      _endBuffer.clear();
      _endBuffer.append(_startBuffer);
      _endBuffer.push_back(static_cast<char>(0x00));
      return;
    }
    i--;
  }

  _endBuffer[i] =
      static_cast<char>(static_cast<unsigned char>(_endBuffer[i]) + 1);
}
