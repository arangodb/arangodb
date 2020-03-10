/*jshint globalstrict:false, strict:false, maxlen: 500 */
/*global assertEqual, assertNotEqual, assertTrue, assertFalse, AQL_EXPLAIN, AQL_EXECUTE */

////////////////////////////////////////////////////////////////////////////////
/// @brief tests for optimizer rules
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2010-2014 triagens GmbH, Cologne, Germany
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
/// @author 
/// @author Copyright 2014, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

var db = require("@arangodb").db;
var jsunity = require("jsunity");
var helper = require("@arangodb/aql-helper");

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////

function gatherBlockTestSuite () {
  var cn1 = "UnitTestsGatherBlock1";
  var cn2 = "UnitTestsGatherBlock2";
  var cn3 = "UnitTestsGatherBlock3";
  var cn4 = "UnitTestsGatherBlock4";
  var c1, c2, c3, c4;
  var idx;
 
  var explain = function (result) {
    return helper.getCompactPlan(result).map(function(node) 
        { return node.type; });
  };

  return {

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief set up
    ////////////////////////////////////////////////////////////////////////////////

    setUpAll: function() {
      var j, k;
      db._drop(cn1);
      db._drop(cn2);
      db._drop(cn3);
      c1 = db._create(cn1, {numberOfShards:3});
      let docs1 = [];
      c2 = db._create(cn2);
      let docs2 = [];
      for (j = 0; j < 400; j++) {
        for (k = 0; k < 10; k++){
          docs1.push({Hallo:k});
          docs2.push({Hallo:k});
        }
      }
      c1.insert(docs1);
      c2.insert(docs2);
      c3 = db._create(cn3, {numberOfShards:3});
      let docs3 = [];
      for (k = 0; k < 10; k++){
        docs3.push({Hallo:k});
      }
      c3.insert(docs3);
    },

    setUp: function() {
      idx = null;
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief tear down
    ////////////////////////////////////////////////////////////////////////////////

    tearDownAll: function () {
      db._drop(cn1);
      db._drop(cn2);
      db._drop(cn3);
      db._drop(cn4);
      c1 = null;
      c2 = null;
      c3 = null;
      c4 = null;
    },

    tearDown: function() {
      assertEqual(c1.count(), 4000);
      if (idx !== null) {
        db._dropIndex(idx);
        idx = null;
      }
      db._drop(cn4);
    },

    testMoreShardsThanDocumentsHeapSort : function () {
      db._drop(cn4);
      c4 = db._create(cn4, {numberOfShards:40});
      let docs = [];
      for (let i = 0; i < 39; ++i) {
        docs.push({ value : i });
      }
      c4.insert(docs);
      
      let query = "FOR doc IN " + cn4 + " SORT doc.value RETURN doc.value";
      // check the return value
      let result = AQL_EXECUTE(query).json;
      assertEqual(39, result.length);
      let last = -1;
      result.forEach(function(value) {
        assertTrue(value >= last);
        last = value;
      });
      
      let nodes = AQL_EXPLAIN(query).plan.nodes;
      let nodeTypes = nodes.map(function(node) { return node.type; });
      // must have a sort and gather node
      assertNotEqual(-1, nodeTypes.indexOf("SortNode"));
      let sortNode = nodes[nodeTypes.indexOf("SortNode")];
      assertEqual(1, sortNode.elements.length);
      assertTrue(sortNode.elements[0].ascending);
      assertNotEqual(-1, nodeTypes.indexOf("GatherNode"));
      let gatherNode = nodes[nodeTypes.indexOf("GatherNode")];
      assertEqual(1, gatherNode.elements.length); // must be a sorting GatherNode
      assertEqual("heap", gatherNode.sortmode);
    },
    
    testMoreShardsThanDocumentsMinElementSort : function () {
      db._drop(cn4);
      c4 = db._create(cn4, {numberOfShards:4});
      let docs = [];
      for (let i = 0; i < 3; ++i) {
        docs.push({ value : i });
      }
      c4.insert(docs);

      let query = "FOR doc IN " + cn4 + " SORT doc.value RETURN doc.value";
      // check the return value
      let result = AQL_EXECUTE(query).json;
      assertEqual(3, result.length);
      let last = -1;
      result.forEach(function(value) {
        assertTrue(value >= last);
        last = value;
      });
      
      let nodes = AQL_EXPLAIN(query).plan.nodes;
      let nodeTypes = nodes.map(function(node) { return node.type; });
      // must have a sort and gather node
      assertNotEqual(-1, nodeTypes.indexOf("SortNode"));
      let sortNode = nodes[nodeTypes.indexOf("SortNode")];
      assertEqual(1, sortNode.elements.length);
      assertTrue(sortNode.elements[0].ascending);
      assertNotEqual(-1, nodeTypes.indexOf("GatherNode"));
      let gatherNode = nodes[nodeTypes.indexOf("GatherNode")];
      assertEqual(1, gatherNode.elements.length); // must be a sorting GatherNode
      assertEqual("minelement", gatherNode.sortmode);
    },
    
    testMoreShardsThanDocumentsNoSort : function () {
      db._drop(cn4);
      c4 = db._create(cn4, {numberOfShards:40});
      let docs = [];
      for (let i = 0; i < 39; ++i) {
        docs.push({ value : i });
      }
      c4.save(docs);
      
      let query = "FOR doc IN " + cn4 + " RETURN doc.value";
      // check the return value
      let result = AQL_EXECUTE(query).json;
      assertEqual(39, result.length);
      
      let nodes = AQL_EXPLAIN(query).plan.nodes;
      let nodeTypes = nodes.map(function(node) { return node.type; });
      // must not have a sort, but a gather node
      assertEqual(-1, nodeTypes.indexOf("SortNode"));
      assertNotEqual(-1, nodeTypes.indexOf("GatherNode"));
      let gatherNode = nodes[nodeTypes.indexOf("GatherNode")];
      assertEqual(0, gatherNode.elements.length); // must be a non-sorting GatherNode
      assertEqual("unset", gatherNode.sortmode);
    },
    
    testMoreDocumentsThanShardsHeapSort : function () {
      db._drop(cn4);
      c4 = db._create(cn4, {numberOfShards:10});
      let docs = [];
      for (let i = 0; i < 20000; ++i) {
        docs.push({ value: i });
        if (docs.length === 1000) {
          c4.insert(docs);
          docs = [];
        }
      }
      c4.insert(docs);
      
      let query = "FOR doc IN " + cn4 + " SORT doc.value RETURN doc.value";
      // check the return value
      let result = AQL_EXECUTE(query).json;
      assertEqual(20000, result.length);
      let last = -1;
      result.forEach(function(value) {
        assertTrue(value >= last);
        last = value;
      });
      
      let nodes = AQL_EXPLAIN(query).plan.nodes;
      let nodeTypes = nodes.map(function(node) { return node.type; });
      // must have a sort and gather node
      assertNotEqual(-1, nodeTypes.indexOf("SortNode"));
      let sortNode = nodes[nodeTypes.indexOf("SortNode")];
      assertEqual(1, sortNode.elements.length);
      assertTrue(sortNode.elements[0].ascending);
      assertNotEqual(-1, nodeTypes.indexOf("GatherNode"));
      let gatherNode = nodes[nodeTypes.indexOf("GatherNode")];
      assertEqual(1, gatherNode.elements.length); // must be a sorting GatherNode
      assertEqual("heap", gatherNode.sortmode);
    },
    
    testMoreDocumentsThanShardsMinElementSort : function () {
      db._drop(cn4);
      c4 = db._create(cn4, {numberOfShards:4});
      let docs = [];
      for (let i = 0; i < 20000; ++i) {
        docs.push({ value: i });
        if (docs.length === 1000) {
          c4.insert(docs);
          docs = [];
        }
      }
      c4.insert(docs);
      
      let query = "FOR doc IN " + cn4 + " SORT doc.value RETURN doc.value";
      // check the return value
      let result = AQL_EXECUTE(query).json;
      assertEqual(20000, result.length);
      let last = -1;
      result.forEach(function(value) {
        assertTrue(value >= last);
        last = value;
      });
      
      let nodes = AQL_EXPLAIN(query).plan.nodes;
      let nodeTypes = nodes.map(function(node) { return node.type; });
      // must have a sort and gather node
      assertNotEqual(-1, nodeTypes.indexOf("SortNode"));
      let sortNode = nodes[nodeTypes.indexOf("SortNode")];
      assertEqual(1, sortNode.elements.length);
      assertTrue(sortNode.elements[0].ascending);
      assertNotEqual(-1, nodeTypes.indexOf("GatherNode"));
      let gatherNode = nodes[nodeTypes.indexOf("GatherNode")];
      assertEqual(1, gatherNode.elements.length); // must be a sorting GatherNode
      assertEqual("minelement", gatherNode.sortmode);
    },
    
    testMoreDocumentsThanShardsNoSort : function () {
      db._drop(cn4);
      c4 = db._create(cn4, {numberOfShards:20});
      let docs = [];
      for (let i = 0; i < 20000; ++i) {
        docs.push({ value: i });
        if (docs.length === 1000) {
          c4.insert(docs);
          docs = [];
        }
      }
      c4.insert(docs);
      
      let query = "FOR doc IN " + cn4 + " RETURN doc.value";
      // check the return value
      let result = AQL_EXECUTE(query).json;
      assertEqual(20000, result.length);
      
      let nodes = AQL_EXPLAIN(query).plan.nodes;
      let nodeTypes = nodes.map(function(node) { return node.type; });
      // must not have a sort, but a gather node
      assertEqual(-1, nodeTypes.indexOf("SortNode"));
      assertNotEqual(-1, nodeTypes.indexOf("GatherNode"));
      let gatherNode = nodes[nodeTypes.indexOf("GatherNode")];
      assertEqual(0, gatherNode.elements.length); // must be a non-sorting GatherNode
      assertEqual("unset", gatherNode.sortmode);
    },

    testSingleShard : function () {
      let query = "FOR doc IN " + cn2 + " SORT doc.Hallo RETURN doc.Hallo";
      // check the return value
      let result = AQL_EXECUTE(query).json;
      let last = -1;
      result.forEach(function(value) {
        assertTrue(value >= last);
        last = value;
      });
      
      let nodes = AQL_EXPLAIN(query).plan.nodes;
      let nodeTypes = nodes.map(function(node) { return node.type; });
      // must have a sort and gather node
      assertNotEqual(-1, nodeTypes.indexOf("SortNode"));
      let sortNode = nodes[nodeTypes.indexOf("SortNode")];
      assertEqual(1, sortNode.elements.length);
      assertTrue(sortNode.elements[0].ascending);
      assertNotEqual(-1, nodeTypes.indexOf("GatherNode"));
      let gatherNode = nodes[nodeTypes.indexOf("GatherNode")];
      assertEqual([], gatherNode.elements); // no sorting in GatherNode
    },
    
    testSingleShardDescending : function () {
      let query = "FOR doc IN " + cn2 + " SORT doc.Hallo DESC RETURN doc.Hallo";
      // check the return value
      let result = AQL_EXECUTE(query).json;
      let last = 99999999999;
      result.forEach(function(value) {
        assertTrue(value <= last);
        last = value;
      });
      
      let nodes = AQL_EXPLAIN(query).plan.nodes;
      let nodeTypes = nodes.map(function(node) { return node.type; });
      // must have a sort and gather node
      assertNotEqual(-1, nodeTypes.indexOf("SortNode"));
      let sortNode = nodes[nodeTypes.indexOf("SortNode")];
      assertEqual(1, sortNode.elements.length);
      assertFalse(sortNode.elements[0].ascending);
      assertNotEqual(-1, nodeTypes.indexOf("GatherNode"));
      let gatherNode = nodes[nodeTypes.indexOf("GatherNode")];
      assertEqual([], gatherNode.elements); // no sorting in GatherNode
    },

    testSingleShardWithIndex : function () {
      idx = c2.ensureIndex({ type: "skiplist", fields: ["Hallo"] });
      let query = "FOR doc IN " + cn2 + " SORT doc.Hallo RETURN doc.Hallo";
      // check the return value
      let result = AQL_EXECUTE(query).json;
      let last = -1;
      result.forEach(function(value) {
        assertTrue(value >= last);
        last = value;
      });
      
      let nodes = AQL_EXPLAIN(query).plan.nodes;
      let nodeTypes = nodes.map(function(node) { return node.type; });
      // must have no sort but a gather node
      assertNotEqual(-1, nodeTypes.indexOf("IndexNode"));
      let indexNode = nodes[nodeTypes.indexOf("IndexNode")];
      assertFalse(indexNode.reverse);
      assertEqual(-1, nodeTypes.indexOf("SortNode"));
      assertNotEqual(-1, nodeTypes.indexOf("GatherNode"));
      let gatherNode = nodes[nodeTypes.indexOf("GatherNode")];
      assertEqual([], gatherNode.elements); // no sorting in GatherNode
    },
    
    testSingleShardWithIndexDescending : function () {
      idx = c2.ensureIndex({ type: "skiplist", fields: ["Hallo"] });
      let query = "FOR doc IN " + cn2 + " SORT doc.Hallo DESC RETURN doc.Hallo";
      // check the return value
      let result = AQL_EXECUTE(query).json;
      let last = 99999999999;
      result.forEach(function(value) {
        assertTrue(value <= last);
        last = value;
      });
      
      let nodes = AQL_EXPLAIN(query).plan.nodes;
      let nodeTypes = nodes.map(function(node) { return node.type; });
      // must have no sort but a gather node
      assertNotEqual(-1, nodeTypes.indexOf("IndexNode"));
      let indexNode = nodes[nodeTypes.indexOf("IndexNode")];
      assertTrue(indexNode.reverse);
      assertEqual(-1, nodeTypes.indexOf("SortNode"));
      assertNotEqual(-1, nodeTypes.indexOf("GatherNode"));
      let gatherNode = nodes[nodeTypes.indexOf("GatherNode")];
      assertEqual([], gatherNode.elements); // no sorting in GatherNode
    },
    
    testMultipleShards : function () {
      let query = "FOR doc IN " + cn1 + " SORT doc.Hallo RETURN doc.Hallo";
      // check the return value
      let result = AQL_EXECUTE(query).json;
      let last = -1;
      result.forEach(function(value) {
        assertTrue(value >= last);
        last = value;
      });
      
      let nodes = AQL_EXPLAIN(query).plan.nodes;
      let nodeTypes = nodes.map(function(node) { return node.type; });
      // must have a sort and gather node
      assertNotEqual(-1, nodeTypes.indexOf("SortNode"));
      assertNotEqual(-1, nodeTypes.indexOf("GatherNode"));
      let gatherNode = nodes[nodeTypes.indexOf("GatherNode")];
      assertEqual(1, gatherNode.elements.length); // must do sorting in GatherNode
      assertTrue(gatherNode.elements[0].ascending);
    },
    
    testMultipleShardsDescending : function () {
      let query = "FOR doc IN " + cn1 + " SORT doc.Hallo DESC RETURN doc.Hallo";
      // check the return value
      let result = AQL_EXECUTE(query).json;
      let last = 99999999999;
      result.forEach(function(value) {
        assertTrue(value <= last);
        last = value;
      });
      
      let nodes = AQL_EXPLAIN(query).plan.nodes;
      let nodeTypes = nodes.map(function(node) { return node.type; });
      // must have a sort and gather node
      assertEqual(-1, nodeTypes.indexOf("IndexNode"));
      assertNotEqual(-1, nodeTypes.indexOf("SortNode"));
      let sortNode = nodes[nodeTypes.indexOf("SortNode")];
      assertEqual(1, sortNode.elements.length); // must do sorting in SortNode
      assertFalse(sortNode.elements[0].ascending);
      assertNotEqual(-1, nodeTypes.indexOf("GatherNode"));
      let gatherNode = nodes[nodeTypes.indexOf("GatherNode")];
      assertEqual(1, gatherNode.elements.length); // must do sorting in GatherNode
      assertFalse(gatherNode.elements[0].ascending);
    },

    testMultipleShardsWithIndex : function () {
      idx = c1.ensureIndex({ type: "skiplist", fields: ["Hallo"] });
      let query = "FOR doc IN " + cn1 + " SORT doc.Hallo RETURN doc.Hallo";
      // check the return value
      let result = AQL_EXECUTE(query).json;
      let last = -1;
      result.forEach(function(value) {
        assertTrue(value >= last);
        last = value;
      });
      
      let nodes = AQL_EXPLAIN(query).plan.nodes;
      let nodeTypes = nodes.map(function(node) { return node.type; });
      // must have no sort but a gather node
      assertNotEqual(-1, nodeTypes.indexOf("IndexNode"));
      let indexNode = nodes[nodeTypes.indexOf("IndexNode")];
      assertFalse(indexNode.reverse);
      assertEqual(-1, nodeTypes.indexOf("SortNode"));
      assertNotEqual(-1, nodeTypes.indexOf("GatherNode"));
      let gatherNode = nodes[nodeTypes.indexOf("GatherNode")];
      assertEqual(1, gatherNode.elements.length); // must do sorting in GatherNode
      assertEqual(["Hallo"], gatherNode.elements[0].path); 
      assertTrue(gatherNode.elements[0].ascending);
    },
    
    testMultipleShardsWithIndexDescending : function () {
      idx = c1.ensureIndex({ type: "skiplist", fields: ["Hallo"] });
      let query = "FOR doc IN " + cn1 + " SORT doc.Hallo DESC RETURN doc.Hallo";
      // check the return value
      let result = AQL_EXECUTE(query).json;
      let last = 99999999999;
      result.forEach(function(value) {
        assertTrue(value <= last);
        last = value;
      });
      
      let nodes = AQL_EXPLAIN(query).plan.nodes;
      let nodeTypes = nodes.map(function(node) { return node.type; });
      // must have no sort but a gather node
      assertNotEqual(-1, nodeTypes.indexOf("IndexNode"));
      let indexNode = nodes[nodeTypes.indexOf("IndexNode")];
      assertTrue(indexNode.reverse);
      assertEqual(-1, nodeTypes.indexOf("SortNode"));
      assertNotEqual(-1, nodeTypes.indexOf("GatherNode"));
      let gatherNode = nodes[nodeTypes.indexOf("GatherNode")];
      assertEqual(1, gatherNode.elements.length); // must do sorting in GatherNode
      assertEqual(["Hallo"], gatherNode.elements[0].path); 
      assertFalse(gatherNode.elements[0].ascending);
    },
    
    testMultipleShardsCollect : function () {
      idx = c1.ensureIndex({ type: "skiplist", fields: ["Hallo"] });
      let query = "FOR doc IN " + cn1 + " FILTER doc.Hallo == 10 COLLECT WITH COUNT INTO length RETURN length";
      
      let nodes = AQL_EXPLAIN(query).plan.nodes;
      let nodeTypes = nodes.map(function(node) { return node.type; });
      // must have no sort but a gather node
      assertNotEqual(-1, nodeTypes.indexOf("IndexNode"));
      let indexNode = nodes[nodeTypes.indexOf("IndexNode")];
      assertEqual(-1, nodeTypes.indexOf("SortNode"));
      assertNotEqual(-1, nodeTypes.indexOf("GatherNode"));
      let gatherNode = nodes[nodeTypes.indexOf("GatherNode")];
      assertEqual(0, gatherNode.elements.length); // no sorting here
    },

    testSubqueryValuePropagation : function () {
      c4 = db._create(cn4, {numberOfShards:3});
      c4.insert({Hallo:1});
      const query = `FOR i IN 1..1 LET s = (FOR j IN 1..i FOR k IN ${cn4} RETURN j) RETURN s`;
      // check the return value
      const expected = [ [ 1 ] ];
      const rules = ['-splice-subqueries'];
      const opts = {optimizer:{rules}};
      const plan = AQL_EXPLAIN(query, {}, opts).plan;
      const nodeTypes = plan.nodes.map(function(node) {
        return node.type;
      });
      assertNotEqual(0, nodeTypes.filter(type => type === 'SubqueryNode').length);
      assertEqual(0, nodeTypes.filter(type => type === 'SubqueryStartNode').length);
      assertEqual(0, nodeTypes.filter(type => type === 'SubqueryEndNode').length);
      const actual = AQL_EXECUTE(query, {}, opts).json;

      assertEqual(expected, actual, query);
    },

    testSplicedSubqueryValuePropagation : function () {
      c4 = db._create(cn4, {numberOfShards:3});
      c4.insert({Hallo:1});
      const query = `FOR i IN 1..1 LET s = (FOR j IN 1..i FOR k IN ${cn4} RETURN j) RETURN s`;
      // check the return value
      const expected = [ [ 1 ] ];
      const rules = ['+splice-subqueries'];
      const opts = {optimizer:{rules}};
      const plan = AQL_EXPLAIN(query, {}, opts).plan;
      const nodeTypes = plan.nodes.map(function(node) {
        return node.type;
      });
      assertEqual(0, nodeTypes.filter(type => type === 'SubqueryNode').length);
      assertNotEqual(0, nodeTypes.filter(type => type === 'SubqueryStartNode').length);
      assertNotEqual(0, nodeTypes.filter(type => type === 'SubqueryEndNode').length);
      const actual = AQL_EXECUTE(query, {}, opts).json;

      assertEqual(expected, actual, query);
    },

    testCalculationNotMovedOverBoundary : function () {
      c4 = db._create(cn4, {numberOfShards:3});
      c4.insert({Hallo:1});
      var query = "FOR i IN " + cn4 + " FOR j IN 1..1 FOR k IN " + cn4 + " RETURN j";
      // check the GatherNode is in the plan!
      assertTrue(explain(AQL_EXPLAIN(query)).indexOf("GatherNode") !== -1, query);
      
      // check the return value
      var expected = [ 1 ];
      var actual = AQL_EXECUTE(query).json;

      assertEqual(expected, actual, query);
    },

    testSimple1 : function () {
      var query = "FOR d IN " + cn1 + " LIMIT 10 RETURN d.Hallo";
      
      // check the GatherNode is in the plan!
      assertTrue(explain(AQL_EXPLAIN(query)).indexOf("GatherNode") !== -1, query);
      
      // check the return value
      var expected = 10;
      var actual = AQL_EXECUTE(query).json.length;
      
      assertEqual(expected, actual, query);
    },

    testSimple2 : function () {
      var query = 
          "FOR d IN " + cn1 + " FILTER d.Hallo in [5,63] RETURN d.Hallo";
      
      // check the GatherNode is in the plan!
      assertTrue(explain(AQL_EXPLAIN(query)).indexOf("GatherNode") !== -1, query);
      
      // check the return value
      var expected = [ ];
      for (var i = 0; i < 400; i++) {
        expected.push(5);
      }
      var actual = AQL_EXECUTE(query).json;

      assertEqual(expected, actual, query);
    },
    
    testSimple3 : function () {
      var query = 
          "FOR d IN " + cn1 
            + " FILTER d.Hallo in [0,0,5,6,6,63] LIMIT 399,2 RETURN d.Hallo";
      
      // check the GatherNode is in the plan!
      assertTrue(explain(AQL_EXPLAIN(query)).indexOf("GatherNode") !== -1, query);
      
      // check the return value
      var expected = 2;
      var actual = AQL_EXECUTE(query).json.length;

      assertEqual(expected, actual, query);
    },
    
    testSimple4 : function () {
      var query = 
          "FOR d IN " + cn1 
            + " FILTER d.Hallo in [0,5,6] LIMIT 995,10 RETURN d.Hallo";
      
      // check the GatherNode is in the plan!
      assertTrue(explain(AQL_EXPLAIN(query)).indexOf("GatherNode") !== -1, query);
      
      // check the return value
      var expected = 10;
      var actual = AQL_EXECUTE(query).json.length;

      assertEqual(expected, actual, query);
    },
    
    testNonSimple1 : function () {
      var query = "FOR d IN " + cn3 + " SORT d.Hallo RETURN d.Hallo";
      
      // check the GatherNode is in the plan!
      assertTrue(explain(AQL_EXPLAIN(query)).indexOf("GatherNode") !== -1, query);
      
      // check the return value
      var expected = [ 0, 1, 2, 3, 4, 5, 6, 7, 8, 9 ];
      var actual = AQL_EXECUTE(query).json;

      assertEqual(expected, actual, query);
    },

    testNonSimple2 : function () {
      var query = 
          "FOR d IN " + cn3 
            + " FILTER d.Hallo in [0,5,6,62] SORT d.Hallo RETURN d.Hallo";
      
      // check the GatherNode is in the plan!
      assertTrue(explain(AQL_EXPLAIN(query)).indexOf("GatherNode") !== -1, query);
      
      // check the return value
      var expected = [ 0, 5, 6 ];
      var actual = AQL_EXECUTE(query).json;

      assertEqual(expected, actual, query);
    },
    
    testNonSimple3 : function () {
      var query = 
          "FOR d IN " + cn1 
            + " FILTER d.Hallo in [0,5,6,62] SORT d.Hallo LIMIT 399,2 RETURN d.Hallo";
      
      // check 2the GatherNode is in the plan!
      assertTrue(explain(AQL_EXPLAIN(query)).indexOf("GatherNode") !== -1, query);
      
      // check the return value
      var expected = [ 0, 5 ];
      var actual = AQL_EXECUTE(query).json;

      assertEqual(expected, actual, query);
    },
    
    testNonSimple4 : function () {
      var query = 
          "FOR d IN " + cn1 
            + " FILTER d.Hallo in [0,5,6,62] SORT d.Hallo LIMIT 1 RETURN d.Hallo";
      
      // check the GatherNode is in the plan!
      assertTrue(explain(AQL_EXPLAIN(query)).indexOf("GatherNode") !== -1, query);
      
      // check the return value
      var expected = [ 0 ];
      var actual = AQL_EXECUTE(query).json;

      assertEqual(expected, actual, query);
    },
    
    testNonSimple5 : function () {
      var query = 
          "FOR d IN " + cn1 
            + " FILTER d.Hallo in [0,5,6] SORT d.Hallo LIMIT 995,10 RETURN d.Hallo";
      
      // check the GatherNode is in the plan!
      assertTrue(explain(AQL_EXPLAIN(query)).indexOf("GatherNode") !== -1, query);
      
      // check the return value
      var expected = [ 6, 6, 6, 6, 6, 6, 6, 6, 6, 6 ];
      var actual = AQL_EXECUTE(query).json;

      assertEqual(expected, actual, query);
    },

    testSimple5 : function () {
      var query = "FOR d IN " + cn2 + " LIMIT 10 RETURN d.Hallo";
      
      // check the GatherNode is in the plan!
      assertTrue(explain(AQL_EXPLAIN(query)).indexOf("GatherNode") !== -1, query);
      
      // check the return value
      var expected = 10;
      var actual = AQL_EXECUTE(query).json.length;

      assertEqual(expected, actual, query);
    },

    testSimple6 : function () {
      var query = 
          "FOR d IN " + cn3 + " FILTER d.Hallo in [5,63] RETURN d.Hallo";
      
      // check the GatherNode is in the plan!
      assertTrue(explain(AQL_EXPLAIN(query)).indexOf("GatherNode") !== -1, query);
      
      // check the return value
      var expected = [ 5 ];
      var actual = AQL_EXECUTE(query).json;

      assertEqual(expected, actual, query);
    },
    
    testSimple7 : function () {
      var query = 
          "FOR d IN " + cn2 
            + " FILTER d.Hallo in [0,0,5,6,6,63] LIMIT 399,2 RETURN d.Hallo";
      
      // check the GatherNode is in the plan!
      assertTrue(explain(AQL_EXPLAIN(query)).indexOf("GatherNode") !== -1, query);
      
      // check the return value
      var expected = 2;
      var actual = AQL_EXECUTE(query).json.length;

      assertEqual(expected, actual, query);
    },
    
    testSimple8 : function () {
      var query = 
          "FOR d IN " + cn2 
            + " FILTER d.Hallo in [0,5,6] LIMIT 990,21 RETURN d.Hallo";
      
      // check the GatherNode is in the plan!
      assertTrue(explain(AQL_EXPLAIN(query)).indexOf("GatherNode") !== -1, query);
      
      // check the return value
      var expected = 21; 
      var actual = AQL_EXECUTE(query).json.length;

      assertEqual(expected, actual, query);
    },
    
    testNonSimple7 : function () {
      var query = 
          "FOR d IN " + cn3 
            + " FILTER d.Hallo in [0,5,6,62] SORT d.Hallo RETURN d.Hallo";
      
      // check the GatherNode is in the plan!
      assertTrue(explain(AQL_EXPLAIN(query)).indexOf("GatherNode") !== -1, query);
      
      // check the return value
      var expected = [ 0, 5, 6 ];
      var actual = AQL_EXECUTE(query).json;

      assertEqual(expected, actual, query);
    },
    
    testNonSimple8 : function () {
      var query = 
          "FOR d IN " + cn2 
            + " FILTER d.Hallo in [0,5,6,62] SORT d.Hallo DESC LIMIT 399,2 RETURN d.Hallo";
      
      // check the GatherNode is in the plan!
      assertTrue(explain(AQL_EXPLAIN(query)).indexOf("GatherNode") !== -1, query);
      
      // check the return value
      var expected = 2;
      var actual = AQL_EXECUTE(query).json.length;

      assertEqual(expected, actual, query);
    },
    
    testNonSimple9 : function () {
      var query = 
          "FOR d IN " + cn2 
            + " FILTER d.Hallo in [0,5,6,62] SORT d.Hallo DESC LIMIT 1 RETURN d.Hallo";
      
      // check the GatherNode is in the plan!
      assertTrue(explain(AQL_EXPLAIN(query)).indexOf("GatherNode") !== -1, query);
      
      // check the return value
      var expected = [ 6 ];
      var actual = AQL_EXECUTE(query).json;

      assertEqual(expected, actual, query);
    },
    
    testNonSimple10 : function () {
      var query = 
          "FOR d IN " + cn2 
            + " FILTER d.Hallo in [0,5,6] SORT d.Hallo DESC LIMIT 995,10 RETURN d.Hallo";
      
      // check the GatherNode is in the plan!
      assertTrue(explain(AQL_EXPLAIN(query)).indexOf("GatherNode") !== -1, query);
      
      // check the return value
      var expected = [ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 ];
      var actual = AQL_EXECUTE(query).json;

      assertEqual(expected, actual, query);
    }

  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suite
////////////////////////////////////////////////////////////////////////////////

jsunity.run(gatherBlockTestSuite);

return jsunity.done();

