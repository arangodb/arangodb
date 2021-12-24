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

#include "RocksDBValue.h"
#include "Basics/Exceptions.h"
#include "Basics/NumberUtils.h"
#include "Basics/StaticStrings.h"
#include "Basics/StringUtils.h"
#include "Replication2/ReplicatedLog/LogCommon.h"
#include "RocksDBEngine/RocksDBFormat.h"

#include "Transaction/Helpers.h"

using namespace arangodb;
using namespace arangodb::rocksutils;

RocksDBValue RocksDBValue::Database(VPackSlice data) {
  return RocksDBValue(RocksDBEntryType::Database, data);
}

RocksDBValue RocksDBValue::Collection(VPackSlice data) {
    return RocksDBValue(RocksDBEntryType::Collection, data);
}

RocksDBValue RocksDBValue::ReplicatedLog(VPackSlice data) {
    return RocksDBValue(RocksDBEntryType::ReplicatedLog, data);
}

RocksDBValue RocksDBValue::PrimaryIndexValue(LocalDocumentId const& docId, RevisionId rev) {
  return RocksDBValue(RocksDBEntryType::PrimaryIndexValue, docId, rev);
}

RocksDBValue RocksDBValue::EdgeIndexValue(std::string_view vertexId) {
  return RocksDBValue(RocksDBEntryType::EdgeIndexValue, vertexId);
}

RocksDBValue RocksDBValue::VPackIndexValue() {
  return RocksDBValue(RocksDBEntryType::VPackIndexValue);
}

RocksDBValue RocksDBValue::VPackIndexValue(VPackSlice data) {
  return RocksDBValue(RocksDBEntryType::VPackIndexValue, data);
}

RocksDBValue RocksDBValue::ZkdIndexValue() {
  return RocksDBValue(RocksDBEntryType::ZkdIndexValue);
}

RocksDBValue RocksDBValue::UniqueZkdIndexValue(LocalDocumentId const& docId) {
  return RocksDBValue(RocksDBEntryType::UniqueZkdIndexValue, docId, RevisionId::none());
}

RocksDBValue RocksDBValue::UniqueVPackIndexValue(LocalDocumentId const& docId) {
  return RocksDBValue(RocksDBEntryType::UniqueVPackIndexValue, docId, RevisionId::none());
}

RocksDBValue RocksDBValue::View(VPackSlice data) {
  return RocksDBValue(RocksDBEntryType::View, data);
}

RocksDBValue RocksDBValue::ReplicationApplierConfig(VPackSlice data) {
  return RocksDBValue(RocksDBEntryType::ReplicationApplierConfig, data);
}

RocksDBValue RocksDBValue::KeyGeneratorValue(VPackSlice data) {
  return RocksDBValue(RocksDBEntryType::KeyGeneratorValue, data);
}

RocksDBValue RocksDBValue::S2Value(S2Point const& p) { return RocksDBValue(p); }

RocksDBValue RocksDBValue::Empty(RocksDBEntryType type) {
  return RocksDBValue(type);
}

RocksDBValue RocksDBValue::LogEntry(replication2::PersistingLogEntry const& entry) {
  return RocksDBValue(RocksDBEntryType::LogEntry, entry);
}

LocalDocumentId RocksDBValue::documentId(RocksDBValue const& value) {
  return documentId(value._buffer.data(), value._buffer.size());
}

LocalDocumentId RocksDBValue::documentId(rocksdb::Slice const& slice) {
  return documentId(slice.data(), slice.size());
}

LocalDocumentId RocksDBValue::documentId(std::string_view s) {
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

std::string_view RocksDBValue::vertexId(rocksdb::Slice const& s) {
  return vertexId(s.data(), s.size());
}

VPackSlice RocksDBValue::data(RocksDBValue const& value) {
  return data(value._buffer.data(), value._buffer.size());
}

VPackSlice RocksDBValue::data(rocksdb::Slice const& slice) {
  return data(slice.data(), slice.size());
}

VPackSlice RocksDBValue::data(std::string_view s) {
  return data(s.data(), s.size());
}

S2Point RocksDBValue::centroid(rocksdb::Slice const& s) {
  TRI_ASSERT(s.size() == sizeof(double) * 3);
  return S2Point(intToDouble(uint64FromPersistent(s.data())),
                 intToDouble(uint64FromPersistent(s.data() + sizeof(uint64_t))),
                 intToDouble(uint64FromPersistent(s.data() + sizeof(uint64_t) * 2)));
}

replication2::LogTerm RocksDBValue::logTerm(rocksdb::Slice const& slice) {
  TRI_ASSERT(slice.size() >= sizeof(uint64_t));
  return replication2::LogTerm(uint64FromPersistent(slice.data()));
}

replication2::LogPayload RocksDBValue::logPayload(rocksdb::Slice const& slice) {
  TRI_ASSERT(slice.size() >= sizeof(uint64_t));
  auto data = slice.ToStringView();
  data.remove_prefix(sizeof(uint64_t));
  return replication2::LogPayload::createFromSlice(
      VPackSlice(reinterpret_cast<uint8_t const*>(data.data())));
}

RocksDBValue::RocksDBValue(RocksDBEntryType type) : _type(type), _buffer() {}

RocksDBValue::RocksDBValue(RocksDBEntryType type, LocalDocumentId const& docId, RevisionId revision)
    : _type(type), _buffer() {
  switch (_type) {
    case RocksDBEntryType::UniqueVPackIndexValue:
    case RocksDBEntryType::UniqueZkdIndexValue:
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

RocksDBValue::RocksDBValue(RocksDBEntryType type, VPackSlice data)
    : _type(type), _buffer() {
  switch (_type) {
    case RocksDBEntryType::VPackIndexValue:
      TRI_ASSERT(data.isArray());
      [[fallthrough]];

    case RocksDBEntryType::Database:
    case RocksDBEntryType::Collection:
    case RocksDBEntryType::ReplicatedLog:
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

RocksDBValue::RocksDBValue(RocksDBEntryType type, std::string_view data)
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

RocksDBValue::RocksDBValue(RocksDBEntryType type, replication2::PersistingLogEntry const& entry) {
  TRI_ASSERT(type == RocksDBEntryType::LogEntry);
  VPackBuilder builder;
  entry.toVelocyPack(builder, replication2::PersistingLogEntry::omitLogIndex);
  _buffer.reserve(builder.size());
  _buffer.append(reinterpret_cast<const char*>(builder.data()), builder.size());
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

std::string_view RocksDBValue::vertexId(char const* data, size_t size) {
  TRI_ASSERT(data != nullptr);
  TRI_ASSERT(size >= sizeof(char));
  return std::string_view(data, size);
}

VPackSlice RocksDBValue::data(char const* data, size_t size) {
  TRI_ASSERT(data != nullptr);
  TRI_ASSERT(size >= sizeof(char));
  return VPackSlice(reinterpret_cast<uint8_t const*>(data));
}
