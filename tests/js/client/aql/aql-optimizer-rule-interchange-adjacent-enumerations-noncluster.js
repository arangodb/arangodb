/*jshint globalstrict:false, strict:false, maxlen: 500 */
/*global assertEqual, assertTrue */

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
const helper = require("@arangodb/aql-helper");
const isEqual = helper.isEqual;
const db = require("@arangodb").db;
const _ = require("lodash");
const executeJson =  helper.executeJson;

function optimizerRuleTestSuite () {
  const ruleName = "interchange-adjacent-enumerations";

  // various choices to control the optimizer: 
  const paramNone     = { optimizer: { rules: [ "-all" ] } };
  const paramEnabled  = { optimizer: { rules: [ "-all", "+" + ruleName ] } };
  const paramDisabled = { optimizer: { rules: [ "+all", "-" + ruleName ] } };

  const collectionName = "UnitTestsAhuacatlOptimizer";

  return {

    setUpAll : function () {
      db._drop(collectionName);
      let collection = db._create(collectionName);

      let docs = [];
      for (let i = 0; i < 10; ++i) {
        docs.push({ value: i });
      }
      collection.insert(docs);
    },

    tearDownAll : function () {
      db._drop(collectionName);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test that rule has no effect when explicitly disabled
////////////////////////////////////////////////////////////////////////////////

    testRuleDisabled : function () {
      let queries = [ 
        "FOR i IN " + collectionName + " FOR j IN " + collectionName + " RETURN 1",
        "FOR j IN " + collectionName + " FILTER j.i == 1 FOR i IN " + collectionName + " RETURN j"
      ];
      
      let opts = _.clone(paramNone);
      opts.allPlans = true;
      opts.verbosePlans = true;

      queries.forEach(function(query) {
        let result = db._createStatement({query, bindVars: {}, options: opts}).explain();
        result.plans.forEach(function(plan) {
          assertEqual([ ], plan.rules);
        });
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test that rule has no effect
////////////////////////////////////////////////////////////////////////////////

    testRuleNoEffect : function () {
      let queries = [ 
        "FOR i IN 1..10 RETURN i",
        "FOR i IN " + collectionName + " RETURN i",
        "FOR i IN " + collectionName + " FILTER i == 1 FOR j IN " + collectionName + " RETURN i",
        "FOR i IN " + collectionName + " LIMIT 1 FOR j IN " + collectionName + " RETURN i",
        "FOR i IN " + collectionName + " RETURN (FOR j IN " + collectionName + " RETURN j)",
        // the following query must not be optimized because "sub" depends on "i"
        "FOR i IN " + collectionName + " FOR sub IN i FILTER sub.value1 == 'test' && sub.value2 != '' RETURN i",
        "FOR i IN " + collectionName + " FOR sub1 IN i FOR sub2 IN sub1 FILTER sub2.value1 == 'test' && sub2.value2 != '' RETURN i",
        "FOR i IN " + collectionName + " FOR sub1 IN i FOR sub2 IN i FILTER sub2.value1 == 'test' && sub2.value2 != '' RETURN i",
        "FOR i IN " + collectionName + " FOR sub1 IN i FOR sub2 IN i FILTER sub2.value1 == 'test' && sub2.value2 != '' && sub2.value != sub1 RETURN i",
        // defined order for EnumerateList -> EnumerateCollection with dependencies
        "FOR i IN 1..10 FOR j IN " + collectionName + " FILTER j.value == i RETURN j",
      ];

      let opts = _.clone(paramEnabled);
      opts.allPlans = true;
      opts.verbosePlans = true;

      queries.forEach(function(query) {
        let result = db._createStatement({query, bindVars: {}, options: opts}).explain();
        result.plans.forEach(function(plan) {
          assertEqual(-1, plan.rules.indexOf(ruleName), query);
        });
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test that rule has an effect
////////////////////////////////////////////////////////////////////////////////

    testRuleHasEffect : function () {
      let queries = [
        [ "FOR i IN " + collectionName + " FOR j IN " + collectionName + " RETURN i", 1 ],
        [ "FOR i IN 1..10 FOR j IN " + collectionName + " FOR k IN " + collectionName + " RETURN i", 5 ],
        [ "FOR i IN " + collectionName + " FOR j IN " + collectionName + " FOR k IN " + collectionName + " RETURN i", 5 ],
        [ "FOR i IN " + collectionName + " FOR j IN " + collectionName + " FOR k IN " + collectionName + " FOR l IN " + collectionName + " RETURN i", 23 ],
        [ "FOR x IN (FOR i IN " + collectionName + " FOR j IN " + collectionName + " RETURN i) RETURN x", 1 ],
        [ "FOR x IN (FOR i IN " + collectionName + " FOR j IN " + collectionName + " FOR k IN " + collectionName + " RETURN i) RETURN x", 5 ],
        [ "FOR x IN (FOR i IN " + collectionName + " FOR j IN " + collectionName + " FOR k IN " + collectionName + " RETURN i) FOR y IN (FOR i IN " + collectionName + " FOR j IN " + collectionName + " RETURN i) RETURN x", 11 ],
        [ "FOR i IN " + collectionName + " FOR j IN " + collectionName + " FILTER i.value == j.value RETURN j", 1 ],
        [ "LET x = 1..10 FOR j IN " + collectionName + " FOR i IN x FILTER j.value == i RETURN j", 1 ],
      ];
      
      let opts = _.clone(paramEnabled);
      opts.allPlans = true;
      opts.verbosePlans = true;

      queries.forEach(function(query) {
        let withRule = 0;
        let withoutRule = 0;

        let result = db._createStatement({query: query[0], bindVars: {}, options: opts}).explain();
        result.plans.forEach(function(plan) {
          if (plan.rules.indexOf(ruleName) === -1) {
            withoutRule++;
          } else {
            withRule++;
          }
        });
      
        // there should still be the original plan
        assertEqual(1, withoutRule, query[0]);

        // put there should also be permuted plans
        assertEqual(query[1], withRule, query[0]);
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test results
////////////////////////////////////////////////////////////////////////////////

    testResults : function () {
      let queries = [ 
        [ "FOR i IN " + collectionName + " FOR j IN " + collectionName + " SORT i.value, j.value FILTER i.value == j.value RETURN i.value", [ 0, 1, 2, 3, 4, 5, 6, 7, 8, 9 ] ],
        [ "FOR j IN " + collectionName + " FOR i IN " + collectionName + " SORT i.value, j.value FILTER i.value == j.value RETURN i.value", [ 0, 1, 2, 3, 4, 5, 6, 7, 8, 9 ] ],
        [ "FOR x IN (FOR i IN " + collectionName + " FOR j IN " + collectionName + " RETURN { i: i.value, j: j.value }) FILTER x.i == x.j SORT x.i RETURN x.i", [ 0, 1, 2, 3, 4, 5, 6, 7, 8, 9 ] ]
      ];
      
      let opts = _.clone(paramEnabled);
      opts.allPlans = true;
      opts.verbosePlans = true;

      queries.forEach(function(query) {
        let planDisabled   = db._createStatement({query: query[0], bindVars: {}, options: paramDisabled}).explain();
        let plansEnabled   = db._createStatement({query: query[0], bindVars: {}, options: opts}).explain();
        let resultDisabled = db._query(query[0], {}, paramDisabled).toArray();

        assertTrue(planDisabled.plan.rules.indexOf(ruleName) === -1, query[0]);
        assertEqual(resultDisabled, query[1]);

        assertTrue(plansEnabled.plans.length > 1);

        // iterate over all plans
        let withRule = 0;
        plansEnabled.plans.forEach(function(plan) {
          let resultEnabled = executeJson(plan).json;
          assertTrue(isEqual(resultDisabled, resultEnabled), query[0]);
          if (plan.rules.indexOf(ruleName) !== -1) {
            withRule++;
          }
          assertEqual(resultEnabled, query[1]);
        });
          
        assertTrue(withRule > 0);

      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test many permutations
////////////////////////////////////////////////////////////////////////////////

    testManyPermutations : function () {
      let query = "FOR i IN " + collectionName + " " +
                  "FOR j IN " + collectionName + " " + 
                  "FOR k IN " + collectionName + " " + 
                  "FOR l IN " + collectionName + " RETURN 1";

      let explain = db._createStatement(query).explain();
      assertEqual(24, explain.stats.plansCreated); // faculty of 4 
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test too many permutations
////////////////////////////////////////////////////////////////////////////////

    testTooManyPermutations : function () {
      let query = "FOR i IN " + collectionName + " " +
                  "FOR j IN " + collectionName + " " + 
                  "FOR k IN " + collectionName + " " + 
                  "FOR l IN " + collectionName + " " + 
                  "FOR m IN " + collectionName + " " + 
                  "FOR n IN " + collectionName + " " + 
                  "FOR o IN " + collectionName + " RETURN 1";

      let explain = db._createStatement(query).explain();
      assertEqual(128, explain.stats.plansCreated); // default limit enforced by optimizer
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test too many permutations
////////////////////////////////////////////////////////////////////////////////

    testTooManyPermutationsUnlimited : function () {
      let query = "FOR i IN " + collectionName + " " +
                  "FOR j IN " + collectionName + " " + 
                  "FOR k IN " + collectionName + " " + 
                  "FOR l IN " + collectionName + " " + 
                  "FOR m IN " + collectionName + " " + 
                  "FOR n IN " + collectionName + " " + 
                  "FOR o IN " + collectionName + " RETURN 1";

      let explain = db._createStatement({query, bindVars: null, options: { maxNumberOfPlans: 9999999 }}).explain();
      assertEqual(5040, explain.stats.plansCreated); // faculty of 7 
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief usage of index hints 
////////////////////////////////////////////////////////////////////////////////

    testIndexHintTwoLoops : function () {
      db[collectionName].ensureIndex({ type: "persistent", fields: ["value"], name: "valueIndex" });

      // test with varying amount of documents
      [0, 10, 100, 100].forEach((n) => {
        db[collectionName].truncate();
        let docs = [];
        for (let i = 0; i < n; ++i) {
          docs.push({value: i});
        }
        db[collectionName].insert(docs);

        // background: in previous versions, the optimizer permuted the
        // order of the two FOR loops, and depending on the number of 
        // documents in the collection, preferred the following of the
        // two resulting plans:
        //   FOR doc IN collectionName OPTIONS ...
        //     FOR i IN [1, 2, 3]
        //       FILTER doc.value == i
        // this resulted in the first FOR loop being turned into a full
        // collection scan, which cannot use the index as specified by
        // the index hint. this then results in the query aborted with
        // the error that the index hint cannot be used.
        // this is now "fixed" by the optimizer not permuting FOR loops
        // if an EnumerateCollectionNode is immediately followed by a
        // filter condition on it.
        let query = `
        FOR i IN [1, 2, 3] 
          FOR doc IN ${collectionName} OPTIONS { indexHint: 'valueIndex', forceIndexHint: true } 
            FILTER doc.value == i 
            RETURN doc`;
        let explain = db._createStatement({query, bindVars: null, options: { allPlans: true }}).explain();
        assertEqual(1, explain.plans.length);
        explain.plans.forEach((p) => {
          let nodes = p.nodes.filter((n) => n.type === 'IndexNode');
          assertEqual(1, nodes.length);
          assertEqual(1, nodes[0].indexes.length);
          assertEqual("valueIndex", nodes[0].indexes[0].name);
          assertEqual(-1, p.rules.indexOf(ruleName));
        });
      });
    },
    
    testIndexHintMultipleLoops : function () {
      db[collectionName].ensureIndex({ type: "persistent", fields: ["value"], name: "valueIndex" });

      // test with varying amount of documents
      [0, 10, 100, 100].forEach((n) => {
        db[collectionName].truncate();
        let docs = [];
        for (let i = 0; i < n; ++i) {
          docs.push({value: i});
        }
        db[collectionName].insert(docs);

        let query = `
        FOR i IN [1, 2, 3] 
          FOR doc1 IN ${collectionName} OPTIONS { indexHint: 'valueIndex', forceIndexHint: true } 
            FILTER doc1.value == i 
            FOR doc2 IN ${collectionName} OPTIONS { indexHint: 'valueIndex', forceIndexHint: true } 
              FILTER doc2.value == i 
              RETURN [doc1, doc2]`;
        let explain = db._createStatement({query, bindVars: null, options: { allPlans: true }}).explain();
        assertEqual(1, explain.plans.length);
        explain.plans.forEach((p) => {
          let nodes = p.nodes.filter((n) => n.type === 'IndexNode');
          assertEqual(2, nodes.length);
          assertEqual(1, nodes[0].indexes.length);
          assertEqual("valueIndex", nodes[0].indexes[0].name);
          assertEqual(-1, p.rules.indexOf(ruleName));
        });
      });
    },

  };
}

jsunity.run(optimizerRuleTestSuite);

return jsunity.done();
