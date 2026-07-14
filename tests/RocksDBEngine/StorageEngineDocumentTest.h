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

#pragma once

#include "RocksDBEngine/StorageEngineDataTest.h"

#include "Basics/StaticStrings.h"
#include "Transaction/OperationOrigin.h"
#include "Transaction/StandaloneContext.h"
#include "Utils/OperationOptions.h"
#include "Utils/OperationResult.h"
#include "Utils/SingleCollectionTransaction.h"
#include "VocBase/AccessMode.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/vocbase.h"

#include <velocypack/Builder.h>
#include <velocypack/Slice.h>

#include <memory>
#include <string_view>
#include <tuple>
#include <utility>

namespace arangodb::tests {

// Document operations run through a SingleCollectionTransaction using the
// synchronous (deprecated) Methods variants on purpose: this minimal setup has
// no Scheduler, and the synchronous API explicitly executes inline rather than
// posting to one. Each write is committed in its own transaction and verified
// by reading it back in a separate transaction, so the tests exercise persisted
// visibility rather than read-your-own-uncommitted-writes.
class StorageEngineDocumentTest : public StorageEngineDataTest {
 protected:
  void SetUp() override {
    LOG_DEVEL << "SetUp";
    StorageEngineDataTest::SetUp();
    _database = makeDatabase("testDatabase", 42);
    _collection = makeCollection(*_database, "testCollection");
    LOG_DEVEL << "SetUp finished";
  }

  static VPackBuilder keyed(std::string_view key, int value) {
    VPackBuilder b;
    b.openObject();
    b.add(StaticStrings::KeyString, VPackValue(key));
    b.add("value", VPackValue(value));
    b.close();
    return b;
  }

  static VPackBuilder keyOnly(std::string_view key) {
    VPackBuilder b;
    b.openObject();
    b.add(StaticStrings::KeyString, VPackValue(key));
    b.close();
    return b;
  }

  auto context() {
    return transaction::StandaloneContext::create(*_database, _origin);
  }

  // Applies a write operation in its own transaction and commits it. Returns
  // the combined result of the operation and the commit.
  template<typename Op>
  Result writeAndCommit(Op&& op) {
    SingleCollectionTransaction trx{context(), *_collection,
                                    AccessMode::Type::WRITE};
    if (auto res = trx.begin(); res.fail()) {
      return res;
    }
    OperationOptions options;
    auto opRes = std::forward<Op>(op)(trx, options);
    auto finishRes = trx.finish(opRes.result);
    return opRes.fail() ? opRes.result : finishRes;
  }

  Result insert(VPackSlice doc) {
    return writeAndCommit([&](auto& trx, auto& options) {
      return trx.insert(_collection->name(), doc, options);
    });
  }

  Result update(VPackSlice doc) {
    return writeAndCommit([&](auto& trx, auto& options) {
      return trx.update(_collection->name(), doc, options);
    });
  }

  Result remove(VPackSlice selector) {
    return writeAndCommit([&](auto& trx, auto& options) {
      return trx.remove(_collection->name(), selector, options);
    });
  }

  // Reads a document by key in its own read-only transaction, committing before
  // returning so the result slice stays valid for the caller.
  OperationResult read(std::string_view key) {
    auto lookup = keyOnly(key);
    SingleCollectionTransaction trx{context(), *_collection,
                                    AccessMode::Type::READ};
    EXPECT_TRUE(trx.begin().ok());
    OperationOptions options;
    auto res = trx.document(_collection->name(), lookup.slice(), options);
    std::ignore = trx.finish(res.result);
    return res;
  }

  transaction::OperationOriginInternal _origin{"unit test"};
  std::unique_ptr<Database> _database;
  std::shared_ptr<LogicalCollection> _collection;
};

}  // namespace arangodb::tests
