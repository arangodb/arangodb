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

const jsunity = require("jsunity");
const helper = require("@arangodb/aql-helper");
const isEqual = helper.isEqual;
const db = require("@arangodb").db;
const ruleName = "optimize-subqueries";

function optimizerRuleTestSuite () {
  // various choices to control the optimizer: 
  const paramNone = { optimizer: { rules: [ "-all" ] } };
  const paramEnabled = { optimizer: { rules: [ "-all", "+" + ruleName ] } };
  const paramDisabled = { optimizer: { rules: [ "+all", "-" + ruleName ] } };
  const cn = "UnitTestsAhuacatlSubqueries";

  return {

    setUp : function () {
      db._drop(cn);
      db._create(cn, { numberOfShards: 4 });
    },

    tearDown : function () {
      db._drop(cn);
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief test that rule has no effect when explicitly disabled
    ////////////////////////////////////////////////////////////////////////////////

    testRuleDisabled : function () {
      const queries = [ 
        "RETURN FIRST(FOR doc IN " + cn + " RETURN doc)",
        "RETURN (FOR doc IN " + cn + " RETURN doc)[0]",
        "LET docs = (FOR doc IN " + cn + " RETURN doc) RETURN docs[0]",
        "LET docs = (FOR doc IN " + cn + " RETURN doc) RETURN docs[1]",
        "RETURN LENGTH(FOR doc IN " + cn + " RETURN doc)",
        "RETURN LENGTH(FOR doc IN " + cn + " RETURN 1)",
        "LET x = (FOR doc IN " + cn + " RETURN doc) RETURN LENGTH(x)",
      ];

      queries.forEach(function(query) {
        let result = AQL_EXPLAIN(query, { }, paramNone);
        assertEqual(-1, result.plan.rules.indexOf(ruleName), query);
      });
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief test that rule has no effect
    ////////////////////////////////////////////////////////////////////////////////

    testRuleNoEffect : function () {
      const queries = [ 
        "RETURN (INSERT {} INTO " + cn + " RETURN NEW)[0]",
        "RETURN (FOR doc IN " + cn + " RETURN doc)[-1]",
        "LET docs = (FOR doc IN " + cn + " RETURN doc) RETURN docs[-1]",
        "LET docs = (FOR doc IN " + cn + " RETURN doc) RETURN [docs[0], docs]",
        "LET docs = (FOR doc IN " + cn + " RETURN doc) RETURN docs",
        "LET docs = (FOR doc IN " + cn + " RETURN doc) RETURN [LENGTH(docs), docs]",
        "LET docs = (FOR doc IN " + cn + " RETURN doc) RETURN [LENGTH(docs), docs[0]]",
        "LET docs = (FOR doc IN " + cn + " RETURN doc) RETURN [LENGTH(docs), FIRST(docs)]",
        "LET docs = (FOR doc IN " + cn + " RETURN doc) RETURN [LENGTH(docs), SUM(docs)]",
        "LET docs = (FOR doc IN " + cn + " RETURN doc) RETURN UNIQUE(docs)",
        "LET docs = (FOR doc IN " + cn + " RETURN doc) RETURN [docs[0], UNIQUE(docs)]",
      ];

      queries.forEach(function(query) {
        let result = AQL_EXPLAIN(query, { }, paramEnabled);
        assertEqual(-1, result.plan.rules.indexOf(ruleName), query);
        result = AQL_EXPLAIN(query, { }, paramNone);
        assertEqual(-1, result.plan.rules.indexOf(ruleName), query);
      });
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief test that rule has an effect
    ////////////////////////////////////////////////////////////////////////////////

    testRuleHasEffect : function () {
      const queries = [ 
        "RETURN (FOR doc IN " + cn + " RETURN doc)[0]",
        "RETURN (FOR doc IN " + cn + " RETURN doc)[0]",
        "RETURN (FOR doc IN " + cn + " FILTER doc.value > 0 RETURN doc)[0]",
        "RETURN (FOR doc IN " + cn + " FILTER doc.value > 0 RETURN doc)[1]",
        "RETURN FIRST(FOR doc IN " + cn + " RETURN doc)",
        "RETURN LENGTH(FOR doc IN " + cn + " RETURN doc)",
        "RETURN COUNT(FOR doc IN " + cn + " RETURN doc)",
        "LET docs = (FOR doc IN " + cn + " RETURN doc) RETURN FIRST(docs)",
        "LET docs = (FOR doc IN " + cn + " RETURN doc) RETURN COUNT(docs)",
        "LET docs = (FOR doc IN " + cn + " RETURN doc) RETURN LENGTH(docs)",
        "LET docs = (FOR doc IN " + cn + " RETURN doc) RETURN docs[0]",
        "LET docs = (FOR doc IN " + cn + " RETURN doc) RETURN docs[1]",
        "LET docs = (FOR doc IN " + cn + " RETURN doc) RETURN docs[100]",
        "LET docs = (FOR doc IN " + cn + " RETURN doc) RETURN [docs[0], docs[1]]",
      ];

      queries.forEach(function(query) {
        let result = AQL_EXPLAIN(query, { }, paramEnabled);
        assertNotEqual(-1, result.plan.rules.indexOf(ruleName), query);
        result = AQL_EXPLAIN(query, { }, paramNone);
        let resultDisabled = AQL_EXECUTE(query, { }, paramDisabled).json;
        let resultEnabled  = AQL_EXECUTE(query, { }, paramEnabled).json;

        assertTrue(isEqual(resultDisabled, resultEnabled), query[0]);
      });
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief test generated results
    ////////////////////////////////////////////////////////////////////////////////

    testResults : function () {
      for (let i = 0; i < 1000; ++i) {
        db[cn].insert({ value : i });
      }

      let queries = [
        [ "RETURN (FOR doc IN " + cn + " SORT doc.value RETURN doc.value)[0]", [ 0 ] ],
        [ "LET docs = (FOR doc IN " + cn + " SORT doc.value RETURN doc.value) RETURN docs[0]", [ 0 ] ],
        [ "RETURN (FOR doc IN " + cn + " SORT doc.value RETURN doc.value)[2]", [ 2 ] ],
        [ "LET docs = (FOR doc IN " + cn + " SORT doc.value RETURN doc.value) RETURN docs[2]", [ 2 ] ],
        [ "RETURN (FOR doc IN " + cn + " SORT doc.value RETURN doc.value)[99]", [ 99 ] ],
        [ "LET docs = (FOR doc IN " + cn + " SORT doc.value RETURN doc.value) RETURN docs[99]", [ 99 ] ],
        [ "LET docs = (FOR doc IN " + cn + " SORT doc.value RETURN doc.value) RETURN [docs[0], docs[1], docs[7]]", [ [ 0, 1, 7 ] ] ],
        [ "RETURN FIRST(FOR doc IN " + cn + " SORT doc.value RETURN doc.value)", [ 0 ] ],
        [ "LET docs = (FOR doc IN " + cn + " SORT doc.value RETURN doc.value) RETURN FIRST(docs)", [ 0 ] ],
        [ "RETURN LENGTH(FOR doc IN " + cn + " RETURN doc)", [ 1000 ] ],
        [ "LET docs = (FOR doc IN " + cn + " RETURN doc) RETURN LENGTH(docs)", [ 1000 ] ],
        [ "RETURN COUNT(FOR doc IN " + cn + " RETURN doc)", [ 1000 ] ],
        [ "LET docs = (FOR doc IN " + cn + " RETURN doc) RETURN COUNT(docs)", [ 1000 ] ],
        [ "RETURN COUNT(FOR doc IN " + cn + " FILTER doc.value < 10 RETURN doc)", [ 10 ] ],
        [ "LET docs = (FOR doc IN " + cn + " FILTER doc.value < 10 RETURN doc) RETURN COUNT(docs)", [ 10 ] ],
      ];
      queries.forEach(function(query) {
        let result = AQL_EXPLAIN(query[0]);
        assertNotEqual(-1, result.plan.rules.indexOf(ruleName), query);

        result = AQL_EXECUTE(query[0]).json;
        assertEqual(query[1], result, query);
      });
    }

  };
}

jsunity.run(optimizerRuleTestSuite);

return jsunity.done();
