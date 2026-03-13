/*jshint globalstrict:false, strict:false, maxlen: 500 */
/*global assertEqual, assertTrue, assertFalse */

// //////////////////////////////////////////////////////////////////////////////
// / DISCLAIMER
// /
// / Copyright 2014-2026 ArangoDB GmbH, Cologne, Germany
// / Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
// /
// / Licensed under the Business Source License 1.1 (the "License");
// / you may not use this file except in compliance with the License.
// / You may obtain a copy of the License at
// /
// /     https://github.com/arangodb/arangodb/blob/devel/LICENSE
// /
// / Unless required by applicable law or agreed to in writing, software
// / distributed under the License is distributed on an "AS IS" BASIS,
// / WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// / See the License for the specific language governing permissions and
// / limitations under the License.
// /
// / Copyright holder is ArangoDB GmbH, Cologne, Germany
// /
/// @author Julia Puget
// //////////////////////////////////////////////////////////////////////////////

const jsunity = require("jsunity");
const internal = require("internal");
const db = require("@arangodb").db;
function attributeDetectorRemoteTestSuite() {
  const cn = "UnitTestsAttributeDetectorRemote";
  let collection;

  const findNodeInPlan = function (plan, nodeType) {
    const nodes = plan.nodes || [];
    for (let i = 0; i < nodes.length; i++) {
      if (nodes[i].type === nodeType) {
        return nodes[i];
      }
    }
    return null;
  };

  return {
    setUp: function () {
      internal.db._drop(cn);
      collection = internal.db._create(cn, { numberOfShards: 3 });

      for (let i = 0; i < 10; ++i) {
        collection.insert({ _key: `doc${i}`, value: i, name: `name${i}`, data: { nested: i } });
      }
    },

    tearDown: function () {
      internal.db._drop(cn);
    },

    testRemoteSingleRead: function () {
      const query = `FOR doc IN ${cn} FILTER doc._key == "doc1" RETURN doc`;
      const explainResult = db._createStatement({ query: query }).explain();
      const plan = explainResult.plan;

      const remoteNode = findNodeInPlan(plan, "SingleRemoteOperationNode");
      if (remoteNode !== null) {
        assertEqual(remoteNode.mode, "IndexNode", "REMOTE_SINGLE should be in INDEX mode for read operations");
      }

      const accesses = explainResult.abacAccesses;
      assertTrue(Array.isArray(accesses), "abacAccesses should be an array");
      const collAccess = accesses.find(a => a.collection === cn);

      assertTrue(collAccess.read.requiresAll, "Should have read access information");
      assertFalse(collAccess.write.requiresAll, "Read-only operation should not require all write attributes");
    },

    testRemoteSingleInsert: function () {
      const query = `INSERT {_key: "newDoc", value: 100} INTO ${cn}`;
      const explainResult = db._createStatement({ query: query }).explain();
      const plan = explainResult.plan;

      const remoteNode = findNodeInPlan(plan, "SingleRemoteOperationNode");
      if (remoteNode !== null) {
        assertEqual(remoteNode.mode, "InsertNode", "REMOTE_SINGLE should be in INSERT mode for insert operations");
      }

      const accesses = explainResult.abacAccesses;
      assertTrue(Array.isArray(accesses), "abacAccesses should be an array");
      const collAccess = accesses.find(a => a.collection === cn);

      assertTrue(collAccess.read.requiresAll, "Insert operations should require all attributes read");
      assertTrue(collAccess.write.requiresAll, "Insert operations should require all attributes write");
    },

    testRemoteSingleUpdate: function () {
      const query = `UPDATE "doc1" WITH {value: 999} IN ${cn}`;
      const explainResult = db._createStatement({ query: query }).explain();
      const plan = explainResult.plan;

      const remoteNode = findNodeInPlan(plan, "SingleRemoteOperationNode");
      if (remoteNode !== null) {
        assertEqual(remoteNode.mode, "UpdateNode", "REMOTE_SINGLE should be in UPDATE mode for update operations");
      }

      const accesses = explainResult.abacAccesses;
      assertTrue(Array.isArray(accesses), "abacAccesses should be an array");
      const collAccess = accesses.find(a => a.collection === cn);

      assertTrue(collAccess.read.requiresAll, "Update operations should require all attributes read");
      assertTrue(collAccess.write.requiresAll, "Update operations should require all attributes write");
    },

    testRemoteSingleReplace: function () {
      const query = `REPLACE "doc1" WITH {_key: "doc1", value: 888} IN ${cn}`;
      const explainResult = db._createStatement({ query: query }).explain();
      const plan = explainResult.plan;

      const remoteNode = findNodeInPlan(plan, "SingleRemoteOperationNode");
      if (remoteNode !== null) {
        assertEqual(remoteNode.mode, "ReplaceNode", "REMOTE_SINGLE should be in REPLACE mode for replace operations");
      }

      const accesses = explainResult.abacAccesses;
      assertTrue(Array.isArray(accesses), "abacAccesses should be an array");
      const collAccess = accesses.find(a => a.collection === cn);

      assertTrue(collAccess.read.requiresAll, "Replace operations should require all attributes read");
      assertTrue(collAccess.write.requiresAll, "Replace operations should require all attributes write");
    },

    testRemoteSingleRemove: function () {
      const query = `REMOVE "doc1" IN ${cn}`;
      const explainResult = db._createStatement({ query: query }).explain();
      const plan = explainResult.plan;

      const remoteNode = findNodeInPlan(plan, "SingleRemoteOperationNode");
      if (remoteNode !== null) {
        assertEqual(remoteNode.mode, "RemoveNode", "REMOTE_SINGLE should be in REMOVE mode for remove operations");
      }

      const accesses = explainResult.abacAccesses;
      assertTrue(Array.isArray(accesses), "abacAccesses should be an array");
      const collAccess = accesses.find(a => a.collection === cn);

      assertTrue(collAccess.read.requiresAll, "Remove operations should require all attributes read");
      assertTrue(collAccess.write.requiresAll, "Remove operations should require all attributes write");
    },

    testRemoteMultipleInsert: function () {
      // REMOTE_MULTIPLE is only produced for FOR doc IN <array> INSERT doc (array from const/singleton)
      const query = `LET docs = [
        {_key: "batch1", value: 1},
        {_key: "batch2", value: 2},
        {_key: "batch3", value: 3},
        {_key: "batch4", value: 4},
        {_key: "batch5", value: 5}
      ] FOR doc IN docs INSERT doc INTO ${cn}`;
      const explainResult = db._createStatement({ query: query }).explain();
      const plan = explainResult.plan;

      const remoteNode = findNodeInPlan(plan, "MultipleRemoteModificationNode");
      assertTrue(remoteNode !== null, "REMOTE_MULTIPLE node should exist for batch insert (FOR doc IN array INSERT doc)");

      const accesses = explainResult.abacAccesses;
      assertTrue(Array.isArray(accesses), "abacAccesses should be an array");
      const collAccess = accesses.find(a => a.collection === cn);


      assertTrue(collAccess.read.requiresAll, "REMOTE_MULTIPLE insert should require all attributes read");
      assertTrue(collAccess.write.requiresAll, "REMOTE_MULTIPLE insert should require all attributes write");
    },

    testRemoteSingleWithReturnOld: function () {
      const query = `UPDATE "doc1" WITH {value: 777} IN ${cn} RETURN OLD`;
      const explainResult = db._createStatement({ query: query }).explain();
      const plan = explainResult.plan;

      const remoteNode = findNodeInPlan(plan, "SingleRemoteOperationNode");
      if (remoteNode !== null) {
        assertEqual(remoteNode.mode, "UpdateNode", "REMOTE_SINGLE should be in UPDATE mode");
      }

      const accesses = explainResult.abacAccesses;
      assertTrue(Array.isArray(accesses), "abacAccesses should be an array");
      const collAccess = accesses.find(a => a.collection === cn);

      assertTrue(collAccess.read.requiresAll, "Update with RETURN OLD should require all attributes read");
      assertTrue(collAccess.write.requiresAll, "Update with RETURN OLD should require all attributes write");
    },

    testRemoteSingleWithReturnNew: function () {
      const query = `UPDATE "doc1" WITH {value: 666} IN ${cn} RETURN NEW`;
      const explainResult = db._createStatement({ query: query }).explain();
      const plan = explainResult.plan;

      const remoteNode = findNodeInPlan(plan, "SingleRemoteOperationNode");
      if (remoteNode !== null) {
        assertEqual(remoteNode.mode, "UpdateNode", "REMOTE_SINGLE should be in UPDATE mode");
      }

      const accesses = explainResult.abacAccesses;
      assertTrue(Array.isArray(accesses), "abacAccesses should be an array");
      const collAccess = accesses.find(a => a.collection === cn);

      assertTrue(collAccess.read.requiresAll, "Update with RETURN NEW should require all attributes read");
      assertTrue(collAccess.write.requiresAll, "Update with RETURN NEW should require all attributes write");
    },

    testRemoteSingleReadWithProjection_AbacAccesses: function () {
      const query = `FOR doc IN ${cn} FILTER doc._key == "doc1" RETURN {name: doc.name, value: doc.value}`;
      const explainResult = db._createStatement({
        query: query,
        options: {}
      }).explain();

      const accesses = explainResult.abacAccesses;
      assertTrue(Array.isArray(accesses), "abacAccesses should be an array");

      const collAccess = accesses.find(a => a.collection === cn);

      assertTrue(collAccess.read.requiresAll,
        "REMOTE_SINGLE should require all attributes (conservative)");
      assertFalse(collAccess.write.requiresAll);
    },

    testRemoteSingleInsert_AbacAccesses: function () {
      const query = `INSERT {_key: "newDoc", value: 100, name: "test"} INTO ${cn}`;
      const explainResult = db._createStatement({
        query: query,
        options: {}
      }).explain();

      const accesses = explainResult.abacAccesses;
      const collAccess = accesses.find(a => a.collection === cn);

      assertTrue(collAccess.read.requiresAll,
        "Insert operations should require all attributes read");
      assertTrue(collAccess.write.requiresAll,
        "Insert operations should require all attributes write");
    },

    testRemoteSingleUpdate_AbacAccesses: function () {
      const query = `UPDATE "doc1" WITH {value: 999} IN ${cn}`;
      const explainResult = db._createStatement({
        query: query,
        options: {}
      }).explain();

      const accesses = explainResult.abacAccesses;
      const collAccess = accesses.find(a => a.collection === cn);

      assertTrue(collAccess.read.requiresAll,
        "Update operations should require all attributes read");
      assertTrue(collAccess.write.requiresAll,
        "Update operations should require all attributes write");
    },

    testRemoteSingleReplace_AbacAccesses: function () {
      const query = `REPLACE "doc1" WITH {_key: "doc1", value: 888, name: "replaced"} IN ${cn}`;
      const explainResult = db._createStatement({
        query: query,
        options: {}
      }).explain();

      const accesses = explainResult.abacAccesses;
      const collAccess = accesses.find(a => a.collection === cn);
      assertTrue(collAccess.read.requiresAll,
        "Replace operations should require all attributes read");
      assertTrue(collAccess.write.requiresAll,
        "Replace operations should require all attributes write");
    },

    testRemoteSingleRemove_AbacAccesses: function () {
      const query = `REMOVE "doc1" IN ${cn}`;
      const explainResult = db._createStatement({
        query: query,
        options: {}
      }).explain();

      const accesses = explainResult.abacAccesses;
      const collAccess = accesses.find(a => a.collection === cn);
      assertTrue(collAccess.read.requiresAll,
        "Remove operations should require all attributes read");
      assertTrue(collAccess.write.requiresAll,
        "Remove operations should require all attributes write");
    },

    testRemoteMultipleInsert_AbacAccesses: function () {
      // REMOTE_MULTIPLE is only produced for FOR doc IN <array> INSERT doc (array from const/singleton)
      const query = `LET docs = [
        {_key: "multi1", value: 1, name: "name1"},
        {_key: "multi2", value: 2, name: "name2"},
        {_key: "multi3", value: 3, name: "name3"},
        {_key: "multi4", value: 4, name: "name4"},
        {_key: "multi5", value: 5, name: "name5"}
      ] FOR doc IN docs INSERT doc INTO ${cn}`;
      const explainResult = db._createStatement({
        query: query,
        options: {}
      }).explain();

      const plan = explainResult.plan;
      const remoteNode = findNodeInPlan(plan, "MultipleRemoteModificationNode");
      assertTrue(remoteNode !== null, "REMOTE_MULTIPLE node should exist for batch insert (FOR doc IN array INSERT doc)");

      const accesses = explainResult.abacAccesses;
      const collAccess = accesses.find(a => a.collection === cn);
      assertTrue(collAccess.read.requiresAll,
        "REMOTE_MULTIPLE insert should require all attributes read");
      assertTrue(collAccess.write.requiresAll,
        "REMOTE_MULTIPLE insert should require all attributes write");
    },

    testRemoteSingle_RequiresAllWithEmptyAttributes: function () {
      const query = `FOR doc IN ${cn} FILTER doc._key == "doc1" RETURN doc`;
      const explainResult = db._createStatement({
        query: query,
        options: {}
      }).explain();

      const accesses = explainResult.abacAccesses;
      const collAccess = accesses.find(a => a.collection === cn);
      assertTrue(collAccess.read.requiresAll);
      assertTrue(Array.isArray(collAccess.read.attributes));
    }
  };
}

jsunity.run(attributeDetectorRemoteTestSuite);

return jsunity.done();
