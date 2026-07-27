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

#include "Basics/ResultAssertions.h"
#include "Basics/StaticStrings.h"
#include "Transaction/CountCache.h"
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
#include <velocypack/String.h>

#include <memory>
#include <string_view>
#include <tuple>
#include <utility>

namespace arangodb::tests {

// Document operations run through a SingleCollectionTransaction using the
// synchronous (deprecated) Methods variants on purpose: this minimal setup has
// no Scheduler, and the synchronous API explicitly executes inline rather than
// posting to one. Most tests use the committed-single-op helpers (insertR /
// updateR / read): each write commits in its own transaction and is verified by
// reading it back in a separate one, so the default pattern exercises persisted
// visibility. Tests that need multi-op or read-own-writes behaviour instead
// drive a SingleCollectionTransaction directly (see the transaction-lifecycle
// group in DocumentTest.cpp).
class StorageEngineDocumentTest : public StorageEngineDataTest {
 protected:
  void SetUp() override {
    StorageEngineDataTest::SetUp();
    _database = makeDatabase("testDatabase", 42);
    _collection = makeCollection(*_database, "testCollection");
  }

  static VPackString keyed(std::string_view key, int value) {
    VPackBuilder b;
    b.openObject();
    b.add(StaticStrings::KeyString, VPackValue(key));
    b.add("value", VPackValue(value));
    b.close();
    return VPackString{b.slice()};
  }

  static VPackString keyOnly(std::string_view key) {
    VPackBuilder b;
    b.openObject();
    b.add(StaticStrings::KeyString, VPackValue(key));
    b.close();
    return VPackString{b.slice()};
  }

  // A selector carrying a _key and an explicit _rev, for precondition checks
  // (used with options.ignoreRevs = false).
  static VPackString keyedRev(std::string_view key, std::string_view rev) {
    VPackBuilder b;
    b.openObject();
    b.add(StaticStrings::KeyString, VPackValue(key));
    b.add(StaticStrings::RevString, VPackValue(rev));
    b.close();
    return VPackString{b.slice()};
  }

  // A selector carrying _key, _rev and a value, for a precondition-checked
  // update/replace that also changes the document.
  static VPackString keyedRev(std::string_view key, std::string_view rev,
                              int value) {
    VPackBuilder b;
    b.openObject();
    b.add(StaticStrings::KeyString, VPackValue(key));
    b.add(StaticStrings::RevString, VPackValue(rev));
    b.add("value", VPackValue(value));
    b.close();
    return VPackString{b.slice()};
  }

  static std::string revOf(OperationResult const& res) {
    return res.slice().get(StaticStrings::RevString).copyString();
  }

  static std::string keyOf(OperationResult const& res) {
    return res.slice().get(StaticStrings::KeyString).copyString();
  }

  auto context() {
    return transaction::StandaloneContext::create(*_database, _origin);
  }

  // Applies a write operation in its own transaction and commits it, returning
  // the full OperationResult. The result body (slice) stays valid after the
  // commit because OperationResult::buffer is a shared_ptr. On a failed
  // begin/commit the returned OperationResult carries that failure.
  template<typename Op>
  OperationResult writeAndCommitResult(Op&& op, OperationOptions options = {}) {
    SingleCollectionTransaction trx{context(), *_collection,
                                    AccessMode::Type::WRITE};
    if (auto res = trx.begin(); res.fail()) {
      return OperationResult{res, options};
    }
    auto opRes = std::forward<Op>(op)(trx, options);
    auto finishRes = trx.finish(opRes.result);
    if (opRes.ok() && finishRes.fail()) {
      return OperationResult{finishRes, options};
    }
    return opRes;
  }

  OperationResult insertR(VPackSlice doc, OperationOptions options = {}) {
    return writeAndCommitResult(
        [&](auto& trx, auto& o) {
          return trx.insert(_collection->name(), doc, o);
        },
        options);
  }

  OperationResult updateR(VPackSlice doc, OperationOptions options = {}) {
    return writeAndCommitResult(
        [&](auto& trx, auto& o) {
          return trx.update(_collection->name(), doc, o);
        },
        options);
  }

  OperationResult replaceR(VPackSlice doc, OperationOptions options = {}) {
    return writeAndCommitResult(
        [&](auto& trx, auto& o) {
          return trx.replace(_collection->name(), doc, o);
        },
        options);
  }

  OperationResult removeR(VPackSlice selector, OperationOptions options = {}) {
    return writeAndCommitResult(
        [&](auto& trx, auto& o) {
          return trx.remove(_collection->name(), selector, o);
        },
        options);
  }

  // Result-only convenience wrappers retained for the CRUD/visibility tests
  // that only assert on ok(); they defer to the result-returning helpers above.
  Result insert(VPackSlice doc) { return insertR(doc).result; }
  Result update(VPackSlice doc) { return updateR(doc).result; }
  Result remove(VPackSlice selector) { return removeR(selector).result; }

  // Reads a document by key in its own read-only transaction, committing before
  // returning so the result slice stays valid for the caller.
  OperationResult read(std::string_view key) {
    auto lookup = keyOnly(key);
    SingleCollectionTransaction trx{context(), *_collection,
                                    AccessMode::Type::READ};
    EXPECT_TRUE(IsOk(trx.begin()));
    OperationOptions options;
    auto res = trx.document(_collection->name(), lookup.slice(), options);
    std::ignore = trx.finish(res.result);
    return res;
  }

  // Number of documents in the collection, counted in its own transaction.
  uint64_t count() {
    SingleCollectionTransaction trx{context(), *_collection,
                                    AccessMode::Type::READ};
    EXPECT_TRUE(IsOk(trx.begin()));
    OperationOptions options;
    auto res = trx.count(_collection->name(), transaction::CountType::kNormal,
                         options);
    std::ignore = trx.finish(res.result);
    EXPECT_TRUE(IsOk(res));
    return res.slice().getNumber<uint64_t>();
  }

  // Remove all documents in the collection, committing in its own transaction.
  Result truncate() {
    return writeAndCommitResult([&](auto& trx, auto& o) {
             return trx.truncate(_collection->name(), o);
           })
        .result;
  }

  transaction::OperationOriginInternal _origin{"unit test"};
  std::unique_ptr<Database> _database;
  std::shared_ptr<LogicalCollection> _collection;
};

}  // namespace arangodb::tests
