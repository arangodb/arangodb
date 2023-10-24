/*jshint globalstrict:false, strict:false, maxlen: 500 */
/*global assertEqual, assertNotEqual, assertTrue, fail, AQL_EXPLAIN, AQL_EXECUTE */

////////////////////////////////////////////////////////////////////////////////
/// @brief tests for optimizer rules
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
const errors = require("internal").errors;

const ruleName = "reduce-extraction-to-projection";

function optimizerRuleTestSuite () {
  let c = null;
  const cn = "UnitTestsOptimizer";

  return {

    setUp : function () {
      db._drop(cn);
      c = db._create(cn, { numberOfShards: 4 });

      let docs = [];
      for (var i = 0; i < 1000; ++i) {
        docs.push({ value1: i, value2: "test" + i, foo: { bar: i } });
      }
      c.insert(docs);
    },

    tearDown : function () {
      db._drop(cn);
    },

    testNotActive : function () {
      const queries = [
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
        let result = db._createStatement({query: query, bindVars:  { "@cn" : cn }}).explain();
        assertEqual(-1, result.plan.rules.indexOf(ruleName), query);
      });
    },

    testNotActiveBecauseIndexHint : function () {
      // these queries may actually use projections, but they must not use the primary
      // index for scanning
      const queries = [
        "FOR doc IN @@cn OPTIONS { indexHint: 'primary' } RETURN doc",
        "FOR doc IN @@cn OPTIONS { indexHint: 'primary' } RETURN doc.value1",
        "FOR doc IN @@cn OPTIONS { indexHint: 'primary' } RETURN doc.value2",
      ];

      queries.forEach(function(query) {
        let result = db._createStatement({query: query, bindVars:  { "@cn" : cn }}).explain().plan;
        let nodeTypes = result.nodes.map(function(node) { return node.type; });
        assertEqual(-1, nodeTypes.indexOf("IndexNode"));
      });
    },

    testFailBecauseIndexHint : function () {
      const queries = [
        "FOR doc IN @@cn OPTIONS { indexHint: 'aha', forceIndexHint: true } RETURN 1",
        "FOR doc IN @@cn OPTIONS { indexHint: 'aha', forceIndexHint: true } RETURN doc",
        "FOR doc IN @@cn OPTIONS { indexHint: 'aha', forceIndexHint: true } RETURN doc.value1",
        "FOR doc IN @@cn OPTIONS { indexHint: 'aha', forceIndexHint: true } RETURN doc.value2",
        "FOR doc IN @@cn OPTIONS { indexHint: 'aha', forceIndexHint: true } RETURN doc._key",
      ];

      queries.forEach(function(query) {
        try {
          db._createStatement({query: query, bindVars:  { "@cn" : cn }}).explain().plan;
          fail();
        } catch (err) {
          assertEqual(errors.ERROR_QUERY_FORCED_INDEX_HINT_UNUSABLE.code, err.errorNum, query);
        }
      });
    },

    testActive : function () {
      const queries = [
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
        let result = db._createStatement({query: query, bindVars:  { "@cn" : cn }}).explain();
        assertNotEqual(-1, result.plan.rules.indexOf(ruleName), query);
      });
    },

    testActiveScanOnly : function () {
      let queries = [
        "FOR doc IN @@cn RETURN 1",
        "FOR doc IN @@cn OPTIONS { indexHint: 'primary' } RETURN 1",
      ];

      queries.forEach(function(query) {
        let result = db._createStatement({query: query, bindVars:  { "@cn" : cn }}).explain();
        assertNotEqual(-1, result.plan.rules.indexOf(ruleName), query);
      });
    },

    testActiveWithIndex : function () {
      c.ensureIndex({ type: "skiplist", fields: ["value1"] });

      let queries = [
        "FOR doc IN @@cn FILTER doc.value1 == 1 RETURN doc.value1",
        "FOR doc IN @@cn SORT doc.value1 RETURN doc.value1",
        "FOR doc IN @@cn COLLECT v = doc.value1 INTO g RETURN v", // g will be optimized away
        "FOR doc IN @@cn FILTER doc.value1 == 1 SORT doc.value1 RETURN doc.value1",
        "FOR doc IN @@cn FILTER doc.value1 > 1 SORT doc.value1 RETURN doc.value1",
        "FOR doc IN @@cn FILTER doc.value1 == { value1: 1 } RETURN doc.value1",
        "FOR doc IN @@cn SORT doc.value1 RETURN doc.value1"
      ];

      queries.forEach(function(query) {
        let result = db._createStatement({query: query, bindVars:  { "@cn" : cn }}).explain();
        assertNotEqual(-1, result.plan.rules.indexOf(ruleName), query);
      });
    },

    testActiveWithIndexMultiple : function () {
      c.ensureIndex({ type: "skiplist", fields: ["foo.bar"] });

      let queries = [
        "FOR doc IN @@cn FILTER doc.foo.bar == 1 RETURN doc.foo.bar",
        "FOR doc IN @@cn SORT doc.foo.bar RETURN doc.foo.bar",
        "FOR doc IN @@cn COLLECT v = doc.foo.bar INTO g RETURN v", // g will be optimized away
        "FOR doc IN @@cn FILTER doc.foo.bar == 1 SORT doc.foo.bar RETURN doc.foo.bar",
        "FOR doc IN @@cn FILTER doc.foo.bar > 1 SORT doc.foo.bar RETURN doc.foo.bar",
        "FOR doc IN @@cn FILTER doc.foo.bar == { value1: 1 } RETURN doc.foo.bar",
        "FOR doc IN @@cn SORT doc.foo.bar RETURN doc.foo.bar"
      ];

      queries.forEach(function(query) {
        let result = db._createStatement({query: query, bindVars:  { "@cn" : cn }}).explain();
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
        let result = db._createStatement({query: query[0], bindVars:  { "@cn" : cn }}).explain();
        assertNotEqual(-1, result.plan.rules.indexOf(ruleName), query[0]);

        result = db._query(query[0], { "@cn" : cn });
        assertEqual(query[1], result.toArray());
      });
    },

    testResultsMultiple : function () {
      let queries = [
        [ "FOR doc IN @@cn FILTER doc.foo.bar == 1 RETURN 42", [ 42 ] ],
        [ "FOR doc IN @@cn FILTER doc.foo.bar == 1 RETURN doc.foo.bar", [ 1 ] ],
        [ "FOR doc IN @@cn FILTER doc.foo.bar <= 1 SORT doc.foo.bar RETURN doc.foo.bar", [ 0, 1 ] ],
        [ "FOR doc IN @@cn FILTER doc.foo.bar >= 132 && doc.foo.bar <= 134 SORT doc.foo.bar RETURN doc.foo.bar", [ 132, 133, 134 ] ]
      ];

      queries.forEach(function(query) {
        let result = db._createStatement({query: query[0], bindVars:  { "@cn" : cn }}).explain();
        assertNotEqual(-1, result.plan.rules.indexOf(ruleName), query[0]);

        result = db._query(query[0], { "@cn" : cn });
        assertEqual(query[1], result.toArray());
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
        let result = db._createStatement({query: query[0], bindVars:  { "@cn" : cn }}).explain();
        assertNotEqual(-1, result.plan.rules.indexOf(ruleName), query[0]);

        result = db._query(query[0], { "@cn" : cn });
        assertEqual(query[1], result.toArray());
      });
    },

    testResultsWithIndexMultiple : function () {
      c.ensureIndex({ type: "skiplist", fields: ["foo.bar"] });

      let queries = [
        [ "FOR doc IN @@cn FILTER doc.foo.bar == 1 RETURN doc.foo.bar", [ 1 ] ],
        [ "FOR doc IN @@cn FILTER doc.foo.bar <= 1 SORT doc.foo.bar RETURN doc.foo.bar", [ 0, 1 ] ],
        [ "FOR doc IN @@cn FILTER doc.foo.bar >= 132 && doc.foo.bar <= 134 SORT doc.foo.bar RETURN doc.foo.bar", [ 132, 133, 134 ] ],
        [ "FOR doc IN @@cn FILTER doc.foo.bar >= 130 && doc.foo.bar <= 139 SORT doc.foo.bar RETURN doc.foo.bar", [ 130, 131, 132, 133, 134, 135, 136, 137, 138, 139 ] ]
      ];

      queries.forEach(function(query) {
        let result = db._createStatement({query: query[0], bindVars:  { "@cn" : cn }}).explain();
        assertNotEqual(-1, result.plan.rules.indexOf(ruleName), query[0]);

        result = db._query(query[0], { "@cn" : cn });
        assertEqual(query[1], result.toArray());
      });
    },

    testJoin : function () {
      c.ensureIndex({ type: "skiplist", fields: ["value"] });

      let queries = [
        "FOR doc1 IN @@cn FOR doc2 IN @@cn FILTER doc1.value == doc2._key RETURN [doc1._key, doc2._key]",
        "FOR doc1 IN @@cn FOR doc2 IN @@cn FILTER doc1.value == doc2._key RETURN [doc1.value, doc2._key]",
      ];

      const queryOptions = {optimizer: {rules: ["-join-index-nodes"]}};
      queries.forEach(function(query) {
        let result = db._createStatement({query: query, bindVars:  { "@cn" : cn }, options: queryOptions}).explain();
        assertNotEqual(-1, result.plan.rules.indexOf(ruleName), query);

        let found = 0;
        result.plan.nodes.filter(function(node) {
          return node.type === 'IndexNode' || node.type === 'EnumerateCollectionNode';
        }).forEach(function(node) {
          ++found;
        });

        assertEqual(2, found);
      });
    },

    testBts562 : function () {
      c.truncate();
      c.insert({ foo: { attr: 1 }, bar: { attr: 2 } });

      let result = db._query(`FOR doc IN @@cn RETURN [ doc.foo.attr, doc.bar.attr ]`, { "@cn" : cn }).toArray();
      assertEqual(1, result.length);
      assertEqual([ 1, 2 ], result[0]);

      c.truncate();
      c.insert({ foo: { attr: 1 }, bar: { attr: 2 }, baz: { attr: 3 } });

      result = db._query(`FOR doc IN @@cn RETURN [ doc.foo.attr, doc.bar.attr, doc.baz.attr ]`, { "@cn" : cn }).toArray();
      assertEqual(1, result.length);
      assertEqual([ 1, 2, 3 ], result[0]);

      c.truncate();
      c.insert({ result: {} });
      c.insert({ result: { status: "ok" } });

      result = db._query(`FOR d IN @@cn COLLECT resultStatus = d.result.status, requestStatus = d.request.status WITH COUNT INTO count SORT resultStatus RETURN { resultStatus, requestStatus, count }`, { "@cn": cn }).toArray();

      assertEqual(2, result.length);
      assertEqual({ resultStatus: null, requestStatus: null, count: 1 }, result[0]);
      assertEqual({ resultStatus: "ok", requestStatus: null, count: 1 }, result[1]);

      result = db._query(`FOR d IN @@cn COLLECT resultStatus = d.result.status, requestOther = d.request.other WITH COUNT INTO count SORT resultStatus RETURN { resultStatus, requestOther, count }`, { "@cn" : cn }).toArray();
      assertEqual(2, result.length);
      assertEqual({ resultStatus: null, requestOther: null, count: 1 }, result[0]);
      assertEqual({ resultStatus: "ok", requestOther: null, count: 1 }, result[1]);

      c.truncate();
      c.insert({ result: { status: "ok" }, request: { status: "ok" } });
      c.insert({ result: { status: "ok" }, request: { status: "blarg" } });

      result = db._query(`FOR d IN @@cn COLLECT resultStatus = d.result.status, requestStatus = d.request.status WITH COUNT INTO count SORT requestStatus RETURN { resultStatus, requestStatus, count }`, { "@cn" : cn }).toArray();

      assertEqual(2, result.length);
      assertEqual({ resultStatus: "ok", requestStatus: "blarg", count: 1 }, result[0]);
      assertEqual({ resultStatus: "ok", requestStatus: "ok", count: 1 }, result[1]);
    },

    testSearch366: function () {
      c.truncate();
      let doc = c.insert({ i: 0 });
      // store document as sub-document
      db._query("FOR doc IN @@cn UPDATE doc WITH { sub: doc } IN @@cn", { "@cn": cn });

      let results = db._query("FOR doc IN @@cn RETURN doc.sub._id", { "@cn": cn }).toArray();
      assertEqual(1, results.length);
      assertEqual(doc._id, results[0]);

      results = db._query("FOR doc IN @@cn RETURN doc.sub._key", { "@cn": cn }).toArray();
      assertEqual(1, results.length);
      assertEqual(doc._key, results[0]);

      results = db._query("FOR doc IN @@cn RETURN doc.sub._rev", { "@cn": cn }).toArray();
      assertEqual(1, results.length);
      assertEqual(doc._rev, results[0]);

      results = db._query("FOR doc IN @@cn RETURN doc.sub.i", { "@cn": cn }).toArray();
      assertEqual(1, results.length);
      assertEqual(0, results[0]);

      c.truncate();
      doc = c.insert({ i: 0 });
      // store document as sub-document
      db._query("FOR doc1 IN @@cn FOR doc2 IN @@cn INSERT { k: { kk: doc1, ll: { dd: doc2 } } } IN @@cn", { "@cn": cn });

      results = db._query("FOR doc IN @@cn FILTER doc.i != 0 RETURN doc.k.kk._id", { "@cn": cn }).toArray();
      assertEqual(1, results.length);
      assertEqual(doc._id, results[0]);

      results = db._query("FOR doc IN @@cn FILTER doc.i != 0 RETURN doc.k.kk._key", { "@cn": cn }).toArray();
      assertEqual(1, results.length);
      assertEqual(doc._key, results[0]);

      results = db._query("FOR doc IN @@cn FILTER doc.i != 0 RETURN doc.k.kk._rev", { "@cn": cn }).toArray();
      assertEqual(1, results.length);
      assertEqual(doc._rev, results[0]);

      results = db._query("FOR doc IN @@cn FILTER doc.i != 0 RETURN doc.k.ll.dd._id", { "@cn": cn }).toArray();
      assertEqual(1, results.length);
      assertEqual(doc._id, results[0]);

      results = db._query("FOR doc IN @@cn FILTER doc.i != 0 RETURN doc.k.ll.dd._key", { "@cn": cn }).toArray();
      assertEqual(1, results.length);
      assertEqual(doc._key, results[0]);

      results = db._query("FOR doc IN @@cn FILTER doc.i != 0 RETURN doc.k.ll.dd._rev", { "@cn": cn }).toArray();
      assertEqual(1, results.length);
      assertEqual(doc._rev, results[0]);
    },

  };
}

jsunity.run(optimizerRuleTestSuite);

return jsunity.done();


