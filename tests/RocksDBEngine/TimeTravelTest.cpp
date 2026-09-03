////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2026 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Business Source License 1.1 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     https://github.com/arangodb/arangodb/blob/devel/LICENSE
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include <gtest/gtest.h>

#include <rocksdb/comparator.h>
#include <rocksdb/db.h>

#include <velocypack/Builder.h>
#include <velocypack/Slice.h>

#include "Basics/StaticStrings.h"
#include "RocksDBEngine/RocksDBCollection.h"
#include "RocksDBEngine/RocksDBColumnFamilyManager.h"
#include "RocksDBEngine/RocksDBEngine.h"
#include "RocksDBEngine/RocksDBKey.h"
#include "RocksDBEngine/RocksDBPrimaryIndex.h"
#include "RocksDBEngine/StorageEngineDataTest.h"
#include "RocksDBEngine/StorageEngineDocumentTest.h"
#include "RocksDBEngine/StorageEngineFixture.h"
#include "VocBase/LogicalCollection.h"

using namespace arangodb;
using namespace arangodb::tests;

namespace {

rocksdb::ColumnFamilyHandle* primaryIndexCf(LogicalCollection& collection) {
  return toRocksDBCollection(collection.getPhysical())
      ->primaryIndex()
      ->columnFamily();
}

}  // namespace

TEST_F(TimeTravelStorageEngineFixture, PrimaryIndexTtColumnFamilyIsUdt) {
  rocksdb::ColumnFamilyHandle* ttCf = RocksDBColumnFamilyManager::get(
      RocksDBColumnFamilyManager::Family::PrimaryIndex_TT);
  ASSERT_NE(ttCf, nullptr);
  EXPECT_EQ(ttCf->GetComparator()->timestamp_size(), 8u);

  rocksdb::ColumnFamilyHandle* primaryCf = RocksDBColumnFamilyManager::get(
      RocksDBColumnFamilyManager::Family::PrimaryIndex);
  ASSERT_NE(primaryCf, nullptr);
  EXPECT_EQ(primaryCf->GetComparator()->timestamp_size(), 0u);
}

TEST_F(TimeTravelStorageEngineDataTest,
       TimeTravelCollectionUsesTtPrimaryIndex) {
  auto database = makeDatabase("testDatabase", 42);
  auto collection =
      makeCollection(*database, "ttCollection", /*timeTravel*/ true);

  EXPECT_EQ(primaryIndexCf(*collection),
            RocksDBColumnFamilyManager::get(
                RocksDBColumnFamilyManager::Family::PrimaryIndex_TT));
}

TEST_F(TimeTravelStorageEngineDataTest,
       NonTimeTravelCollectionUsesRegularPrimaryIndex) {
  auto database = makeDatabase("testDatabase", 42);
  auto collection =
      makeCollection(*database, "plainCollection", /*timeTravel*/ false);

  EXPECT_EQ(primaryIndexCf(*collection),
            RocksDBColumnFamilyManager::get(
                RocksDBColumnFamilyManager::Family::PrimaryIndex));
}

TEST_F(TimeTravelStorageEngineDataTest, TimeTravelFlagPersistsInProperties) {
  auto database = makeDatabase("testDatabase", 42);
  auto collection =
      makeCollection(*database, "ttCollection", /*timeTravel*/ true);

  EXPECT_TRUE(
      toRocksDBCollection(collection->getPhysical())->timeTravelEnabled());

  VPackBuilder builder;
  builder.openObject();
  collection->getPhysical()->getPropertiesVPack(builder);
  builder.close();

  auto slice = builder.slice().get(StaticStrings::EnableTimeTravel);
  ASSERT_TRUE(slice.isBool());
  EXPECT_TRUE(slice.getBool());
}

TEST_F(TimeTravelStorageEngineDataTest, RegularCollectionReportsFlagFalse) {
  auto database = makeDatabase("testDatabase", 42);
  auto collection =
      makeCollection(*database, "plainCollection", /*timeTravel*/ false);

  EXPECT_FALSE(
      toRocksDBCollection(collection->getPhysical())->timeTravelEnabled());
}

TEST_F(TimeTravelStorageEngineDataTest, TimeTravelFlagIsImmutable) {
  auto database = makeDatabase("testDatabase", 42);
  auto collection =
      makeCollection(*database, "ttCollection", /*timeTravel*/ true);

  VPackBuilder update;
  update.openObject();
  update.add(StaticStrings::EnableTimeTravel, VPackValue(false));
  update.close();

  auto res = collection->getPhysical()->updateProperties(update.slice());
  EXPECT_TRUE(res.ok()) << res.errorMessage();

  EXPECT_TRUE(
      toRocksDBCollection(collection->getPhysical())->timeTravelEnabled());
}

TEST_F(TimeTravelStorageEngineDataTest,
       RegularCollectionCannotEnableTimeTravel) {
  auto database = makeDatabase("testDatabase", 42);
  auto collection =
      makeCollection(*database, "plainCollection", /*timeTravel*/ false);

  VPackBuilder update;
  update.openObject();
  update.add(StaticStrings::EnableTimeTravel, VPackValue(true));
  update.close();

  auto res = collection->getPhysical()->updateProperties(update.slice());
  EXPECT_TRUE(res.ok()) << res.errorMessage();

  EXPECT_FALSE(
      toRocksDBCollection(collection->getPhysical())->timeTravelEnabled());
}

// ================ insert with custom _created ================

namespace {

VPackString createdDoc(std::string_view key, int value, uint64_t created) {
  VPackBuilder b;
  b.openObject();
  b.add(StaticStrings::KeyString, VPackValue(key));
  b.add("value", VPackValue(value));
  b.add(StaticStrings::Created, VPackValue(created));
  b.close();
  return VPackString{b.slice()};
}

}  // namespace

TEST_F(TimeTravelStorageEngineDocumentTest, InsertStampsCreatedAndNullExpired) {
  auto doc = createdDoc("k1", 42, /*created*/ 1000);
  auto ins = insertR(doc.slice());
  ASSERT_TRUE(ins.ok()) << ins.errorMessage();

  auto res = read("k1");
  ASSERT_TRUE(res.ok()) << res.errorMessage();
  auto slice = res.slice();
  ASSERT_TRUE(slice.get(StaticStrings::Created).isNumber()) << slice.toJson();
  EXPECT_EQ(slice.get(StaticStrings::Created).getNumber<uint64_t>(), 1000u);
  EXPECT_TRUE(slice.get(StaticStrings::Expired).isNull()) << slice.toJson();
  EXPECT_EQ(slice.get("value").getNumber<int>(), 42);
}

TEST_F(TimeTravelStorageEngineDocumentTest, InsertWithoutCreatedIsRejected) {
  VPackBuilder b;
  b.openObject();
  b.add(StaticStrings::KeyString, VPackValue("k1"));
  b.add("value", VPackValue(42));
  b.close();

  auto ins = insertR(b.slice());
  ASSERT_TRUE(ins.fail());
  EXPECT_EQ(ins.errorNumber(), TRI_ERROR_BAD_PARAMETER) << ins.errorMessage();

  EXPECT_TRUE(read("k1").fail());
}

TEST_F(TimeTravelStorageEngineDocumentTest,
       InsertWithNonNumericCreatedIsRejected) {
  VPackBuilder b;
  b.openObject();
  b.add(StaticStrings::KeyString, VPackValue("k1"));
  b.add("value", VPackValue(42));
  b.add(StaticStrings::Created, VPackValue("not-a-number"));
  b.close();

  auto ins = insertR(b.slice());
  ASSERT_TRUE(ins.fail());
  EXPECT_EQ(ins.errorNumber(), TRI_ERROR_BAD_PARAMETER) << ins.errorMessage();
  EXPECT_TRUE(read("k1").fail());
}

TEST_F(StorageEngineDocumentTest,
       NonTimeTravelInsertDoesNotMaterializeExpired) {
  auto doc = createdDoc("k1", 42, /*created*/ 1000);
  auto ins = insertR(doc.slice());
  ASSERT_TRUE(ins.ok()) << ins.errorMessage();

  auto res = read("k1");
  ASSERT_TRUE(res.ok()) << res.errorMessage();
  auto slice = res.slice();
  EXPECT_TRUE(slice.get(StaticStrings::Expired).isNone()) << slice.toJson();
  // _created was carried through verbatim as a plain attribute.
  EXPECT_EQ(slice.get(StaticStrings::Created).getNumber<uint64_t>(), 1000u);
}

// Proves the primary-index entry was actually written at the UDT timestamp T1
// (not merely "some timestamp"): a raw Get on the PrimaryIndex_TT family finds
// the entry at and after T1 but not strictly before it. Point-in-time reads
// through the engine arrive with COR-652.
TEST_F(TimeTravelStorageEngineDocumentTest,
       PrimaryIndexEntryLivesAtCreatedTimestamp) {
  constexpr uint64_t T1 = 1000;
  ASSERT_TRUE(insertR(createdDoc("k1", 42, T1).slice()).ok());

  auto* index = toRocksDBCollection(_collection->getPhysical())->primaryIndex();
  RocksDBKey key;
  key.constructPrimaryIndexValue(index->objectId(), "k1");

  rocksdb::ColumnFamilyHandle* cf = RocksDBColumnFamilyManager::get(
      RocksDBColumnFamilyManager::Family::PrimaryIndex_TT);

  auto getAt = [&](uint64_t ts) {
    std::string tsBuf;
    rocksdb::Slice tsSlice = rocksdb::EncodeU64Ts(ts, &tsBuf);
    rocksdb::ReadOptions ro;
    ro.timestamp = &tsSlice;
    rocksdb::PinnableSlice val;
    return engine().db()->Get(ro, cf, key.string(), &val);
  };

  EXPECT_TRUE(getAt(T1).ok());
  EXPECT_TRUE(getAt(T1 + 1000).ok());
  EXPECT_TRUE(getAt(T1 - 1).IsNotFound());
}

// A rocksdb transaction commits its UDT families with a single timestamp, so
// two documents written in one transaction with different _created values
// cannot both be honored - the second write is rejected.
TEST_F(TimeTravelStorageEngineDocumentTest,
       MixedCreatedTimestampsInOneTransactionRejected) {
  SingleCollectionTransaction trx{context(), *_collection,
                                  AccessMode::Type::WRITE};
  ASSERT_TRUE(trx.begin().ok());
  OperationOptions options;

  auto r1 = trx.insert(_collection->name(), createdDoc("k1", 1, 1000).slice(),
                       options);
  ASSERT_TRUE(r1.ok()) << r1.errorMessage();

  auto r2 = trx.insert(_collection->name(), createdDoc("k2", 2, 2000).slice(),
                       options);
  EXPECT_TRUE(r2.fail());
  EXPECT_EQ(r2.errorNumber(), TRI_ERROR_BAD_PARAMETER) << r2.errorMessage();

  std::ignore = trx.finish(r2.result);
}

TEST_F(TimeTravelStorageEngineDocumentTest,
       SharedCreatedTimestampInOneTransactionCommits) {
  {
    SingleCollectionTransaction trx{context(), *_collection,
                                    AccessMode::Type::WRITE};
    ASSERT_TRUE(trx.begin().ok());
    OperationOptions options;
    ASSERT_TRUE(trx.insert(_collection->name(),
                           createdDoc("k1", 1, 1000).slice(), options)
                    .ok());
    ASSERT_TRUE(trx.insert(_collection->name(),
                           createdDoc("k2", 2, 1000).slice(), options)
                    .ok());
    ASSERT_TRUE(trx.finish(Result{}).ok());
  }

  auto a = read("k1");
  ASSERT_TRUE(a.ok()) << a.errorMessage();
  EXPECT_EQ(a.slice().get(StaticStrings::Created).getNumber<uint64_t>(), 1000u);
  auto b = read("k2");
  ASSERT_TRUE(b.ok()) << b.errorMessage();
  EXPECT_EQ(b.slice().get(StaticStrings::Created).getNumber<uint64_t>(), 1000u);
}
