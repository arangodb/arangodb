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

#include "Basics/ResultAssertions.h"
#include "Transaction/Hints.h"
#include "Utils/OperationResult.h"
#include "VelocypackUtils/VelocyPackStringLiteral.h"

using namespace arangodb;
using namespace arangodb::tests;
using namespace arangodb::velocypack;

// ===========================================================================
// CRUD & basic visibility
// ===========================================================================

TEST_F(StorageEngineDocumentTest, InsertDocument) {
  ASSERT_TRUE(IsOk(insert(keyed("key1", 1))));

  auto readRes = read("key1");
  ASSERT_TRUE(IsOk(readRes));
  EXPECT_EQ(readRes.slice().get("value").getInt(), 1);
}

TEST_F(StorageEngineDocumentTest, UpdateDocument) {
  ASSERT_TRUE(IsOk(insert(keyed("key1", 1))));
  ASSERT_TRUE(IsOk(update(keyed("key1", 2))));

  auto readRes = read("key1");
  ASSERT_TRUE(IsOk(readRes));
  EXPECT_EQ(readRes.slice().get("value").getInt(), 2);
}

TEST_F(StorageEngineDocumentTest, RemoveDocument) {
  ASSERT_TRUE(IsOk(insert(keyed("key1", 1))));
  ASSERT_TRUE(IsOk(remove(keyOnly("key1"))));

  EXPECT_TRUE(IsError(read("key1"), TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND));
}

// ===========================================================================
// Revisions & preconditions
// ===========================================================================

TEST_F(StorageEngineDocumentTest, UpdateAssignsNewRevision) {
  auto ins = insertR(keyed("k", 1));
  ASSERT_TRUE(IsOk(ins));
  auto upd = updateR(keyed("k", 2));
  ASSERT_TRUE(IsOk(upd));

  EXPECT_FALSE(revOf(ins).empty());
  EXPECT_FALSE(revOf(upd).empty());
  EXPECT_NE(revOf(ins), revOf(upd));
}

TEST_F(StorageEngineDocumentTest, UpdateWithMatchingRevSucceeds) {
  auto ins = insertR(keyed("k", 1));
  ASSERT_TRUE(IsOk(ins));

  OperationOptions options;
  options.ignoreRevs = false;
  auto upd = updateR(keyedRev("k", revOf(ins), 2), options);
  ASSERT_TRUE(IsOk(upd));

  auto rd = read("k");
  ASSERT_TRUE(IsOk(rd));
  EXPECT_EQ(rd.slice().get("value").getInt(), 2);
}

TEST_F(StorageEngineDocumentTest, UpdateWithStaleRevConflicts) {
  auto ins = insertR(keyed("k", 1));
  ASSERT_TRUE(IsOk(ins));
  auto staleRev = revOf(ins);
  // Supersede the revision so staleRev no longer matches the stored document.
  ASSERT_TRUE(IsOk(updateR(keyed("k", 2))));

  OperationOptions options;
  options.ignoreRevs = false;
  auto upd = updateR(keyedRev("k", staleRev, 3), options);
  EXPECT_TRUE(IsError(upd, TRI_ERROR_ARANGO_CONFLICT));

  auto rd = read("k");
  ASSERT_TRUE(IsOk(rd));
  EXPECT_EQ(rd.slice().get("value").getInt(), 2);
}

TEST_F(StorageEngineDocumentTest, ReplaceWithMatchingRevSucceeds) {
  auto ins = insertR(keyed("k", 1));
  ASSERT_TRUE(IsOk(ins));

  OperationOptions options;
  options.ignoreRevs = false;
  auto rep = replaceR(keyedRev("k", revOf(ins), 2), options);
  ASSERT_TRUE(IsOk(rep));

  auto rd = read("k");
  ASSERT_TRUE(IsOk(rd));
  EXPECT_EQ(rd.slice().get("value").getInt(), 2);
}

TEST_F(StorageEngineDocumentTest, ReplaceWithStaleRevConflicts) {
  auto ins = insertR(keyed("k", 1));
  ASSERT_TRUE(IsOk(ins));
  auto staleRev = revOf(ins);
  ASSERT_TRUE(IsOk(updateR(keyed("k", 2))));

  OperationOptions options;
  options.ignoreRevs = false;
  auto rep = replaceR(keyedRev("k", staleRev, 3), options);
  EXPECT_TRUE(IsError(rep, TRI_ERROR_ARANGO_CONFLICT));

  auto rd = read("k");
  ASSERT_TRUE(IsOk(rd));
  EXPECT_EQ(rd.slice().get("value").getInt(), 2);
}

TEST_F(StorageEngineDocumentTest, RemoveWithMatchingRevSucceeds) {
  auto ins = insertR(keyed("k", 1));
  ASSERT_TRUE(IsOk(ins));

  OperationOptions options;
  options.ignoreRevs = false;
  auto rm = removeR(keyedRev("k", revOf(ins)), options);
  ASSERT_TRUE(IsOk(rm));

  EXPECT_TRUE(IsError(read("k"), TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND));
}

TEST_F(StorageEngineDocumentTest, RemoveWithStaleRevConflicts) {
  auto ins = insertR(keyed("k", 1));
  ASSERT_TRUE(IsOk(ins));
  auto staleRev = revOf(ins);
  ASSERT_TRUE(IsOk(updateR(keyed("k", 2))));

  OperationOptions options;
  options.ignoreRevs = false;
  auto rm = removeR(keyedRev("k", staleRev), options);
  EXPECT_TRUE(IsError(rm, TRI_ERROR_ARANGO_CONFLICT));

  // The stale-rev remove must leave the document content untouched.
  auto rd = read("k");
  ASSERT_TRUE(IsOk(rd));
  EXPECT_EQ(rd.slice().get("value").getInt(), 2);
}

TEST_F(StorageEngineDocumentTest, IgnoreRevsDefaultBypassesPrecondition) {
  auto ins = insertR(keyed("k", 1));
  ASSERT_TRUE(IsOk(ins));
  auto staleRev = revOf(ins);
  ASSERT_TRUE(IsOk(updateR(keyed("k", 2))));

  // Default options keep ignoreRevs = true, so the stale _rev is ignored.
  auto upd = updateR(keyedRev("k", staleRev, 3));
  ASSERT_TRUE(IsOk(upd));

  auto rd = read("k");
  ASSERT_TRUE(IsOk(rd));
  EXPECT_EQ(rd.slice().get("value").getInt(), 3);
}

// ===========================================================================
// Overwrite / UPSERT modes
// ===========================================================================

TEST_F(StorageEngineDocumentTest, InsertOverwriteReplaceReplacesDocument) {
  ASSERT_TRUE(IsOk(insertR(R"({"_key":"k","a":1})"_vpack)));

  OperationOptions options;
  options.overwriteMode = OperationOptions::OverwriteMode::Replace;
  ASSERT_TRUE(IsOk(insertR(R"({"_key":"k","b":2})"_vpack, options)));

  auto rd = read("k");
  ASSERT_TRUE(IsOk(rd));
  EXPECT_FALSE(rd.slice().hasKey("a"));
  ASSERT_TRUE(rd.slice().hasKey("b"));
  EXPECT_EQ(rd.slice().get("b").getInt(), 2);
}

TEST_F(StorageEngineDocumentTest, InsertOverwriteUpdateMergesDocument) {
  ASSERT_TRUE(IsOk(insertR(R"({"_key":"k","a":1})"_vpack)));

  OperationOptions options;
  options.overwriteMode = OperationOptions::OverwriteMode::Update;
  ASSERT_TRUE(IsOk(insertR(R"({"_key":"k","b":2})"_vpack, options)));

  auto rd = read("k");
  ASSERT_TRUE(IsOk(rd));
  ASSERT_TRUE(rd.slice().hasKey("a"));
  ASSERT_TRUE(rd.slice().hasKey("b"));
  EXPECT_EQ(rd.slice().get("a").getInt(), 1);
  EXPECT_EQ(rd.slice().get("b").getInt(), 2);
}

TEST_F(StorageEngineDocumentTest, InsertOverwriteIgnoreKeepsExisting) {
  ASSERT_TRUE(IsOk(insertR(keyed("k", 1))));

  OperationOptions options;
  options.overwriteMode = OperationOptions::OverwriteMode::Ignore;
  auto res = insertR(keyed("k", 99), options);
  EXPECT_TRUE(IsOk(res));

  auto rd = read("k");
  ASSERT_TRUE(IsOk(rd));
  EXPECT_EQ(rd.slice().get("value").getInt(), 1);
}

TEST_F(StorageEngineDocumentTest, InsertOverwriteConflictModeFails) {
  ASSERT_TRUE(IsOk(insertR(keyed("k", 1))));

  OperationOptions options;
  options.overwriteMode = OperationOptions::OverwriteMode::Conflict;
  auto res = insertR(keyed("k", 2), options);
  EXPECT_TRUE(IsError(res, TRI_ERROR_ARANGO_UNIQUE_CONSTRAINT_VIOLATED));

  auto rd = read("k");
  ASSERT_TRUE(IsOk(rd));
  EXPECT_EQ(rd.slice().get("value").getInt(), 1);
}

// Insert-overwrite in Replace mode returns both the superseded document (old)
// and the new one (new); Replace semantics mean old carries only the previous
// attributes and new only the incoming ones, with differing revisions.
TEST_F(StorageEngineDocumentTest, InsertOverwriteReplaceReturnsOldAndNew) {
  ASSERT_TRUE(IsOk(insertR(R"({"_key":"k","a":1})"_vpack)));

  OperationOptions options;
  options.overwriteMode = OperationOptions::OverwriteMode::Replace;
  options.returnOld = true;
  options.returnNew = true;
  auto res = insertR(R"({"_key":"k","b":2})"_vpack, options);
  ASSERT_TRUE(IsOk(res));

  auto old = res.slice().get("old");
  auto nw = res.slice().get("new");
  ASSERT_TRUE(old.isObject());
  ASSERT_TRUE(nw.isObject());
  EXPECT_EQ(old.get("a").getInt(), 1);
  EXPECT_FALSE(old.hasKey("b"));
  EXPECT_EQ(nw.get("b").getInt(), 2);
  EXPECT_FALSE(nw.hasKey("a"));
  EXPECT_NE(old.get(StaticStrings::RevString).copyString(),
            nw.get(StaticStrings::RevString).copyString());
}

TEST_F(StorageEngineDocumentTest, InsertDuplicateKeyFails) {
  ASSERT_TRUE(IsOk(insertR(keyed("k", 1))));

  auto dup = insertR(keyed("k", 2));
  EXPECT_TRUE(IsError(dup, TRI_ERROR_ARANGO_UNIQUE_CONSTRAINT_VIOLATED));

  auto rd = read("k");
  ASSERT_TRUE(IsOk(rd));
  EXPECT_EQ(rd.slice().get("value").getInt(), 1);
}

// ===========================================================================
// Update / replace semantics
// ===========================================================================

TEST_F(StorageEngineDocumentTest, UpdateKeepNullFalseRemovesAttribute) {
  ASSERT_TRUE(IsOk(insertR(R"({"_key":"k","a":1})"_vpack)));

  OperationOptions options;
  options.keepNull = false;
  ASSERT_TRUE(IsOk(updateR(R"({"_key":"k","a":null})"_vpack, options)));

  auto rd = read("k");
  ASSERT_TRUE(IsOk(rd));
  EXPECT_FALSE(rd.slice().hasKey("a"));
}

TEST_F(StorageEngineDocumentTest, UpdateKeepNullDefaultKeepsNull) {
  ASSERT_TRUE(IsOk(insertR(R"({"_key":"k","a":1})"_vpack)));

  // Default keepNull = true keeps the attribute with a null value.
  ASSERT_TRUE(IsOk(updateR(R"({"_key":"k","a":null})"_vpack)));

  auto rd = read("k");
  ASSERT_TRUE(IsOk(rd));
  ASSERT_TRUE(rd.slice().hasKey("a"));
  EXPECT_TRUE(rd.slice().get("a").isNull());
}

TEST_F(StorageEngineDocumentTest, PartialUpdatePreservesUntouchedAttributes) {
  ASSERT_TRUE(IsOk(insertR(R"({"_key":"k","a":1,"b":2})"_vpack)));

  ASSERT_TRUE(IsOk(updateR(R"({"_key":"k","b":20})"_vpack)));

  auto rd = read("k");
  ASSERT_TRUE(IsOk(rd));
  EXPECT_EQ(rd.slice().get("a").getInt(), 1);
  EXPECT_EQ(rd.slice().get("b").getInt(), 20);
}

TEST_F(StorageEngineDocumentTest, ReplaceDropsAbsentAttributes) {
  ASSERT_TRUE(IsOk(insertR(R"({"_key":"k","a":1,"b":2})"_vpack)));

  ASSERT_TRUE(IsOk(replaceR(R"({"_key":"k","b":20})"_vpack)));

  auto rd = read("k");
  ASSERT_TRUE(IsOk(rd));
  EXPECT_FALSE(rd.slice().hasKey("a"));
  EXPECT_EQ(rd.slice().get("b").getInt(), 20);
}

TEST_F(StorageEngineDocumentTest, UpdateMergeObjectsFalseReplacesSubObject) {
  ASSERT_TRUE(IsOk(insertR(R"({"_key":"k","o":{"a":1}})"_vpack)));

  OperationOptions options;
  options.mergeObjects = false;
  ASSERT_TRUE(IsOk(updateR(R"({"_key":"k","o":{"b":2}})"_vpack, options)));

  auto rd = read("k");
  ASSERT_TRUE(IsOk(rd));
  auto o = rd.slice().get("o");
  ASSERT_TRUE(o.isObject());
  EXPECT_FALSE(o.hasKey("a"));
  EXPECT_EQ(o.get("b").getInt(), 2);
}

TEST_F(StorageEngineDocumentTest, UpdateMergeObjectsDefaultMergesSubObject) {
  ASSERT_TRUE(IsOk(insertR(R"({"_key":"k","o":{"a":1}})"_vpack)));

  // Default mergeObjects = true deep-merges the sub-object.
  ASSERT_TRUE(IsOk(updateR(R"({"_key":"k","o":{"b":2}})"_vpack)));

  auto rd = read("k");
  ASSERT_TRUE(IsOk(rd));
  auto o = rd.slice().get("o");
  ASSERT_TRUE(o.isObject());
  EXPECT_EQ(o.get("a").getInt(), 1);
  EXPECT_EQ(o.get("b").getInt(), 2);
}

// ===========================================================================
// returnOld / returnNew
// ===========================================================================

TEST_F(StorageEngineDocumentTest, InsertReturnNewContainsFullDocument) {
  OperationOptions options;
  options.returnNew = true;
  auto res = insertR(keyed("k", 1), options);
  ASSERT_TRUE(IsOk(res));

  auto nw = res.slice().get("new");
  ASSERT_TRUE(nw.isObject());
  EXPECT_EQ(nw.get("value").getInt(), 1);
  EXPECT_EQ(nw.get(StaticStrings::KeyString).copyString(), "k");
  EXPECT_FALSE(nw.get(StaticStrings::RevString).copyString().empty());
}

TEST_F(StorageEngineDocumentTest, UpdateReturnOldAndNew) {
  ASSERT_TRUE(IsOk(insertR(keyed("k", 1))));

  OperationOptions options;
  options.returnOld = true;
  options.returnNew = true;
  auto res = updateR(keyed("k", 2), options);
  ASSERT_TRUE(IsOk(res));

  auto old = res.slice().get("old");
  auto nw = res.slice().get("new");
  ASSERT_TRUE(old.isObject());
  ASSERT_TRUE(nw.isObject());
  EXPECT_EQ(old.get("value").getInt(), 1);
  EXPECT_EQ(nw.get("value").getInt(), 2);
  EXPECT_NE(old.get(StaticStrings::RevString).copyString(),
            nw.get(StaticStrings::RevString).copyString());
}

TEST_F(StorageEngineDocumentTest, RemoveReturnOldContainsDocument) {
  auto ins = insertR(keyed("k", 7));
  ASSERT_TRUE(IsOk(ins));

  OperationOptions options;
  options.returnOld = true;
  auto res = removeR(keyOnly("k"), options);
  ASSERT_TRUE(IsOk(res));

  auto old = res.slice().get("old");
  ASSERT_TRUE(old.isObject());
  EXPECT_EQ(old.get("value").getInt(), 7);
  EXPECT_EQ(old.get(StaticStrings::KeyString).copyString(), "k");
  EXPECT_EQ(old.get(StaticStrings::RevString).copyString(), revOf(ins));
}

// ===========================================================================
// Multi-document (babies)
// ===========================================================================

TEST_F(StorageEngineDocumentTest, InsertArrayAllSucceed) {
  auto docs = R"([{"_key":"a"},{"_key":"b"},{"_key":"c"}])"_vpack;
  auto res = insertR(docs);
  ASSERT_TRUE(IsOk(res));
  ASSERT_TRUE(res.slice().isArray());
  EXPECT_EQ(res.slice().length(), 3u);
  EXPECT_TRUE(res.countErrorCodes.empty());

  for (auto const* key : {"a", "b", "c"}) {
    EXPECT_TRUE(IsOk(read(key)));
  }
}

// An array insert with a duplicate key succeeds overall but reports the failed
// position as an error object in the result array (no _key), while the
// non-conflicting documents are committed. countErrorCodes summarizes the
// per-position errors.
TEST_F(StorageEngineDocumentTest, InsertArrayPartialFailureReportsPerDocument) {
  auto docs = R"([{"_key":"a"},{"_key":"a"},{"_key":"b"}])"_vpack;
  auto res = insertR(docs);
  ASSERT_TRUE(IsOk(res));
  ASSERT_TRUE(res.slice().isArray());
  ASSERT_EQ(res.slice().length(), 3u);

  EXPECT_EQ(res.slice().at(0).get(StaticStrings::KeyString).copyString(), "a");

  auto failed = res.slice().at(1);
  ASSERT_TRUE(failed.get("error").getBool());
  EXPECT_EQ(ErrorCode{failed.get("errorNum").getNumber<int>()},
            TRI_ERROR_ARANGO_UNIQUE_CONSTRAINT_VIOLATED);

  EXPECT_EQ(res.slice().at(2).get(StaticStrings::KeyString).copyString(), "b");

  ASSERT_EQ(
      res.countErrorCodes.count(TRI_ERROR_ARANGO_UNIQUE_CONSTRAINT_VIOLATED),
      1u);
  EXPECT_EQ(res.countErrorCodes.at(TRI_ERROR_ARANGO_UNIQUE_CONSTRAINT_VIOLATED),
            1u);

  EXPECT_TRUE(IsOk(read("a")));
  EXPECT_TRUE(IsOk(read("b")));
}

// A partial-failure array update reports the missing key as a per-position
// error (DOCUMENT_NOT_FOUND) and applies the patch to the documents that exist.
TEST_F(StorageEngineDocumentTest, UpdateArrayPartialFailure) {
  ASSERT_TRUE(IsOk(insertR(keyed("a", 1))));
  ASSERT_TRUE(IsOk(insertR(keyed("b", 2))));

  auto patch = R"([{"_key":"a","value":10},{"_key":"missing","value":99},)"
               R"({"_key":"b","value":20}])"_vpack;
  auto res = updateR(patch);
  ASSERT_TRUE(IsOk(res));
  ASSERT_TRUE(res.slice().isArray());
  ASSERT_EQ(res.slice().length(), 3u);

  EXPECT_EQ(res.slice().at(0).get(StaticStrings::KeyString).copyString(), "a");
  auto failed = res.slice().at(1);
  ASSERT_TRUE(failed.get("error").getBool());
  EXPECT_EQ(ErrorCode{failed.get("errorNum").getNumber<int>()},
            TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND);
  EXPECT_EQ(res.slice().at(2).get(StaticStrings::KeyString).copyString(), "b");

  ASSERT_EQ(res.countErrorCodes.count(TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND), 1u);

  auto rdA = read("a");
  ASSERT_TRUE(IsOk(rdA));
  EXPECT_EQ(rdA.slice().get("value").getInt(), 10);
  auto rdB = read("b");
  ASSERT_TRUE(IsOk(rdB));
  EXPECT_EQ(rdB.slice().get("value").getInt(), 20);
}

TEST_F(StorageEngineDocumentTest, RemoveArrayPartialFailure) {
  ASSERT_TRUE(IsOk(insertR(keyed("a", 1))));
  ASSERT_TRUE(IsOk(insertR(keyed("b", 2))));

  auto sel = R"([{"_key":"a"},{"_key":"missing"},{"_key":"b"}])"_vpack;
  auto res = removeR(sel);
  ASSERT_TRUE(IsOk(res));
  ASSERT_TRUE(res.slice().isArray());
  ASSERT_EQ(res.slice().length(), 3u);

  EXPECT_EQ(res.slice().at(0).get(StaticStrings::KeyString).copyString(), "a");
  auto failed = res.slice().at(1);
  ASSERT_TRUE(failed.get("error").getBool());
  EXPECT_EQ(ErrorCode{failed.get("errorNum").getNumber<int>()},
            TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND);
  EXPECT_EQ(res.slice().at(2).get(StaticStrings::KeyString).copyString(), "b");

  ASSERT_EQ(res.countErrorCodes.count(TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND), 1u);
  EXPECT_EQ(res.countErrorCodes.at(TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND), 1u);

  EXPECT_TRUE(IsError(read("a"), TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND));
  EXPECT_TRUE(IsError(read("b"), TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND));
}

// ===========================================================================
// silent
// ===========================================================================

// With options.silent = true the operation still succeeds and persists the
// document, but the result body carries no document: hasSlice() is true (the
// buffer exists) while the slice itself is a None value.
TEST_F(StorageEngineDocumentTest, SilentInsertSuppressesResultBody) {
  OperationOptions options;
  options.silent = true;
  auto res = insertR(keyed("k", 1), options);
  ASSERT_TRUE(IsOk(res));
  EXPECT_TRUE(res.slice().isNone());

  auto rd = read("k");
  ASSERT_TRUE(IsOk(rd));
  EXPECT_EQ(rd.slice().get("value").getInt(), 1);
}

// silent suppresses the successful entries of a babies operation, but errors
// are still reported: the result body is an array carrying only the error
// entries, and countErrorCodes still summarizes them. The non-conflicting
// documents are committed regardless.
TEST_F(StorageEngineDocumentTest, SilentBabiesPartialFailureReportsErrorCodes) {
  OperationOptions options;
  options.silent = true;
  auto docs = R"([{"_key":"a"},{"_key":"a"},{"_key":"b"}])"_vpack;
  auto res = insertR(docs, options);
  ASSERT_TRUE(IsOk(res));

  ASSERT_TRUE(res.slice().isArray());
  ASSERT_EQ(res.slice().length(), 1u);
  auto failed = res.slice().at(0);
  ASSERT_TRUE(failed.get("error").getBool());
  EXPECT_EQ(ErrorCode{failed.get("errorNum").getNumber<int>()},
            TRI_ERROR_ARANGO_UNIQUE_CONSTRAINT_VIOLATED);

  ASSERT_EQ(
      res.countErrorCodes.count(TRI_ERROR_ARANGO_UNIQUE_CONSTRAINT_VIOLATED),
      1u);

  EXPECT_TRUE(IsOk(read("a")));
  EXPECT_TRUE(IsOk(read("b")));
}

// ===========================================================================
// Key handling / _key
// ===========================================================================

TEST_F(StorageEngineDocumentTest, InsertWithoutKeyGeneratesKey) {
  auto res = insertR(R"({"value":1})"_vpack);
  ASSERT_TRUE(IsOk(res));
  auto key = keyOf(res);
  EXPECT_FALSE(key.empty());

  auto rd = read(key);
  ASSERT_TRUE(IsOk(rd));
  EXPECT_EQ(rd.slice().get("value").getInt(), 1);
}

TEST_F(StorageEngineDocumentTest, InsertProducesConsistentDocumentId) {
  auto res = insertR(keyed("k", 1));
  ASSERT_TRUE(IsOk(res));

  auto id = res.slice().get(StaticStrings::IdString).copyString();
  EXPECT_EQ(id, _collection->name() + "/" + keyOf(res));
}

TEST_F(StorageEngineDocumentTest, InsertRejectsMalformedKey) {
  auto res = insertR(keyed("bad key!", 1));
  EXPECT_TRUE(IsError(res, TRI_ERROR_ARANGO_DOCUMENT_KEY_BAD));

  // A rejected insert must leave no trace.
  EXPECT_EQ(count(), 0u);
}

// A revision id in the engine's HLC-encoded string format, supplied by the
// caller in the isRestore tests below.
static constexpr std::string_view kSuppliedRev = "_l4NAPbG---";

// A normal insert ignores any _rev supplied in the document and assigns a fresh
// one; only isRestore replays the supplied revision (next test).
TEST_F(StorageEngineDocumentTest, InsertIgnoresSuppliedRevision) {
  auto res = insertR(keyedRev("k", kSuppliedRev, 1));
  ASSERT_TRUE(IsOk(res));
  EXPECT_NE(revOf(res), kSuppliedRev);
}

// isRestore's distinctive effect on insert is that a supplied _rev is stored
// verbatim instead of being replaced by a freshly generated one -- this is what
// lets arangorestore/replication reproduce a document's original revision. (It
// also bypasses a collection's allowUserKeys=false setting, but the default key
// generator used here accepts user-supplied keys regardless, so that facet is
// not observable in this fixture.)
TEST_F(StorageEngineDocumentTest, InsertIsRestorePreservesGivenRevision) {
  OperationOptions options;
  options.isRestore = true;
  auto res = insertR(keyedRev("k", kSuppliedRev, 1), options);
  ASSERT_TRUE(IsOk(res));
  EXPECT_EQ(revOf(res), kSuppliedRev);

  auto rd = read("k");
  ASSERT_TRUE(IsOk(rd));
  EXPECT_EQ(rd.slice().get(StaticStrings::RevString).copyString(),
            kSuppliedRev);
}

// ===========================================================================
// Missing-document failures
// ===========================================================================

TEST_F(StorageEngineDocumentTest, UpdateNonExistentKeyFails) {
  auto res = updateR(keyed("missing", 1));
  EXPECT_TRUE(IsError(res, TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND));
}

TEST_F(StorageEngineDocumentTest, ReplaceNonExistentKeyFails) {
  auto res = replaceR(keyed("missing", 1));
  EXPECT_TRUE(IsError(res, TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND));
}

TEST_F(StorageEngineDocumentTest, RemoveNonExistentKeyFails) {
  auto res = removeR(keyOnly("missing"));
  EXPECT_TRUE(IsError(res, TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND));
}

// ===========================================================================
// Truncate & count
// ===========================================================================

TEST_F(StorageEngineDocumentTest, CountReflectsCommittedInserts) {
  EXPECT_EQ(count(), 0u);
  ASSERT_TRUE(IsOk(insertR(keyed("a", 1))));
  ASSERT_TRUE(IsOk(insertR(keyed("b", 2))));
  EXPECT_EQ(count(), 2u);

  // Updating an existing document does not change the count.
  ASSERT_TRUE(IsOk(updateR(keyed("a", 10))));
  EXPECT_EQ(count(), 2u);

  ASSERT_TRUE(IsOk(removeR(keyOnly("b"))));
  EXPECT_EQ(count(), 1u);
}

TEST_F(StorageEngineDocumentTest, TruncateEmptiesCollection) {
  ASSERT_TRUE(IsOk(insertR(keyed("a", 1))));
  ASSERT_TRUE(IsOk(insertR(keyed("b", 2))));
  ASSERT_TRUE(IsOk(insertR(keyed("c", 3))));
  ASSERT_EQ(count(), 3u);

  ASSERT_TRUE(IsOk(truncate()));

  EXPECT_EQ(count(), 0u);
  EXPECT_TRUE(IsError(read("a"), TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND));
}

TEST_F(StorageEngineDocumentTest, TruncateEmptyCollectionSucceeds) {
  ASSERT_TRUE(IsOk(truncate()));
  EXPECT_EQ(count(), 0u);
}

// ===========================================================================
// Transaction lifecycle & read-own-writes
// ===========================================================================

TEST_F(StorageEngineDocumentTest, AbortLeavesNoTrace) {
  {
    SingleCollectionTransaction trx{context(), *_collection,
                                    AccessMode::Type::WRITE};
    ASSERT_TRUE(IsOk(trx.begin()));
    OperationOptions options;
    auto doc = keyed("k", 1);
    ASSERT_TRUE(IsOk(trx.insert(_collection->name(), doc, options)));
    ASSERT_TRUE(IsOk(trx.abort()));
  }

  EXPECT_TRUE(IsError(read("k"), TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND));
}

TEST_F(StorageEngineDocumentTest, StreamingTransactionAbortDiscardsAllWrites) {
  {
    SingleCollectionTransaction trx{context(), *_collection,
                                    AccessMode::Type::WRITE};
    trx.addHint(transaction::Hints::Hint::GLOBAL_MANAGED);
    ASSERT_TRUE(IsOk(trx.begin()));
    OperationOptions options;
    for (auto const* key : {"a", "b", "c"}) {
      auto doc = keyed(key, 1);
      ASSERT_TRUE(IsOk(trx.insert(_collection->name(), doc, options)));
    }
    ASSERT_TRUE(IsOk(trx.abort()));
  }

  for (auto const* key : {"a", "b", "c"}) {
    EXPECT_TRUE(IsError(read(key), TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND));
  }
}

TEST_F(StorageEngineDocumentTest, StreamingTransactionCommitPersistsAllWrites) {
  {
    SingleCollectionTransaction trx{context(), *_collection,
                                    AccessMode::Type::WRITE};
    trx.addHint(transaction::Hints::Hint::GLOBAL_MANAGED);
    ASSERT_TRUE(IsOk(trx.begin()));
    OperationOptions options;
    for (auto const* key : {"a", "b", "c"}) {
      auto doc = keyed(key, 1);
      ASSERT_TRUE(IsOk(trx.insert(_collection->name(), doc, options)));
    }
    ASSERT_TRUE(IsOk(trx.finish(Result{})));
  }

  for (auto const* key : {"a", "b", "c"}) {
    EXPECT_TRUE(IsOk(read(key)));
  }
}

// A streaming transaction (GLOBAL_MANAGED) observes writes performed by earlier
// operations in the same transaction, before commit. This is the mechanism
// read-own-writes hinges on: with the GLOBAL_MANAGED hint the RocksDB
// transaction methods serve reads from the live write batch; without it (a
// plain standalone transaction, see next test) reads go to the base snapshot.
// A single document's lifecycle exercises every observable of that batch: an
// inserted value, an update to it, and finally its removal as a tombstone.
TEST_F(StorageEngineDocumentTest, StreamingTransactionReadsOwnWrites) {
  SingleCollectionTransaction trx{context(), *_collection,
                                  AccessMode::Type::WRITE};
  trx.addHint(transaction::Hints::Hint::GLOBAL_MANAGED);
  ASSERT_TRUE(IsOk(trx.begin()));

  OperationOptions options;
  auto lookup = keyOnly("k");

  // insert: the new (uncommitted) value is visible.
  auto ins = keyed("k", 1);
  ASSERT_TRUE(IsOk(trx.insert(_collection->name(), ins, options)));
  auto afterInsert = trx.document(_collection->name(), lookup, options);
  ASSERT_TRUE(IsOk(afterInsert));
  EXPECT_EQ(afterInsert.slice().get("value").getInt(), 1);

  // update: the read reflects the updated value, not the inserted one.
  auto upd = keyed("k", 2);
  ASSERT_TRUE(IsOk(trx.update(_collection->name(), upd, options)));
  auto afterUpdate = trx.document(_collection->name(), lookup, options);
  ASSERT_TRUE(IsOk(afterUpdate));
  EXPECT_EQ(afterUpdate.slice().get("value").getInt(), 2);

  // remove: the read observes the tombstone as "not found".
  ASSERT_TRUE(IsOk(trx.remove(_collection->name(), lookup, options)));
  auto afterRemove = trx.document(_collection->name(), lookup, options);
  EXPECT_TRUE(IsError(afterRemove, TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND));

  std::ignore = trx.abort();
}

// A standalone transaction, by contrast, does not observe its own uncommitted
// writes: the same read returns "not found" until the write is committed.
TEST_F(StorageEngineDocumentTest, StandaloneTransactionDoesNotReadOwnWrites) {
  SingleCollectionTransaction trx{context(), *_collection,
                                  AccessMode::Type::WRITE};
  ASSERT_TRUE(IsOk(trx.begin()));

  OperationOptions options;
  auto doc = keyed("key1", 1);
  auto insertRes = trx.insert(_collection->name(), doc, options);
  ASSERT_TRUE(IsOk(insertRes));

  auto lookup = keyOnly("key1");
  auto readRes = trx.document(_collection->name(), lookup, options);
  EXPECT_TRUE(IsError(readRes, TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND));

  ASSERT_TRUE(IsOk(trx.finish(insertRes.result)));
}
