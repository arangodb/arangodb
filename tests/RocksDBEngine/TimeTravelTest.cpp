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

#include <limits>

#include <rocksdb/comparator.h>
#include <rocksdb/db.h>

#include <velocypack/Builder.h>
#include <velocypack/Slice.h>

#include "Basics/StaticStrings.h"
#include "RocksDBEngine/RocksDBCollection.h"
#include "RocksDBEngine/RocksDBColumnFamilyManager.h"
#include "RocksDBEngine/RocksDBPrimaryIndex.h"
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

// ================ point-in-time reads ================

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

  ASSERT_TRUE(insertR(createdDoc("k1", 1, T1).slice()).ok());
  ASSERT_TRUE(updateR(createdDoc("k1", 2, T2).slice()).ok());

  auto res = read("k1");
  ASSERT_TRUE(res.ok()) << res.errorMessage();
  EXPECT_EQ(res.slice().get("value").getNumber<int>(), 2);
  EXPECT_EQ(res.slice().get(StaticStrings::Created).getNumber<uint64_t>(), T2);
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
// *different* LocalDocumentId and materialize the matching document body,
// validity interval included.
TEST_F(TimeTravelStorageEngineDocumentTest,
       ReadResolvesTheVersionLiveAtEachTimestamp) {
  constexpr uint64_t T1 = 1000;
  constexpr uint64_t T2 = 2000;
  constexpr uint64_t T3 = 3000;

  ASSERT_TRUE(insertR(createdDoc("k1", 1, T1).slice()).ok());
  ASSERT_TRUE(updateR(createdDoc("k1", 2, T2).slice()).ok());
  ASSERT_TRUE(updateR(createdDoc("k1", 3, T3).slice()).ok());

  auto versionAt = [&](uint64_t ts) {
    auto res = readAt("k1", ts);
    EXPECT_TRUE(res.ok()) << "ts=" << ts << ": " << res.errorMessage();
    return res;
  };

  // each timestamp sees the version live then, carrying the validity interval
  // it was written with: created at its own timestamp, expired by its successor
  auto v1 = versionAt(T1);
  EXPECT_EQ(v1.slice().get("value").getNumber<int>(), 1);
  EXPECT_EQ(v1.slice().get(StaticStrings::Created).getNumber<uint64_t>(), T1);
  EXPECT_EQ(v1.slice().get(StaticStrings::Expired).getNumber<uint64_t>(), T2);

  auto v2 = versionAt(T2);
  EXPECT_EQ(v2.slice().get("value").getNumber<int>(), 2);
  EXPECT_EQ(v2.slice().get(StaticStrings::Created).getNumber<uint64_t>(), T2);
  EXPECT_EQ(v2.slice().get(StaticStrings::Expired).getNumber<uint64_t>(), T3);

  auto v3 = versionAt(T3);
  EXPECT_EQ(v3.slice().get("value").getNumber<int>(), 3);
  EXPECT_EQ(v3.slice().get(StaticStrings::Created).getNumber<uint64_t>(), T3);
  EXPECT_TRUE(v3.slice().get(StaticStrings::Expired).isNull());

  // a version stays live until the next one supersedes it
  EXPECT_EQ(versionAt(T2 - 1).slice().get("value").getNumber<int>(), 1);
  EXPECT_EQ(versionAt(T3 - 1).slice().get("value").getNumber<int>(), 2);
  EXPECT_EQ(versionAt(T3 + 1000).slice().get("value").getNumber<int>(), 3);

  // nothing at all exists before the first version
  auto before = readAt("k1", T1 - 1);
  ASSERT_TRUE(before.fail());
  EXPECT_EQ(before.errorNumber(), TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND)
      << before.errorMessage();

  // every version resolved to its own document, none of them shared
  EXPECT_NE(v1.slice().get(StaticStrings::RevString).stringView(),
            v2.slice().get(StaticStrings::RevString).stringView());
  EXPECT_NE(v2.slice().get(StaticStrings::RevString).stringView(),
            v3.slice().get(StaticStrings::RevString).stringView());
}

// ================ update / replace with custom _created ================

// Replace versions exactly like update (see
// ReadResolvesTheVersionLiveAtEachTimestamp); only the merge semantics for the
// body differ, so it needs its own proof that the previous version survives.
TEST_F(TimeTravelStorageEngineDocumentTest,
       ReplaceRetainsPreviousVersionAndStampsExpired) {
  constexpr uint64_t T1 = 1000;
  constexpr uint64_t T2 = 2000;

  ASSERT_TRUE(insertR(createdDoc("k1", 1, T1).slice()).ok());
  auto rep = replaceR(createdDoc("k1", 2, T2).slice());
  ASSERT_TRUE(rep.ok()) << rep.errorMessage();

  auto before = readAt("k1", T1);
  ASSERT_TRUE(before.ok()) << before.errorMessage();
  EXPECT_EQ(before.slice().get("value").getNumber<int>(), 1);
  EXPECT_EQ(before.slice().get(StaticStrings::Expired).getNumber<uint64_t>(),
            T2);

  auto after = readAt("k1", T2);
  ASSERT_TRUE(after.ok()) << after.errorMessage();
  EXPECT_EQ(after.slice().get("value").getNumber<int>(), 2);
  EXPECT_TRUE(after.slice().get(StaticStrings::Expired).isNull());
}

// The new version must not inherit the previous version's _created, which the
// merge would otherwise carry over as an ordinary attribute.
TEST_F(TimeTravelStorageEngineDocumentTest, UpdateWithoutCreatedIsRejected) {
  ASSERT_TRUE(insertR(createdDoc("k1", 1, 1000).slice()).ok());

  auto upd = updateR(keyed("k1", 2).slice());
  ASSERT_TRUE(upd.fail());
  EXPECT_EQ(upd.errorNumber(), TRI_ERROR_BAD_PARAMETER) << upd.errorMessage();
  // rejected while building the new version, not by rocksdb at commit time
  EXPECT_NE(upd.errorMessage().find(StaticStrings::Created), std::string::npos)
      << upd.errorMessage();

  // the failed update left the existing version untouched
  auto current = read("k1");
  ASSERT_TRUE(current.ok()) << current.errorMessage();
  EXPECT_EQ(current.slice().get("value").getNumber<int>(), 1);
  EXPECT_TRUE(current.slice().get(StaticStrings::Expired).isNull());
}

TEST_F(TimeTravelStorageEngineDocumentTest, ReplaceWithoutCreatedIsRejected) {
  ASSERT_TRUE(insertR(createdDoc("k1", 1, 1000).slice()).ok());

  auto rep = replaceR(keyed("k1", 2).slice());
  ASSERT_TRUE(rep.fail());
  EXPECT_EQ(rep.errorNumber(), TRI_ERROR_BAD_PARAMETER) << rep.errorMessage();
  EXPECT_NE(rep.errorMessage().find(StaticStrings::Created), std::string::npos)
      << rep.errorMessage();
}

// A user-supplied _expired on an update is ignored: the version being written
// is by definition still live.
TEST_F(TimeTravelStorageEngineDocumentTest, UpdateIgnoresUserSuppliedExpired) {
  ASSERT_TRUE(insertR(createdDoc("k1", 1, 1000).slice()).ok());

  VPackBuilder b;
  b.openObject();
  b.add(StaticStrings::KeyString, VPackValue("k1"));
  b.add("value", VPackValue(2));
  b.add(StaticStrings::Created, VPackValue(2000));
  b.add(StaticStrings::Expired, VPackValue(9999));
  b.close();
  ASSERT_TRUE(updateR(b.slice()).ok());

  auto current = read("k1");
  ASSERT_TRUE(current.ok()) << current.errorMessage();
  EXPECT_TRUE(current.slice().get(StaticStrings::Expired).isNull())
      << current.slice().toJson();
}

// An update replaces the current version rather than adding a document, so the
// current-state count is unchanged.
TEST_F(TimeTravelStorageEngineDocumentTest,
       UpdateLeavesDocumentCountUnchanged) {
  ASSERT_TRUE(insertR(createdDoc("k1", 1, 1000).slice()).ok());
  ASSERT_EQ(count(), 1u);

  ASSERT_TRUE(updateR(createdDoc("k1", 2, 2000).slice()).ok());
  EXPECT_EQ(count(), 1u);
}

// A non-time-travel update must keep physically deleting the old version.
TEST_F(StorageEngineDocumentTest, NonTimeTravelUpdateDropsPreviousVersion) {
  ASSERT_TRUE(insertR(keyed("k1", 1).slice()).ok());
  ASSERT_TRUE(updateR(keyed("k1", 2).slice()).ok());

  auto current = read("k1");
  ASSERT_TRUE(current.ok()) << current.errorMessage();
  EXPECT_EQ(current.slice().get("value").getNumber<int>(), 2);
  EXPECT_TRUE(current.slice().get(StaticStrings::Expired).isNone())
      << current.slice().toJson();
  EXPECT_EQ(count(), 1u);
}

// ================ write-write conflict detection ================

// A transaction that pinned its snapshot before another transaction modified a
// key must not be allowed to update that key afterwards: it would compute the
// new version from the version it saw at its snapshot and silently drop the
// other transaction's write. Single-operation transactions are safe by
// construction (they lock before taking a snapshot), so the race needs a
// multi-operation transaction whose snapshot is already pinned.
TEST_F(TimeTravelStorageEngineDocumentTest,
       UpdateFromAStaleSnapshotIsRejected) {
  constexpr uint64_t T1 = 1000;
  constexpr uint64_t T2 = 2000;
  constexpr uint64_t T3 = 3000;

  ASSERT_TRUE(insertR(createdDoc("k1", 1, T1).slice()).ok());
  ASSERT_TRUE(insertR(createdDoc("k2", 1, T1).slice()).ok());

  SingleCollectionTransaction slow{context(), *_collection,
                                   AccessMode::Type::WRITE};
  slow.addHint(transaction::Hints::Hint::GLOBAL_MANAGED);
  ASSERT_TRUE(slow.begin().ok());
  OperationOptions options;

  // touching an unrelated key pins `slow`'s snapshot before the racing write
  ASSERT_TRUE(
      slow.update(_collection->name(), createdDoc("k2", 2, T3).slice(), options)
          .ok());

  // a second transaction updates k1 at an *earlier* timestamp and commits, so
  // what follows can only be a concurrency conflict and never a rejected
  // backdated timestamp
  ASSERT_TRUE(updateR(createdDoc("k1", 2, T2).slice()).ok());

  // `slow` would now build its new version of k1 from the version it saw at
  // its snapshot, losing the update above
  auto res = slow.update(_collection->name(), createdDoc("k1", 3, T3).slice(),
                         options);
  EXPECT_TRUE(res.fail());
  EXPECT_EQ(res.errorNumber(), TRI_ERROR_ARANGO_CONFLICT) << res.errorMessage();

  std::ignore = slow.finish(res.result);

  // the committed update survived
  auto current = read("k1");
  ASSERT_TRUE(current.ok()) << current.errorMessage();
  EXPECT_EQ(current.slice().get("value").getNumber<int>(), 2);
}

// Write-write validation reads at `_created - 1`, so a zero timestamp has no
// instant to validate against and is rejected along with negative values.
TEST_F(TimeTravelStorageEngineDocumentTest, InsertWithZeroCreatedIsRejected) {
  auto ins = insertR(createdDoc("k1", 1, 0).slice());
  ASSERT_TRUE(ins.fail());
  EXPECT_EQ(ins.errorNumber(), TRI_ERROR_BAD_PARAMETER) << ins.errorMessage();
  EXPECT_TRUE(read("k1").fail());
}

TEST_F(TimeTravelStorageEngineDocumentTest,
       InsertWithNegativeCreatedIsRejected) {
  VPackBuilder b;
  b.openObject();
  b.add(StaticStrings::KeyString, VPackValue("k1"));
  b.add(StaticStrings::Created, VPackValue(-1));
  b.close();

  auto ins = insertR(b.slice());
  ASSERT_TRUE(ins.fail());
  EXPECT_EQ(ins.errorNumber(), TRI_ERROR_BAD_PARAMETER) << ins.errorMessage();
  EXPECT_TRUE(read("k1").fail());
}

// A fractional _created cannot be a UDT timestamp; truncating it silently would
// put the version at a different instant than the document claims.
TEST_F(TimeTravelStorageEngineDocumentTest,
       InsertWithFractionalCreatedIsRejected) {
  VPackBuilder b;
  b.openObject();
  b.add(StaticStrings::KeyString, VPackValue("k1"));
  b.add(StaticStrings::Created, VPackValue(1000.5));
  b.close();

  auto ins = insertR(b.slice());
  ASSERT_TRUE(ins.fail());
  EXPECT_EQ(ins.errorNumber(), TRI_ERROR_BAD_PARAMETER) << ins.errorMessage();
}

// Remove is not supported on time-travel collections yet (COR-653): it has no
// way to say which timestamp it removes at. It must fail rather than silently
// destroy history.
TEST_F(TimeTravelStorageEngineDocumentTest, RemoveIsNotSupportedYet) {
  ASSERT_TRUE(insertR(createdDoc("k1", 1, 1000).slice()).ok());

  auto rem = removeR(keyOnly("k1").slice());
  EXPECT_TRUE(rem.fail()) << "remove must not silently drop history";

  // the document is still there, unexpired
  auto current = read("k1");
  ASSERT_TRUE(current.ok()) << current.errorMessage();
  EXPECT_TRUE(current.slice().get(StaticStrings::Expired).isNull());
}

// A version chain must move forward in time: a new version created at or before
// the current one would give the superseded version an _expired that precedes
// its own _created. The user supplies these timestamps, so this is bad input
// and must be reported as such.
TEST_F(TimeTravelStorageEngineDocumentTest, UpdateWithOlderCreatedIsRejected) {
  ASSERT_TRUE(insertR(createdDoc("k1", 1, 1000).slice()).ok());
  ASSERT_TRUE(updateR(createdDoc("k1", 2, 3000).slice()).ok());

  auto upd = updateR(createdDoc("k1", 3, 2000).slice());
  ASSERT_TRUE(upd.fail());
  EXPECT_EQ(upd.errorNumber(), TRI_ERROR_BAD_PARAMETER) << upd.errorMessage();
  // the message names both timestamps, which also proves the current version's
  // was read back out of the primary index entry
  EXPECT_NE(upd.errorMessage().find(StaticStrings::Created), std::string::npos)
      << upd.errorMessage();
  EXPECT_NE(upd.errorMessage().find("3000"), std::string::npos)
      << upd.errorMessage();
  EXPECT_NE(upd.errorMessage().find("2000"), std::string::npos)
      << upd.errorMessage();
}

TEST_F(TimeTravelStorageEngineDocumentTest, UpdateWithEqualCreatedIsRejected) {
  ASSERT_TRUE(insertR(createdDoc("k1", 1, 1000).slice()).ok());

  auto upd = updateR(createdDoc("k1", 2, 1000).slice());
  ASSERT_TRUE(upd.fail());
  EXPECT_EQ(upd.errorNumber(), TRI_ERROR_BAD_PARAMETER) << upd.errorMessage();
}

TEST_F(TimeTravelStorageEngineDocumentTest,
       UpdateWithCreatedBeforeFirstVersionIsRejected) {
  ASSERT_TRUE(insertR(createdDoc("k1", 1, 2000).slice()).ok());

  auto upd = updateR(createdDoc("k1", 2, 1000).slice());
  ASSERT_TRUE(upd.fail());
  EXPECT_EQ(upd.errorNumber(), TRI_ERROR_BAD_PARAMETER) << upd.errorMessage();
}

// The maximum timestamp is the reserved "current state" value: rocksdb reads a
// commit timestamp of that value back as "none was assigned" and fails the
// commit, so it has to be rejected up front like 0 is.
TEST_F(TimeTravelStorageEngineDocumentTest, InsertWithMaxCreatedIsRejected) {
  auto doc = createdDoc("k1", 1, std::numeric_limits<uint64_t>::max());
  auto ins = insertR(doc.slice());
  ASSERT_TRUE(ins.fail());
  EXPECT_EQ(ins.errorNumber(), TRI_ERROR_BAD_PARAMETER) << ins.errorMessage();
  // rejected by our own validation, not by rocksdb failing the commit with
  // "Must assign a commit timestamp"
  EXPECT_NE(ins.errorMessage().find(StaticStrings::Created), std::string::npos)
      << ins.errorMessage();
  EXPECT_TRUE(read("k1").fail());
}

// Inserting over an existing key with a backdated _created trips the same key
// lock as a backdated update, and must be diagnosed the same way rather than
// reported as a write-write conflict the caller could retry.
TEST_F(TimeTravelStorageEngineDocumentTest,
       InsertOverExistingKeyWithOlderCreatedIsRejected) {
  ASSERT_TRUE(insertR(createdDoc("k1", 1, 1000).slice()).ok());

  auto ins = insertR(createdDoc("k1", 2, 500).slice());
  ASSERT_TRUE(ins.fail());
  EXPECT_EQ(ins.errorNumber(), TRI_ERROR_BAD_PARAMETER) << ins.errorMessage();
  EXPECT_NE(ins.errorMessage().find("1000"), std::string::npos)
      << ins.errorMessage();

  // the existing version is untouched
  auto current = read("k1");
  ASSERT_TRUE(current.ok()) << current.errorMessage();
  EXPECT_EQ(current.slice().get("value").getNumber<int>(), 1);
}

// A *newer* timestamp on an existing key is still a plain duplicate key: the
// timestamp is fine, the key is taken.
TEST_F(TimeTravelStorageEngineDocumentTest,
       InsertOverExistingKeyWithNewerCreatedIsDuplicate) {
  ASSERT_TRUE(insertR(createdDoc("k1", 1, 1000).slice()).ok());

  auto ins = insertR(createdDoc("k1", 2, 2000).slice());
  ASSERT_TRUE(ins.fail());
  EXPECT_EQ(ins.errorNumber(), TRI_ERROR_ARANGO_UNIQUE_CONSTRAINT_VIOLATED)
      << ins.errorMessage();
}
