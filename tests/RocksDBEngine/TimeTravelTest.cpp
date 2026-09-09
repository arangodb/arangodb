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
#include <velocypack/Iterator.h>
#include <velocypack/Slice.h>

#include "Basics/StaticStrings.h"
#include "Basics/StringUtils.h"
#include "RocksDBEngine/RocksDBCollection.h"
#include "RocksDBEngine/RocksDBColumnFamilyManager.h"
#include "RocksDBEngine/RocksDBEngine.h"
#include "RocksDBEngine/RocksDBKey.h"
#include "RocksDBEngine/RocksDBPrimaryIndex.h"
#include "RocksDBEngine/RocksDBValue.h"
#include "RocksDBEngine/StorageEngineDataTest.h"
#include "RocksDBEngine/StorageEngineDocumentTest.h"
#include "RocksDBEngine/StorageEngineFixture.h"
#include "Transaction/Hints.h"
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

namespace {

// Writes an additional version of an existing document straight through
// rocksdb: a new Documents entry under a fresh LocalDocumentId, plus a primary
// index entry for the same _key committed at `created`. This is what an update
// on a time-travel collection will do once COR-651 lands; until then it is the
// only way to get several versions of one key, which is exactly what
// point-in-time reads exist to tell apart.
void writeRawVersion(RocksDBEngine& engine, LogicalCollection& collection,
                     std::string_view key, int value, uint64_t created) {
  auto* physical = toRocksDBCollection(collection.getPhysical());
  auto* index = physical->primaryIndex();
  auto* documentsCf = RocksDBColumnFamilyManager::get(
      RocksDBColumnFamilyManager::Family::Documents);
  auto* indexCf = RocksDBColumnFamilyManager::get(
      RocksDBColumnFamilyManager::Family::PrimaryIndex_TT);

  // Resolve the newest version so the new one can inherit the immutable system
  // attributes (_key, _id) byte for byte - _id is a Custom slice that cannot be
  // rebuilt from a document read back through the engine.
  RocksDBKey indexKey;
  indexKey.constructPrimaryIndexValue(index->objectId(), key);
  rocksdb::Slice latest = rocksdb::MaxU64Ts();
  rocksdb::ReadOptions indexReadOptions;
  indexReadOptions.timestamp = &latest;
  rocksdb::PinnableSlice indexValue;
  ASSERT_TRUE(
      engine.db()
          ->Get(indexReadOptions, indexCf, indexKey.string(), &indexValue)
          .ok());

  RocksDBKey previousDocumentKey;
  previousDocumentKey.constructDocument(physical->objectId(),
                                        RocksDBValue::documentId(indexValue));
  rocksdb::PinnableSlice previousBody;
  ASSERT_TRUE(engine.db()
                  ->Get(rocksdb::ReadOptions{}, documentsCf,
                        previousDocumentKey.string(), &previousBody)
                  .ok());

  RevisionId revision = collection.newRevisionId();
  LocalDocumentId documentId = LocalDocumentId::create(revision);

  char ridBuffer[basics::maxUInt64StringSize];
  VPackBuilder body;
  body.openObject();
  VPackSlice previous{reinterpret_cast<uint8_t const*>(previousBody.data())};
  for (auto it : VPackObjectIterator(previous, true)) {
    auto name = it.key.stringView();
    if (name == StaticStrings::RevString) {
      body.add(name, revision.toValuePair(ridBuffer));
    } else if (name == StaticStrings::Created) {
      body.add(name, VPackValue(created));
    } else if (name == "value") {
      body.add(name, VPackValue(value));
    } else {
      body.add(name, it.value);
    }
  }
  body.close();

  RocksDBKey documentKey;
  documentKey.constructDocument(physical->objectId(), documentId);
  auto entry = RocksDBValue::PrimaryIndexValue(documentId, revision);

  std::unique_ptr<rocksdb::Transaction> trx{
      engine.db()->BeginTransaction(rocksdb::WriteOptions{})};
  ASSERT_TRUE(trx->Put(documentsCf, documentKey.string(),
                       rocksdb::Slice(body.slice().startAs<char>(),
                                      body.slice().byteSize()))
                  .ok());
  ASSERT_TRUE(trx->Put(indexCf, indexKey.string(), entry.string()).ok());
  ASSERT_TRUE(trx->SetCommitTimestamp(created).ok());
  ASSERT_TRUE(trx->Commit().ok());
}

}  // namespace

// ================ point-in-time reads ================

TEST_F(TimeTravelStorageEngineDocumentTest,
       ReadBeforeCreatedTimestampFindsNothing) {
  constexpr uint64_t T1 = 1000;
  ASSERT_TRUE(insertR(createdDoc("k1", 1, T1).slice()).ok());

  auto before = readAt("k1", T1 - 1);
  ASSERT_TRUE(before.fail());
  EXPECT_EQ(before.errorNumber(), TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND)
      << before.errorMessage();

  auto at = readAt("k1", T1);
  ASSERT_TRUE(at.ok()) << at.errorMessage();
  EXPECT_EQ(at.slice().get("value").getNumber<int>(), 1);
}

// Keys have independent timelines: a read at T1 sees the key created then and
// not the one created later, even though both live in the same collection.
TEST_F(TimeTravelStorageEngineDocumentTest, ReadAtTimestampHidesLaterVersions) {
  constexpr uint64_t T1 = 1000;
  constexpr uint64_t T2 = 2000;
  ASSERT_TRUE(insertR(createdDoc("k1", 1, T1).slice()).ok());
  ASSERT_TRUE(insertR(createdDoc("k2", 2, T2).slice()).ok());

  EXPECT_TRUE(readAt("k1", T1).ok());
  EXPECT_TRUE(readAt("k2", T1).fail());

  EXPECT_TRUE(readAt("k1", T2).ok());
  EXPECT_TRUE(readAt("k2", T2).ok());
}

// The "no read timestamp == current state" invariant. With several versions of
// the key on disk, a read without a timestamp must still resolve to the newest
// one - exactly what the same read on a non-time-travel collection returns.
TEST_F(TimeTravelStorageEngineDocumentTest,
       ReadWithoutTimestampSeesNewestVersion) {
  constexpr uint64_t T1 = 1000;
  constexpr uint64_t T2 = 2000;
  constexpr uint64_t T3 = 3000;

  ASSERT_TRUE(insertR(createdDoc("k1", 1, T1).slice()).ok());
  writeRawVersion(engine(), *_collection, "k1", 2, T2);
  writeRawVersion(engine(), *_collection, "k1", 3, T3);

  auto res = read("k1");
  ASSERT_TRUE(res.ok()) << res.errorMessage();
  EXPECT_EQ(res.slice().get("value").getNumber<int>(), 3);
  EXPECT_EQ(res.slice().get(StaticStrings::Created).getNumber<uint64_t>(), T3);
  EXPECT_TRUE(res.slice().get(StaticStrings::Expired).isNull());
}

// A read timestamp selects a point in the past, which cannot be reconciled with
// writing at the (later) commit timestamp - so it is rejected up front.
TEST_F(TimeTravelStorageEngineDocumentTest,
       ReadTimestampRejectedOnWriteTransaction) {
  transaction::Options trxOptions;
  trxOptions.readTimestamp = 1000;

  SingleCollectionTransaction trx{context(), *_collection,
                                  AccessMode::Type::WRITE, trxOptions};
  auto res = trx.begin();
  ASSERT_TRUE(res.fail());
  EXPECT_EQ(res.errorNumber(), TRI_ERROR_BAD_PARAMETER) << res.errorMessage();
}

// Reads that serve a transaction's own uncommitted writes go through a
// different RocksDBMethods implementation than the read-only ones, and must
// supply the current-state timestamp just the same.
TEST_F(TimeTravelStorageEngineDocumentTest,
       ReadOwnWritesInsideWriteTransaction) {
  SingleCollectionTransaction trx{context(), *_collection,
                                  AccessMode::Type::WRITE};
  trx.addHint(transaction::Hints::Hint::GLOBAL_MANAGED);
  ASSERT_TRUE(trx.begin().ok());
  OperationOptions options;
  ASSERT_TRUE(trx.insert(_collection->name(), createdDoc("k1", 1, 1000).slice(),
                         options)
                  .ok());

  auto lookup = keyOnly("k1");
  auto res = trx.document(_collection->name(), lookup.slice(), options);
  ASSERT_TRUE(res.ok()) << res.errorMessage();
  EXPECT_EQ(res.slice().get("value").getNumber<int>(), 1);

  std::ignore = trx.abort();
}

// A plain collection keeps no history, so a read timestamp has nothing to
// select from: it is ignored and the read answers from the current state.
TEST_F(StorageEngineDocumentTest, ReadTimestampIsInertOnPlainCollection) {
  ASSERT_TRUE(insertR(keyed("k1", 1).slice()).ok());

  auto res = readAt("k1", 1);
  ASSERT_TRUE(res.ok()) << res.errorMessage();
  EXPECT_EQ(res.slice().get("value").getNumber<int>(), 1);
}

// The core of point-in-time reading: one _key with three versions, each read
// back at its own timestamp. Every read must resolve the primary index to a
// *different* LocalDocumentId and materialize the matching document body.
TEST_F(TimeTravelStorageEngineDocumentTest,
       ReadResolvesTheVersionLiveAtEachTimestamp) {
  constexpr uint64_t T1 = 1000;
  constexpr uint64_t T2 = 2000;
  constexpr uint64_t T3 = 3000;

  ASSERT_TRUE(insertR(createdDoc("k1", 1, T1).slice()).ok());
  writeRawVersion(engine(), *_collection, "k1", 2, T2);
  writeRawVersion(engine(), *_collection, "k1", 3, T3);

  auto versionAt = [&](uint64_t ts) {
    auto res = readAt("k1", ts);
    EXPECT_TRUE(res.ok()) << "ts=" << ts << ": " << res.errorMessage();
    return res;
  };

  // each timestamp sees the version created then, with its own _created stamp
  auto v1 = versionAt(T1);
  EXPECT_EQ(v1.slice().get("value").getNumber<int>(), 1);
  EXPECT_EQ(v1.slice().get(StaticStrings::Created).getNumber<uint64_t>(), T1);

  auto v2 = versionAt(T2);
  EXPECT_EQ(v2.slice().get("value").getNumber<int>(), 2);
  EXPECT_EQ(v2.slice().get(StaticStrings::Created).getNumber<uint64_t>(), T2);

  auto v3 = versionAt(T3);
  EXPECT_EQ(v3.slice().get("value").getNumber<int>(), 3);
  EXPECT_EQ(v3.slice().get(StaticStrings::Created).getNumber<uint64_t>(), T3);

  // a version stays live until the next one supersedes it
  EXPECT_EQ(versionAt(T2 - 1).slice().get("value").getNumber<int>(), 1);
  EXPECT_EQ(versionAt(T3 - 1).slice().get("value").getNumber<int>(), 2);
  EXPECT_EQ(versionAt(T3 + 1000).slice().get("value").getNumber<int>(), 3);

  // nothing at all exists before the first version
  EXPECT_TRUE(readAt("k1", T1 - 1).fail());

  // every version resolved to its own document, none of them shared
  EXPECT_NE(v1.slice().get(StaticStrings::RevString).stringView(),
            v2.slice().get(StaticStrings::RevString).stringView());
  EXPECT_NE(v2.slice().get(StaticStrings::RevString).stringView(),
            v3.slice().get(StaticStrings::RevString).stringView());
}
