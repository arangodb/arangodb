/*jshint globalstrict:false, strict:false, maxlen: 500 */
/*global assertTrue, assertFalse, assertEqual, assertNotEqual, AQL_EXPLAIN, AQL_EXECUTE */

////////////////////////////////////////////////////////////////////////////////
/// @brief tests for sparse index usage
///
/// @file
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
const db = require("@arangodb").db;

function optimizerSparseTestSuite () {
  let c;

  return {
    setUp : function () {
      db._drop("UnitTestsCollection");
      c = db._create("UnitTestsCollection");

      for (let i = 0; i < 2000; ++i) {
        c.insert({ _key: "test" + i, value1: i });
      }
    },

    tearDown : function () {
      db._drop("UnitTestsCollection");
    },

    testSparseHashEq : function () {
      c.ensureIndex({ type: "hash", fields: ["value1"], sparse: true });
      
      let query = "FOR doc IN " + c.name() + " FILTER doc.value1 == 10 RETURN doc";
      let results = AQL_EXECUTE(query).json;
      assertEqual(1, results.length);

      let nodes = AQL_EXPLAIN(query).plan.nodes;
      let nodeTypes = nodes.map(function(n) { return n.type; });
      let index = nodeTypes.indexOf("IndexNode");
      assertNotEqual(-1, index);
      assertTrue(nodes[index].indexes[0].sparse);
    },
    
    testSparseHashEqNull : function () {
      c.ensureIndex({ type: "hash", fields: ["value1"], sparse: true });
      
      let query = "FOR doc IN " + c.name() + " FILTER doc.value1 == null RETURN doc";
      let results = AQL_EXECUTE(query).json;
      assertEqual(0, results.length);

      let nodes = AQL_EXPLAIN(query).plan.nodes;
      let nodeTypes = nodes.map(function(n) { return n.type; });
      let index = nodeTypes.indexOf("IndexNode");
      assertEqual(-1, index);
    },
    
    testSparseHashEqFunc : function () {
      c.ensureIndex({ type: "hash", fields: ["value1"], sparse: true });
      
      let query = "FOR doc IN " + c.name() + " FILTER doc.value1 == NOOPT(10) RETURN doc";
      let results = AQL_EXECUTE(query).json;
      assertEqual(1, results.length);

      let nodes = AQL_EXPLAIN(query).plan.nodes;
      let nodeTypes = nodes.map(function(n) { return n.type; });
      let index = nodeTypes.indexOf("IndexNode");
      assertEqual(-1, index);
    },
    
    testSparseHashEqFuncAndNotNull : function () {
      c.ensureIndex({ type: "hash", fields: ["value1"], sparse: true });
      
      let query = "FOR doc IN " + c.name() + " FILTER doc.value1 == NOOPT(10) && doc.value1 != null RETURN doc";
      let results = AQL_EXECUTE(query).json;
      assertEqual(1, results.length);

      let nodes = AQL_EXPLAIN(query).plan.nodes;
      let nodeTypes = nodes.map(function(n) { return n.type; });
      let index = nodeTypes.indexOf("IndexNode");
      assertNotEqual(-1, index);
      assertTrue(nodes[index].indexes[0].sparse);
    },
    
    testSparseSkiplistEq : function () {
      c.ensureIndex({ type: "skiplist", fields: ["value1"], sparse: true });
      
      let query = "FOR doc IN " + c.name() + " FILTER doc.value1 == 10 RETURN doc";
      let results = AQL_EXECUTE(query).json;
      assertEqual(1, results.length);

      let nodes = AQL_EXPLAIN(query).plan.nodes;
      let nodeTypes = nodes.map(function(n) { return n.type; });
      let index = nodeTypes.indexOf("IndexNode");
      assertNotEqual(-1, index);
      assertTrue(nodes[index].indexes[0].sparse);
    },
    
    testSparseSkiplistGt : function () {
      c.ensureIndex({ type: "skiplist", fields: ["value1"], sparse: true });
      
      let query = "FOR doc IN " + c.name() + " FILTER doc.value1 > 10 RETURN doc";
      let results = AQL_EXECUTE(query).json;
      assertEqual(1989, results.length);

      let nodes = AQL_EXPLAIN(query).plan.nodes;
      let nodeTypes = nodes.map(function(n) { return n.type; });
      let index = nodeTypes.indexOf("IndexNode");
      assertNotEqual(-1, index);
      assertTrue(nodes[index].indexes[0].sparse);
    },
    
    testSparseSkiplistGe : function () {
      c.ensureIndex({ type: "skiplist", fields: ["value1"], sparse: true });
      
      let query = "FOR doc IN " + c.name() + " FILTER doc.value1 >= 10 RETURN doc";
      let results = AQL_EXECUTE(query).json;
      assertEqual(1990, results.length);

      let nodes = AQL_EXPLAIN(query).plan.nodes;
      let nodeTypes = nodes.map(function(n) { return n.type; });
      let index = nodeTypes.indexOf("IndexNode");
      assertNotEqual(-1, index);
      assertTrue(nodes[index].indexes[0].sparse);
    },
    
    testSparseSkiplistLt : function () {
      c.ensureIndex({ type: "skiplist", fields: ["value1"], sparse: true });
      
      let query = "FOR doc IN " + c.name() + " FILTER doc.value1 < 10 RETURN doc";
      let results = AQL_EXECUTE(query).json;
      assertEqual(10, results.length);

      let nodes = AQL_EXPLAIN(query).plan.nodes;
      let nodeTypes = nodes.map(function(n) { return n.type; });
      let index = nodeTypes.indexOf("IndexNode");
      assertEqual(-1, index);
    },
    
    testSparseSkiplistLe : function () {
      c.ensureIndex({ type: "skiplist", fields: ["value1"], sparse: true });
      
      let query = "FOR doc IN " + c.name() + " FILTER doc.value1 <= 10 RETURN doc";
      let results = AQL_EXECUTE(query).json;
      assertEqual(11, results.length);

      let nodes = AQL_EXPLAIN(query).plan.nodes;
      let nodeTypes = nodes.map(function(n) { return n.type; });
      let index = nodeTypes.indexOf("IndexNode");
      assertEqual(-1, index);
    },
    
    testSparseSkiplistLeNotNull : function () {
      c.ensureIndex({ type: "skiplist", fields: ["value1"], sparse: true });
      
      let query = "FOR doc IN " + c.name() + " FILTER doc.value1 <= 10 && doc.value1 != null RETURN doc";
      let results = AQL_EXECUTE(query).json;
      assertEqual(11, results.length);

      let nodes = AQL_EXPLAIN(query).plan.nodes;
      let nodeTypes = nodes.map(function(n) { return n.type; });
      let index = nodeTypes.indexOf("IndexNode");
      assertNotEqual(-1, index);
      assertTrue(nodes[index].indexes[0].sparse);
    },
    
    testSparseSkiplistLtNotNull : function () {
      c.ensureIndex({ type: "skiplist", fields: ["value1"], sparse: true });
      
      let query = "FOR doc IN " + c.name() + " FILTER doc.value1 < 10 && doc.value1 != null RETURN doc";
      let results = AQL_EXECUTE(query).json;
      assertEqual(10, results.length);

      let nodes = AQL_EXPLAIN(query).plan.nodes;
      let nodeTypes = nodes.map(function(n) { return n.type; });
      let index = nodeTypes.indexOf("IndexNode");
      assertNotEqual(-1, index);
      assertTrue(nodes[index].indexes[0].sparse);
    },
    
    testSparseSkiplistGeNullRange : function () {
      c.ensureIndex({ type: "skiplist", fields: ["value1"], sparse: true });
      
      let query = "FOR doc IN " + c.name() + " FILTER doc.value1 >= null && doc.value1 <= 10 RETURN doc";
      let results = AQL_EXECUTE(query).json;
      assertEqual(11, results.length);

      let nodes = AQL_EXPLAIN(query).plan.nodes;
      let nodeTypes = nodes.map(function(n) { return n.type; });
      let index = nodeTypes.indexOf("IndexNode");
      assertEqual(-1, index);
    },
    
    testSparseSkiplistGeNullRangeNotNull : function () {
      c.ensureIndex({ type: "skiplist", fields: ["value1"], sparse: true });
      
      let query = "FOR doc IN " + c.name() + " FILTER doc.value1 >= null && doc.value1 <= 10 && doc.value1 != null RETURN doc";
      let results = AQL_EXECUTE(query).json;
      assertEqual(11, results.length);

      let nodes = AQL_EXPLAIN(query).plan.nodes;
      let nodeTypes = nodes.map(function(n) { return n.type; });
      let index = nodeTypes.indexOf("IndexNode");
      assertNotEqual(-1, index);
      assertTrue(nodes[index].indexes[0].sparse);
    },
    
    testSparseSkiplistGtNullRange : function () {
      c.ensureIndex({ type: "skiplist", fields: ["value1"], sparse: true });
      
      let query = "FOR doc IN " + c.name() + " FILTER doc.value1 > null && doc.value1 <= 10 RETURN doc";
      let results = AQL_EXECUTE(query).json;
      assertEqual(11, results.length);

      let nodes = AQL_EXPLAIN(query).plan.nodes;
      let nodeTypes = nodes.map(function(n) { return n.type; });
      let index = nodeTypes.indexOf("IndexNode");
      assertNotEqual(-1, index);
      assertTrue(nodes[index].indexes[0].sparse);
    },
    
    testSparseSkiplistEqNull : function () {
      c.ensureIndex({ type: "skiplist", fields: ["value1"], sparse: true });
      
      let query = "FOR doc IN " + c.name() + " FILTER doc.value1 == null RETURN doc";
      let results = AQL_EXECUTE(query).json;
      assertEqual(0, results.length);

      let nodes = AQL_EXPLAIN(query).plan.nodes;
      let nodeTypes = nodes.map(function(n) { return n.type; });
      let index = nodeTypes.indexOf("IndexNode");
      assertEqual(-1, index);
    },
    
    testSparseSkiplistEqFunc : function () {
      c.ensureIndex({ type: "skiplist", fields: ["value1"], sparse: true });
      
      let query = "FOR doc IN " + c.name() + " FILTER doc.value1 == NOOPT(10) RETURN doc";
      let results = AQL_EXECUTE(query).json;
      assertEqual(1, results.length);

      let nodes = AQL_EXPLAIN(query).plan.nodes;
      let nodeTypes = nodes.map(function(n) { return n.type; });
      let index = nodeTypes.indexOf("IndexNode");
      assertEqual(-1, index);
    },
    
    testSparseSkiplistEqFuncAndNotNull : function () {
      c.ensureIndex({ type: "skiplist", fields: ["value1"], sparse: true });
      
      let query = "FOR doc IN " + c.name() + " FILTER doc.value1 == NOOPT(10) && doc.value1 != null RETURN doc";
      let results = AQL_EXECUTE(query).json;
      assertEqual(1, results.length);

      let nodes = AQL_EXPLAIN(query).plan.nodes;
      let nodeTypes = nodes.map(function(n) { return n.type; });
      let index = nodeTypes.indexOf("IndexNode");
      assertNotEqual(-1, index);
      assertTrue(nodes[index].indexes[0].sparse);
    },
  };
}

jsunity.run(optimizerSparseTestSuite);

return jsunity.done();
