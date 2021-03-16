/*jshint globalstrict:false, strict:false, maxlen: 7000 */
/*global assertEqual, assertTrue, assertFalse, AQL_EXPLAIN */

////////////////////////////////////////////////////////////////////////////////
/// @brief tests for index selection
///
/// DISCLAIMER
///
/// Copyright 2010-2012 triagens GmbH, Cologne, Germany
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
/// Copyright holder is triAGENS GmbH, Cologne, Germany
///
/// @author Jan Steemann
/// @author Copyright 2012, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

const jsunity = require("jsunity");
const db = require("internal").db;
const deriveTestSuite = require('@arangodb/test-helper').deriveTestSuite;
const cn = 'UnitTestsCollection';

let setupCollection = function(cn, n) {
  db._drop(cn);
  let c = db._create(cn);
  let docs = [];
  for (let i = 0; i < n; ++i) {
    docs.push({ _key: "test" + i, uid: i, pid: (i % 100), dt: Date.now() - ((i * 10) % 10000) });
    if (docs.length === 5000) {
      c.insert(docs);
      docs = [];
    }
  }
  if (docs.length) {
    c.insert(docs);
  }
};


function BaseTestConfig () {
  let resetIndexes = function(cn) {
    db[cn].indexes().filter((idx) => idx.type !== 'primary').forEach((idx) => {
      db[cn].dropIndex(idx);
    });
  };

  return {
    tearDownAll : function() {
      db._drop(cn);
    },

    setUp: function() {
      resetIndexes(cn);
    },
    
    tearDown: function() {
      resetIndexes(cn);
    },
    
    testSingleIndexSingleFilter: function() {
      db[cn].ensureIndex({ type: "persistent", fields: ["uid"] });
      
      [
        `FOR doc IN ${cn} FILTER doc.uid == 1234 RETURN doc`,
        `FOR doc IN ${cn} FILTER doc.uid > 1234 RETURN doc`,
        `FOR doc IN ${cn} FILTER doc.uid >= 1234 RETURN doc`
      ].forEach((q) => {
        let nodes = AQL_EXPLAIN(q).plan.nodes;
        assertEqual(1, nodes.filter((n) => n.type === 'IndexNode').length);
        let indexNode = nodes.filter((n) => n.type === 'IndexNode')[0];
        assertEqual([], indexNode.projections);
        assertFalse(indexNode.indexCoversProjections);
        assertEqual(1, indexNode.indexes.length);
        let index = indexNode.indexes[0];
        assertEqual("persistent", index.type);
        assertEqual(["uid"], index.fields);
      });
    },
    
    testSingleIndexSingleFilterAndProjection: function() {
      db[cn].ensureIndex({ type: "persistent", fields: ["uid"] });
      
      [
        `FOR doc IN ${cn} FILTER doc.uid == 1234 RETURN doc.uid`,
        `FOR doc IN ${cn} FILTER doc.uid > 1234 RETURN doc.uid`,
        `FOR doc IN ${cn} FILTER doc.uid >= 1234 RETURN doc.uid`
      ].forEach((q) => {
        let nodes = AQL_EXPLAIN(q).plan.nodes;
        assertEqual(1, nodes.filter((n) => n.type === 'IndexNode').length);
        let indexNode = nodes.filter((n) => n.type === 'IndexNode')[0];
        assertEqual(["uid"], indexNode.projections);
        assertTrue(indexNode.indexCoversProjections);
        assertEqual(1, indexNode.indexes.length);
        let index = indexNode.indexes[0];
        assertEqual("persistent", index.type);
        assertEqual(["uid"], index.fields);
      });
    },
    
    testMultipleIndexesSingleFilter: function() {
      db[cn].ensureIndex({ type: "persistent", fields: ["uid"] });
      db[cn].ensureIndex({ type: "persistent", fields: ["uid", "dt"] });
      
      [
        `FOR doc IN ${cn} FILTER doc.uid == 1234 RETURN doc`,
        `FOR doc IN ${cn} FILTER doc.uid > 1234 RETURN doc`,
        `FOR doc IN ${cn} FILTER doc.uid >= 1234 RETURN doc`
      ].forEach((q) => {
        let nodes = AQL_EXPLAIN(q).plan.nodes;
        assertEqual(1, nodes.filter((n) => n.type === 'IndexNode').length);
        let indexNode = nodes.filter((n) => n.type === 'IndexNode')[0];
        assertEqual([], indexNode.projections);
        assertFalse(indexNode.indexCoversProjections);
        assertEqual(1, indexNode.indexes.length);
        let index = indexNode.indexes[0];
        assertEqual("persistent", index.type);
        // the optimizer does not know about projections when selecting the index,
        // so it always picks the "longer" of the 2 indexes here in case everything
        // else is equal
        assertEqual(["uid", "dt"], index.fields);
      });
    },
    
    testMultipleIndexesSingleFilterAndProjection: function() {
      db[cn].ensureIndex({ type: "persistent", fields: ["uid"] });
      db[cn].ensureIndex({ type: "persistent", fields: ["uid", "dt"] });
      
      [
        `FOR doc IN ${cn} FILTER doc.uid == 1234 RETURN doc.uid`,
        `FOR doc IN ${cn} FILTER doc.uid > 1234 RETURN doc.uid`,
        `FOR doc IN ${cn} FILTER doc.uid >= 1234 RETURN doc.uid`
      ].forEach((q) => {
        let nodes = AQL_EXPLAIN(q).plan.nodes;
        assertEqual(1, nodes.filter((n) => n.type === 'IndexNode').length);
        let indexNode = nodes.filter((n) => n.type === 'IndexNode')[0];
        assertEqual(["uid"], indexNode.projections);
        assertTrue(indexNode.indexCoversProjections);
        assertEqual(1, indexNode.indexes.length);
        let index = indexNode.indexes[0];
        assertEqual("persistent", index.type);
        // the optimizer does not know about projections when selecting the index,
        // so it always picks the "longer" of the 2 indexes here in case everything
        // else is equal
        assertEqual(["uid", "dt"], index.fields);
      });
      
      [
        `FOR doc IN ${cn} FILTER doc.uid == 1234 RETURN doc.dt`,
        `FOR doc IN ${cn} FILTER doc.uid > 1234 RETURN doc.dt`,
        `FOR doc IN ${cn} FILTER doc.uid >= 1234 RETURN doc.dt`
      ].forEach((q) => {
        let nodes = AQL_EXPLAIN(q).plan.nodes;
        assertEqual(1, nodes.filter((n) => n.type === 'IndexNode').length);
        let indexNode = nodes.filter((n) => n.type === 'IndexNode')[0];
        assertEqual(["dt"], indexNode.projections);
        assertTrue(indexNode.indexCoversProjections);
        assertEqual(1, indexNode.indexes.length);
        let index = indexNode.indexes[0];
        assertEqual("persistent", index.type);
        // the optimizer does not know about projections when selecting the index,
        // so it always picks the "longer" of the 2 indexes here in case everything
        // else is equal
        assertEqual(["uid", "dt"], index.fields);
      });
      
      [
        `FOR doc IN ${cn} FILTER doc.uid == 1234 RETURN [doc.dt, doc.uid]`,
        `FOR doc IN ${cn} FILTER doc.uid > 1234 RETURN [doc.dt, doc.uid]`,
        `FOR doc IN ${cn} FILTER doc.uid >= 1234 RETURN [doc.dt, doc.uid]`
      ].forEach((q) => {
        let nodes = AQL_EXPLAIN(q).plan.nodes;
        assertEqual(1, nodes.filter((n) => n.type === 'IndexNode').length);
        let indexNode = nodes.filter((n) => n.type === 'IndexNode')[0];
        assertEqual(["dt", "uid"], indexNode.projections.sort());
        assertTrue(indexNode.indexCoversProjections);
        assertEqual(1, indexNode.indexes.length);
        let index = indexNode.indexes[0];
        assertEqual("persistent", index.type);
        // the optimizer does not know about projections when selecting the index,
        // so it always picks the "longer" of the 2 indexes here in case everything
        // else is equal
        assertEqual(["uid", "dt"], index.fields);
      });
    },
    
    testSingleIndexMultipleFilters: function() {
      db[cn].ensureIndex({ type: "persistent", fields: ["uid"] });
      
      [
        `FOR doc IN ${cn} FILTER doc.uid == 1234 && doc.dt == 1234 RETURN doc`,
        `FOR doc IN ${cn} FILTER doc.uid == 1234 && doc.dt >= 1234 RETURN doc`,
        `FOR doc IN ${cn} FILTER doc.uid == 1234 && doc.dt <= 1234 RETURN doc`,
        `FOR doc IN ${cn} FILTER doc.uid == 1234 && doc.dt >= 1234 && doc.dt <= 9999 RETURN doc`
      ].forEach((q) => {
        let nodes = AQL_EXPLAIN(q).plan.nodes;
        assertEqual(1, nodes.filter((n) => n.type === 'IndexNode').length);
        let indexNode = nodes.filter((n) => n.type === 'IndexNode')[0];
        assertEqual([], indexNode.projections);
        assertFalse(indexNode.indexCoversProjections);
        assertEqual(1, indexNode.indexes.length);
        let index = indexNode.indexes[0];
        assertEqual("persistent", index.type);
        assertEqual(["uid"], index.fields);
      });
    },
    
    testSingleIndexMultipleFiltersAndProjection: function() {
      db[cn].ensureIndex({ type: "persistent", fields: ["uid"] });
      
      [
        `FOR doc IN ${cn} FILTER doc.uid == 1234 && doc.dt == 1234 RETURN doc.uid`,
        `FOR doc IN ${cn} FILTER doc.uid == 1234 && doc.dt >= 1234 RETURN doc.uid`,
        `FOR doc IN ${cn} FILTER doc.uid == 1234 && doc.dt <= 1234 RETURN doc.uid`,
        `FOR doc IN ${cn} FILTER doc.uid == 1234 && doc.dt >= 1234 && doc.dt <= 9999 RETURN doc.uid`
      ].forEach((q) => {
        let nodes = AQL_EXPLAIN(q).plan.nodes;
        assertEqual(1, nodes.filter((n) => n.type === 'IndexNode').length);
        let indexNode = nodes.filter((n) => n.type === 'IndexNode')[0];
        assertTrue(["uid"], indexNode.projections);
        assertFalse(indexNode.indexCoversProjections);
        assertEqual(1, indexNode.indexes.length);
        let index = indexNode.indexes[0];
        assertEqual("persistent", index.type);
        assertEqual(["uid"], index.fields);
      });
      
      [
        `FOR doc IN ${cn} FILTER doc.uid == 1234 && doc.dt == 1234 RETURN doc.dt`,
        `FOR doc IN ${cn} FILTER doc.uid == 1234 && doc.dt >= 1234 RETURN doc.dt`,
        `FOR doc IN ${cn} FILTER doc.uid == 1234 && doc.dt <= 1234 RETURN doc.dt`,
        `FOR doc IN ${cn} FILTER doc.uid == 1234 && doc.dt >= 1234 && doc.dt <= 9999 RETURN doc.dt`
      ].forEach((q) => {
        let nodes = AQL_EXPLAIN(q).plan.nodes;
        assertEqual(1, nodes.filter((n) => n.type === 'IndexNode').length);
        let indexNode = nodes.filter((n) => n.type === 'IndexNode')[0];
        assertEqual(["dt"], indexNode.projections);
        assertFalse(indexNode.indexCoversProjections);
        assertEqual(1, indexNode.indexes.length);
        let index = indexNode.indexes[0];
        assertEqual("persistent", index.type);
        assertEqual(["uid"], index.fields);
      });
    },
    
    testMultipleIndexesMultipleFilters: function() {
      db[cn].ensureIndex({ type: "persistent", fields: ["uid"] });
      db[cn].ensureIndex({ type: "persistent", fields: ["uid", "dt"] });

      [
        `FOR doc IN ${cn} FILTER doc.uid == 1234 && doc.dt == 1234 RETURN doc`,
        `FOR doc IN ${cn} FILTER doc.uid == 1234 && doc.dt >= 1234 RETURN doc`,
        `FOR doc IN ${cn} FILTER doc.uid == 1234 && doc.dt <= 1234 RETURN doc`,
        `FOR doc IN ${cn} FILTER doc.uid == 1234 && doc.dt >= 1234 && doc.dt <= 9999 RETURN doc`
      ].forEach((q) => {
        let nodes = AQL_EXPLAIN(q).plan.nodes;
        assertEqual(1, nodes.filter((n) => n.type === 'IndexNode').length);
        let indexNode = nodes.filter((n) => n.type === 'IndexNode')[0];
        assertEqual([], indexNode.projections);
        assertFalse(indexNode.indexCoversProjections);
        assertEqual(1, indexNode.indexes.length);
        let index = indexNode.indexes[0];
        assertEqual("persistent", index.type);
        assertEqual(["uid", "dt"], index.fields);
      });
    },
    
    testMultipleIndexesMultipleFiltersAndProjection: function() {
      db[cn].ensureIndex({ type: "persistent", fields: ["uid"] });
      db[cn].ensureIndex({ type: "persistent", fields: ["uid", "dt"] });

      [
        `FOR doc IN ${cn} FILTER doc.uid == 1234 && doc.dt == 1234 RETURN doc.uid`,
        `FOR doc IN ${cn} FILTER doc.uid == 1234 && doc.dt >= 1234 RETURN doc.uid`,
        `FOR doc IN ${cn} FILTER doc.uid == 1234 && doc.dt <= 1234 RETURN doc.uid`,
        `FOR doc IN ${cn} FILTER doc.uid == 1234 && doc.dt >= 1234 && doc.dt <= 9999 RETURN doc.uid`
      ].forEach((q) => {
        let nodes = AQL_EXPLAIN(q).plan.nodes;
        assertEqual(1, nodes.filter((n) => n.type === 'IndexNode').length);
        let indexNode = nodes.filter((n) => n.type === 'IndexNode')[0];
        assertEqual(["uid"], indexNode.projections);
        assertTrue(indexNode.indexCoversProjections);
        assertEqual(1, indexNode.indexes.length);
        let index = indexNode.indexes[0];
        assertEqual("persistent", index.type);
        assertEqual(["uid", "dt"], index.fields);
      });
      
      [
        `FOR doc IN ${cn} FILTER doc.uid == 1234 && doc.dt == 1234 RETURN doc.dt`,
        `FOR doc IN ${cn} FILTER doc.uid == 1234 && doc.dt >= 1234 RETURN doc.dt`,
        `FOR doc IN ${cn} FILTER doc.uid == 1234 && doc.dt <= 1234 RETURN doc.dt`,
        `FOR doc IN ${cn} FILTER doc.uid == 1234 && doc.dt >= 1234 && doc.dt <= 9999 RETURN doc.dt`
      ].forEach((q) => {
        let nodes = AQL_EXPLAIN(q).plan.nodes;
        assertEqual(1, nodes.filter((n) => n.type === 'IndexNode').length);
        let indexNode = nodes.filter((n) => n.type === 'IndexNode')[0];
        assertEqual(["dt"], indexNode.projections);
        assertTrue(indexNode.indexCoversProjections);
        assertEqual(1, indexNode.indexes.length);
        let index = indexNode.indexes[0];
        assertEqual("persistent", index.type);
        assertEqual(["uid", "dt"], index.fields);
      });
      
      [
        `FOR doc IN ${cn} FILTER doc.uid == 1234 && doc.dt == 1234 RETURN [doc.dt, doc.uid]`,
      ].forEach((q) => {
        let nodes = AQL_EXPLAIN(q).plan.nodes;
        assertEqual(1, nodes.filter((n) => n.type === 'IndexNode').length);
        let indexNode = nodes.filter((n) => n.type === 'IndexNode')[0];
        assertEqual(["dt", "uid"], indexNode.projections.sort());
        assertTrue(indexNode.indexCoversProjections);
        assertEqual(1, indexNode.indexes.length);
        let index = indexNode.indexes[0];
        assertEqual("persistent", index.type);
        assertEqual(["uid", "dt"], index.fields);
      });

      [
        `FOR doc IN ${cn} FILTER doc.uid == 1234 && doc.dt >= 1234 RETURN [doc.dt, doc.uid]`,
        `FOR doc IN ${cn} FILTER doc.uid == 1234 && doc.dt <= 1234 RETURN [doc.dt, doc.uid]`,
        `FOR doc IN ${cn} FILTER doc.uid == 1234 && doc.dt >= 1234 && doc.dt <= 9999 RETURN [doc.dt, doc.uid]`
      ].forEach((q) => {
        let nodes = AQL_EXPLAIN(q).plan.nodes;
        assertEqual(1, nodes.filter((n) => n.type === 'IndexNode').length);
        let indexNode = nodes.filter((n) => n.type === 'IndexNode')[0];
        assertEqual(["dt", "uid"], indexNode.projections.sort());
        assertTrue(indexNode.indexCoversProjections);
        assertEqual(1, indexNode.indexes.length);
        let index = indexNode.indexes[0];
        assertEqual("persistent", index.type);
        assertEqual(["uid", "dt"], index.fields);
      });
    },
    
    testSingleIndexSingleFilterAndSort1: function() {
      db[cn].ensureIndex({ type: "persistent", fields: ["uid"] });
      
      [
        `FOR doc IN ${cn} FILTER doc.uid == 1234 SORT doc.dt RETURN doc`,
        `FOR doc IN ${cn} FILTER doc.uid > 1234 SORT doc.dt RETURN doc`,
        `FOR doc IN ${cn} FILTER doc.uid >= 1234 SORT doc.dt RETURN doc`
      ].forEach((q) => {
        let nodes = AQL_EXPLAIN(q).plan.nodes;
        assertEqual(1, nodes.filter((n) => n.type === 'IndexNode').length);
        assertEqual(1, nodes.filter((n) => n.type === 'SortNode').length);
        let indexNode = nodes.filter((n) => n.type === 'IndexNode')[0];
        assertEqual([], indexNode.projections);
        assertFalse(indexNode.indexCoversProjections);
        assertEqual(1, indexNode.indexes.length);
        let index = indexNode.indexes[0];
        assertEqual("persistent", index.type);
        assertEqual(["uid"], index.fields);
      });
    },
    
    testSingleIndexSingleFilterAndSort2: function() {
      db[cn].ensureIndex({ type: "persistent", fields: ["uid"] });
      
      [
        `FOR doc IN ${cn} FILTER doc.uid == 1234 SORT doc.uid RETURN doc`,
        `FOR doc IN ${cn} FILTER doc.uid > 1234 SORT doc.uid RETURN doc`,
        `FOR doc IN ${cn} FILTER doc.uid >= 1234 SORT doc.uid RETURN doc`
      ].forEach((q) => {
        let nodes = AQL_EXPLAIN(q).plan.nodes;
        assertEqual(1, nodes.filter((n) => n.type === 'IndexNode').length);
        assertEqual(0, nodes.filter((n) => n.type === 'SortNode').length);
        let indexNode = nodes.filter((n) => n.type === 'IndexNode')[0];
        assertEqual([], indexNode.projections);
        assertFalse(indexNode.indexCoversProjections);
        assertEqual(1, indexNode.indexes.length);
        let index = indexNode.indexes[0];
        assertEqual("persistent", index.type);
        assertEqual(["uid"], index.fields);
      });
    },
    
    testMultipleIndexesSingleFilterAndSort: function() {
      db[cn].ensureIndex({ type: "persistent", fields: ["uid"] });
      db[cn].ensureIndex({ type: "persistent", fields: ["uid", "dt"] });
      
      [
        `FOR doc IN ${cn} FILTER doc.uid == 1234 SORT doc.dt RETURN doc`,
      ].forEach((q) => {
        let nodes = AQL_EXPLAIN(q).plan.nodes;
        assertEqual(1, nodes.filter((n) => n.type === 'IndexNode').length);
        assertEqual(0, nodes.filter((n) => n.type === 'SortNode').length);
        let indexNode = nodes.filter((n) => n.type === 'IndexNode')[0];
        assertEqual([], indexNode.projections);
        assertFalse(indexNode.indexCoversProjections);
        assertEqual(1, indexNode.indexes.length);
        let index = indexNode.indexes[0];
        assertEqual("persistent", index.type);
        assertEqual(["uid", "dt"], index.fields);
      });

      [
        `FOR doc IN ${cn} FILTER doc.uid > 1234 SORT doc.dt RETURN doc`,
        `FOR doc IN ${cn} FILTER doc.uid >= 1234 SORT doc.dt RETURN doc`
      ].forEach((q) => {
        let nodes = AQL_EXPLAIN(q).plan.nodes;
        assertEqual(1, nodes.filter((n) => n.type === 'IndexNode').length);
        assertEqual(1, nodes.filter((n) => n.type === 'SortNode').length);
        let indexNode = nodes.filter((n) => n.type === 'IndexNode')[0];
        assertEqual([], indexNode.projections);
        assertFalse(indexNode.indexCoversProjections);
        assertEqual(1, indexNode.indexes.length);
        let index = indexNode.indexes[0];
        assertEqual("persistent", index.type);
        assertEqual(["uid", "dt"], index.fields);
      });
    },
    
    testMultipleIndexesMultipleFiltersAndSort: function() {
      db[cn].ensureIndex({ type: "persistent", fields: ["uid"] });
      db[cn].ensureIndex({ type: "persistent", fields: ["uid", "dt"] });

      [
        `FOR doc IN ${cn} FILTER doc.dt == 1234 SORT doc.uid RETURN doc`,
        `FOR doc IN ${cn} FILTER doc.dt >= 1234 SORT doc.uid RETURN doc`,
        `FOR doc IN ${cn} FILTER doc.dt <= 1234 SORT doc.uid RETURN doc`,
        `FOR doc IN ${cn} FILTER doc.dt >= 1234 && doc.dt < 9999 SORT doc.uid RETURN doc`,
      ].forEach((q) => {
        let nodes = AQL_EXPLAIN(q).plan.nodes;
        assertEqual(1, nodes.filter((n) => n.type === 'IndexNode').length);
        assertEqual(0, nodes.filter((n) => n.type === 'SortNode').length);
        let indexNode = nodes.filter((n) => n.type === 'IndexNode')[0];
        assertEqual([], indexNode.projections);
        assertFalse(indexNode.indexCoversProjections);
        assertEqual(1, indexNode.indexes.length);
        let index = indexNode.indexes[0];
        assertEqual("persistent", index.type);
        assertEqual(["uid", "dt"], index.fields);
      });
      
      [
        `FOR doc IN ${cn} FILTER doc.uid == 1234 && doc.dt == 1234 SORT doc.dt RETURN doc`,
        `FOR doc IN ${cn} FILTER doc.uid == 1234 && doc.dt >= 1234 SORT doc.dt RETURN doc`,
        `FOR doc IN ${cn} FILTER doc.uid == 1234 && doc.dt <= 1234 SORT doc.dt RETURN doc`,
        `FOR doc IN ${cn} FILTER doc.uid == 1234 && doc.dt >= 1234 && doc.dt < 9999 SORT doc.dt RETURN doc`,
      ].forEach((q) => {
        let nodes = AQL_EXPLAIN(q).plan.nodes;
        assertEqual(1, nodes.filter((n) => n.type === 'IndexNode').length);
        assertEqual(0, nodes.filter((n) => n.type === 'SortNode').length);
        let indexNode = nodes.filter((n) => n.type === 'IndexNode')[0];
        assertEqual([], indexNode.projections);
        assertFalse(indexNode.indexCoversProjections);
        assertEqual(1, indexNode.indexes.length);
        let index = indexNode.indexes[0];
        assertEqual("persistent", index.type);
        assertEqual(["uid", "dt"], index.fields);
      });

      [
        `FOR doc IN ${cn} FILTER doc.uid > 1234 && doc.dt == 1234 SORT doc.dt RETURN doc`,
        `FOR doc IN ${cn} FILTER doc.uid >= 1234 && doc.dt >= 1234 SORT doc.dt RETURN doc`,
        `FOR doc IN ${cn} FILTER doc.uid >= 1234 && doc.dt <= 1234 SORT doc.dt RETURN doc`,
        `FOR doc IN ${cn} FILTER doc.uid >= 1234 && doc.dt >= 1234 && doc.dt < 9999 SORT doc.dt RETURN doc`
      ].forEach((q) => {
        let nodes = AQL_EXPLAIN(q).plan.nodes;
        assertEqual(1, nodes.filter((n) => n.type === 'IndexNode').length);
        assertEqual(1, nodes.filter((n) => n.type === 'SortNode').length);
        let indexNode = nodes.filter((n) => n.type === 'IndexNode')[0];
        assertEqual([], indexNode.projections);
        assertFalse(indexNode.indexCoversProjections);
        assertEqual(1, indexNode.indexes.length);
        let index = indexNode.indexes[0];
        assertEqual("persistent", index.type);
        assertEqual(["uid", "dt"], index.fields);
      });
    },
    
    testMultipleIndexesFiltersAndSort: function() {
      db[cn].ensureIndex({ type: "persistent", fields: ["uid"] });
      db[cn].ensureIndex({ type: "persistent", fields: ["dt"] });

      [
        `FOR doc IN ${cn} FILTER doc.dt == 1234 SORT doc.uid RETURN doc`,
      ].forEach((q) => {
        let nodes = AQL_EXPLAIN(q).plan.nodes;
        assertEqual(1, nodes.filter((n) => n.type === 'IndexNode').length);
        assertEqual(1, nodes.filter((n) => n.type === 'SortNode').length);
        let indexNode = nodes.filter((n) => n.type === 'IndexNode')[0];
        assertEqual([], indexNode.projections);
        assertFalse(indexNode.indexCoversProjections);
        assertEqual(1, indexNode.indexes.length);
        let index = indexNode.indexes[0];
        assertEqual("persistent", index.type);
        assertEqual(["dt"], index.fields);
      });
      
      [
        `FOR doc IN ${cn} FILTER doc.dt >= 1234 SORT doc.uid RETURN doc`,
        `FOR doc IN ${cn} FILTER doc.dt <= 1234 SORT doc.uid RETURN doc`,
        `FOR doc IN ${cn} FILTER doc.dt >= 1234 && doc.dt < 9999 SORT doc.uid RETURN doc`,
      ].forEach((q) => {
        let nodes = AQL_EXPLAIN(q).plan.nodes;
        assertEqual(1, nodes.filter((n) => n.type === 'IndexNode').length);
        assertEqual(0, nodes.filter((n) => n.type === 'SortNode').length);
        let indexNode = nodes.filter((n) => n.type === 'IndexNode')[0];
        assertEqual([], indexNode.projections);
        assertFalse(indexNode.indexCoversProjections);
        assertEqual(1, indexNode.indexes.length);
        let index = indexNode.indexes[0];
        assertEqual("persistent", index.type);
        assertEqual(["uid"], index.fields);
      });
    },
    
    testSortedCollect: function() {
      db[cn].ensureIndex({ type: "persistent", fields: ["dt"] });

      [
        `FOR doc IN ${cn} COLLECT dt = doc.dt RETURN dt`,
        `FOR doc IN ${cn} FILTER doc.dt == 1234 COLLECT dt = doc.dt RETURN dt`,
        `FOR doc IN ${cn} SORT doc.dt COLLECT dt = doc.dt RETURN dt`,
        `FOR doc IN ${cn} FILTER doc.dt == 1234 SORT doc.dt COLLECT dt = doc.dt RETURN dt`,
      ].forEach((q) => {
        let nodes = AQL_EXPLAIN(q).plan.nodes;
        assertEqual(1, nodes.filter((n) => n.type === 'IndexNode').length);
        assertEqual(0, nodes.filter((n) => n.type === 'SortNode').length);
        let indexNode = nodes.filter((n) => n.type === 'IndexNode')[0];
        assertEqual(["dt"], indexNode.projections);
        assertTrue(indexNode.indexCoversProjections);
        assertEqual(1, indexNode.indexes.length);
        let index = indexNode.indexes[0];
        assertEqual("persistent", index.type);
        assertEqual(["dt"], index.fields);
        let collectNode = nodes.filter((n) => n.type === 'CollectNode')[0];
        assertEqual("sorted", collectNode.collectOptions.method);
      });
    },
    
    testSortedCollectMultipleIndexes: function() {
      db[cn].ensureIndex({ type: "persistent", fields: ["dt"] });
      db[cn].ensureIndex({ type: "persistent", fields: ["uid", "dt"] });

      [
        `FOR doc IN ${cn} FILTER doc.uid == 1234 COLLECT dt = doc.dt RETURN dt`,
        `FOR doc IN ${cn} FILTER doc.uid == 1234 SORT doc.dt COLLECT dt = doc.dt RETURN dt`,
      ].forEach((q) => {
        let nodes = AQL_EXPLAIN(q).plan.nodes;
        assertEqual(1, nodes.filter((n) => n.type === 'IndexNode').length);
        assertEqual(0, nodes.filter((n) => n.type === 'SortNode').length);
        let indexNode = nodes.filter((n) => n.type === 'IndexNode')[0];
        assertEqual(["dt"], indexNode.projections);
        assertTrue(indexNode.indexCoversProjections);
        assertEqual(1, indexNode.indexes.length);
        let index = indexNode.indexes[0];
        assertEqual("persistent", index.type);
        assertEqual(["uid", "dt"], index.fields);
        let collectNode = nodes.filter((n) => n.type === 'CollectNode')[0];
        assertEqual("sorted", collectNode.collectOptions.method);
      });
    },
  };
}

function aqlIndexChoiceMiniCollectionSuite () {
  'use strict';

  let suite = {
    setUpAll: function() {
      setupCollection(cn, 100);
    },
  };

  deriveTestSuite(BaseTestConfig(), suite, '_MiniCollectionSuite');
  return suite;
}

function aqlIndexChoiceSmallCollectionSuite () {
  'use strict';

  let suite = {
    setUpAll: function() {
      setupCollection(cn, 5000);
    },
  };

  deriveTestSuite(BaseTestConfig(), suite, '_SmallCollectionSuite');
  return suite;
}

function aqlIndexChoiceMediumCollectionSuite () {
  'use strict';

  let suite = {
    setUpAll: function() {
      setupCollection(cn, 50000);
    },
  };

  deriveTestSuite(BaseTestConfig(), suite, '_MediumCollectionSuite');
  return suite;
}

function aqlIndexChoiceLargeCollectionSuite () {
  'use strict';

  let suite = {
    setUpAll: function() {
      setupCollection(cn, 150000);
    },
  };

  deriveTestSuite(BaseTestConfig(), suite, '_LargeCollectionSuite');
  return suite;
}

jsunity.run(aqlIndexChoiceMiniCollectionSuite);
jsunity.run(aqlIndexChoiceSmallCollectionSuite);
jsunity.run(aqlIndexChoiceMediumCollectionSuite);
jsunity.run(aqlIndexChoiceLargeCollectionSuite);

return jsunity.done();
