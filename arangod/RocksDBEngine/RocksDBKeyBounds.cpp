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

RocksDBKeyBounds RocksDBKeyBounds::Empty() { return RocksDBKeyBounds(); }

RocksDBKeyBounds RocksDBKeyBounds::Databases() {
  return RocksDBKeyBounds(RocksDBEntryType::Database);
}

RocksDBKeyBounds RocksDBKeyBounds::DatabaseCollections(
    TRI_voc_tick_t databaseId) {
  return RocksDBKeyBounds(RocksDBEntryType::Collection, databaseId);
}

RocksDBKeyBounds RocksDBKeyBounds::CollectionDocuments(
    uint64_t collectionObjectId) {
  return RocksDBKeyBounds(RocksDBEntryType::Document, collectionObjectId);
}

RocksDBKeyBounds RocksDBKeyBounds::PrimaryIndex(uint64_t indexId) {
  return RocksDBKeyBounds(RocksDBEntryType::PrimaryIndexValue, indexId);
}

RocksDBKeyBounds RocksDBKeyBounds::EdgeIndex(uint64_t indexId) {
  return RocksDBKeyBounds(RocksDBEntryType::EdgeIndexValue, indexId);
}

RocksDBKeyBounds RocksDBKeyBounds::EdgeIndexVertex(
    uint64_t indexId, arangodb::StringRef const& vertexId) {
  return RocksDBKeyBounds(RocksDBEntryType::EdgeIndexValue, indexId, vertexId);
}

RocksDBKeyBounds RocksDBKeyBounds::IndexEntries(uint64_t indexId) {
  return RocksDBKeyBounds(RocksDBEntryType::IndexValue, indexId);
}

RocksDBKeyBounds RocksDBKeyBounds::UniqueIndex(uint64_t indexId) {
  return RocksDBKeyBounds(RocksDBEntryType::UniqueIndexValue, indexId);
}

RocksDBKeyBounds RocksDBKeyBounds::FulltextIndex(uint64_t indexId) {
  return RocksDBKeyBounds(RocksDBEntryType::FulltextIndexValue, indexId);
}

RocksDBKeyBounds RocksDBKeyBounds::GeoIndex(uint64_t indexId) {
  return RocksDBKeyBounds(RocksDBEntryType::GeoIndexValue, indexId);
}

RocksDBKeyBounds RocksDBKeyBounds::GeoIndex(uint64_t indexId, bool isSlot) {
  RocksDBKeyBounds b;
  size_t length = sizeof(char) + sizeof(uint64_t) * 2;
  b._startBuffer.reserve(length);
  b._startBuffer.push_back(static_cast<char>(RocksDBEntryType::GeoIndexValue));
  uint64ToPersistent(b._startBuffer, indexId);

  b._endBuffer.clear();
  b._endBuffer.append(b._startBuffer);  // append common prefix

  uint64_t norm = isSlot ? 0xFFU : 0;        // encode slot|pot in lowest bit
  uint64ToPersistent(b._startBuffer, norm);  // lower endian
  norm = norm | (0xFFFFFFFFULL << 32);
  uint64ToPersistent(b._endBuffer, norm);

  b._start = rocksdb::Slice(b._startBuffer);
  b._end = rocksdb::Slice(b._endBuffer);
  return b;
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

RocksDBKeyBounds RocksDBKeyBounds::IndexEstimateValues() {
  return RocksDBKeyBounds(RocksDBEntryType::IndexEstimateValue);
}

RocksDBKeyBounds RocksDBKeyBounds::FulltextIndexPrefix(
    uint64_t indexId, arangodb::StringRef const& word) {
  // I did not want to pass a bool to the constructor for this
  RocksDBKeyBounds bounds;
  size_t length = sizeof(char) + sizeof(uint64_t) + word.size();
  bounds._startBuffer.reserve(length);
  bounds._startBuffer.push_back(
      static_cast<char>(RocksDBEntryType::FulltextIndexValue));
  uint64ToPersistent(bounds._startBuffer, indexId);
  bounds._startBuffer.append(word.data(), word.length());

  bounds._endBuffer.clear();
  bounds._endBuffer.append(bounds._startBuffer);
  bounds._endBuffer.push_back(
      0xFFU);  // invalid UTF-8 character, higher than with memcmp

  bounds._start = rocksdb::Slice(bounds._startBuffer);
  bounds._end = rocksdb::Slice(bounds._endBuffer);

  return bounds;
}

RocksDBKeyBounds RocksDBKeyBounds::FulltextIndexComplete(
    uint64_t indexId, arangodb::StringRef const& word) {
  return RocksDBKeyBounds(RocksDBEntryType::FulltextIndexValue, indexId, word);
}

// ============================ Member Methods ==============================

RocksDBKeyBounds& RocksDBKeyBounds::operator=(RocksDBKeyBounds const& other) {
  _type = other._type;
  _startBuffer = other._startBuffer;
  _endBuffer = other._endBuffer;
  _start = rocksdb::Slice(_startBuffer);
  _end = rocksdb::Slice(_endBuffer);

  return *this;
}

rocksdb::Slice const& RocksDBKeyBounds::start() const {
  TRI_ASSERT(_start.size() > 0);
  return _start;
}

rocksdb::Slice const& RocksDBKeyBounds::end() const {
  TRI_ASSERT(_end.size() > 0);
  return _end;
}

uint64_t RocksDBKeyBounds::objectId() const {
  RocksDBEntryType type = static_cast<RocksDBEntryType>(_startBuffer[0]);
  switch (type) {
    case RocksDBEntryType::Document:
    case RocksDBEntryType::PrimaryIndexValue:
    case RocksDBEntryType::EdgeIndexValue:
    case RocksDBEntryType::IndexValue:
    case RocksDBEntryType::UniqueIndexValue: {
      TRI_ASSERT(_startBuffer.size() >= (sizeof(char) + sizeof(uint64_t)));
      return uint64FromPersistent(_startBuffer.data() + sizeof(char));
    }

    default:
      THROW_ARANGO_EXCEPTION(TRI_ERROR_TYPE_ERROR);
  }
}

// constructor for an empty bound. do not use for anything but to
// default-construct a key bound!
RocksDBKeyBounds::RocksDBKeyBounds()
    : _type(RocksDBEntryType::Database), _startBuffer(), _endBuffer() {}

RocksDBKeyBounds::RocksDBKeyBounds(RocksDBEntryType type)
    : _type(type), _startBuffer(), _endBuffer() {
  switch (_type) {
    case RocksDBEntryType::Database: {
      size_t length = sizeof(char);
      _startBuffer.reserve(length);
      _startBuffer.push_back(static_cast<char>(_type));

      _endBuffer.append(_startBuffer);
      _endBuffer[0]++; // TODO: better solution?

      break;
    }
    case RocksDBEntryType::CounterValue:
    case RocksDBEntryType::IndexEstimateValue: {
      size_t length = sizeof(char) + sizeof(uint64_t);
      _startBuffer.reserve(length);
      _startBuffer.push_back(static_cast<char>(_type));
      uint64ToPersistent(_startBuffer, 0);

      _endBuffer.reserve(length);
      _endBuffer.push_back(static_cast<char>(_type));
      uint64ToPersistent(_endBuffer, UINT64_MAX);
      break;
    }
    default:
      THROW_ARANGO_EXCEPTION(TRI_ERROR_BAD_PARAMETER);
  }
  _start = rocksdb::Slice(_startBuffer);
  _end = rocksdb::Slice(_endBuffer);
}

RocksDBKeyBounds::RocksDBKeyBounds(RocksDBEntryType type, uint64_t first)
    : _type(type), _startBuffer(), _endBuffer() {
  switch (_type) {
    case RocksDBEntryType::IndexValue:
    case RocksDBEntryType::UniqueIndexValue: {
      // Unique VPack index values are stored as follows:
      // 7 + 8-byte object ID of index + VPack array with index value(s) ....
      // prefix is the same for non-unique indexes
      // static slices with an array with one entry
      VPackSlice min("\x02\x03\x1e");  // [minSlice]
      VPackSlice max("\x02\x03\x1f");  // [maxSlice]

      size_t length = sizeof(char) + sizeof(uint64_t) + min.byteSize();
      _startBuffer.reserve(length);
      _startBuffer.push_back(static_cast<char>(_type));
      uint64ToPersistent(_startBuffer, first);
      // append common prefix
      _endBuffer.clear();
      _endBuffer.append(_startBuffer);

      // construct min max
      _startBuffer.append((char*)(min.begin()), min.byteSize());
      _endBuffer.append((char*)(max.begin()), max.byteSize());
      break;
    }

    case RocksDBEntryType::Collection:
    case RocksDBEntryType::Document:
    case RocksDBEntryType::GeoIndexValue:
    case RocksDBEntryType::View: {
      // Collections are stored as follows:
      // Key: 1 + 8-byte ArangoDB database ID + 8-byte ArangoDB collection ID
      //
      // Documents are stored as follows:
      // Key: 3 + 8-byte object ID of collection + 8-byte document revision ID
      size_t length = sizeof(char) + sizeof(uint64_t) * 2;
      _startBuffer.reserve(length);
      _startBuffer.push_back(static_cast<char>(_type));
      uint64ToPersistent(_startBuffer, first);
      // append common prefix
      _endBuffer.clear();
      _endBuffer.append(_startBuffer);

      // construct min max
      uint64ToPersistent(_startBuffer, 0);
      uint64ToPersistent(_endBuffer, UINT64_MAX);
      break;
    }

    case RocksDBEntryType::PrimaryIndexValue:
    case RocksDBEntryType::EdgeIndexValue:
    case RocksDBEntryType::FulltextIndexValue: {
      size_t length = sizeof(char) + sizeof(uint64_t);
      _startBuffer.reserve(length);
      _startBuffer.push_back(static_cast<char>(_type));
      uint64ToPersistent(_startBuffer, first);

      _endBuffer.clear();
      _endBuffer.append(_startBuffer);
      _endBuffer.push_back(0xFFU);

      break;
    }

    default:
      THROW_ARANGO_EXCEPTION(TRI_ERROR_BAD_PARAMETER);
  }
  _start = rocksdb::Slice(_startBuffer);
  _end = rocksdb::Slice(_endBuffer);
}

RocksDBKeyBounds::RocksDBKeyBounds(RocksDBEntryType type, uint64_t first,
                                   arangodb::StringRef const& second)
    : _type(type), _startBuffer(), _endBuffer() {
  switch (_type) {
    case RocksDBEntryType::FulltextIndexValue:
    case RocksDBEntryType::EdgeIndexValue: {
      size_t length =
          sizeof(char) + sizeof(uint64_t) + second.size() + sizeof(char);
      _startBuffer.reserve(length);
      _startBuffer.push_back(static_cast<char>(_type));
      uint64ToPersistent(_startBuffer, first);
      _startBuffer.append(second.data(), second.length());
      _startBuffer.push_back(_stringSeparator);

      _endBuffer.clear();
      _endBuffer.append(_startBuffer);
      _endBuffer.push_back(0xFFU);

      break;
    }

    default:
      THROW_ARANGO_EXCEPTION(TRI_ERROR_BAD_PARAMETER);
  }
  _start = rocksdb::Slice(_startBuffer);
  _end = rocksdb::Slice(_endBuffer);
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
      TRI_ASSERT(_startBuffer.length() == startLength);

      size_t endLength = sizeof(char) + sizeof(uint64_t) +
                         static_cast<size_t>(third.byteSize()) + sizeof(char);
      _endBuffer.reserve(endLength);
      _endBuffer.push_back(static_cast<char>(_type));
      uint64ToPersistent(_endBuffer, first);
      _endBuffer.append(reinterpret_cast<char const*>(third.begin()),
                        static_cast<size_t>(third.byteSize()));
      _endBuffer.push_back(_stringSeparator + 1); // compare greater than
                                                  // actual key
      break;
    }

    default:
      THROW_ARANGO_EXCEPTION(TRI_ERROR_BAD_PARAMETER);
  }
  _start = rocksdb::Slice(_startBuffer);
  _end = rocksdb::Slice(_endBuffer);
}
