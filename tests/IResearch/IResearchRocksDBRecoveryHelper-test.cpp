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
/// @author Andrei Lobov
////////////////////////////////////////////////////////////////////////////////

#include "IResearch/IResearchRocksDBRecoveryHelper.h"
#include "gtest/gtest.h"

TEST(IResearchRocksDBRecoveryHelperIndexId, comparison_equal) {
  arangodb::iresearch::IResearchRocksDBRecoveryHelper::IndexId id(
      1, arangodb::DataSourceId(2), arangodb::IndexId(3));
  arangodb::iresearch::IResearchRocksDBRecoveryHelper::IndexId id2(
      1, arangodb::DataSourceId(2), arangodb::IndexId(3));
  ASSERT_FALSE(id < id2);
  ASSERT_FALSE(id2 < id);
}

TEST(IResearchRocksDBRecoveryHelperIndexId, comparison_less_db) {
  arangodb::iresearch::IResearchRocksDBRecoveryHelper::IndexId id(
      1, arangodb::DataSourceId(2), arangodb::IndexId(3));
  arangodb::iresearch::IResearchRocksDBRecoveryHelper::IndexId id2(
      2, arangodb::DataSourceId(2), arangodb::IndexId(3));
  ASSERT_TRUE(id < id2);
  ASSERT_FALSE(id2 < id);
}

TEST(IResearchRocksDBRecoveryHelperIndexId, comparison_less_db2) {
  arangodb::iresearch::IResearchRocksDBRecoveryHelper::IndexId id(
      1, arangodb::DataSourceId(3), arangodb::IndexId(3));
  arangodb::iresearch::IResearchRocksDBRecoveryHelper::IndexId id2(
      2, arangodb::DataSourceId(2), arangodb::IndexId(3));
  ASSERT_TRUE(id < id2);
  ASSERT_FALSE(id2 < id);
}

TEST(IResearchRocksDBRecoveryHelperIndexId, comparison_less_db3) {
  arangodb::iresearch::IResearchRocksDBRecoveryHelper::IndexId id(
      1, arangodb::DataSourceId(3), arangodb::IndexId(4));
  arangodb::iresearch::IResearchRocksDBRecoveryHelper::IndexId id2(
      2, arangodb::DataSourceId(2), arangodb::IndexId(3));
  ASSERT_TRUE(id < id2);
  ASSERT_FALSE(id2 < id);
}

TEST(IResearchRocksDBRecoveryHelperIndexId, comparison_less_db4) {
  arangodb::iresearch::IResearchRocksDBRecoveryHelper::IndexId id(
      1, arangodb::DataSourceId(2), arangodb::IndexId(4));
  arangodb::iresearch::IResearchRocksDBRecoveryHelper::IndexId id2(
      2, arangodb::DataSourceId(2), arangodb::IndexId(3));
  ASSERT_TRUE(id < id2);
  ASSERT_FALSE(id2 < id);
}

TEST(IResearchRocksDBRecoveryHelperIndexId, comparison_less_cid) {
  arangodb::iresearch::IResearchRocksDBRecoveryHelper::IndexId id(
      1, arangodb::DataSourceId(2), arangodb::IndexId(3));
  arangodb::iresearch::IResearchRocksDBRecoveryHelper::IndexId id2(
      1, arangodb::DataSourceId(3), arangodb::IndexId(3));
  ASSERT_TRUE(id < id2);
  ASSERT_FALSE(id2 < id);
}

TEST(IResearchRocksDBRecoveryHelperIndexId, comparison_less_cid2) {
  arangodb::iresearch::IResearchRocksDBRecoveryHelper::IndexId id(
      1, arangodb::DataSourceId(2), arangodb::IndexId(4));
  arangodb::iresearch::IResearchRocksDBRecoveryHelper::IndexId id2(
      1, arangodb::DataSourceId(3), arangodb::IndexId(3));
  ASSERT_TRUE(id < id2);
  ASSERT_FALSE(id2 < id);
}


TEST(IResearchRocksDBRecoveryHelperIndexId, comparison_less_iid) {
  arangodb::iresearch::IResearchRocksDBRecoveryHelper::IndexId id(
      1, arangodb::DataSourceId(2), arangodb::IndexId(3));
  arangodb::iresearch::IResearchRocksDBRecoveryHelper::IndexId id2(
      1, arangodb::DataSourceId(2), arangodb::IndexId(4));
  ASSERT_TRUE(id < id2);
  ASSERT_FALSE(id2 < id);
}
