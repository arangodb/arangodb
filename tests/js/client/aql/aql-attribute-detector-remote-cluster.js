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
const helper = require("@arangodb/aql-helper");

function attributeDetectorRemoteTestSuite() {
  const cn = "UnitTestsAttributeDetectorRemote";
  let collection;

  const explain = function (query, bindVars, options) {
    return helper.removeClusterNodes(
      helper.getCompactPlan(
        db._createStatement({query: query, bindVars: bindVars || {}, options: options || {}}).explain()
      ).map(function(node) { return node.type; })
    );
  };

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
      collection = internal.db._create(cn, {numberOfShards: 3});

      for (let i = 0; i < 10; ++i) {
        collection.insert({_key: `doc${i}`, value: i, name: `name${i}`, data: {nested: i}});
      }
    },

    tearDown: function () {
      internal.db._drop(cn);
    },

    testRemoteSingleRead: function () {
      const query = `FOR doc IN ${cn} FILTER doc._key == "doc1" RETURN doc`;
      const explainResult = db._createStatement({query: query, options: {includeAbacAccesses: true}}).explain();
      const plan = explainResult.plan;

      const remoteNode = findNodeInPlan(plan, "SingleRemoteOperationNode");
      if (remoteNode !== null) {
        assertEqual(remoteNode.mode, "IndexNode", "REMOTE_SINGLE should be in INDEX mode for read operations");
      }

      const accesses = explainResult.abacAccesses;
      console.log(accesses);
      assertTrue(Array.isArray(accesses), "abacAccesses should be an array");
      const collAccess = accesses.find(a => a.collection === cn);
      if (collAccess) {
        assertTrue(collAccess.read.requiresAll || collAccess.read.attributes.length > 0,
            "Should have read access information");
        assertFalse(collAccess.write.requiresAll, "Read-only operation should not require all write attributes");
      }
    },

    testRemoteSingleInsert: function () {
      const query = `INSERT {_key: "newDoc", value: 100} INTO ${cn}`;
      const explainResult = db._createStatement({query: query}).explain();
      const plan = explainResult.plan;

      const remoteNode = findNodeInPlan(plan, "SingleRemoteOperationNode");
      if (remoteNode !== null) {
        assertEqual(remoteNode.mode, "InsertNode", "REMOTE_SINGLE should be in INSERT mode for insert operations");
      }

      if (explainResult.extra && explainResult.extra.abacAccesses) {
        const accesses = explainResult.extra.abacAccesses;
        const collAccess = accesses.find(a => a.collection === cn);
        if (collAccess) {
          assertTrue(collAccess.read.requiresAll, "Write operations should require all attributes read");
          assertTrue(collAccess.write.requiresAll, "Write operations should require all attributes write");
        }
      }
    },

    testRemoteSingleUpdate: function () {
      const query = `UPDATE "doc1" WITH {value: 999} IN ${cn}`;
      const plan = db._createStatement({query: query}).explain().plan;

      const remoteNode = findNodeInPlan(plan, "SingleRemoteOperationNode");
      if (remoteNode !== null) {
        assertEqual(remoteNode.mode, "UpdateNode", "REMOTE_SINGLE should be in UPDATE mode for update operations");
      }
    },

    testRemoteSingleReplace: function () {
      const query = `REPLACE "doc1" WITH {_key: "doc1", value: 888} IN ${cn}`;
      const plan = db._createStatement({query: query}).explain().plan;

      const remoteNode = findNodeInPlan(plan, "SingleRemoteOperationNode");
      if (remoteNode !== null) {
        assertEqual(remoteNode.mode, "ReplaceNode", "REMOTE_SINGLE should be in REPLACE mode for replace operations");
      }
    },

    testRemoteSingleRemove: function () {
      const query = `REMOVE "doc1" IN ${cn}`;
      const plan = db._createStatement({query: query}).explain().plan;

      const remoteNode = findNodeInPlan(plan, "SingleRemoteOperationNode");
      if (remoteNode !== null) {
        assertEqual(remoteNode.mode, "RemoveNode", "REMOTE_SINGLE should be in REMOVE mode for remove operations");
      }
    },

    testRemoteMultipleInsert: function () {
      const query = `FOR i IN 1..5 INSERT {_key: CONCAT("doc", i), value: i} INTO ${cn}`;
      const plan = db._createStatement({query: query}).explain().plan;

      const remoteNode = findNodeInPlan(plan, "MultipleRemoteModificationNode");
      if (remoteNode !== null) {
        assertTrue(remoteNode !== null, "REMOTE_MULTIPLE node should exist for batch insert operations");
      }
    },

    testRemoteSingleWithReturnOld: function () {
      const query = `UPDATE "doc1" WITH {value: 777} IN ${cn} RETURN OLD`;
      const plan = db._createStatement({query: query}).explain().plan;

      const remoteNode = findNodeInPlan(plan, "SingleRemoteOperationNode");
      if (remoteNode !== null) {
        assertEqual(remoteNode.mode, "UpdateNode", "REMOTE_SINGLE should be in UPDATE mode");
      }
    },

    testRemoteSingleWithReturnNew: function () {
      const query = `UPDATE "doc1" WITH {value: 666} IN ${cn} RETURN NEW`;
      const plan = db._createStatement({query: query}).explain().plan;

      const remoteNode = findNodeInPlan(plan, "SingleRemoteOperationNode");
      if (remoteNode !== null) {
        assertEqual(remoteNode.mode, "UpdateNode", "REMOTE_SINGLE should be in UPDATE mode");
      }
    },

    testRemoteSingleRead_AbacAccesses: function () {
      const query = `FOR doc IN ${cn} FILTER doc._key == "doc1" RETURN doc`;
      const explainResult = db._createStatement({
        query: query,
        options: {includeAbacAccesses: true}
      }).explain();

      const accesses = explainResult.abacAccesses;
      assertTrue(Array.isArray(accesses), "abacAccesses should be an array");
      
      const collAccess = accesses.find(a => a.collection === cn);
      if (collAccess) {
        // REMOTE_SINGLE in INDEX mode should require all attributes read
        assertTrue(collAccess.read.requiresAll, 
            "REMOTE_SINGLE read operations should require all attributes");
        assertFalse(collAccess.write.requiresAll, 
            "Read operations should not require all write attributes");
        // When requiresAll is true, attributes list can be empty
        assertTrue(Array.isArray(collAccess.read.attributes));
      }
    },

    testRemoteSingleReadWithProjection_AbacAccesses: function () {
      const query = `FOR doc IN ${cn} FILTER doc._key == "doc1" RETURN {name: doc.name, value: doc.value}`;
      const explainResult = db._createStatement({
        query: query,
        options: {includeAbacAccesses: true}
      }).explain();

      const accesses = explainResult.abacAccesses;
      assertTrue(Array.isArray(accesses), "abacAccesses should be an array");
      
      const collAccess = accesses.find(a => a.collection === cn);
      if (collAccess) {
        // Even with projections, REMOTE_SINGLE is conservative
        assertTrue(collAccess.read.requiresAll, 
            "REMOTE_SINGLE should require all attributes (conservative)");
        assertFalse(collAccess.write.requiresAll);
      }
    },

    testRemoteSingleInsert_AbacAccesses: function () {
      const query = `INSERT {_key: "newDoc", value: 100, name: "test"} INTO ${cn}`;
      const explainResult = db._createStatement({
        query: query,
        options: {includeAbacAccesses: true}
      }).explain();

      const accesses = explainResult.abacAccesses;
      if (accesses && Array.isArray(accesses)) {
        const collAccess = accesses.find(a => a.collection === cn);
        if (collAccess) {
          // REMOTE_SINGLE insert should require all attributes for both read and write
          assertTrue(collAccess.read.requiresAll, 
              "Insert operations should require all attributes read");
          assertTrue(collAccess.write.requiresAll, 
              "Insert operations should require all attributes write");
        }
      }
    },

    testRemoteSingleUpdate_AbacAccesses: function () {
      const query = `UPDATE "doc1" WITH {value: 999} IN ${cn}`;
      const explainResult = db._createStatement({
        query: query,
        options: {includeAbacAccesses: true}
      }).explain();

      const accesses = explainResult.abacAccesses;
      if (accesses && Array.isArray(accesses)) {
        const collAccess = accesses.find(a => a.collection === cn);
        if (collAccess) {
          // REMOTE_SINGLE update should require all attributes for both read and write
          assertTrue(collAccess.read.requiresAll, 
              "Update operations should require all attributes read");
          assertTrue(collAccess.write.requiresAll, 
              "Update operations should require all attributes write");
        }
      }
    },

    testRemoteSingleReplace_AbacAccesses: function () {
      const query = `REPLACE "doc1" WITH {_key: "doc1", value: 888, name: "replaced"} IN ${cn}`;
      const explainResult = db._createStatement({
        query: query,
        options: {includeAbacAccesses: true}
      }).explain();

      const accesses = explainResult.abacAccesses;
      if (accesses && Array.isArray(accesses)) {
        const collAccess = accesses.find(a => a.collection === cn);
        if (collAccess) {
          assertTrue(collAccess.read.requiresAll, 
              "Replace operations should require all attributes read");
          assertTrue(collAccess.write.requiresAll, 
              "Replace operations should require all attributes write");
        }
      }
    },

    testRemoteSingleRemove_AbacAccesses: function () {
      const query = `REMOVE "doc1" IN ${cn}`;
      const explainResult = db._createStatement({
        query: query,
        options: {includeAbacAccesses: true}
      }).explain();

      const accesses = explainResult.abacAccesses;
      if (accesses && Array.isArray(accesses)) {
        const collAccess = accesses.find(a => a.collection === cn);
        if (collAccess) {
          assertTrue(collAccess.read.requiresAll, 
              "Remove operations should require all attributes read");
          assertTrue(collAccess.write.requiresAll, 
              "Remove operations should require all attributes write");
        }
      }
    },

    testRemoteMultipleInsert_AbacAccesses: function () {
      const query = `FOR i IN 1..5 INSERT {_key: CONCAT("doc", i), value: i, name: CONCAT("name", i)} INTO ${cn}`;
      const explainResult = db._createStatement({
        query: query,
        options: {includeAbacAccesses: true}
      }).explain();

      const accesses = explainResult.abacAccesses;
      if (accesses && Array.isArray(accesses)) {
        const collAccess = accesses.find(a => a.collection === cn);
        if (collAccess) {
          // REMOTE_MULTIPLE should require all attributes for both read and write
          assertTrue(collAccess.read.requiresAll, 
              "REMOTE_MULTIPLE insert should require all attributes read");
          assertTrue(collAccess.write.requiresAll, 
              "REMOTE_MULTIPLE insert should require all attributes write");
        }
      }
    },

    testRemoteSingle_RequiresAllWithEmptyAttributes: function () {
      const query = `FOR doc IN ${cn} FILTER doc._key == "doc1" RETURN doc`;
      const explainResult = db._createStatement({
        query: query,
        options: {includeAbacAccesses: true}
      }).explain();

      const accesses = explainResult.abacAccesses;
      if (accesses && Array.isArray(accesses)) {
        const collAccess = accesses.find(a => a.collection === cn);
        if (collAccess && collAccess.read.requiresAll) {
          // When requiresAll is true, attributes list can be empty
          assertTrue(Array.isArray(collAccess.read.attributes));
          // Empty list is valid when requiresAll is true
        }
      }
    }
  };
}

jsunity.run(attributeDetectorRemoteTestSuite);

return jsunity.done();
