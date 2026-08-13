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
#include "RocksDBEngine/RocksDBPrimaryIndex.h"
#include "RocksDBEngine/StorageEngineDataTest.h"
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
  // ts_sz = 8: the comparator carries an 8-byte timestamp.
  EXPECT_EQ(ttCf->GetComparator()->timestamp_size(), 8u);

  // The non-UDT primary index remains a plain comparator (no timestamp),
  // confirming the UDT comparator is scoped to the TT family only.
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
