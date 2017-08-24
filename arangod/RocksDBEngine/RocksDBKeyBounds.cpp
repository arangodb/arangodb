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

#include "RocksDBKeyBounds.h"
#include "Basics/Exceptions.h"
#include "RocksDBEngine/RocksDBColumnFamily.h"
#include "RocksDBEngine/RocksDBCommon.h"
#include "RocksDBEngine/RocksDBTypes.h"

#include <iostream>

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

RocksDBKeyBounds RocksDBKeyBounds::VPackIndex(uint64_t indexId) {
  return RocksDBKeyBounds(RocksDBEntryType::VPackIndexValue, indexId);
}

RocksDBKeyBounds RocksDBKeyBounds::UniqueVPackIndex(uint64_t indexId) {
  return RocksDBKeyBounds(RocksDBEntryType::UniqueVPackIndexValue, indexId);
}

RocksDBKeyBounds RocksDBKeyBounds::FulltextIndex(uint64_t indexId) {
  return RocksDBKeyBounds(RocksDBEntryType::FulltextIndexValue, indexId);
}

RocksDBKeyBounds RocksDBKeyBounds::GeoIndex(uint64_t indexId) {
  return RocksDBKeyBounds(RocksDBEntryType::GeoIndexValue, indexId);
}

RocksDBKeyBounds RocksDBKeyBounds::GeoIndex(uint64_t indexId, bool isSlot) {
  RocksDBKeyBounds b(RocksDBEntryType::GeoIndexValue);
  auto& internals = b.internals();
  internals.reserve(4 * sizeof(uint64_t));
  uint64ToPersistent(internals.buffer(), indexId);
  uint64_t norm = isSlot ? 0xFFU : 0;  // encode slot|pot in lowest bit
  uint64ToPersistent(internals.buffer(), norm);  // lower endian

  internals.separate();

  uint64ToPersistent(internals.buffer(), indexId);
  norm = norm | (0xFFFFFFFFULL << 32);
  uint64ToPersistent(internals.buffer(), norm);

  return b;
}

RocksDBKeyBounds RocksDBKeyBounds::VPackIndex(uint64_t indexId,
                                              VPackSlice const& left,
                                              VPackSlice const& right) {
  return RocksDBKeyBounds(RocksDBEntryType::VPackIndexValue, indexId, left,
                          right);
}

/// used for seeking lookups
RocksDBKeyBounds RocksDBKeyBounds::UniqueVPackIndex(uint64_t indexId,
                                                    VPackSlice const& left,
                                                    VPackSlice const& right) {
  return RocksDBKeyBounds(RocksDBEntryType::UniqueVPackIndexValue, indexId,
                          left, right);
}

/// used for point lookups
RocksDBKeyBounds RocksDBKeyBounds::UniqueVPackIndex(uint64_t indexId,
                                                    VPackSlice const& left) {
  return RocksDBKeyBounds(RocksDBEntryType::UniqueVPackIndexValue, indexId, left);
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

RocksDBKeyBounds RocksDBKeyBounds::KeyGenerators() {
  return RocksDBKeyBounds(RocksDBEntryType::KeyGeneratorValue);
}

RocksDBKeyBounds RocksDBKeyBounds::FulltextIndexPrefix(
    uint64_t objectId, arangodb::StringRef const& word) {
  // I did not want to pass a bool to the constructor for this
  RocksDBKeyBounds b(RocksDBEntryType::FulltextIndexValue);

  auto& internals = b.internals();
  internals.reserve(2 * (sizeof(uint64_t) + word.size()) + 1);
  uint64ToPersistent(internals.buffer(), objectId);
  internals.buffer().append(word.data(), word.length());
  // no sperator byte, so we match all suffixes

  internals.separate();
  
  uint64ToPersistent(internals.buffer(), objectId);
  internals.buffer().append(word.data(), word.length());
  internals.push_back(0xFFU);
  // 0xFF is higher than any valud utf-8 character
  return b;
}

RocksDBKeyBounds RocksDBKeyBounds::FulltextIndexComplete(
    uint64_t indexId, arangodb::StringRef const& word) {
  return RocksDBKeyBounds(RocksDBEntryType::FulltextIndexValue, indexId, word);
}

// ============================ Member Methods ==============================

RocksDBKeyBounds::RocksDBKeyBounds(RocksDBKeyBounds const& other)
    : _type(other._type), _internals(other._internals) {}

RocksDBKeyBounds::RocksDBKeyBounds(RocksDBKeyBounds&& other)
    : _type(other._type), _internals(std::move(other._internals)) {}

RocksDBKeyBounds& RocksDBKeyBounds::operator=(RocksDBKeyBounds const& other) {
  if (this != &other) {
    _type = other._type;
    _internals = other._internals;
  }

  return *this;
}

RocksDBKeyBounds& RocksDBKeyBounds::operator=(RocksDBKeyBounds&& other) {
  if (this != &other) {
    _type = other._type;
    _internals = std::move(other._internals);
  }

  return *this;
}

uint64_t RocksDBKeyBounds::objectId() const {
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  switch (_type) {
    case RocksDBEntryType::Document:
    case RocksDBEntryType::PrimaryIndexValue:
    case RocksDBEntryType::EdgeIndexValue:
    case RocksDBEntryType::VPackIndexValue:
    case RocksDBEntryType::UniqueVPackIndexValue:
    case RocksDBEntryType::GeoIndexValue:
    case RocksDBEntryType::FulltextIndexValue: {
      TRI_ASSERT(_internals.buffer().size() > sizeof(uint64_t));
      return uint64FromPersistent(_internals.buffer().data());
    }

    default:
      THROW_ARANGO_EXCEPTION(TRI_ERROR_TYPE_ERROR);
  }
#else
  return uint64FromPersistent(_internals.buffer().data());
#endif
}

rocksdb::ColumnFamilyHandle* RocksDBKeyBounds::columnFamily() const {
  switch (_type) {
    case RocksDBEntryType::Document:
      return RocksDBColumnFamily::documents();
    case RocksDBEntryType::PrimaryIndexValue:
      return RocksDBColumnFamily::primary();
    case RocksDBEntryType::EdgeIndexValue:
      return RocksDBColumnFamily::edge();
    case RocksDBEntryType::VPackIndexValue:
    case RocksDBEntryType::UniqueVPackIndexValue:
      return RocksDBColumnFamily::vpack();
    case RocksDBEntryType::FulltextIndexValue:
      return RocksDBColumnFamily::fulltext();
    case RocksDBEntryType::GeoIndexValue:
      return RocksDBColumnFamily::geo();
    case RocksDBEntryType::Database:
    case RocksDBEntryType::Collection:
    case RocksDBEntryType::CounterValue:
    case RocksDBEntryType::SettingsValue:
    case RocksDBEntryType::ReplicationApplierConfig:
    case RocksDBEntryType::IndexEstimateValue:
    case RocksDBEntryType::KeyGeneratorValue:
    case RocksDBEntryType::View:
      return RocksDBColumnFamily::definitions();
  }
  THROW_ARANGO_EXCEPTION(TRI_ERROR_TYPE_ERROR);
}

// constructor for an empty bound. do not use for anything but to
// default-construct a key bound!
RocksDBKeyBounds::RocksDBKeyBounds() : _type(RocksDBEntryType::Database) {}

RocksDBKeyBounds::RocksDBKeyBounds(RocksDBEntryType type) : _type(type) {
  switch (_type) {
    
    case RocksDBEntryType::Database: {
      _internals.reserve(2 * sizeof(char));
      _internals.push_back(static_cast<char>(_type));

      _internals.separate();
      _internals.push_back(static_cast<char>(_type));
      _internals.push_back(0xFFU);
      break;
    }
    case RocksDBEntryType::CounterValue:
    case RocksDBEntryType::IndexEstimateValue:
    case RocksDBEntryType::KeyGeneratorValue: {
      _internals.reserve(2 * (sizeof(char) + sizeof(uint64_t)));
      _internals.push_back(static_cast<char>(_type));
      uint64ToPersistent(_internals.buffer(), 0);

      _internals.separate();
      _internals.push_back(static_cast<char>(_type));
      uint64ToPersistent(_internals.buffer(), UINT64_MAX);
      break;
    }
    case RocksDBEntryType::GeoIndexValue:
    case RocksDBEntryType::FulltextIndexValue:
      break;
      
    default:
      THROW_ARANGO_EXCEPTION(TRI_ERROR_BAD_PARAMETER);
  }
}

/// bounds to iterate over entire index
RocksDBKeyBounds::RocksDBKeyBounds(RocksDBEntryType type, uint64_t first)
    : _type(type) {
  switch (_type) {
    case RocksDBEntryType::VPackIndexValue:
    case RocksDBEntryType::UniqueVPackIndexValue: {
      // Unique VPack index values are stored as follows:
      // 7 + 8-byte object ID of index + VPack array with index value(s) ....
      // prefix is the same for non-unique indexes
      // static slices with an array with one entry
      VPackSlice min("\x02\x03\x1e");  // [minSlice]
      VPackSlice max("\x02\x03\x1f");  // [maxSlice]
      _internals.reserve(2 * sizeof(uint64_t) + min.byteSize() + max.byteSize());
      
      uint64ToPersistent(_internals.buffer(), first);
      _internals.buffer().append((char*)(min.begin()), min.byteSize());

      _internals.separate();
      
      uint64ToPersistent(_internals.buffer(), first);
      _internals.buffer().append((char*)(max.begin()), max.byteSize());
      break;
    }

    case RocksDBEntryType::Collection:
    case RocksDBEntryType::View: {
      // Collections are stored as follows:
      // Key: 1 + 8-byte ArangoDB database ID + 8-byte ArangoDB collection ID
      _internals.reserve(2 * sizeof(char) + 3 * sizeof(uint64_t));
      _internals.push_back(static_cast<char>(_type));
      uint64ToPersistent(_internals.buffer(), first);
      _internals.separate();
      _internals.push_back(static_cast<char>(_type));
      uint64ToPersistent(_internals.buffer(), first);
      uint64ToPersistent(_internals.buffer(), UINT64_MAX);

      break;
    }
    case RocksDBEntryType::Document:
    case RocksDBEntryType::GeoIndexValue: {
      // Documents are stored as follows:
      // Key: 8-byte object ID of collection + 8-byte document revision ID
      _internals.reserve(3 * sizeof(uint64_t));
      uint64ToPersistent(_internals.buffer(), first);
      _internals.separate();
      uint64ToPersistent(_internals.buffer(), first);
      uint64ToPersistent(_internals.buffer(), UINT64_MAX);
      break;
    }

    case RocksDBEntryType::PrimaryIndexValue:
    case RocksDBEntryType::EdgeIndexValue:
    case RocksDBEntryType::FulltextIndexValue: {
      size_t length = 2 * sizeof(uint64_t) + 4 * sizeof(char);
      _internals.reserve(length);
      uint64ToPersistent(_internals.buffer(), first);
      if (type == RocksDBEntryType::EdgeIndexValue) {
        _internals.push_back('\0');
        _internals.push_back(_stringSeparator);
      }

      _internals.separate();

      uint64ToPersistent(_internals.buffer(), first);
      _internals.push_back(0xFFU);
      if (type == RocksDBEntryType::EdgeIndexValue) {
        _internals.push_back(_stringSeparator);
      }
      break;
    }

    default:
      THROW_ARANGO_EXCEPTION(TRI_ERROR_BAD_PARAMETER);
  }
}

/// bounds to iterate over specified word or edge
RocksDBKeyBounds::RocksDBKeyBounds(RocksDBEntryType type, uint64_t first,
                                   arangodb::StringRef const& second)
    : _type(type) {
  switch (_type) {
    case RocksDBEntryType::FulltextIndexValue:
    case RocksDBEntryType::EdgeIndexValue: {
      _internals.reserve(2 * (sizeof(uint64_t) + second.size() + 2)+1);
      uint64ToPersistent(_internals.buffer(), first);
      _internals.buffer().append(second.data(), second.length());
      _internals.push_back(_stringSeparator);

      _internals.separate();

      uint64ToPersistent(_internals.buffer(), first);
      _internals.buffer().append(second.data(), second.length());
      _internals.push_back(_stringSeparator);
      uint64ToPersistent(_internals.buffer(), UINT64_MAX);
      if (type == RocksDBEntryType::EdgeIndexValue) {
        _internals.push_back(0xFFU);
      }
      break;
    }

    default:
      THROW_ARANGO_EXCEPTION(TRI_ERROR_BAD_PARAMETER);
  }
}

/// point lookups for unique velocypack indexes
RocksDBKeyBounds::RocksDBKeyBounds(RocksDBEntryType type, uint64_t first,
                                   VPackSlice const& second) 
    : _type(type) {
  switch (_type) {
    case RocksDBEntryType::UniqueVPackIndexValue: {
      size_t startLength =
          sizeof(uint64_t) + static_cast<size_t>(second.byteSize());

      _internals.reserve(startLength);
      uint64ToPersistent(_internals.buffer(), first);
      _internals.buffer().append(reinterpret_cast<char const*>(second.begin()),
                                 static_cast<size_t>(second.byteSize()));

      _internals.separate();
      // second bound is intentionally left empty!
      break;
    }

    default:
      THROW_ARANGO_EXCEPTION(TRI_ERROR_BAD_PARAMETER);
  }
}

/// iterate over the specified bounds of the velocypack index
RocksDBKeyBounds::RocksDBKeyBounds(RocksDBEntryType type, uint64_t first,
                                   VPackSlice const& second,
                                   VPackSlice const& third)
    : _type(type) {
  switch (_type) {
    case RocksDBEntryType::VPackIndexValue:
    case RocksDBEntryType::UniqueVPackIndexValue: {
      size_t startLength =
          sizeof(uint64_t) + static_cast<size_t>(second.byteSize());
      size_t endLength =
          2 * sizeof(uint64_t) + static_cast<size_t>(third.byteSize());

      _internals.reserve(startLength + endLength);
      uint64ToPersistent(_internals.buffer(), first);
      _internals.buffer().append(reinterpret_cast<char const*>(second.begin()),
                                 static_cast<size_t>(second.byteSize()));

      _internals.separate();

      uint64ToPersistent(_internals.buffer(), first);
      _internals.buffer().append(reinterpret_cast<char const*>(third.begin()),
                                 static_cast<size_t>(third.byteSize()));
      uint64ToPersistent(_internals.buffer(), UINT64_MAX);
      break;
    }

    default:
      THROW_ARANGO_EXCEPTION(TRI_ERROR_BAD_PARAMETER);
  }
}

namespace arangodb {

std::ostream& operator<<(std::ostream& stream, RocksDBKeyBounds const& bounds) {
  stream << "[bounds cf: " << RocksDBColumnFamily::columnFamilyName(bounds.columnFamily()) << " type: " << arangodb::rocksDBEntryTypeName(bounds.type()) << " ";

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

  dump(bounds.start());
  stream << " - ";
  dump(bounds.end());
  stream << "]";

  return stream;
}
}
