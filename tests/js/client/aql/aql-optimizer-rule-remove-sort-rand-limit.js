/*jshint globalstrict:false, strict:false, maxlen: 500 */
/*global assertEqual, assertTrue, assertFalse, assertNotEqual, assertUndefined */

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

var jsunity = require("jsunity");
var helper = require("@arangodb/aql-helper");
var db = require("@arangodb").db;
var removeAlwaysOnClusterRules = helper.removeAlwaysOnClusterRules;
var removeClusterNodes = helper.removeClusterNodes;

function optimizerRuleTestSuite () {
  const ruleName = "remove-sort-rand-limit-1";
  let c;

  return {

    setUpAll : function () {
      db._drop("UnitTestsCollection");
      c = db._create("UnitTestsCollection");

      let docs = [];
      for (let i = 0; i < 1000; ++i) {
        docs.push({ value: i });
      }
      c.insert(docs);
    },

    tearDownAll : function () {
      db._drop("UnitTestsCollection");
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test that rule has no effect
////////////////////////////////////////////////////////////////////////////////

    testRuleNoEffect : function () {
      let queries = [ 
        "FOR i IN 1..10 SORT RAND() RETURN i",  // no collection
        "FOR i IN 1..10 SORT RAND() LIMIT 1 RETURN i",  // no collection
        "FOR i IN " + c.name() + " SORT RAND() LIMIT 2 RETURN i", 
        "FOR i IN " + c.name() + " SORT RAND() LIMIT 10 RETURN i", 
        "FOR i IN " + c.name() + " SORT RAND() LIMIT 0 RETURN i", 
        "FOR i IN " + c.name() + " SORT i.value, RAND() LIMIT 1 RETURN i", 
        "FOR i IN " + c.name() + " SORT RAND(), i.value RETURN i", // more than one sort criterion
        "FOR i IN " + c.name() + " FOR j IN " + c.name() + " SORT RAND() RETURN i", // more than one collection
        "FOR i IN " + c.name() + " FOR j IN " + c.name() + " SORT RAND() LIMIT 1 RETURN i", // more than one collection
      ];

      queries.forEach(function(query) {
        let result = db._createStatement(query).explain();
        assertEqual(-1, result.plan.rules.indexOf(ruleName), query);
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test that rule has an effect
////////////////////////////////////////////////////////////////////////////////

    testRuleHasEffect : function () {
      let queries = [ 
        "FOR i IN " + c.name() + " SORT RAND() LIMIT 1 RETURN i", 
        "FOR i IN " + c.name() + " SORT RAND() LIMIT 1 RETURN i.value", 
        "FOR i IN " + c.name() + " SORT RAND() LIMIT 1 RETURN i._key", 
        "FOR i IN " + c.name() + " SORT RAND() LIMIT 1 RETURN i._id", 
        "FOR i IN " + c.name() + " SORT RAND() ASC LIMIT 1 RETURN i", 
        "FOR i IN " + c.name() + " SORT RAND() ASC LIMIT 1 RETURN i.value", 
        "FOR i IN " + c.name() + " SORT RAND() ASC LIMIT 1 RETURN i._key", 
        "FOR i IN " + c.name() + " SORT RAND() ASC LIMIT 1 RETURN i._id", 
        "FOR i IN " + c.name() + " SORT RAND() DESC LIMIT 1 RETURN i", 
        "FOR i IN " + c.name() + " SORT RAND() DESC LIMIT 1 RETURN i.value", 
        "FOR i IN " + c.name() + " SORT RAND() DESC LIMIT 1 RETURN i._key", 
        "FOR i IN " + c.name() + " SORT RAND() DESC LIMIT 1 RETURN i._id", 
      ];

      queries.forEach(function(query) {
        let result = db._createStatement(query).explain();
        assertNotEqual(-1, result.plan.rules.indexOf(ruleName), query);
        assertEqual(-1, result.plan.nodes.map(function(node) { return node.type; }).indexOf("IndexNode"));
        let collectionNode = result.plan.nodes.map(function(node) { return node.type; }).indexOf("EnumerateCollectionNode");
        if (collectionNode !== -1) {
          assertTrue(result.plan.nodes[collectionNode].random); // check for random iteration flag
          assertTrue(result.plan.nodes[collectionNode].estimatedNrItems <= 1);
        }
      });
    },

  };
}

jsunity.run(optimizerRuleTestSuite);

return jsunity.done();
