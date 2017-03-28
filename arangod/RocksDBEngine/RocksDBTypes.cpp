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
/// @author Jan Christoph Uhde
////////////////////////////////////////////////////////////////////////////////

#include "RocksDBTypes.h"

using namespace arangodb;

namespace {

static RocksDBEntryType database = arangodb::RocksDBEntryType::Database;
static rocksdb::Slice Database(
    reinterpret_cast<std::underlying_type<RocksDBEntryType>::type*>(&database),
    1);

static RocksDBEntryType collection = RocksDBEntryType::Collection;
static rocksdb::Slice Collection(
    reinterpret_cast<std::underlying_type<RocksDBEntryType>::type*>(
        &collection),
    1);

static RocksDBEntryType index = RocksDBEntryType::Index;
static rocksdb::Slice Index(
    reinterpret_cast<std::underlying_type<RocksDBEntryType>::type*>(&index), 1);

static RocksDBEntryType document = RocksDBEntryType::Document;
static rocksdb::Slice Document(
    reinterpret_cast<std::underlying_type<RocksDBEntryType>::type*>(&document),
    1);

static RocksDBEntryType primaryIndexValue = RocksDBEntryType::PrimaryIndexValue;
static rocksdb::Slice PrimaryIndexValue(
    reinterpret_cast<std::underlying_type<RocksDBEntryType>::type*>(
        &primaryIndexValue),
    1);

static RocksDBEntryType edgeIndexValue = RocksDBEntryType::EdgeIndexValue;
static rocksdb::Slice EdgeIndexValue(
    reinterpret_cast<std::underlying_type<RocksDBEntryType>::type*>(
        &edgeIndexValue),
    1);

static RocksDBEntryType indexValue = RocksDBEntryType::IndexValue;
static rocksdb::Slice IndexValue(
    reinterpret_cast<std::underlying_type<RocksDBEntryType>::type*>(
        &indexValue),
    1);

static RocksDBEntryType uniqueIndexValue = RocksDBEntryType::UniqueIndexValue;
static rocksdb::Slice UniqueIndexValue(
    reinterpret_cast<std::underlying_type<RocksDBEntryType>::type*>(
        &uniqueIndexValue),
    1);

static RocksDBEntryType view = RocksDBEntryType::View;
static rocksdb::Slice View(
    reinterpret_cast<std::underlying_type<RocksDBEntryType>::type*>(&view), 1);
}

rocksdb::Slice const& arangodb::rocksDBSlice(RocksDBEntryType const& type) {
  switch (type) {
    case RocksDBEntryType::Document:
      return Document;
    case RocksDBEntryType::Collection:
      return Collection;
    case RocksDBEntryType::Database:
      return Database;
    case RocksDBEntryType::PrimaryIndexValue:
      return PrimaryIndexValue;
    case RocksDBEntryType::EdgeIndexValue:
      return EdgeIndexValue;
    case RocksDBEntryType::Index:
      return Index;
    case RocksDBEntryType::IndexValue:
      return IndexValue;
    case RocksDBEntryType::UniqueIndexValue:
      return UniqueIndexValue;
    case RocksDBEntryType::View:
      return View;
  }

  return Document;  // avoids warning - errorslice instead ?!
}
