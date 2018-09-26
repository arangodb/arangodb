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
      for (let i = 0; i < 2000; ++i) {
        c.insert({ value: i });
      }
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
      for (let i = 0; i < 2000; ++i) {
        c.insert({ value: i });
      }
      let query = "FOR doc IN " + colName + " SORT doc.value ASC RETURN doc";
      let plan = AQL_EXPLAIN(query).plan;
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
      assertEqual(1, nodes.length);
      assertEqual([], nodes[0].aggregates);
      assertTrue(nodes[0].count);
      assertEqual("hash", nodes[0].collectOptions.method);

      assertEqual(-1, plan.rules.indexOf(ruleName));
      
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
      assertEqual(1, nodes.length);
      assertEqual([], nodes[0].aggregates);
      assertTrue(nodes[0].count);
      assertEqual("sorted", nodes[0].collectOptions.method);
   
      assertNotEqual(-1, plan.rules.indexOf(ruleName));
    
      let results = AQL_EXECUTE(query).json;
      assertEqual(2000, results.length);
      results.forEach(function(doc) {
        assertEqual(1, doc.l, doc);
      });
    },
    
    testCollectHashKeepGatherNonUnique : function () {
      // add the same values again
      for (let i = 0; i < 2000; ++i) {
        c.insert({ value: i });
      }
      let query = "FOR doc IN " + colName + " COLLECT value = doc.value WITH COUNT INTO l OPTIONS { method: 'hash' } RETURN { value, l }";
      let plan = AQL_EXPLAIN(query).plan;
      let nodes = plan.nodes.filter(function(n) { return n.type === 'CollectNode'; });
      assertEqual(1, nodes.length);
      assertEqual([], nodes[0].aggregates);
      assertTrue(nodes[0].count);
      assertEqual("hash", nodes[0].collectOptions.method);

      assertEqual(-1, plan.rules.indexOf(ruleName));
    
      let results = AQL_EXECUTE(query).json;
      assertEqual(2000, results.length);
      results.forEach(function(doc) {
        assertEqual(2, doc.l, doc);
      });
    },
    
    testCollectSortedKeepGatherNonUnique : function () {
      // add the same values again
      for (let i = 0; i < 2000; ++i) {
        c.insert({ value: i });
      }
      let query = "FOR doc IN " + colName + " COLLECT value = doc.value WITH COUNT INTO l OPTIONS { method: 'sorted' } RETURN { value, l }";
      let plan = AQL_EXPLAIN(query).plan;
      let nodes = plan.nodes.filter(function(n) { return n.type === 'CollectNode'; });
      assertEqual(1, nodes.length);
      assertEqual([], nodes[0].aggregates);
      assertTrue(nodes[0].count);
      assertEqual("sorted", nodes[0].collectOptions.method);

      assertNotEqual(-1, plan.rules.indexOf(ruleName));
    
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
