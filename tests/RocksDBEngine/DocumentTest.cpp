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

#include "RocksDBEngine/StorageEngineDocumentTest.h"

#include "Utils/OperationResult.h"

using namespace arangodb;
using namespace arangodb::tests;

TEST_F(StorageEngineDocumentTest, InsertAndReadDocument) {
  LOG_DEVEL << "starting rocksdb test";
  ASSERT_TRUE(insert(keyed("key1", 1).slice()).ok());
  LOG_DEVEL << "insert finished";

  auto readRes = read("key1");
  ASSERT_TRUE(readRes.ok()) << readRes.errorMessage();
  EXPECT_EQ(readRes.slice().get("value").getInt(), 1);
  LOG_DEVEL << "test finished";
}

TEST_F(StorageEngineDocumentTest, UpdateDocument) {
  ASSERT_TRUE(insert(keyed("key1", 1).slice()).ok());
  ASSERT_TRUE(update(keyed("key1", 2).slice()).ok());

  auto readRes = read("key1");
  ASSERT_TRUE(readRes.ok()) << readRes.errorMessage();
  EXPECT_EQ(readRes.slice().get("value").getInt(), 2);
}

TEST_F(StorageEngineDocumentTest, RemoveDocument) {
  ASSERT_TRUE(insert(keyed("key1", 1).slice()).ok());
  ASSERT_TRUE(remove(keyOnly("key1").slice()).ok());

  auto readRes = read("key1");
  EXPECT_TRUE(readRes.fail());
  EXPECT_EQ(readRes.errorNumber(), TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND);
}
