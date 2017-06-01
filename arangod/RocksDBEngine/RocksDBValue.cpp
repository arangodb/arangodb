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

#include "RocksDBEngine/RocksDBValue.h"
#include "Basics/Exceptions.h"
#include "RocksDBEngine/RocksDBCommon.h"

using namespace arangodb;
using namespace arangodb::rocksutils;

RocksDBValue RocksDBValue::Database(VPackSlice const& data) {
  return RocksDBValue(RocksDBEntryType::Database, data);
}

RocksDBValue RocksDBValue::Collection(VPackSlice const& data) {
  return RocksDBValue(RocksDBEntryType::Collection, data);
}

RocksDBValue RocksDBValue::Document(VPackSlice const& data) {
  return RocksDBValue(RocksDBEntryType::Document, data);
}

RocksDBValue RocksDBValue::PrimaryIndexValue(TRI_voc_rid_t revisionId) {
  return RocksDBValue(RocksDBEntryType::PrimaryIndexValue, revisionId);
}

RocksDBValue RocksDBValue::EdgeIndexValue() {
  return RocksDBValue(RocksDBEntryType::EdgeIndexValue);
}

RocksDBValue RocksDBValue::IndexValue() {
  return RocksDBValue(RocksDBEntryType::IndexValue);
}

RocksDBValue RocksDBValue::UniqueIndexValue(TRI_voc_rid_t revisionId) {
  return RocksDBValue(RocksDBEntryType::UniqueIndexValue, revisionId);
}

RocksDBValue RocksDBValue::View(VPackSlice const& data) {
  return RocksDBValue(RocksDBEntryType::View, data);
}

RocksDBValue RocksDBValue::ReplicationApplierConfig(VPackSlice const& data) {
  return RocksDBValue(RocksDBEntryType::ReplicationApplierConfig, data);
}

RocksDBValue RocksDBValue::Empty(RocksDBEntryType type) {
  return RocksDBValue(type);
}

TRI_voc_rid_t RocksDBValue::revisionId(RocksDBValue const& value) {
  return revisionId(value._buffer.data(), value._buffer.size());
}

TRI_voc_rid_t RocksDBValue::revisionId(rocksdb::Slice const& slice) {
  return revisionId(slice.data(), slice.size());
}

TRI_voc_rid_t RocksDBValue::revisionId(std::string const& s) {
  return revisionId(s.data(), s.size());
}

StringRef RocksDBValue::primaryKey(RocksDBValue const& value) {
  return primaryKey(value._buffer.data(), value._buffer.size());
}

StringRef RocksDBValue::primaryKey(rocksdb::Slice const& slice) {
  return primaryKey(slice.data(), slice.size());
}

StringRef RocksDBValue::primaryKey(std::string const& s) {
  return primaryKey(s.data(), s.size());
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

RocksDBValue::RocksDBValue(RocksDBEntryType type) : _type(type), _buffer() {}

RocksDBValue::RocksDBValue(RocksDBEntryType type, uint64_t data)
    : _type(type), _buffer() {
  switch (_type) {
    case RocksDBEntryType::UniqueIndexValue:
    case RocksDBEntryType::PrimaryIndexValue: {
      _buffer.reserve(sizeof(uint64_t));
      uint64ToPersistent(_buffer, data);  // revision id
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
    case RocksDBEntryType::Document:
    case RocksDBEntryType::View:
    case RocksDBEntryType::ReplicationApplierConfig: {
      _buffer.reserve(static_cast<size_t>(data.byteSize()));
      _buffer.append(reinterpret_cast<char const*>(data.begin()),
                     static_cast<size_t>(data.byteSize()));
      break;
    }

    default:
      THROW_ARANGO_EXCEPTION(TRI_ERROR_BAD_PARAMETER);
  }
}

TRI_voc_rid_t RocksDBValue::revisionId(char const* data, uint64_t size) {
  TRI_ASSERT(data != nullptr && size >= sizeof(uint64_t));
  return uint64FromPersistent(data);
}

StringRef RocksDBValue::primaryKey(char const* data, size_t size) {
  TRI_ASSERT(data != nullptr);
  TRI_ASSERT(size >= sizeof(char));
  return StringRef(data, size);
}

VPackSlice RocksDBValue::data(char const* data, size_t size) {
  TRI_ASSERT(data != nullptr);
  TRI_ASSERT(size >= sizeof(char));
  return VPackSlice(data);
}
