/*jshint globalstrict:false, strict:false */
/*global assertEqual, assertTrue, AQL_EXPLAIN */

////////////////////////////////////////////////////////////////////////////////
/// @brief test the index
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2018-2019 ArangoDB GmbH, Cologne, Germany
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
/// @author 2018 Jan Steemann
////////////////////////////////////////////////////////////////////////////////

const jsunity = require("jsunity");
const internal = require("internal");
const db = internal.db;

function indexSelectivitySuite() {
  'use strict';
  const cn = "UnitTestsCollectionIdx";

  let assertIndexUsed = function(expected, plan) {
    let nodes = plan.nodes.filter(function(node) {
      return node.type === 'IndexNode';
    });
    assertEqual(1, nodes.length);
    let node = nodes[0];
    assertEqual(expected, node.indexes[0].fields);
  };
  
  return {
    setUp : function () {
      db._drop(cn);
      db._create(cn);
    },

    tearDown : function () {
      db._drop(cn);
    },
    
    testTwoIndexesSingleField: function () {
      let c = db._collection(cn);
      c.ensureIndex({ type: "hash", fields: ["a"] });
      c.ensureIndex({ type: "hash", fields: ["b"] });

      // index on "a" has lower selectivity than index on "b"
      for (let i = 0; i < 1000; ++i) {
        c.insert({ a: (i < 100 ? i : 100), b: (i < 200 ? i : 200), c: i });
      }

      internal.waitForEstimatorSync();
      let indexes = c.indexes();
      assertEqual(["a"], indexes[1].fields);
      assertEqual(["b"], indexes[2].fields);
      assertTrue(indexes[1].selectivityEstimate < indexes[2].selectivityEstimate);

      let query, plan;
      
      query = "FOR doc IN @@collection FILTER doc.a == @value RETURN doc";
      plan = AQL_EXPLAIN(query, { "@collection": cn, value: 2 }).plan;
      assertIndexUsed(["a"], plan);
      
      query = "FOR doc IN @@collection FILTER doc.b == @value RETURN doc";
      plan = AQL_EXPLAIN(query, { "@collection": cn, value: 2 }).plan;
      assertIndexUsed(["b"], plan);
    },
    
    testTwoIndexesMultipleFields: function () {
      let c = db._collection(cn);
      c.ensureIndex({ type: "hash", fields: ["a"] });
      c.ensureIndex({ type: "hash", fields: ["b"] });

      // index on "a" has lower selectivity than index on "b"
      for (let i = 0; i < 1000; ++i) {
        c.insert({ a: (i < 100 ? i : 100), b: (i < 200 ? i : 200), c: i });
      }

      internal.waitForEstimatorSync();
      let indexes = c.indexes();
      assertEqual(["a"], indexes[1].fields);
      assertEqual(["b"], indexes[2].fields);
      assertTrue(indexes[1].selectivityEstimate < indexes[2].selectivityEstimate);

      let query, plan;
      
      query = "FOR doc IN @@collection FILTER doc.a == @value && doc.b == @value RETURN doc";
      plan = AQL_EXPLAIN(query, { "@collection": cn, value: 2 }).plan;
      assertIndexUsed(["b"], plan);
      
      query = "FOR doc IN @@collection FILTER doc.b == @value && doc.a == @value RETURN doc";
      plan = AQL_EXPLAIN(query, { "@collection": cn, value: 2 }).plan;
      assertIndexUsed(["b"], plan);
    },
    
    testTwoIndexesMultipleFieldsOtherIndexCreationOrder: function () {
      let c = db._collection(cn);
      c.ensureIndex({ type: "hash", fields: ["b"] });
      c.ensureIndex({ type: "hash", fields: ["a"] });

      // index on "a" has lower selectivity than index on "b"
      for (let i = 0; i < 1000; ++i) {
        c.insert({ a: (i < 100 ? i : 100), b: (i < 200 ? i : 200), c: i });
      }

      internal.waitForEstimatorSync();
      let indexes = c.indexes();
      assertEqual(["b"], indexes[1].fields);
      assertEqual(["a"], indexes[2].fields);
      assertTrue(indexes[1].selectivityEstimate > indexes[2].selectivityEstimate);

      let query, plan;
      
      query = "FOR doc IN @@collection FILTER doc.a == @value && doc.b == @value RETURN doc";
      plan = AQL_EXPLAIN(query, { "@collection": cn, value: 2 }).plan;
      assertIndexUsed(["b"], plan);
      
      query = "FOR doc IN @@collection FILTER doc.b == @value && doc.a == @value RETURN doc";
      plan = AQL_EXPLAIN(query, { "@collection": cn, value: 2 }).plan;
      assertIndexUsed(["b"], plan);
    },
    
    testTwoCompositeIndexesMultipleFields: function () {
      let c = db._collection(cn);
      c.ensureIndex({ type: "hash", fields: ["a", "b"] });
      c.ensureIndex({ type: "hash", fields: ["a", "b", "c"] });

      // index on "a" has lower selectivity than index on "b"
      for (let i = 0; i < 1000; ++i) {
        c.insert({ a: (i < 100 ? i : 100), b: (i < 200 ? i : 200), c: i });
      }

      internal.waitForEstimatorSync();
      let indexes = c.indexes();
      assertEqual(["a", "b"], indexes[1].fields);
      assertEqual(["a", "b", "c"], indexes[2].fields);
      assertTrue(indexes[1].selectivityEstimate < indexes[2].selectivityEstimate);

      let query, plan;
      
      query = "FOR doc IN @@collection FILTER doc.a == @value && doc.b == @value && doc.c == @value RETURN doc";
      plan = AQL_EXPLAIN(query, { "@collection": cn, value: 2 }).plan;
      assertIndexUsed(["a", "b", "c"], plan);
    },
    
    testTwoCompositeIndexesMultipleFieldsOtherIndexCreationOrder: function () {
      let c = db._collection(cn);
      c.ensureIndex({ type: "hash", fields: ["a", "b", "c"] });
      c.ensureIndex({ type: "hash", fields: ["a", "b"] });

      // index on "a" has lower selectivity than index on "b"
      for (let i = 0; i < 1000; ++i) {
        c.insert({ a: (i < 100 ? i : 100), b: (i < 200 ? i : 200), c: i });
      }

      internal.waitForEstimatorSync();
      let indexes = c.indexes();
      assertEqual(["a", "b", "c"], indexes[1].fields);
      assertEqual(["a", "b"], indexes[2].fields);
      assertTrue(indexes[1].selectivityEstimate > indexes[2].selectivityEstimate);

      let query, plan;
      
      query = "FOR doc IN @@collection FILTER doc.a == @value && doc.b == @value && doc.c == @value RETURN doc";
      plan = AQL_EXPLAIN(query, { "@collection": cn, value: 2 }).plan;
      assertIndexUsed(["a", "b", "c"], plan);
    },
    
    testTwoCompositeIndexesMultipleFieldsPartialLookup: function () {
      let c = db._collection(cn);
      c.ensureIndex({ type: "hash", fields: ["a", "b"] });
      c.ensureIndex({ type: "hash", fields: ["a", "b", "c"] });

      // index on "a" has lower selectivity than index on "b"
      for (let i = 0; i < 1000; ++i) {
        c.insert({ a: (i < 100 ? i : 100), b: (i < 200 ? i : 200), c: i });
      }

      internal.waitForEstimatorSync();
      let indexes = c.indexes();
      assertEqual(["a", "b"], indexes[1].fields);
      assertEqual(["a", "b", "c"], indexes[2].fields);
      assertTrue(indexes[1].selectivityEstimate < indexes[2].selectivityEstimate);

      let query, plan;
      
      query = "FOR doc IN @@collection FILTER doc.a == @value && doc.b == @value RETURN doc";
      plan = AQL_EXPLAIN(query, { "@collection": cn, value: 2 }).plan;
      assertIndexUsed(["a", "b"], plan);
    },
    
    testTwoCompositeIndexesMultipleFieldsPartialLookupOtherIndexCreationOrder: function () {
      let c = db._collection(cn);
      c.ensureIndex({ type: "hash", fields: ["a", "b", "c"] });
      c.ensureIndex({ type: "hash", fields: ["a", "b"] });

      // index on "a" has lower selectivity than index on "b"
      for (let i = 0; i < 1000; ++i) {
        c.insert({ a: (i < 100 ? i : 100), b: (i < 200 ? i : 200), c: i });
      }

      internal.waitForEstimatorSync();
      let indexes = c.indexes();
      assertEqual(["a", "b", "c"], indexes[1].fields);
      assertEqual(["a", "b"], indexes[2].fields);
      assertTrue(indexes[1].selectivityEstimate > indexes[2].selectivityEstimate);

      let query, plan;
      
      query = "FOR doc IN @@collection FILTER doc.a == @value && doc.b == @value RETURN doc";
      plan = AQL_EXPLAIN(query, { "@collection": cn, value: 2 }).plan;
      assertIndexUsed(["a", "b"], plan);
    },
    
  };

}

jsunity.run(indexSelectivitySuite);

return jsunity.done();
