/*jshint globalstrict:false, strict:false, maxlen: 500 */
/*global assertEqual, assertNotEqual */

// //////////////////////////////////////////////////////////////////////////////
// / DISCLAIMER
// /
// / Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
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
/// @author Jan Steemann
/// @author Copyright 2012, triAGENS GmbH, Cologne, Germany
// //////////////////////////////////////////////////////////////////////////////

const jsunity = require("jsunity");
const db = require('internal').db;

function optimizerRuleTestSuite () {
  const ruleName = "async-prefetch";

  const modificationNodes = ['InsertNode', 'UpdateNode', 'ReplaceNode', 'RemoveNode', 'UpsertNode'];

  const cn = "UnitTestsCollection";
  const en = "UnitTestsEdge";
  const vn = "UnitTestsView";

  return {
    setUpAll : function () {
      let c = db._create(cn);
      c.ensureIndex({ type: "persistent", fields: ["indexed"] });

      db._createEdgeCollection(en);

      db._createView(vn, "arangosearch", {});
    },
    
    tearDownAll : function () {
      db._dropView(vn);
      db._drop(en);
      db._drop(cn);
    },

    testRuleDisabled : function () {
      let queries = [
        "FOR i IN 1..100 RETURN i",
        "FOR i IN 1..100 LET b = i * 2 RETURN b",
        "FOR i IN 1..100 LET b = i * 2 LET c = i * 3 RETURN [b, c]",
        "FOR i IN 1..100 COLLECT v = i % 2 RETURN v",
        "FOR doc IN " + vn + " RETURN doc",
        "FOR doc IN " + cn + " RETURN doc",
        "FOR doc IN " + cn + " RETURN doc._key",
        "FOR doc IN " + cn + " FILTER doc.value > 25 RETURN doc._key",
        "FOR doc IN " + cn + " SORT doc.value RETURN doc._key",
        "FOR doc IN " + cn + " FILTER doc.value > 25 SORT doc.value RETURN doc._key",
      ];

      queries.forEach(function(query) {
        let result = db._createStatement({query, options: {optimizer: {rules: ["-" + ruleName] }}}).explain();
        assertEqual(-1, result.plan.rules.indexOf(ruleName));
      });
    },

    testSinglePlans : function () {
      let queries = [
        [ "FOR i IN 1..100 LET b = i * 2 RETURN b", [ ["SingletonNode", false], ["CalculationNode", true], ["EnumerateListNode", true], ["CalculationNode", true], ["ReturnNode", false] ] ],
        [ "FOR doc IN " + cn + " FILTER doc.value > 3 RETURN doc.a", [ ["SingletonNode", false], ["EnumerateCollectionNode", true], ["ReturnNode", false] ] ],
        [ "FOR doc IN " + cn + " SORT doc.value1 RETURN doc.value2", [ ["SingletonNode", false], ["EnumerateCollectionNode", true], ["SortNode", true], ["ReturnNode", false] ] ],
        [ "FOR i IN 1..100 SORT i RETURN i", [ ["SingletonNode", false], ["CalculationNode", true], ["EnumerateListNode", true], ["SortNode", true], ["ReturnNode", false] ] ],
        [ "FOR doc IN " + cn + " COLLECT v = doc.value % 2 RETURN v", [ ["SingletonNode", false], ["EnumerateCollectionNode", true], ["CalculationNode", true], ["CollectNode", true], ["SortNode", true], ["ReturnNode", false] ] ],
        [ "FOR doc IN " + cn + " RETURN DISTINCT doc.value", [ ["SingletonNode", false], ["EnumerateCollectionNode", true], ["CollectNode", true], ["ReturnNode", false] ] ],
        [ "FOR doc IN " + cn + " FILTER doc.value > 3 LIMIT 10 RETURN doc", [ ["SingletonNode", false], ["EnumerateCollectionNode", false], ["LimitNode", false], ["ReturnNode", false] ] ],
        [ "FOR doc IN " + cn + " FILTER doc.a == '123' RETURN doc", [ ["SingletonNode", false], ["EnumerateCollectionNode", true], ["ReturnNode", false] ] ],
        [ "FOR doc IN " + cn + " FILTER doc._key == '123' RETURN doc", [ ["SingletonNode", false], ["IndexNode", true], ["ReturnNode", false] ] ],
        [ "FOR doc IN " + cn + " FILTER doc._key == '123' LIMIT 3 RETURN doc", [ ["SingletonNode", false], ["IndexNode", false], ["LimitNode", false], ["ReturnNode", false] ] ],
        // views
        [ "FOR doc IN " + vn + " FILTER doc.value > 3 RETURN doc", [ ["SingletonNode", false], ["EnumerateViewNode", true], ["CalculationNode", true], ["FilterNode", true], ["ReturnNode", false] ] ],
        [ "FOR doc IN " + vn + " FILTER doc.value > 3 LIMIT 3 RETURN doc", [ ["SingletonNode", false], ["EnumerateViewNode", false], ["CalculationNode", false], ["FilterNode", false], ["LimitNode", false], ["ReturnNode", false] ] ],
        // modification queries
        [ "FOR i IN 1..1000 INSERT {} INTO " + cn, [ ["SingletonNode", false], ["CalculationNode", false], ["CalculationNode", false], ["EnumerateListNode", false], ["InsertNode", false] ] ],
        [ "FOR doc IN " + cn + " REMOVE doc IN " + cn, [ ["SingletonNode", false], ["IndexNode", false], ["RemoveNode", false] ] ],
        [ "FOR doc IN " + cn + " UPDATE doc WITH {} IN " + cn, [ ["SingletonNode", false], ["CalculationNode", false], ["IndexNode", false], ["UpdateNode", false] ] ],
        [ "FOR doc IN " + cn + " REPLACE doc WITH {} IN " + cn, [ ["SingletonNode", false], ["CalculationNode", false], ["IndexNode", false], ["ReplaceNode", false] ] ],
        // usage of V8
        [ "FOR doc IN " + cn + " FILTER doc.a == V8('123') RETURN doc", [ ["SingletonNode", false], ["EnumerateCollectionNode", true], ["CalculationNode", false], ["FilterNode", true], ["ReturnNode", false] ] ],
        [ "FOR doc IN " + cn + " FILTER doc._key == V8('123') LIMIT 3 RETURN doc", [ ["SingletonNode", false], ["IndexNode", false], ["LimitNode", false], ["ReturnNode", false] ] ],
        [ "FOR doc IN " + cn + " FILTER doc._key == 'fuchs' FILTER doc.a == V8('123') RETURN doc", [ ["SingletonNode", false], ["IndexNode", true], ["CalculationNode", false], ["FilterNode", true], ["ReturnNode", false] ] ],
        // join
        [ "FOR doc1 IN " + cn + " SORT doc1.indexed FOR doc2 IN " + cn + " FILTER doc2.indexed == doc1.indexed RETURN [doc1, doc2]", [ ["SingletonNode", false], ["JoinNode", true], ["CalculationNode", true], ["ReturnNode", false] ] ],
        // subquery
        [ "FOR i IN 1..100 LET sub = (FOR doc IN " + cn + " SORT doc.value RETURN doc.value) FILTER LENGTH(sub) > 0 RETURN sub[0]", [ ["SingletonNode", false], ["SubqueryStartNode", false], ["EnumerateCollectionNode", false], ["SortNode", false], ["SubqueryEndNode", false], ["CalculationNode", true], ["FilterNode", true], ["CalculationNode", true], ["CalculationNode", true], ["EnumerateListNode", true], ["ReturnNode", false] ] ],
        // traversal
        [ "FOR v, e, p IN 1..3 OUTBOUND '" + cn + "/test' " + en + " FILTER v.value == 1 RETURN p", [ ["SingletonNode", false], ["TraversalNode", true], ["ReturnNode", false] ] ],
        [ "FOR v, e, p IN 1..3 OUTBOUND '" + cn + "/test' " + en + " FILTER NOOPT(v.value == 1) SORT e.value RETURN p", [ ["SingletonNode", false], ["TraversalNode", true], ["CalculationNode", false], ["FilterNode", true], ["CalculationNode", true], ["SortNode", true], ["ReturnNode", false] ] ],
        // path queries
        [ "FOR r IN OUTBOUND SHORTEST_PATH '" + cn + "/test1' TO '" + cn + "/test2' " + en + " FILTER LENGTH(r) > 10 RETURN r", [ ["SingletonNode", false], ["ShortestPathNode", true], ["CalculationNode", true], ["FilterNode", true], ["ReturnNode", false] ] ],
        [ "FOR r IN OUTBOUND K_SHORTEST_PATHS '" + cn + "/test1' TO '" + cn + "/test2' " + en + " FILTER LENGTH(r) > 10 RETURN r", [ ["SingletonNode", false], ["EnumeratePathsNode", true], ["CalculationNode", true], ["FilterNode", true], ["ReturnNode", false] ] ],
        [ "FOR r IN 1..3 OUTBOUND K_PATHS '" + cn + "/test1' TO '" + cn + "/test2' " + en + " FILTER LENGTH(r) > 10 RETURN r", [ ["SingletonNode", false], ["EnumeratePathsNode", true], ["CalculationNode", true], ["FilterNode", true], ["ReturnNode", false] ] ],
      ];

      queries.forEach(function(query) {
        let [qs, expectedNodes] = query;

        let result = db._createStatement({query: qs, optimizer: {rules: ["-interchange-adjacent-enumerations"]}}).explain();

        let actualNodes = result.plan.nodes.map((n) => [n.type, n.isAsyncPrefetchEnabled === true ? true : false]);
        assertEqual(expectedNodes, actualNodes, query);
        
        let expectPrefetchRule = expectedNodes.filter((n) => n[1]).length > 0;
        if (expectPrefetchRule) {
          assertNotEqual(-1, result.plan.rules.indexOf(ruleName));
        } else {
          assertEqual(-1, result.plan.rules.indexOf(ruleName));
        }
        
        let containsModification = result.plan.nodes.filter((n) => modificationNodes.includes(n.type)).length > 0;
        if (containsModification) {
          assertEqual(-1, result.plan.rules.indexOf(ruleName));
        }
      });
    },

  };
}

jsunity.run(optimizerRuleTestSuite);

return jsunity.done();
