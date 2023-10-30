/*jshint globalstrict:false, strict:false, maxlen: 500 */
/*global assertEqual, assertNotEqual, assertMatch, assertTrue, fail */

////////////////////////////////////////////////////////////////////////////////
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
const normalize = require("@arangodb/aql-helper").normalizeProjections;

const ruleName = "optimize-projections";

function optimizerRuleTestSuite () {
  let c = null;
  const cn = "UnitTestsOptimizer";

  return {

    setUp : function () {
      c = db._create(cn);
    },

    tearDown : function () {
      db._drop(cn);
    },

    testNoProjections : function () {
      const queries = [
        // note: must use disableIndex: true here, so that the optimizer does not turn the full
        // scan into an index scan
        "FOR doc IN @@cn OPTIONS { disableIndex: true } RETURN doc",
        "FOR doc IN @@cn OPTIONS { disableIndex: true } FILTER doc.value1 == 1 RETURN doc",
        "FOR doc IN @@cn OPTIONS { disableIndex: true } FILTER doc.value1 == 1 RETURN DISTINCT doc",
        "FOR doc IN @@cn OPTIONS { disableIndex: true } COLLECT value = doc.value INTO g KEEP doc RETURN [value, g]",
      ];

      queries.forEach(function(query) {
        let result = db._createStatement({query, bindVars: { "@cn" : cn }}).explain();
        let nodes = result.plan.nodes.filter((node) => node.type === 'EnumerateCollectionNode');
        assertEqual(1, nodes.length);
        assertEqual([], nodes[0].projections);
        if (nodes[0].filterProjections.length) {
          assertNotEqual(-1, result.plan.rules.indexOf(ruleName), query);
        } else {
          assertEqual(-1, result.plan.rules.indexOf(ruleName), query);
        }
      });
    },

    testProjections : function () {
      const queries = [
        // note: must use disableIndex: true here, so that the optimizer does not turn the full
        // scan into an index scan
        ["FOR doc IN @@cn OPTIONS { disableIndex: true } RETURN doc.value1", ["value1"], 0 ],
        ["FOR doc IN @@cn OPTIONS { disableIndex: true } FILTER doc.value1 > 3 RETURN doc.value1", ["value1"], 0 ],
        ["FOR doc IN @@cn OPTIONS { disableIndex: true } FILTER doc.value1 > 3 RETURN doc.value2", ["value2"], 0 ],
        ["FOR doc IN @@cn OPTIONS { disableIndex: true } FILTER doc.value1 > 3 RETURN [doc.value1, doc.value2]", ["value1", "value2"], 1 ],
        ["FOR doc IN @@cn OPTIONS { disableIndex: true } FILTER doc.value1 > 3 FILTER doc.value2 > 3 RETURN doc.value5", ["value5"], 0 ],
        ["FOR doc IN @@cn OPTIONS { disableIndex: true } FILTER doc.value1 > 3 FILTER doc.value2 > 3 RETURN [doc.value4, doc.value5]", ["value4", "value5"], 1 ],
        ["FOR doc IN @@cn OPTIONS { disableIndex: true } FILTER doc.value1 > 3 FILTER doc.value2 > 3 FILTER doc.value3 > 4 RETURN doc.value5", ["value5"], 0 ],
        ["FOR doc IN @@cn OPTIONS { disableIndex: true } FILTER doc.value1 > 3 FILTER doc.value2 > 3 FILTER doc.value3 > 4 RETURN doc.value1 + doc.value2 + doc.value3", ["value1", "value2", "value3"], 1 ],
      ];

      queries.forEach(function(query) {
        let result = db._createStatement({query: query[0], bindVars:{ "@cn" : cn }}).explain();
        let nodes = result.plan.nodes.filter((node) => node.type === 'EnumerateCollectionNode');
        assertEqual(1, nodes.length);
        assertEqual(normalize(query[1]), normalize(nodes[0].projections), query);
        nodes[0].projections.forEach((p) => {
          assertTrue(p.hasOwnProperty("variable"));
          assertMatch(/^\d+$/, p.variable.name);
        });
        assertNotEqual(-1, result.plan.rules.indexOf(ruleName), query);
        assertEqual(query[2], result.plan.nodes.filter((node) => node.type === 'CalculationNode').length, query);
      });
    },

  };
}

jsunity.run(optimizerRuleTestSuite);

return jsunity.done();
