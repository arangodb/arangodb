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

static RocksDBEntryType counterVal = RocksDBEntryType::CounterValue;
static rocksdb::Slice CounterValue(
    reinterpret_cast<std::underlying_type<RocksDBEntryType>::type*>(
        &counterVal),
    1);

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
  
static RocksDBEntryType fulltextIndexValue =
RocksDBEntryType::FulltextIndexValue;
static rocksdb::Slice FulltextIndexValue(
     reinterpret_cast<std::underlying_type<RocksDBEntryType>::type*>(
        &fulltextIndexValue),
     1);
  
static RocksDBEntryType geoIndexValue =
RocksDBEntryType::GeoIndexValue;
static rocksdb::Slice GeoIndexValue(
     reinterpret_cast<std::underlying_type<RocksDBEntryType>::type*>(
        &geoIndexValue),
     1);

static RocksDBEntryType view = RocksDBEntryType::View;
static rocksdb::Slice View(
    reinterpret_cast<std::underlying_type<RocksDBEntryType>::type*>(&view), 1);

static RocksDBEntryType settingsValue = RocksDBEntryType::SettingsValue;
static rocksdb::Slice SettingsValue(
    reinterpret_cast<std::underlying_type<RocksDBEntryType>::type*>(
        &settingsValue),
    1);

static RocksDBEntryType replicationApplierConfig =
    RocksDBEntryType::ReplicationApplierConfig;
static rocksdb::Slice ReplicationApplierConfig(
    reinterpret_cast<std::underlying_type<RocksDBEntryType>::type*>(
        &replicationApplierConfig),
    1);
}

char const* arangodb::rocksDBEntryTypeName(arangodb::RocksDBEntryType type) {
  switch (type) {
    case arangodb::RocksDBEntryType::Database: return "Database";
    case arangodb::RocksDBEntryType::Collection: return "Collection";
    case arangodb::RocksDBEntryType::CounterValue: return "CounterValue";
    case arangodb::RocksDBEntryType::Document: return "Document";
    case arangodb::RocksDBEntryType::PrimaryIndexValue: return "PrimaryIndexValue";
    case arangodb::RocksDBEntryType::EdgeIndexValue: return "EdgeIndexValue";
    case arangodb::RocksDBEntryType::IndexValue: return "IndexValue";
    case arangodb::RocksDBEntryType::UniqueIndexValue: return "UniqueIndexValue";
    case arangodb::RocksDBEntryType::View: return "View";
    case arangodb::RocksDBEntryType::SettingsValue: return "SettingsValue";
    case arangodb::RocksDBEntryType::ReplicationApplierConfig: return "ReplicationApplierConfig";
    case arangodb::RocksDBEntryType::FulltextIndexValue: return "FulltextIndexValue";
    case arangodb::RocksDBEntryType::GeoIndexValue: return "GeoIndexValue";
  }
  return "Invalid";
}

char const* arangodb::rocksDBLogTypeName(arangodb::RocksDBLogType type) {
  switch (type) {
    case arangodb::RocksDBLogType::DatabaseCreate: return "DatabaseCreate";
    case arangodb::RocksDBLogType::DatabaseDrop: return "DatabaseDrop";
    case arangodb::RocksDBLogType::CollectionCreate: return "CollectionCreate";
    case arangodb::RocksDBLogType::CollectionDrop: return "CollectionDrop";
    case arangodb::RocksDBLogType::CollectionRename: return "CollectionRename";
    case arangodb::RocksDBLogType::CollectionChange: return "CollectionChange";
    case arangodb::RocksDBLogType::IndexCreate: return "IndexCreate";
    case arangodb::RocksDBLogType::IndexDrop: return "IndexDrop";
    case arangodb::RocksDBLogType::ViewCreate: return "ViewCreate";
    case arangodb::RocksDBLogType::ViewDrop: return "ViewDrop";
    case arangodb::RocksDBLogType::ViewChange: return "ViewChange";
    case arangodb::RocksDBLogType::BeginTransaction: return "BeginTransaction";
    case arangodb::RocksDBLogType::DocumentOperationsPrologue: return "DocumentOperationsPrologue";
    case arangodb::RocksDBLogType::DocumentRemove: return "DocumentRemove";
    case arangodb::RocksDBLogType::SinglePut: return "SinglePut";
    case arangodb::RocksDBLogType::SingleRemove: return "SingleRemove";
    case arangodb::RocksDBLogType::Invalid: return "Invalid";
  }
  return "Invalid";
}

rocksdb::Slice const& arangodb::rocksDBSlice(RocksDBEntryType const& type) {
  switch (type) {
    case RocksDBEntryType::Database:
      return Database;
    case RocksDBEntryType::Collection:
      return Collection;
    case RocksDBEntryType::CounterValue:
      return CounterValue;
    case RocksDBEntryType::Document:
      return Document;
    case RocksDBEntryType::PrimaryIndexValue:
      return PrimaryIndexValue;
    case RocksDBEntryType::EdgeIndexValue:
      return EdgeIndexValue;
    case RocksDBEntryType::IndexValue:
      return IndexValue;
    case RocksDBEntryType::UniqueIndexValue:
      return UniqueIndexValue;
    case RocksDBEntryType::FulltextIndexValue:
      return FulltextIndexValue;
    case RocksDBEntryType::GeoIndexValue:
      return GeoIndexValue;
    case RocksDBEntryType::View:
      return View;
    case RocksDBEntryType::SettingsValue:
      return SettingsValue;
    case RocksDBEntryType::ReplicationApplierConfig:
      return ReplicationApplierConfig;
  }

  return Document;  // avoids warning - errorslice instead ?!
}
