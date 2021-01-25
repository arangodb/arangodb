/*jshint globalstrict:false, strict:false, maxlen: 500 */
/*global assertEqual, assertFalse, assertTrue, assertNotEqual, AQL_EXPLAIN, AQL_EXECUTE */

////////////////////////////////////////////////////////////////////////////////
/// @brief tests for optimizer rules
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

var internal = require("internal");
var db = internal.db;
var jsunity = require("jsunity");
var helper = require("@arangodb/aql-helper");
var isEqual = helper.isEqual;
var findExecutionNodes = helper.findExecutionNodes;
var findReferencedNodes = helper.findReferencedNodes;
var getQueryMultiplePlansAndExecutions = helper.getQueryMultiplePlansAndExecutions;
var removeAlwaysOnClusterRules = helper.removeAlwaysOnClusterRules;

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////

function optimizerRuleTestSuite() {
  const ruleName = "use-index-for-sort";
  const colName = "UnitTestsUseIndexForSort";
  let c;

  return {

    setUp : function () {
      internal.db._drop(colName);
      c = internal.db._create(colName, {numberOfShards: 5});
      let docs = [];
      for (let i = 0; i < 2000; ++i) {
        docs.push({ value: i });
      }
      c.insert(docs);

      c.ensureIndex({ type: "skiplist", fields: [ "value" ] });
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief tear down
    ////////////////////////////////////////////////////////////////////////////////

    tearDown : function () {
      internal.db._drop(colName);
    },

    testSortAscKeepGather : function () {
      let query = "FOR doc IN " + colName + " SORT doc.value ASC RETURN doc";
      let plan = AQL_EXPLAIN(query).plan;
      let nodes = plan.nodes.filter(function(n) { return n.type === 'GatherNode'; });
      assertEqual(1, nodes.length);
      assertEqual("doc", nodes[0].elements[0].inVariable.name);
      assertEqual(["value"], nodes[0].elements[0].path);
      assertTrue(nodes[0].elements[0].ascending);
      assertNotEqual(-1, plan.rules.indexOf(ruleName));

      let results = AQL_EXECUTE(query).json;
      assertEqual(2000, results.length);
      let last = null;
      results.forEach(function(doc) {
        assertTrue(last === null || doc.value > last);
        last = doc.value;
      });
    },

    testSortDescKeepGather : function () {
      let query = "FOR doc IN " + colName + " SORT doc.value DESC RETURN doc";
      let plan = AQL_EXPLAIN(query).plan;
      let nodes = plan.nodes.filter(function(n) { return n.type === 'GatherNode'; });
      assertEqual(1, nodes.length);
      assertEqual("doc", nodes[0].elements[0].inVariable.name);
      assertEqual(["value"], nodes[0].elements[0].path);
      assertFalse(nodes[0].elements[0].ascending);
      assertNotEqual(-1, plan.rules.indexOf(ruleName));
      
      let results = AQL_EXECUTE(query).json;
      assertEqual(2000, results.length);
      let last = null;
      results.forEach(function(doc) {
        assertTrue(last === null || doc.value < last);
        last = doc.value;
      });
    },
    
    testSortAscKeepGatherNonUnique : function () {
      // add the same values again
      let docs = [];
      for (let i = 0; i < 2000; ++i) {
        docs.push({ value: i });
      }
      c.insert(docs);
      let query = "FOR doc IN " + colName + " SORT doc.value ASC RETURN doc";
      let plan = AQL_EXPLAIN(query).plan;
      let nodes = plan.nodes.filter(function(n) { return n.type === 'GatherNode'; });
      assertEqual(1, nodes.length);
      assertEqual("doc", nodes[0].elements[0].inVariable.name);
      assertEqual(["value"], nodes[0].elements[0].path);
      assertTrue(nodes[0].elements[0].ascending);
      assertNotEqual(-1, plan.rules.indexOf(ruleName));

      let results = AQL_EXECUTE(query).json;
      assertEqual(4000, results.length);
      let last = null;
      results.forEach(function(doc) {
        assertTrue(last === null || doc.value >= last);
        last = doc.value;
      });
    },
    
    testCollectHashKeepGather : function () {
      let query = "FOR doc IN " + colName + " COLLECT value = doc.value WITH COUNT INTO l OPTIONS { method: 'hash' } RETURN { value, l }";
      let plan = AQL_EXPLAIN(query).plan;
      let nodes = plan.nodes.filter(function(n) { return n.type === 'CollectNode'; });
      assertEqual(2, nodes.length);
      assertEqual(1, nodes[0].aggregates.length);
      assertEqual("LENGTH", nodes[0].aggregates[0].type);
      assertEqual("hash", nodes[0].collectOptions.method);
      assertEqual("SUM", nodes[1].aggregates[0].type);
      assertEqual("hash", nodes[1].collectOptions.method);
      assertEqual(nodes[0].aggregates[0].outVariable.name, nodes[1].aggregates[0].inVariable.name);

      nodes = plan.nodes.filter(function(n) { return n.type === 'GatherNode'; });
      assertEqual(1, nodes.length);
      assertEqual(0, nodes[0].elements.length);

      assertEqual(-1, plan.rules.indexOf(ruleName));
      assertNotEqual(-1, plan.rules.indexOf("collect-in-cluster"));
      
      let results = AQL_EXECUTE(query).json;
      assertEqual(2000, results.length);
      results.forEach(function(doc) {
        assertEqual(1, doc.l, doc);
      });
    },
   
    testCollectSortedKeepGather : function () {
      let query = "FOR doc IN " + colName + " COLLECT value = doc.value WITH COUNT INTO l OPTIONS { method: 'sorted' } RETURN { value, l }";
      let plan = AQL_EXPLAIN(query).plan;
      let nodes = plan.nodes.filter(function(n) { return n.type === 'CollectNode'; });
      assertEqual(2, nodes.length);
      assertEqual(1, nodes[0].aggregates.length);
      assertEqual("LENGTH", nodes[0].aggregates[0].type);
      assertEqual("sorted", nodes[0].collectOptions.method);
      assertEqual("SUM", nodes[1].aggregates[0].type);
      assertEqual("sorted", nodes[1].collectOptions.method);
      assertEqual(nodes[0].aggregates[0].outVariable.name, nodes[1].aggregates[0].inVariable.name);
   
      nodes = plan.nodes.filter(function(n) { return n.type === 'GatherNode'; });
      assertEqual(1, nodes.length);
      assertTrue(nodes[0].elements[0].ascending);

      assertNotEqual(-1, plan.rules.indexOf(ruleName));
      assertNotEqual(-1, plan.rules.indexOf("collect-in-cluster"));
    
      let results = AQL_EXECUTE(query).json;
      assertEqual(2000, results.length);
      let last = null;
      results.forEach(function(doc) {
        assertTrue(last === null || doc.value >= last);
        last = doc.value;
        assertEqual(1, doc.l, doc);
      });
    },
    
    testCollectIntoSortedKeepGather : function () {
      let query = "FOR doc IN " + colName + " COLLECT value = doc.value INTO g OPTIONS { method: 'sorted' } RETURN { value, g }";
      let plan = AQL_EXPLAIN(query).plan;
      let nodes = plan.nodes.filter(function(n) { return n.type === 'CollectNode'; });
      assertEqual(1, nodes.length);
      assertEqual([], nodes[0].aggregates);
      assertEqual("sorted", nodes[0].collectOptions.method);
      
      nodes = plan.nodes.filter(function(n) { return n.type === 'GatherNode'; });
      assertEqual(1, nodes.length);
      assertTrue(nodes[0].elements[0].ascending);

      assertNotEqual(-1, plan.rules.indexOf(ruleName));
      assertEqual(-1, plan.rules.indexOf("collect-in-cluster"));
    
      let results = AQL_EXECUTE(query).json;
      assertEqual(2000, results.length);
      let last = null;
      results.forEach(function(doc) {
        assertTrue(last === null || doc.value >= last);
        last = doc.value;
        assertEqual(1, doc.g.length);
        assertEqual(doc.value, doc.g[0].doc.value);
      });
    },
    
    testCollectIntoSortedKeepGatherNonUnique : function () {
      // add the same values again
      let docs = [];
      for (let i = 0; i < 2000; ++i) {
        docs.push({ value: i });
      }
      c.insert(docs);
      let query = "FOR doc IN " + colName + " COLLECT value = doc.value INTO g OPTIONS { method: 'sorted' } RETURN { value, g }";
      let plan = AQL_EXPLAIN(query).plan;
      let nodes = plan.nodes.filter(function(n) { return n.type === 'CollectNode'; });
      assertEqual(1, nodes.length);
      assertEqual([], nodes[0].aggregates);
      assertEqual("sorted", nodes[0].collectOptions.method);
      
      nodes = plan.nodes.filter(function(n) { return n.type === 'GatherNode'; });
      assertEqual(1, nodes.length);
      assertTrue(nodes[0].elements[0].ascending);

      assertNotEqual(-1, plan.rules.indexOf(ruleName));
      assertEqual(-1, plan.rules.indexOf("collect-in-cluster"));
    
      let results = AQL_EXECUTE(query).json;
      assertEqual(2000, results.length);
      let last = null;
      results.forEach(function(doc) {
        assertTrue(last === null || doc.value >= last);
        last = doc.value;
        assertEqual(2, doc.g.length);
        assertEqual(doc.value, doc.g[0].doc.value);
        assertEqual(doc.value, doc.g[1].doc.value);
      });
    },
    
    testCollectHashKeepGatherNonUnique : function () {
      // add the same values again
      let docs = [];
      for (let i = 0; i < 2000; ++i) {
        docs.push({ value: i });
      }
      c.insert(docs);
      let query = "FOR doc IN " + colName + " COLLECT value = doc.value WITH COUNT INTO l OPTIONS { method: 'hash' } RETURN { value, l }";
      let plan = AQL_EXPLAIN(query).plan;
      let nodes = plan.nodes.filter(function(n) { return n.type === 'CollectNode'; });
      assertEqual(2, nodes.length);
      assertEqual(1, nodes[0].aggregates.length);
      assertEqual("LENGTH", nodes[0].aggregates[0].type);
      assertEqual("hash", nodes[0].collectOptions.method);
      assertEqual("SUM", nodes[1].aggregates[0].type);
      assertEqual("hash", nodes[1].collectOptions.method);
      assertEqual(nodes[0].aggregates[0].outVariable.name, nodes[1].aggregates[0].inVariable.name);
   
      nodes = plan.nodes.filter(function(n) { return n.type === 'GatherNode'; });
      assertEqual(1, nodes.length);

      assertEqual(-1, plan.rules.indexOf(ruleName));
      assertNotEqual(-1, plan.rules.indexOf("collect-in-cluster"));
    
      let results = AQL_EXECUTE(query).json;
      assertEqual(2000, results.length);
      results.forEach(function(doc) {
        assertEqual(2, doc.l, doc);
      });
    },
    
    testCollectSortedKeepGatherNonUnique : function () {
      // add the same values again
      let docs = [];
      for (let i = 0; i < 2000; ++i) {
        docs.push({ value: i });
      }
      c.insert(docs);
      let query = "FOR doc IN " + colName + " COLLECT value = doc.value WITH COUNT INTO l OPTIONS { method: 'sorted' } RETURN { value, l }";
      let plan = AQL_EXPLAIN(query).plan;
      let nodes = plan.nodes.filter(function(n) { return n.type === 'CollectNode'; });

      require("console").log(nodes);
      assertEqual(2, nodes.length);
      assertEqual(1, nodes[0].aggregates.length);
      assertEqual("LENGTH", nodes[0].aggregates[0].type);
      assertEqual("sorted", nodes[0].collectOptions.method);
      assertEqual("SUM", nodes[1].aggregates[0].type);
      assertEqual("sorted", nodes[1].collectOptions.method);
      assertEqual(nodes[0].aggregates[0].outVariable.name, nodes[1].aggregates[0].inVariable.name);
   
      nodes = plan.nodes.filter(function(n) { return n.type === 'GatherNode'; });
      assertEqual(1, nodes.length);
      assertTrue(nodes[0].elements[0].ascending);

      assertNotEqual(-1, plan.rules.indexOf(ruleName));
      assertNotEqual(-1, plan.rules.indexOf("collect-in-cluster"));
    
      let results = AQL_EXECUTE(query).json;
      assertEqual(2000, results.length);
      results.forEach(function(doc) {
        assertEqual(2, doc.l, doc);
      });
    }

  };
}

jsunity.run(optimizerRuleTestSuite);

return jsunity.done();
