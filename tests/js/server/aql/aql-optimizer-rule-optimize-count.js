/*jshint globalstrict:false, strict:false, maxlen: 500 */
/*global assertEqual, assertNotEqual, AQL_EXPLAIN, AQL_EXECUTE */

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

let jsunity = require("jsunity");
let db = require("internal").db;

function optimizerRuleTestSuite () {
  const ruleName = "optimize-count";
  const cn = "UnitTestsCollection";

  return {

    setUpAll : function () {
      db._drop(cn);
      let c = db._create(cn, { numberOfShards: 3 });

      let docs = [];
      for (let i = 0; i < 5000; ++i) {
        docs.push({ _key: "test" + i, value1: i, value2: i % 5, value3: i });
      }
      c.insert(docs);

      c.ensureIndex({ type: "hash", fields: ["value1"] });
      c.ensureIndex({ type: "hash", fields: ["value2"] });
    },

    tearDownAll : function () {
      db._drop(cn);
    },

    testRuleDisabled : function () {
      let queries = [
        "FOR i IN 1..100 LET c = COUNT(FOR doc IN " + cn + " RETURN doc) RETURN [i, c]",
        "FOR i IN 1..100 LET c = LENGTH(FOR doc IN " + cn + " RETURN doc) RETURN [i, c]",
        "FOR i IN 1..100 RETURN [i, COUNT(FOR doc IN " + cn + " RETURN doc)]",
        "FOR i IN 1..100 RETURN [i, LENGTH(FOR doc IN " + cn + " RETURN doc)]",
        "FOR i IN 1..100 LET c = COUNT(FOR doc IN " + cn + " FILTER doc.value1 == 1 RETURN doc) RETURN [i, c]",
        "FOR i IN 1..100 LET c = LENGTH(FOR doc IN " + cn + " FILTER doc.value1 == 1 RETURN doc) RETURN [i, c]",
      ];

      queries.forEach(function(query) {
        const ruleDisabled = { optimizer: { rules: [ "-" + ruleName ] } };
        let result = AQL_EXPLAIN(query, {}, ruleDisabled);
        assertEqual(-1, result.plan.rules.indexOf(ruleName), query);
      });
    },

    testRuleNoEffect : function () {
      let queries = [
        // subquery result used for something else
        "FOR i IN 1..100 LET sub = (FOR doc IN " + cn + " RETURN doc) LET c = COUNT(sub) + FIRST(sub) RETURN [i, c]",
        "FOR i IN 1..100 LET sub = (FOR doc IN " + cn + " RETURN doc) LET c = FIRST(sub) + COUNT(sub) RETURN [i, c]",
        "FOR i IN 1..100 LET sub = (FOR doc IN " + cn + " RETURN doc) LET c = FIRST(sub) RETURN [i, c]",
        "FOR i IN 1..100 LET sub = (FOR doc IN " + cn + " RETURN doc) LET c = COUNT(sub) LET d = FIRST(sub) RETURN [i, c, d]",
        "FOR i IN 1..100 LET sub = (FOR doc IN " + cn + " RETURN doc) LET c = FIRST(sub) LET d = COUNT(sub) RETURN [i, c, d]",
        "FOR i IN 1..100 LET sub = (FOR doc IN " + cn + " RETURN doc) LET c = COUNT(sub) RETURN [i, c, sub]",
        "FOR i IN 1..100 LET sub = (FOR doc IN " + cn + " RETURN doc) LET c = COUNT(sub) LET d = APPEND(sub, []) RETURN [i, c, d]",
        
        "FOR i IN 1..100 LET sub = (FOR doc IN " + cn + " RETURN doc) LET alias = sub LET c = COUNT(alias) LET d = FIRST(alias) RETURN [i, c, d]",
        "FOR i IN 1..100 LET sub = (FOR doc IN " + cn + " RETURN doc) LET alias = sub LET c = COUNT(alias) LET d = FIRST(sub) RETURN [i, c, d]",

        // nested loops
        "FOR i IN 1..100 LET sub = (FOR x IN 1..2 FOR doc IN " + cn + " RETURN doc) LET c = COUNT(sub) RETURN [i, c]",
        "FOR i IN 1..100 LET sub = (FOR x IN " + cn + " FOR doc IN " + cn + " RETURN doc) LET c = COUNT(sub) RETURN [i, c]",
        "FOR i IN 1..100 LET sub = (FOR doc IN " + cn + " FOR x IN 1..2 RETURN doc) LET c = COUNT(sub) RETURN [i, c]",
        "FOR i IN 1..100 LET sub = (FOR doc IN " + cn + " FOR x IN " + cn + " RETURN doc) LET c = COUNT(sub) RETURN [i, c]",
        
        // modification subquery
        "FOR i IN 1..100 LET sub = (REMOVE CONCAT('foobar', i) IN " + cn + " OPTIONS { ignoreErrors: true }) LET c = COUNT(sub) RETURN [i, c]",
        
        // non deterministic
        "FOR i IN 1..100 LET sub = (FOR doc IN " + cn + " SORT RAND() RETURN doc) LET c = COUNT(sub) RETURN [i, c]",
        
        // different indexes used
        "FOR i IN 1..100 LET sub = (FOR doc IN " + cn + " FILTER doc._key == 'test1' || doc.value1 == 2 RETURN doc) LET c = COUNT(sub) RETURN [i, c]",
        "FOR i IN 1..100 LET sub = (FOR doc IN " + cn + " FILTER doc.value1 == 1 || doc.value2 == 2 RETURN doc) LET c = COUNT(sub) RETURN [i, c]",
        
        // early pruning
        "FOR i IN 1..100 LET sub = (FOR doc IN " + cn + " FILTER doc.value1 == 1 FILTER doc.value2 == 2 RETURN doc) LET c = COUNT(sub) RETURN [i, c]",
        "FOR i IN 1..100 LET sub = (FOR doc IN " + cn + " FILTER doc.value1 == 1 && doc.value2 == 2 RETURN doc) LET c = COUNT(sub) RETURN [i, c]",

        // something else affecting the result
        "FOR i IN 1..100 LET sub = (FOR doc IN " + cn + " FILTER doc.value3 == 1 RETURN doc) LET c = COUNT(sub) RETURN [i, c]",
        
        // TODO: limit not yet supported
        "FOR i IN 1..3 LET sub = (FOR doc IN " + cn + " LIMIT 0, 1 RETURN doc) LET c = COUNT(sub) RETURN c", 
        "FOR i IN 1..3 LET sub = (FOR doc IN " + cn + " LIMIT 1, 1 RETURN doc) LET c = COUNT(sub) RETURN c", 
        "FOR i IN 1..3 LET sub = (FOR doc IN " + cn + " LIMIT 500, 10 RETURN doc) LET c = COUNT(sub) RETURN c", 
        "FOR i IN 1..3 LET sub = (FOR doc IN " + cn + " LIMIT 999, 2 RETURN doc) LET c = COUNT(sub) RETURN c", 
        "FOR i IN 1..3 LET sub = (FOR doc IN " + cn + " LIMIT 4999, 2 RETURN doc) LET c = COUNT(sub) RETURN c", 
        "FOR i IN 1..3 LET sub = (FOR doc IN " + cn + " LIMIT 5000, 2 RETURN doc) LET c = COUNT(sub) RETURN c", 
      
        "FOR i IN 1..3 LET sub = (FOR doc IN " + cn + " FILTER doc.value1 == i LIMIT 1, 1 RETURN doc) LET c = COUNT(sub) RETURN c",
        "FOR i IN 1..3 LET sub = (FOR doc IN " + cn + " FILTER doc.value2 == i LIMIT 1, 1 RETURN doc) LET c = COUNT(sub) RETURN c", 
        "FOR i IN 1..3 LET sub = (FOR doc IN " + cn + " FILTER doc.value2 == i LIMIT 1, 1000 RETURN doc) LET c = COUNT(sub) RETURN c",
        "FOR i IN 1..3 LET sub = (FOR doc IN " + cn + " FILTER doc.value2 == i LIMIT 500, 600 RETURN doc) LET c = COUNT(sub) RETURN c",
        "FOR i IN 1..3 LET sub = (FOR doc IN " + cn + " FILTER doc.value2 == i LIMIT 1000, 1 RETURN doc) LET c = COUNT(sub) RETURN c", 
      ];

      queries.forEach(function(query) {
        let result = AQL_EXPLAIN(query);
        assertEqual(-1, result.plan.rules.indexOf(ruleName), query);
        
        // recheck again with early pruning off
        const noEarlyPruning = { optimizer: { rules: [ "-move-filters-into-enumerate" ] } };
        result = AQL_EXPLAIN(query, null, noEarlyPruning);
        assertEqual(-1, result.plan.rules.indexOf(ruleName), query);
      });
    },

    testFullCount : function () {
      let query = "FOR i IN 1..100 LET c = COUNT(FOR doc IN " + cn + " RETURN doc) RETURN [i, c]";
      let result = AQL_EXPLAIN(query, {}, { fullCount: true });
      assertEqual(-1, result.plan.rules.indexOf(ruleName), query);
    },

    testResultsAllRules : function () {
      let queries = [
        ["FOR i IN 1..3 LET key = CONCAT('test', i) LET sub = (FOR doc IN " + cn + " FILTER doc._key == key RETURN doc) LET c = COUNT(sub) RETURN [key, c]", [ ["test1", 1], ["test2", 1], ["test3", 1] ] ],
        ["FOR i IN 1..3 LET key = CONCAT('test', i) LET sub = (FOR doc IN " + cn + " RETURN doc) LET c = COUNT(sub) RETURN [key, c]", [ ["test1", 5000], ["test2", 5000], ["test3", 5000] ] ],
        ["FOR i IN 1..3 LET sub = (FOR doc IN " + cn + " FILTER doc.value1 == i RETURN doc) LET c = COUNT(sub) RETURN c", [ 1, 1, 1 ] ],
        ["FOR i IN 1..3 LET sub = (FOR doc IN " + cn + " FILTER doc.value2 == i RETURN doc) LET c = COUNT(sub) RETURN c", [ 1000, 1000, 1000 ] ],
        
        ["FOR i IN 1..3 LET sub = (FOR doc IN " + cn + " RETURN doc) LET c = COUNT(sub) RETURN c", [ 5000, 5000, 5000 ] ],
        
        ["FOR i IN 1..3 LET key = CONCAT('none', i) LET sub = (FOR doc IN " + cn + " FILTER doc._key == key RETURN doc) LET c = COUNT(sub) RETURN [key, c]", [ ["none1", 0], ["none2", 0], ["none3", 0] ] ],
      ];

      queries.forEach(function(query) {
        let result = AQL_EXPLAIN(query[0]);
        assertNotEqual(-1, result.plan.rules.indexOf(ruleName), query);
        result = AQL_EXECUTE(query[0]).json;
        assertEqual(query[1], result, query);
      });
    },
    
    testResultsNoProjection : function () {
      let queries = [
        ["FOR i IN 1..3 LET key = CONCAT('test', i) LET sub = (FOR doc IN " + cn + " FILTER doc._key == key RETURN doc) LET c = COUNT(sub) RETURN [key, c]", [ ["test1", 1], ["test2", 1], ["test3", 1] ] ],
        ["FOR i IN 1..3 LET key = CONCAT('test', i) LET sub = (FOR doc IN " + cn + " RETURN doc) LET c = COUNT(sub) RETURN [key, c]", [ ["test1", 5000], ["test2", 5000], ["test3", 5000] ] ],
        ["FOR i IN 1..3 LET sub = (FOR doc IN " + cn + " FILTER doc.value1 == i RETURN doc) LET c = COUNT(sub) RETURN c", [ 1, 1, 1 ] ],
        ["FOR i IN 1..3 LET sub = (FOR doc IN " + cn + " FILTER doc.value2 == i RETURN doc) LET c = COUNT(sub) RETURN c", [ 1000, 1000, 1000 ] ],
        
        ["FOR i IN 1..3 LET sub = (FOR doc IN " + cn + " RETURN doc) LET c = COUNT(sub) RETURN c", [ 5000, 5000, 5000 ] ],
        
        ["FOR i IN 1..3 LET key = CONCAT('none', i) LET sub = (FOR doc IN " + cn + " FILTER doc._key == key RETURN doc) LET c = COUNT(sub) RETURN [key, c]", [ ["none1", 0], ["none2", 0], ["none3", 0] ] ],
      ];

      queries.forEach(function(query) {
        let opts = { optimizer: { rules: ["-reduce-extraction-to-projection"] } };
        let result = AQL_EXPLAIN(query[0], null, opts);
        assertNotEqual(-1, result.plan.rules.indexOf(ruleName), query);
        result = AQL_EXECUTE(query[0], null, opts).json;
        assertEqual(query[1], result, query);
      });
    },

  };
}

jsunity.run(optimizerRuleTestSuite);

return jsunity.done();
