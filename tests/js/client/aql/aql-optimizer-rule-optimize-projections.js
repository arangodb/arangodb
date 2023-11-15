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
const isEnterprise = require("internal").isEnterprise();
const isCluster = require("internal").isCluster();
const normalize = require("@arangodb/aql-helper").normalizeProjections;
const ruleName = "optimize-projections";

function optimizerRuleTestSuite () {
  let c = null;
  const cn = "UnitTestsOptimizer";

  return {

    setUpAll : function () {
      c = db._create(cn);
      c.ensureIndex({ type: "persistent", fields: ["indexed1"] });
      c.ensureIndex({ type: "persistent", fields: ["indexed2", "indexed3"] });
    },

    tearDownAll : function () {
      db._drop(cn);
    },

    testNoProjections : function () {
      const queries = [
        // EnumerateCollectionNode
        // note: must use disableIndex: true here, so that the optimizer does not turn the full
        // scan into an index scan
        ["FOR doc IN @@cn OPTIONS { disableIndex: true } RETURN doc", false],
        ["FOR doc IN @@cn OPTIONS { disableIndex: true } FILTER doc.value1 == 1 RETURN doc", false],
        ["FOR doc IN @@cn OPTIONS { disableIndex: true } FILTER doc.value1 == 1 RETURN DISTINCT doc", false],
        ["FOR doc IN @@cn OPTIONS { disableIndex: true } COLLECT value = doc.value INTO g KEEP doc RETURN [value, g]", false],

        // IndexNode
        ["FOR doc IN @@cn SORT doc.indexed1 RETURN doc", true],
        ["FOR doc IN @@cn FILTER doc.indexed1 == 1 RETURN DISTINCT doc", true],
        ["FOR doc IN @@cn COLLECT value = doc.indexed1 INTO g KEEP doc RETURN [value, g]", true],
      ];

      queries.forEach(function(query) {
        let result = db._createStatement({query: query[0], bindVars: { "@cn" : cn }}).explain();
        let nodes;
        if (query[1]) {
          nodes = result.plan.nodes.filter((node) => node.type === 'IndexNode');
        } else {
          nodes = result.plan.nodes.filter((node) => node.type === 'EnumerateCollectionNode');
        }
        assertEqual(1, nodes.length, query);
        assertEqual([], nodes[0].projections, query);
        if (nodes[0].filterProjections.length) {
          assertNotEqual(-1, result.plan.rules.indexOf(ruleName), query);
        } else {
          assertEqual(-1, result.plan.rules.indexOf(ruleName), query);
        }
      });
    },
    
    testProjections : function () {
      const enn = "EnumerateCollectionNode";
      const inn = "IndexNode";
      const jnn = "JoinNode";

      let queries = [
        // EnumerateCollectionNode
        // note: must use disableIndex: true here, so that the optimizer does not turn the full
        // scan into an index scan
        ["FOR doc IN @@cn OPTIONS { disableIndex: true } RETURN doc.value1", enn, ["value1"], 0 ],
        ["FOR doc IN @@cn OPTIONS { disableIndex: true } FILTER doc.value1 > 3 RETURN doc.value1", enn, ["value1"], 0 ],
        ["FOR doc IN @@cn OPTIONS { disableIndex: true } FILTER doc.value1 > 3 RETURN doc.value2", enn, ["value2"], 0 ],
        ["FOR doc IN @@cn OPTIONS { disableIndex: true } FILTER doc.value1 > 3 RETURN [doc.value1, doc.value2]", enn, ["value1", "value2"], 1 ],
        ["FOR doc IN @@cn OPTIONS { disableIndex: true } FILTER doc.value1 > 3 FILTER doc.value2 > 3 RETURN doc.value5", enn, ["value5"], 0 ],
        ["FOR doc IN @@cn OPTIONS { disableIndex: true } FILTER doc.value1 > 3 FILTER doc.value2 > 3 RETURN [doc.value4, doc.value5]", enn, ["value4", "value5"], 1 ],
        ["FOR doc IN @@cn OPTIONS { disableIndex: true } FILTER doc.value1 > 3 FILTER doc.value2 > 3 FILTER doc.value3 > 4 RETURN doc.value5", enn, ["value5"], 0 ],
        ["FOR doc IN @@cn OPTIONS { disableIndex: true } FILTER doc.value1 > 3 FILTER doc.value2 > 3 FILTER doc.value3 > 4 RETURN doc.value1 + doc.value2 + doc.value3", enn, ["value1", "value2", "value3"], 1 ],
        
        // IndexNode
        ["FOR doc IN @@cn FILTER doc.indexed1 == 1 RETURN doc.indexed1", inn, ["indexed1"], 0 ],
        ["FOR doc IN @@cn FILTER doc.indexed1 == 1 RETURN doc.indexed2", inn, ["indexed2"], 0 ],
        ["FOR doc IN @@cn FILTER doc.indexed1 == 1 RETURN [doc.indexed1, doc.indexed2]", inn, ["indexed1", "indexed2"], 1 ],
        ["FOR doc IN @@cn FILTER doc.indexed1 == 1 RETURN [doc._key, doc.indexed2]", inn, ["_key", "indexed2"], 1 ],
        ["FOR doc IN @@cn FILTER doc.indexed1 == 1 FILTER doc.indexed2 == 2 RETURN doc.indexed2", inn, ["indexed2"], 0 ],
        ["FOR doc IN @@cn FILTER doc.indexed1 == 1 FILTER doc.indexed2 == 2 RETURN doc._key", inn, ["_key"], 0 ],
        ["FOR doc IN @@cn FILTER doc.indexed1 == 1 FILTER doc.indexed2 == 2 RETURN doc._id", inn, ["_id"], 0 ],
        ["FOR doc IN @@cn SORT doc.indexed2 FILTER doc.indexed3 == 2 RETURN doc.indexed2", inn, ["indexed2"], 0 ],
        ["FOR doc IN @@cn SORT doc.indexed2 FILTER doc.indexed3 == 2 RETURN doc._id", inn, ["_id"], 0 ],
      ];

      if (!isCluster || isEnterprise) {
        queries = queries.concat([
          // JoinNode
          ["FOR doc1 IN @@cn SORT doc1.indexed1 FOR doc2 IN @@cn FILTER doc1.indexed1 == doc2.indexed1 RETURN doc1._key", jnn, [["_key"], []], 0 ],
          ["FOR doc1 IN @@cn SORT doc1.indexed1 FOR doc2 IN @@cn FILTER doc1.indexed1 == doc2.indexed1 RETURN [doc1._key, doc2.indexed1]", jnn, [["_key"], ["indexed1"]], 1 ],
          ["FOR doc1 IN @@cn SORT doc1.indexed1 FOR doc2 IN @@cn FILTER doc1.indexed1 == doc2.indexed1 RETURN [doc1._key, doc2._key, doc2.foo]", jnn, [["_key"], ["_key", "foo"]], 1 ],
        ]);
      }

      queries.forEach(function(query) {
        let result = db._createStatement({query: query[0], bindVars: { "@cn" : cn }, options: {optimizer: { rules: ["-interchange-adjacent-enumerations"] }}}).explain();
        let nodes = result.plan.nodes.filter((node) => node.type === query[1]);
        assertEqual(1, nodes.length, query);
        if (query[1] === jnn) {
          // JoinNode needs special handling
          nodes[0].indexInfos.forEach((ii, i) => {
            assertEqual(normalize(query[2][i]), normalize(ii.projections), query);
            ii.projections.forEach((p) => {
              assertTrue(p.hasOwnProperty("variable"));
              assertMatch(/^\d+$/, p.variable.name);
            });
          });
        } else {
          assertEqual(normalize(query[2]), normalize(nodes[0].projections), query);
          nodes[0].projections.forEach((p) => {
            assertTrue(p.hasOwnProperty("variable"));
            assertMatch(/^\d+$/, p.variable.name);
          });
        }
        assertNotEqual(-1, result.plan.rules.indexOf(ruleName), query);
        assertEqual(query[3], result.plan.nodes.filter((node) => node.type === 'CalculationNode').length, query);
      });
    },

  };
}

jsunity.run(optimizerRuleTestSuite);

return jsunity.done();
