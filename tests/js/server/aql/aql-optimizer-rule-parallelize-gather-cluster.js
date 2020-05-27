/*jshint globalstrict:false, strict:false, maxlen: 500 */
/*global assertTrue, assertEqual, assertNotEqual, AQL_EXPLAIN, AQL_EXECUTE */

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

let db = require("@arangodb").db;
let jsunity = require("jsunity");

function optimizerRuleTestSuite () {
  const ruleName = "parallelize-gather";
  const cn = "UnitTestsAqlOptimizerRule";
  const en = "UnitTestsAqlOptimizerRuleEdges";
  
  return {

    setUpAll : function () {
      db._drop(cn);
      db._drop(en);
      db._createEdgeCollection(en, { numberOfShards: 5 });
      let c =  db._create(cn, { numberOfShards: 5 });
      let docs = [];
      for (let i = 0; i < 50000; ++i) {
        docs.push({ _key: "test" + i, value1: i, value2: "test" + i });
        if (docs.length === 5000) {
          c.insert(docs);
          docs = [];
        }
      }
      c.ensureIndex({ type: "skiplist", fields: ["value1"] });
    },

    tearDownAll : function () {
      db._drop(en);
      db._drop(cn);
    },

    testDisabled : function () {
      let queries = [ 
        "FOR doc IN " + cn + " RETURN doc",
        "FOR doc IN " + cn + " LIMIT 100 RETURN doc",
        "FOR doc IN " + cn + " SORT doc.value1 RETURN doc",
        "FOR doc IN " + cn + " REMOVE doc IN " + cn + " RETURN doc",
        "FOR doc IN " + cn + " REPLACE doc WITH {} IN " + cn + " RETURN doc",
        "FOR doc IN " + cn + " UPDATE doc WITH {} IN " + cn + " RETURN doc",
      ];

      queries.forEach(function(query) {
        let result = AQL_EXPLAIN(query, null, { optimizer: { rules: ["-" + ruleName ] } });
        assertEqual(-1, result.plan.rules.indexOf(ruleName), query);
      });
    },

    testRuleNoEffect : function () {
      let queries = [  
        "FOR doc IN " + cn + " LIMIT 10 UPDATE doc WITH {} IN " + cn,
        "FOR i IN 1..1000 INSERT {} IN " + cn,
        "FOR doc1 IN " + cn + " FOR doc2 IN " + cn + " FILTER doc1._key == doc2._key RETURN doc1",
        "FOR doc1 IN " + cn + " FOR doc2 IN " + cn + " FOR doc3 IN " + cn + " FILTER doc1._key == doc2._key FILTER doc2._key == doc3._key RETURN doc1",
        "FOR i IN 1..1000 IN " + cn + " FOR doc IN " + cn + " FILTER doc.value == i RETURN doc",
        "FOR i IN 1..100 LET sub = (FOR doc IN " + cn + " FILTER doc.value == i RETURN doc) RETURN sub",
        "LET sub = (FOR doc IN " + cn + " FILTER doc.value == 12 LIMIT 10 RETURN doc) FOR doc IN sub RETURN doc",
        "FOR v, e, p IN 1..1 OUTBOUND '" + cn + "/1' " + en + " RETURN p",
        "FOR doc IN " + cn + " FOR v, e, p IN 1..1 OUTBOUND doc._id " + en + " RETURN p",
        "FOR s IN OUTBOUND SHORTEST_PATH '" + cn + "/1' TO '" + cn + "/2' " + en + " RETURN s",
        "FOR doc IN " + cn + " FOR s IN OUTBOUND SHORTEST_PATH doc._id TO '" + cn + "/2' " + en + " RETURN s",
        "FOR s IN OUTBOUND K_SHORTEST_PATHS '" + cn + "/1' TO '" + cn + "/2' " + en + " RETURN s",
        "FOR doc IN " + cn + " FOR s IN OUTBOUND K_SHORTEST_PATHS doc._id TO '" + cn + "/2' " + en + " RETURN s",
      ];

      queries.forEach(function(query) {
        let result = AQL_EXPLAIN(query, null, { optimizer: { rules: ["-smart-joins", "-inline-subqueries"] } });
        assertEqual(-1, result.plan.rules.indexOf(ruleName), query);
      });
    },

    testRuleHasEffect : function () {
      let queries = [ 
        "FOR doc IN " + cn + " RETURN doc",
        "FOR doc IN " + cn + " LIMIT 1000 RETURN doc",
        "FOR doc IN " + cn + " LIMIT 1000, 1000 RETURN doc",
        "FOR doc IN " + cn + " SORT doc.value1 RETURN doc",
        "FOR doc IN " + cn + " SORT doc.value1 LIMIT 1000 RETURN doc",
        "FOR doc IN " + cn + " SORT doc.value1 LIMIT 1000, 1000 RETURN doc",
      ];

      if (require("internal").options()["query.parallelize-gather-writes"]) {
        queries.concat([
          "FOR doc IN " + cn + " REMOVE doc IN " + cn,
          "FOR doc IN " + cn + " REMOVE doc._key IN " + cn,
          "FOR doc IN " + cn + " REPLACE doc WITH {} IN " + cn,
          "FOR doc IN " + cn + " REPLACE doc WITH {a: 1} IN " + cn,
          "FOR doc IN " + cn + " REPLACE doc._key WITH {} IN " + cn,
          "FOR doc IN " + cn + " REPLACE doc._key WITH {a:1} IN " + cn,
          "FOR doc IN " + cn + " UPDATE doc WITH {} IN " + cn,
          "FOR doc IN " + cn + " UPDATE doc WITH {a: 1} IN " + cn,
          "FOR doc IN " + cn + " UPDATE doc._key WITH {} IN " + cn,
          "FOR doc IN " + cn + " UPDATE doc._key WITH {a:1} IN " + cn,
        ]);
      }

      queries.forEach(function(query) {
        let result = AQL_EXPLAIN(query,);
        assertNotEqual(-1, result.plan.rules.indexOf(ruleName), query);
      });
    },
    
    testRuleHasEffectWrites : function () {
      let queries = [ 
        "FOR doc IN " + cn + " RETURN doc",
        "FOR doc IN " + cn + " LIMIT 1000 RETURN doc",
        "FOR doc IN " + cn + " LIMIT 1000, 1000 RETURN doc",
        "FOR doc IN " + cn + " SORT doc.value1 RETURN doc",
        "FOR doc IN " + cn + " SORT doc.value1 LIMIT 1000 RETURN doc",
        "FOR doc IN " + cn + " SORT doc.value1 LIMIT 1000, 1000 RETURN doc",
      ];

      if (require("internal").options()["query.parallelize-gather-writes"]) {
        queries.concat([
          "FOR doc IN " + cn + " REMOVE doc IN " + cn,
          "FOR doc IN " + cn + " REMOVE doc._key IN " + cn,
          "FOR doc IN " + cn + " REPLACE doc WITH {} IN " + cn,
          "FOR doc IN " + cn + " REPLACE doc WITH {a: 1} IN " + cn,
          "FOR doc IN " + cn + " REPLACE doc._key WITH {} IN " + cn,
          "FOR doc IN " + cn + " REPLACE doc._key WITH {a:1} IN " + cn,
          "FOR doc IN " + cn + " UPDATE doc WITH {} IN " + cn,
          "FOR doc IN " + cn + " UPDATE doc WITH {a: 1} IN " + cn,
          "FOR doc IN " + cn + " UPDATE doc._key WITH {} IN " + cn,
          "FOR doc IN " + cn + " UPDATE doc._key WITH {a:1} IN " + cn,
        ]);
      }

      queries.forEach(function(query) {
        let result = AQL_EXPLAIN(query,);
        assertNotEqual(-1, result.plan.rules.indexOf(ruleName), query);
      });
    },
    
    testResults : function () {
      let queries = [ 
        [ "FOR doc IN " + cn + " RETURN doc", 50000 ],
        [ "FOR doc IN " + cn + " LIMIT 1000 RETURN doc", 1000 ],
        [ "FOR doc IN " + cn + " LIMIT 1000, 1000 RETURN doc", 1000 ],
        [ "FOR doc IN " + cn + " LIMIT 49500, 1000 RETURN doc", 500 ],
        [ "FOR doc IN " + cn + " SORT doc.value1 RETURN doc", 50000 ],
        [ "FOR doc IN " + cn + " SORT doc.value1 LIMIT 1000 RETURN doc", 1000 ],
        [ "FOR doc IN " + cn + " SORT doc.value1 LIMIT 1000, 1000 RETURN doc", 1000 ],
        [ "FOR doc IN " + cn + " SORT doc.value1 LIMIT 49500, 1000 RETURN doc", 500 ],
      ];

      queries.forEach(function(query) {
        let result = AQL_EXPLAIN(query[0]);
        assertNotEqual(-1, result.plan.rules.indexOf(ruleName), query);

        result = AQL_EXECUTE(query[0]).json;
        assertEqual(query[1], result.length);

        // compare if keys are unique and matching our expectations
        let found = {};
        result.forEach(function(doc) {
          found[doc._key] = true;
        });
        assertEqual(query[1], Object.keys(found).length);
      });
    },
    
    testSortedResults : function () {
      let queries = [ 
        [ "FOR doc IN " + cn + " SORT doc.value1 RETURN doc", 0, 50000 ],
        [ "FOR doc IN " + cn + " SORT doc.value1 LIMIT 1000 RETURN doc", 0, 1000 ],
        [ "FOR doc IN " + cn + " SORT doc.value1 LIMIT 1000, 1000 RETURN doc", 1000, 2000 ],
        [ "FOR doc IN " + cn + " SORT doc.value1 LIMIT 49500, 1000 RETURN doc", 49500, 50000 ],
      ];

      queries.forEach(function(query) {
        let result = AQL_EXPLAIN(query[0]);
        assertNotEqual(-1, result.plan.rules.indexOf(ruleName), query);
        
        result = AQL_EXECUTE(query[0]).json;

        // compare if keys are unique and matching our expectations
        let expected = query[1];
        result.forEach(function(doc) {
          assertEqual(expected, doc.value1);
          ++expected;
        });
        assertEqual(query[2], expected);
      });
    },
    
  };
}

jsunity.run(optimizerRuleTestSuite);

return jsunity.done();
