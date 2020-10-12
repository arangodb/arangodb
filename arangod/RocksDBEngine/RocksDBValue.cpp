////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2020 ArangoDB GmbH, Cologne, Germany
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

#include "RocksDBValue.h"
#include "Basics/Exceptions.h"
#include "Basics/NumberUtils.h"
#include "Basics/StaticStrings.h"
#include "Basics/StringUtils.h"
#include "RocksDBEngine/RocksDBFormat.h"

#include "Transaction/Helpers.h"

using namespace arangodb;
using namespace arangodb::rocksutils;

RocksDBValue RocksDBValue::Database(VPackSlice const& data) {
  return RocksDBValue(RocksDBEntryType::Database, data);
}

RocksDBValue RocksDBValue::Collection(VPackSlice const& data) {
  return RocksDBValue(RocksDBEntryType::Collection, data);
}

RocksDBValue RocksDBValue::PrimaryIndexValue(LocalDocumentId const& docId, RevisionId rev) {
  return RocksDBValue(RocksDBEntryType::PrimaryIndexValue, docId, rev);
}

RocksDBValue RocksDBValue::EdgeIndexValue(arangodb::velocypack::StringRef const& vertexId) {
  return RocksDBValue(RocksDBEntryType::EdgeIndexValue, vertexId);
}

RocksDBValue RocksDBValue::VPackIndexValue() {
  return RocksDBValue(RocksDBEntryType::VPackIndexValue);
}

RocksDBValue RocksDBValue::UniqueVPackIndexValue(LocalDocumentId const& docId) {
  return RocksDBValue(RocksDBEntryType::UniqueVPackIndexValue, docId, RevisionId::none());
}

RocksDBValue RocksDBValue::View(VPackSlice const& data) {
  return RocksDBValue(RocksDBEntryType::View, data);
}

RocksDBValue RocksDBValue::ReplicationApplierConfig(VPackSlice const& data) {
  return RocksDBValue(RocksDBEntryType::ReplicationApplierConfig, data);
}

RocksDBValue RocksDBValue::KeyGeneratorValue(VPackSlice const& data) {
  return RocksDBValue(RocksDBEntryType::KeyGeneratorValue, data);
}

RocksDBValue RocksDBValue::S2Value(S2Point const& p) { return RocksDBValue(p); }

RocksDBValue RocksDBValue::Empty(RocksDBEntryType type) {
  return RocksDBValue(type);
}

LocalDocumentId RocksDBValue::documentId(RocksDBValue const& value) {
  return documentId(value._buffer.data(), value._buffer.size());
}

LocalDocumentId RocksDBValue::documentId(rocksdb::Slice const& slice) {
  return documentId(slice.data(), slice.size());
}

LocalDocumentId RocksDBValue::documentId(std::string const& s) {
  return documentId(s.data(), s.size());
}

bool RocksDBValue::revisionId(rocksdb::Slice const& slice, RevisionId& id) {
  if (slice.size() == sizeof(LocalDocumentId::BaseType) + sizeof(RevisionId)) {
    id = RevisionId::fromPersistent(slice.data() + sizeof(LocalDocumentId::BaseType));
    return true;
  }
  return false;
}

RevisionId RocksDBValue::revisionId(RocksDBValue const& value) {
  RevisionId id;
  if (revisionId(rocksdb::Slice(value.string()), id)) {
    return id;
  }
  THROW_ARANGO_EXCEPTION_MESSAGE(
      TRI_ERROR_INTERNAL, "Could not extract revisionId from rocksdb::Slice");
}

RevisionId RocksDBValue::revisionId(rocksdb::Slice const& slice) {
  RevisionId id;
  if (revisionId(slice, id)) {
    return id;
  }
  THROW_ARANGO_EXCEPTION_MESSAGE(
      TRI_ERROR_INTERNAL, "Could not extract revisionId from rocksdb::Slice");
}

arangodb::velocypack::StringRef RocksDBValue::vertexId(rocksdb::Slice const& s) {
  return vertexId(s.data(), s.size());
}

VPackSlice RocksDBValue::data(RocksDBValue const& value) {
  return data(value._buffer.data(), value._buffer.size());
}

VPackSlice RocksDBValue::data(rocksdb::Slice const& slice) {
  return data(slice.data(), slice.size());
}

VPackSlice RocksDBValue::data(std::string const& s) {
  return data(s.data(), s.size());
}

uint64_t RocksDBValue::keyValue(rocksdb::Slice const& slice) {
  return keyValue(slice.data(), slice.size());
}

S2Point RocksDBValue::centroid(rocksdb::Slice const& s) {
  TRI_ASSERT(s.size() == sizeof(double) * 3);
  return S2Point(intToDouble(uint64FromPersistent(s.data())),
                 intToDouble(uint64FromPersistent(s.data() + sizeof(uint64_t))),
                 intToDouble(uint64FromPersistent(s.data() + sizeof(uint64_t) * 2)));
}

RocksDBValue::RocksDBValue(RocksDBEntryType type) : _type(type), _buffer() {}

RocksDBValue::RocksDBValue(RocksDBEntryType type, LocalDocumentId const& docId, RevisionId revision)
    : _type(type), _buffer() {
  switch (_type) {
    case RocksDBEntryType::UniqueVPackIndexValue:
    case RocksDBEntryType::PrimaryIndexValue: {
      if (!revision) {
        _buffer.reserve(sizeof(uint64_t));
        uint64ToPersistent(_buffer, docId.id());  // LocalDocumentId
      } else {
        _buffer.reserve(sizeof(uint64_t) * 2);
        uint64ToPersistent(_buffer, docId.id());  // LocalDocumentId
        revision.toPersistent(_buffer);           // revision
      }
      break;
    }

    default:
      THROW_ARANGO_EXCEPTION(TRI_ERROR_BAD_PARAMETER);
  }
}

RocksDBValue::RocksDBValue(RocksDBEntryType type, VPackSlice const& data)
    : _type(type), _buffer() {
  switch (_type) {
    case RocksDBEntryType::Database:
    case RocksDBEntryType::Collection:
    case RocksDBEntryType::View:
    case RocksDBEntryType::KeyGeneratorValue:
    case RocksDBEntryType::ReplicationApplierConfig: {
      _buffer.reserve(static_cast<size_t>(data.byteSize()));
      _buffer.append(reinterpret_cast<char const*>(data.begin()),
                     static_cast<size_t>(data.byteSize()));
      break;
    }

    case RocksDBEntryType::Document:
      TRI_ASSERT(false);  // use for document => get free schellen
      break;

    default:
      THROW_ARANGO_EXCEPTION(TRI_ERROR_BAD_PARAMETER);
  }
}

RocksDBValue::RocksDBValue(RocksDBEntryType type, arangodb::velocypack::StringRef const& data)
    : _type(type), _buffer() {
  switch (_type) {
    case RocksDBEntryType::EdgeIndexValue: {
      _buffer.reserve(static_cast<size_t>(data.size()));
      _buffer.append(data.data(), data.size());
      break;
    }

    default:
      THROW_ARANGO_EXCEPTION(TRI_ERROR_BAD_PARAMETER);
  }
}

RocksDBValue::RocksDBValue(S2Point const& p)
    : _type(RocksDBEntryType::GeoIndexValue), _buffer() {
  _buffer.reserve(sizeof(uint64_t) * 3);
  uint64ToPersistent(_buffer, rocksutils::doubleToInt(p.x()));
  uint64ToPersistent(_buffer, rocksutils::doubleToInt(p.y()));
  uint64ToPersistent(_buffer, rocksutils::doubleToInt(p.z()));
}

LocalDocumentId RocksDBValue::documentId(char const* data, uint64_t size) {
  TRI_ASSERT(data != nullptr && size >= sizeof(LocalDocumentId::BaseType));
  return LocalDocumentId(uint64FromPersistent(data));
}

arangodb::velocypack::StringRef RocksDBValue::vertexId(char const* data, size_t size) {
  TRI_ASSERT(data != nullptr);
  TRI_ASSERT(size >= sizeof(char));
  return arangodb::velocypack::StringRef(data, size);
}

VPackSlice RocksDBValue::data(char const* data, size_t size) {
  TRI_ASSERT(data != nullptr);
  TRI_ASSERT(size >= sizeof(char));
  return VPackSlice(reinterpret_cast<uint8_t const*>(data));
}

uint64_t RocksDBValue::keyValue(char const* data, size_t size) {
  TRI_ASSERT(data != nullptr);
  TRI_ASSERT(size >= sizeof(char));
  VPackSlice key = transaction::helpers::extractKeyFromDocument(VPackSlice(reinterpret_cast<uint8_t const*>(data)));
  if (key.isString()) {
    VPackValueLength l;
    char const* p = key.getStringUnchecked(l);
    if (l > 0 && *p >= '0' && *p <= '9') {
      return NumberUtils::atoi_zero<uint64_t>(p, p + l);
    }
  }

  return 0;
}
