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

RocksDBKeyBounds RocksDBKeyBounds::IndexRange(uint64_t indexId,
                                              VPackSlice const& left,
                                              VPackSlice const& right) {
  return RocksDBKeyBounds(RocksDBEntryType::IndexValue, indexId, left, right);
}

RocksDBKeyBounds RocksDBKeyBounds::UniqueIndexRange(uint64_t indexId,
                                                    VPackSlice const& left,
                                                    VPackSlice const& right) {
  return RocksDBKeyBounds(RocksDBEntryType::UniqueIndexValue, indexId, left,
                          right);
}

RocksDBKeyBounds RocksDBKeyBounds::DatabaseViews(TRI_voc_tick_t databaseId) {
  return RocksDBKeyBounds(RocksDBEntryType::View, databaseId);
}

RocksDBKeyBounds RocksDBKeyBounds::CounterValues() {
  return RocksDBKeyBounds(RocksDBEntryType::CounterValue);
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

      _endBuffer.clear();
      _endBuffer.append(_startBuffer);
      nextPrefix(_endBuffer);

      break;
    }
    case RocksDBEntryType::CounterValue: {
      size_t length = sizeof(char);
      _startBuffer.reserve(length);
      _startBuffer.push_back(static_cast<char>(_type));
      
      _endBuffer.clear();
      _endBuffer.append(_startBuffer);
      uint64ToPersistent(_startBuffer, UINT64_MAX);
      //nextPrefix(_endBuffer);
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

      _endBuffer.clear();
      _endBuffer.append(_startBuffer);
      nextPrefix(_endBuffer);

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

      _endBuffer.clear();
      _endBuffer.append(_startBuffer);
      nextPrefix(_endBuffer);

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
      _startBuffer.push_back(_stringSeparator);

      _endBuffer.clear();
      _endBuffer.append(_startBuffer);
      nextPrefix(_endBuffer);

      break;
    }

    default:
      THROW_ARANGO_EXCEPTION(TRI_ERROR_BAD_PARAMETER);
  }
}

RocksDBKeyBounds::RocksDBKeyBounds(RocksDBEntryType type, uint64_t first,
                                   VPackSlice const& second,
                                   VPackSlice const& third)
    : _type(type), _startBuffer(), _endBuffer() {
  switch (_type) {
    case RocksDBEntryType::IndexValue:
    case RocksDBEntryType::UniqueIndexValue: {
      size_t startLength = sizeof(char) + sizeof(uint64_t) +
                           static_cast<size_t>(second.byteSize()) +
                           sizeof(char);
      _startBuffer.reserve(startLength);
      _startBuffer.push_back(static_cast<char>(_type));
      uint64ToPersistent(_startBuffer, first);
      _startBuffer.append(reinterpret_cast<char const*>(second.begin()),
                          static_cast<size_t>(second.byteSize()));
      _startBuffer.push_back(_stringSeparator);

      size_t endLength = sizeof(char) + sizeof(uint64_t) +
                         static_cast<size_t>(third.byteSize()) + sizeof(char);
      _endBuffer.reserve(endLength);
      _endBuffer.push_back(static_cast<char>(_type));
      uint64ToPersistent(_endBuffer, first);
      _endBuffer.append(reinterpret_cast<char const*>(third.begin()),
                        static_cast<size_t>(third.byteSize()));
      _endBuffer.push_back(_stringSeparator);
      nextPrefix(_endBuffer);

      break;
    }

    default:
      THROW_ARANGO_EXCEPTION(TRI_ERROR_BAD_PARAMETER);
  }
}

void RocksDBKeyBounds::nextPrefix(std::string& s) {
  TRI_ASSERT(s.size() >= 1);

  size_t i = s.size() - 1;
  for (; (i > 0) && (s[i] == '\xff'); --i) {
  }

  if ((i == 0) && (s[i] == '\xff')) {
    s.push_back('\x00');
    return;
  }

  s[i] = static_cast<char>(static_cast<unsigned char>(s[i]) + 1);
  for (i = i + 1; i < s.size(); i++) {
    s[i] = '\x00';
  }
}
