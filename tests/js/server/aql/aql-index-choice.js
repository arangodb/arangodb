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
    docs.push({ _key: "test" + i, uid: i, pid: (i % 100), dt: Date.now() - ((i * 10) % 10000), other: i });
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

    testManyIndexesWithPrefixes: function() {
      db[cn].ensureIndex({ type: "persistent", fields: ["uid", "other2"] });
      db[cn].ensureIndex({ type: "persistent", fields: ["uid", "other3", "other4", "other5"] });
      db[cn].ensureIndex({ type: "persistent", fields: ["uid", "other3", "other4"] });
      db[cn].ensureIndex({ type: "persistent", fields: ["uid", "other6", "other4", "other5"] });
      db[cn].ensureIndex({ type: "persistent", fields: ["uid", "other6", "other4"] });
      db[cn].ensureIndex({ type: "persistent", fields: ["other5", "uid"] });
      db[cn].ensureIndex({ type: "persistent", fields: ["uid", "other6", "pid", "other5"] });
      db[cn].ensureIndex({ type: "persistent", fields: ["uid", "other6", "pid"] });
      db[cn].ensureIndex({ type: "persistent", fields: ["pid", "dt", "uid", "other4"] });
      
      [
        [ `FOR doc IN ${cn} FILTER doc.pid == 1234 FILTER doc.dt == 1234 FILTER doc.uid == 1234 RETURN doc`, ["pid", "dt", "uid", "other4"] ],
        [ `FOR doc IN ${cn} FILTER doc.pid == 1234 FILTER doc.dt == 1234 FILTER doc.uid == 1234 SORT doc.other5 RETURN doc`, ["pid", "dt", "uid", "other4"] ],
      ].forEach((q) => {
        let nodes = AQL_EXPLAIN(q[0]).plan.nodes;
        assertEqual(1, nodes.filter((n) => n.type === 'IndexNode').length);
        let indexNode = nodes.filter((n) => n.type === 'IndexNode')[0];
        assertFalse(indexNode.indexCoversProjections);
        assertEqual(1, indexNode.indexes.length);
        let index = indexNode.indexes[0];
        assertEqual("persistent", index.type);
        assertEqual(q[1], index.fields, q);
      });
    },
    
    testIndexPrefixes: function() {
      db[cn].ensureIndex({ type: "persistent", fields: ["pid"] });
      db[cn].ensureIndex({ type: "persistent", fields: ["pid", "dt"] });
      db[cn].ensureIndex({ type: "persistent", fields: ["uid"] });
      db[cn].ensureIndex({ type: "persistent", fields: ["uid", "dt"] });
      db[cn].ensureIndex({ type: "persistent", fields: ["uid", "pid"] });
      db[cn].ensureIndex({ type: "persistent", fields: ["uid", "pid", "dt"] });
      db[cn].ensureIndex({ type: "persistent", fields: ["uid", "pid", "dt"] });
      db[cn].ensureIndex({ type: "persistent", fields: ["dt", "other"] });
      db[cn].ensureIndex({ type: "persistent", fields: ["uid", "other"] });
      db[cn].ensureIndex({ type: "persistent", fields: ["uid", "other", "pid"] });
     
      [
        [ `FOR doc IN ${cn} FILTER doc.uid == 1234 RETURN doc`, ["uid", "pid", "dt"] ],
        [ `FOR doc IN ${cn} FILTER doc.pid == 1234 RETURN doc`, ["pid", "dt"] ],
        [ `FOR doc IN ${cn} FILTER doc.pid == 1234 && doc.uid == 1234 RETURN doc`, ["uid", "pid", "dt"] ],
        [ `FOR doc IN ${cn} FILTER doc.pid == 1234 && doc.dt == 1234 RETURN doc`, ["pid", "dt"] ],
        [ `FOR doc IN ${cn} FILTER doc.pid == 1234 && doc.dt == 1234 && doc.uid == 1234 RETURN doc`, ["uid", "pid", "dt"] ],
      ].forEach((q) => {
        let nodes = AQL_EXPLAIN(q[0]).plan.nodes;
        assertEqual(1, nodes.filter((n) => n.type === 'IndexNode').length);
        let indexNode = nodes.filter((n) => n.type === 'IndexNode')[0];
        assertFalse(indexNode.indexCoversProjections);
        assertEqual(1, indexNode.indexes.length);
        let index = indexNode.indexes[0];
        assertEqual("persistent", index.type);
        assertEqual(q[1], index.fields, q);
      });
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
    
    testSingleFilterUpperAndLowerBound: function() {
      db[cn].ensureIndex({ type: "persistent", fields: ["uid"] });
      [
        `
          FOR doc IN ${cn}
            LET lowerBound = 1
            LET upperBound = 100
            FILTER doc.uid >= lowerBound AND doc.uid <= upperBound
            RETURN doc
        `,
        `
          FOR doc IN ${cn}
            LET lowerBound = 1
            LET upperBound = 100
            FILTER lowerBound <= doc.uid AND upperBound >= doc.uid
            RETURN doc
        `,
        `
          FOR src IN [{uid: 123}, {uid: 124}]
            LET upperBound = src.uid + 100
            FOR dst IN ${cn}
              FILTER dst.uid >= src.uid AND dst.uid <= upperBound
              RETURN { src, dst }
        `
      ].forEach((q) => {
        const nodes = AQL_EXPLAIN(q, {},
          { optimizer: { rules: ["-interchange-adjacent-enumerations", "-move-filters-into-enumerate"] }}).plan.nodes;
        assertEqual(1, nodes.filter((n) => n.type === 'IndexNode').length);
        const indexNode = nodes.filter((n) => n.type === 'IndexNode')[0];
        assertEqual(1, indexNode.condition.subNodes.length);
        assertEqual("n-ary and", indexNode.condition.subNodes[0].type);
        // index covers both conditions
        assertEqual(2, indexNode.condition.subNodes[0].subNodes.length);
        // since the index covers all conditions, the filter should be completely removed
        assertEqual(0, nodes.filter((n) => n.type === 'FilterNode').length);
      });
    },
    
    testSingleFilterUpperAndLowerBoundWithRedunantCondition: function() {
      db[cn].ensureIndex({ type: "persistent", fields: ["uid"] });
      // generate the cross product of the given arrays
      const product = (...a) => a.reduce((a, b) => a.flatMap(d => b.map(e => [d, e].flat())));
      // generate a list of all permutations of the given array
      const perm = a => a.length
        ? a.reduce((r, v, i) => [...r, ...perm([...a.slice(0, i), ...a.slice(i + 1)]).map(x => [v, ...x])], [])
        : [[]];
        
      // lowerBound = 10
      // upperBound = 100
      const lowerBound = ["doc.uid >= lowerBound", "lowerBound <= doc.uid"];
      const upperBound = ["doc.uid <= upperBound", "upperBound >= doc.uid"];
      const bounds = product(lowerBound, upperBound);
      [
        // TODO - the commented cases currently do not work correctly.
        // The optimizer picks the wrong (weaker) bound and keeps an additional filter for
        // the stronger bound. The problem is Condition::optimize which does not properly
        // handle these scenarios, but this needs to be fixed in a separate PR.
        [["doc.uid > 5", "5 < doc.uid"], ">=", 10, "<=", 100],
        [["doc.uid >= 5", "5 <= doc.uid"], ">=", 10, "<=", 100],
        [["doc.uid > 42", "42 < doc.uid"], ">", 42, "<=", 100],
        //[["doc.uid >= 42", "42 <= doc.uid"], ">=", 42, "<=", 100],
        [["doc.uid < 42", "42 > doc.uid"], ">=", 10, "<", 42],
        //[["doc.uid <= 42", "42 >= doc.uid"], ">=", 10, "<=", 42],
        //[["doc.uid < 1234", "1234 > doc.uid"], ">=", 10, "<=", 100],
        //[["doc.uid <= 1234", "1234 >= doc.uid"], ">=", 10, "<=", 100],
        [["doc.uid != 5"], ">=", 10, "<=", 100],
        [["doc.uid != 1234"], ">=", 10, "<=", 100],
        [product(["doc.uid > 20", "20 < doc.uid"],  ["doc.uid < 90", "90 > doc.uid"]), ">", 20, "<", 90],
        //[product(["doc.uid >= 20", "20 <= doc.uid"],  ["doc.uid <= 90", "90 >= doc.uid"]), ">=", 20, "<=", 90],
      ].forEach(([filters, lbOp, lb, ubOp, ub]) => {
        product(bounds, filters)
          .map(list => perm(list.flat()))
          .flat(1)
          .forEach(filter => {
            const q = `
                FOR doc IN ${cn}
                  LET lowerBound = 10
                  LET upperBound = 100
                  FILTER ${filter.join(" AND ")}
                  RETURN doc
              `;
            const nodes = AQL_EXPLAIN(q, {}, { optimizer: { rules: ["-move-filters-into-enumerate"] }}).plan.nodes;
            assertEqual(1, nodes.filter((n) => n.type === 'IndexNode').length);
            const indexNode = nodes.filter((n) => n.type === 'IndexNode')[0];
            assertEqual(1, indexNode.condition.subNodes.length);
            assertEqual("n-ary and", indexNode.condition.subNodes[0].type);
            // index covers both conditions, so both conditions should be present in the index node
            assertEqual(2, indexNode.condition.subNodes[0].subNodes.length);
            const conds = indexNode.condition.subNodes[0].subNodes;
            assertEqual("compare " + lbOp, conds[0].type);
            assertEqual(lb, conds[0].subNodes[1].value);
            assertEqual("compare " + ubOp, conds[1].type);
            assertEqual(ub, conds[1].subNodes[1].value);
            // since the index covers all conditions, the filter should be completely removed
            assertEqual(0, nodes.filter((n) => n.type === 'FilterNode').length);
          });
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
