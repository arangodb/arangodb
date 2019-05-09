/*jshint globalstrict:false, strict:false, maxlen: 500 */
/*global assertEqual, assertNotEqual, assertTrue, AQL_EXPLAIN, AQL_EXECUTE */

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

var jsunity = require("jsunity");
var helper = require("@arangodb/aql-helper");
var isEqual = helper.isEqual;
var db = require("@arangodb").db;
var ruleName = "reduce-extraction-to-projection";

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////

function optimizerRuleTestSuite () {
  var c = null;
  var cn = "UnitTestsOptimizer";

  return {

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief set up
    ////////////////////////////////////////////////////////////////////////////////

    setUp : function () {
      db._drop(cn);
      c = db._create(cn, { numberOfShards: 4 });

      for (var i = 0; i < 1000; ++i) {
        c.insert({ value1: i, value2: "test" + i, foo: { bar: i } });
      }
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief tear down
    ////////////////////////////////////////////////////////////////////////////////

    tearDown : function () {
      db._drop(cn);
      c = null;
    },

    testNotActive : function () {
      var queries = [
        "FOR doc IN @@cn FILTER doc.value1 == 1 && doc.value2 == 1 && doc.value3 == 1 && doc.value4 == 1 && doc.value5 == 1 && doc.value6 == 1 RETURN doc",
        "FOR doc IN @@cn FILTER doc.value1 == 1 && doc.value2 == 1 RETURN doc",
        "FOR doc IN @@cn FILTER doc.value1 == 1 RETURN doc",
        "FOR doc IN @@cn SORT doc.value1, doc.value2 RETURN doc",
        "FOR doc IN @@cn COLLECT v = doc.value1 INTO g RETURN g",
        "FOR doc IN @@cn FILTER doc.value1 == 1 RETURN doc",
        "FOR doc IN @@cn FILTER doc == { value1: 1 } RETURN doc",
        "FOR doc IN @@cn FILTER doc && doc.value1 == 1 RETURN doc.value1",
        "FOR doc IN @@cn INSERT MERGE(doc, { foo: doc.value }) INTO @@cn"
      ];

      queries.forEach(function(query) {
        var result = AQL_EXPLAIN(query, { "@cn" : cn });
        assertEqual(-1, result.plan.rules.indexOf(ruleName), query);
      });
    },
    
    testNotActiveBecauseIndexHint : function () {
      // these queries may actually use projections, but they must not use the primary
      // index for scanning
      var queries = [
        "FOR doc IN @@cn OPTIONS { indexHint: 'aha', forceIndexHint: true } RETURN 1",
        "FOR doc IN @@cn OPTIONS { indexHint: 'aha', forceIndexHint: true } RETURN doc",
        "FOR doc IN @@cn OPTIONS { indexHint: 'aha', forceIndexHint: true } RETURN doc.value1",
        "FOR doc IN @@cn OPTIONS { indexHint: 'aha', forceIndexHint: true } RETURN doc.value2",
        "FOR doc IN @@cn OPTIONS { indexHint: 'aha', forceIndexHint: true } RETURN doc._key",
        "FOR doc IN @@cn OPTIONS { indexHint: 'primary' } RETURN doc",
        "FOR doc IN @@cn OPTIONS { indexHint: 'primary' } RETURN doc.value1",
        "FOR doc IN @@cn OPTIONS { indexHint: 'primary' } RETURN doc.value2",
      ];

      queries.forEach(function(query) {
        var result = AQL_EXPLAIN(query, { "@cn" : cn }).plan;
        let nodeTypes = result.nodes.map(function(node) { return node.type; });
        assertEqual(-1, nodeTypes.indexOf("IndexNode"));
      });
    },

    testActive : function () {
      var queries = [
        "FOR doc IN @@cn OPTIONS { indexHint: 'primary' } RETURN 1",
        "FOR doc IN @@cn OPTIONS { indexHint: 'primary' } RETURN doc._key",
        "FOR doc IN @@cn FILTER doc.value1 == 1 RETURN doc.value1",
        "FOR doc IN @@cn FILTER doc.value1 == 1 RETURN doc.value2",
        "FOR doc IN @@cn FILTER doc.value1 == 1 && doc.value2 == 1 && doc.value3 == 1 RETURN doc.value1", 
        "FOR doc IN @@cn FILTER doc.value1 == 1 && doc.value2 == 1 && doc.value3 == 1 && doc.value4 == 1 RETURN doc.value1", 
        "FOR doc IN @@cn FILTER doc.value1 == 1 && doc.value2 == 1 && doc.value3 == 1 && doc.value4 == 1 && doc.value5 == 1 RETURN doc.value1", 
        "FOR doc IN @@cn FILTER doc.value1 == 1 && doc.value2 == 1 RETURN doc.value1",
        "FOR doc IN @@cn FILTER doc.value1 == 1 && doc.value2 == 1 RETURN doc.value2",
        "FOR doc IN @@cn FILTER doc.value1 == 1 && doc.value2 == 1 RETURN [ doc.value1, doc.value2 ]",
        "FOR doc IN @@cn FILTER doc.value1 == 1 RETURN doc.value2",
        "FOR doc IN @@cn RETURN [doc.value1, doc.value2]",
        "FOR doc IN @@cn FILTER doc.value1 >= 132 && doc.value <= 134 SORT doc.value1 RETURN doc.value1",
        "FOR doc IN @@cn SORT doc.value1 RETURN doc.value2",
        "FOR doc IN @@cn FILTER doc.value1 == 1 SORT doc.value2 RETURN doc.value1",
        "FOR doc IN @@cn FILTER doc.value1 == 1 RETURN 1",
        "FOR doc IN @@cn FILTER doc.value1 == 1 FILTER doc.value2 == 1 RETURN doc.value1",
        "FOR doc IN @@cn FILTER doc.value1 == 1 && doc.value2 == 1 RETURN doc.value2",
        "FOR doc IN @@cn SORT doc.value1 RETURN doc.value1",
        "FOR doc IN @@cn SORT doc.value1 RETURN 1",
        "FOR doc IN @@cn COLLECT v = doc.value1 INTO g RETURN v", // g will be optimized away
        "FOR doc IN @@cn FILTER doc.value1 == 1 SORT doc.value1 RETURN doc.value1",
        "FOR doc IN @@cn FILTER doc.value1 > 1 SORT doc.value1 RETURN doc.value1",
        "FOR doc IN @@cn FILTER doc.value1 == { value1: 1 } RETURN doc.value1",
        "FOR doc IN @@cn SORT doc.value1 RETURN doc.value1",
        "FOR doc IN @@cn RETURN doc.foo.bar",
        "FOR doc IN @@cn SORT doc.foo.bar RETURN doc.foo.bar"
      ];
      
      queries.forEach(function(query) {
        var result = AQL_EXPLAIN(query, { "@cn" : cn });
        assertNotEqual(-1, result.plan.rules.indexOf(ruleName), query);
      });
    },
    
    testActiveScanOnly : function () {
      var queries = [
        "FOR doc IN @@cn RETURN 1",
        "FOR doc IN @@cn OPTIONS { indexHint: 'primary' } RETURN 1",
      ];
      
      queries.forEach(function(query) {
        var result = AQL_EXPLAIN(query, { "@cn" : cn });
        assertNotEqual(-1, result.plan.rules.indexOf(ruleName), query);
      });
    },
    
    testActiveWithIndex : function () {
      c.ensureIndex({ type: "skiplist", fields: ["value1"] });

      var queries = [
        "FOR doc IN @@cn FILTER doc.value1 == 1 RETURN doc.value1",
        "FOR doc IN @@cn SORT doc.value1 RETURN doc.value1",
        "FOR doc IN @@cn COLLECT v = doc.value1 INTO g RETURN v", // g will be optimized away
        "FOR doc IN @@cn FILTER doc.value1 == 1 SORT doc.value1 RETURN doc.value1",
        "FOR doc IN @@cn FILTER doc.value1 > 1 SORT doc.value1 RETURN doc.value1",
        "FOR doc IN @@cn FILTER doc.value1 == { value1: 1 } RETURN doc.value1",
        "FOR doc IN @@cn SORT doc.value1 RETURN doc.value1"
      ];
      
      queries.forEach(function(query) {
        var result = AQL_EXPLAIN(query, { "@cn" : cn });
        assertNotEqual(-1, result.plan.rules.indexOf(ruleName), query);
      });
    },
    
    testActiveWithIndexMultiple : function () {
      c.ensureIndex({ type: "skiplist", fields: ["foo.bar"] });

      var queries = [
        "FOR doc IN @@cn FILTER doc.foo.bar == 1 RETURN doc.foo.bar",
        "FOR doc IN @@cn SORT doc.foo.bar RETURN doc.foo.bar",
        "FOR doc IN @@cn COLLECT v = doc.foo.bar INTO g RETURN v", // g will be optimized away
        "FOR doc IN @@cn FILTER doc.foo.bar == 1 SORT doc.foo.bar RETURN doc.foo.bar",
        "FOR doc IN @@cn FILTER doc.foo.bar > 1 SORT doc.foo.bar RETURN doc.foo.bar",
        "FOR doc IN @@cn FILTER doc.foo.bar == { value1: 1 } RETURN doc.foo.bar",
        "FOR doc IN @@cn SORT doc.foo.bar RETURN doc.foo.bar"
      ];
      
      queries.forEach(function(query) {
        var result = AQL_EXPLAIN(query, { "@cn" : cn });
        assertNotEqual(-1, result.plan.rules.indexOf(ruleName), query);
      });
    },

    testResults : function () {
      var queries = [
        [ "FOR doc IN @@cn FILTER doc.value1 == 1 RETURN 42", [ 42 ] ],
        [ "FOR doc IN @@cn FILTER doc.value1 == 1 RETURN doc.value1", [ 1 ] ],
        [ "FOR doc IN @@cn FILTER doc.value1 <= 1 SORT doc.value1 RETURN doc.value1", [ 0, 1 ] ],
        [ "FOR doc IN @@cn FILTER doc.value1 >= 132 && doc.value1 <= 134 SORT doc.value1 RETURN doc.value1", [ 132, 133, 134 ] ]
      ];
      
      queries.forEach(function(query) {
        var result = AQL_EXPLAIN(query[0], { "@cn" : cn });
        assertNotEqual(-1, result.plan.rules.indexOf(ruleName), query[0]);
        
        result = AQL_EXECUTE(query[0], { "@cn" : cn });
        assertEqual(query[1], result.json);
      });
    },
    
    testResultsMultiple : function () {
      var queries = [
        [ "FOR doc IN @@cn FILTER doc.foo.bar == 1 RETURN 42", [ 42 ] ],
        [ "FOR doc IN @@cn FILTER doc.foo.bar == 1 RETURN doc.foo.bar", [ 1 ] ],
        [ "FOR doc IN @@cn FILTER doc.foo.bar <= 1 SORT doc.foo.bar RETURN doc.foo.bar", [ 0, 1 ] ],
        [ "FOR doc IN @@cn FILTER doc.foo.bar >= 132 && doc.foo.bar <= 134 SORT doc.foo.bar RETURN doc.foo.bar", [ 132, 133, 134 ] ]
      ];
      
      queries.forEach(function(query) {
        var result = AQL_EXPLAIN(query[0], { "@cn" : cn });
        assertNotEqual(-1, result.plan.rules.indexOf(ruleName), query[0]);
        
        result = AQL_EXECUTE(query[0], { "@cn" : cn });
        assertEqual(query[1], result.json);
      });
    },
    
    testResultsWithIndex : function () {
      c.ensureIndex({ type: "skiplist", fields: ["value1"] });

      var queries = [
        [ "FOR doc IN @@cn FILTER doc.value1 == 1 RETURN doc.value1", [ 1 ] ],
        [ "FOR doc IN @@cn FILTER doc.value1 <= 1 SORT doc.value1 RETURN doc.value1", [ 0, 1 ] ],
        [ "FOR doc IN @@cn FILTER doc.value1 >= 132 && doc.value1 <= 134 SORT doc.value1 RETURN doc.value1", [ 132, 133, 134 ] ],
        [ "FOR doc IN @@cn FILTER doc.value1 >= 130 && doc.value1 <= 139 SORT doc.value1 RETURN doc.value1", [ 130, 131, 132, 133, 134, 135, 136, 137, 138, 139 ] ]
      ];
      
      queries.forEach(function(query) {
        var result = AQL_EXPLAIN(query[0], { "@cn" : cn });
        assertNotEqual(-1, result.plan.rules.indexOf(ruleName), query[0]);
        
        result = AQL_EXECUTE(query[0], { "@cn" : cn });
        assertEqual(query[1], result.json);
      });
    },
    
    testResultsWithIndexMultiple : function () {
      c.ensureIndex({ type: "skiplist", fields: ["foo.bar"] });

      var queries = [
        [ "FOR doc IN @@cn FILTER doc.foo.bar == 1 RETURN doc.foo.bar", [ 1 ] ],
        [ "FOR doc IN @@cn FILTER doc.foo.bar <= 1 SORT doc.foo.bar RETURN doc.foo.bar", [ 0, 1 ] ],
        [ "FOR doc IN @@cn FILTER doc.foo.bar >= 132 && doc.foo.bar <= 134 SORT doc.foo.bar RETURN doc.foo.bar", [ 132, 133, 134 ] ],
        [ "FOR doc IN @@cn FILTER doc.foo.bar >= 130 && doc.foo.bar <= 139 SORT doc.foo.bar RETURN doc.foo.bar", [ 130, 131, 132, 133, 134, 135, 136, 137, 138, 139 ] ]
      ];
      
      queries.forEach(function(query) {
        var result = AQL_EXPLAIN(query[0], { "@cn" : cn });
        assertNotEqual(-1, result.plan.rules.indexOf(ruleName), query[0]);
        
        result = AQL_EXECUTE(query[0], { "@cn" : cn });
        assertEqual(query[1], result.json);
      });
    },

    testJoin : function () {
      c.ensureIndex({ type: "skiplist", fields: ["value"] });

      let queries = [
        "FOR doc1 IN @@cn FOR doc2 IN @@cn FILTER doc1.value == doc2._key RETURN [doc1._key, doc2._key]",
        "FOR doc1 IN @@cn FOR doc2 IN @@cn FILTER doc1.value == doc2._key RETURN [doc1.value, doc2._key]",
      ];
      
      queries.forEach(function(query) {
        let result = AQL_EXPLAIN(query, { "@cn" : cn });
        assertNotEqual(-1, result.plan.rules.indexOf(ruleName), query);
      
        let found = 0;
        result.plan.nodes.filter(function(node) { 
          return node.type === 'IndexNode' || node.type === 'EnumerateCollectionNode';
        }).forEach(function(node) {
          assertNotEqual([], node.projections);
          ++found;
        });

        assertEqual(2, found);
      });
    }
    
  };
}

/// @brief executes the test suite
jsunity.run(optimizerRuleTestSuite);

return jsunity.done();
