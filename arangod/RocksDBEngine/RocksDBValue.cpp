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

#include "RocksDBValue.h"
#include "Basics/Exceptions.h"
#include "Basics/StaticStrings.h"
#include "Basics/StringUtils.h"
#include "RocksDBEngine/RocksDBCommon.h"

using namespace arangodb;
using namespace arangodb::rocksutils;

RocksDBValue RocksDBValue::Database(VPackSlice const& data) {
  return RocksDBValue(RocksDBEntryType::Database, data);
}

RocksDBValue RocksDBValue::Collection(VPackSlice const& data) {
  return RocksDBValue(RocksDBEntryType::Collection, data);
}

RocksDBValue RocksDBValue::PrimaryIndexValue(TRI_voc_rid_t revisionId) {
  return RocksDBValue(RocksDBEntryType::PrimaryIndexValue, revisionId);
}

RocksDBValue RocksDBValue::EdgeIndexValue(arangodb::StringRef const& vertexId) {
  return RocksDBValue(RocksDBEntryType::EdgeIndexValue, vertexId);
}

RocksDBValue RocksDBValue::VPackIndexValue() {
  return RocksDBValue(RocksDBEntryType::VPackIndexValue);
}

RocksDBValue RocksDBValue::UniqueVPackIndexValue(TRI_voc_rid_t revisionId) {
  return RocksDBValue(RocksDBEntryType::UniqueVPackIndexValue, revisionId);
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

StringRef RocksDBValue::vertexId(rocksdb::Slice const& s) {
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

uint64_t RocksDBValue::keyValue(RocksDBValue const& value) {
  return keyValue(value._buffer.data(), value._buffer.size());
}

uint64_t RocksDBValue::keyValue(rocksdb::Slice const& slice) {
  return keyValue(slice.data(), slice.size());
}

uint64_t RocksDBValue::keyValue(std::string const& s) {
  return keyValue(s.data(), s.size());
}

RocksDBValue::RocksDBValue(RocksDBEntryType type) : _type(type), _buffer() {}

RocksDBValue::RocksDBValue(RocksDBEntryType type, uint64_t data)
    : _type(type), _buffer() {
  switch (_type) {
    case RocksDBEntryType::UniqueVPackIndexValue:
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
    case RocksDBEntryType::View:
    case RocksDBEntryType::KeyGeneratorValue:
    case RocksDBEntryType::ReplicationApplierConfig: {
      _buffer.reserve(static_cast<size_t>(data.byteSize()));
      _buffer.append(reinterpret_cast<char const*>(data.begin()),
                     static_cast<size_t>(data.byteSize()));
      break;
    }
      
    case RocksDBEntryType::Document:
      TRI_ASSERT(false);// use for document => get free schellen
      break;

    default:
      THROW_ARANGO_EXCEPTION(TRI_ERROR_BAD_PARAMETER);
  }
}

RocksDBValue::RocksDBValue(RocksDBEntryType type, StringRef const& data)
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

TRI_voc_rid_t RocksDBValue::revisionId(char const* data, uint64_t size) {
  TRI_ASSERT(data != nullptr && size >= sizeof(uint64_t));
  return uint64FromPersistent(data);
}

StringRef RocksDBValue::vertexId(char const* data, size_t size) {
  TRI_ASSERT(data != nullptr);
  TRI_ASSERT(size >= sizeof(char));
  return StringRef(data, size);
}

VPackSlice RocksDBValue::data(char const* data, size_t size) {
  TRI_ASSERT(data != nullptr);
  TRI_ASSERT(size >= sizeof(char));
  return VPackSlice(data);
}

uint64_t RocksDBValue::keyValue(char const* data, size_t size) {
  TRI_ASSERT(data != nullptr);
  TRI_ASSERT(size >= sizeof(char));
  VPackSlice slice(data);
  VPackSlice key = slice.get(StaticStrings::KeyString);
  if (key.isString()) {
    std::string s = key.copyString();
    if (s.size() > 0 && s[0] >= '0' && s[0] <= '9') {
      return basics::StringUtils::uint64(s);
    }
  }

  return 0;
}
