/*jshint globalstrict:false, strict:false, maxlen: 500 */
/*global assertEqual, assertNotEqual, assertMatch, assertTrue, fail */

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
// //////////////////////////////////////////////////////////////////////////////

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

function optimizerRuleRegressionsTestSuite () {
  const vn = ["v1", "v2", "v3"];
  const en = ["e1", "e2"];

  return {
    setUp : function () {
      vn.forEach((n) => { db._create(n); });
      en.forEach((n) => { db._createEdgeCollection(n); });
    },

    tearDown : function () {
      vn.forEach((n) => { db._drop(n); });
      en.forEach((n) => { db._drop(n); });
    },

    // verifies an issue with projection variables not being correctly replaced
    // inside traversals
    testOptimizeProjectionsTraversalRegression: function () {
      let k1 = db.v1.insert({ from: 1704067200000 })._id;
      let k2 = db.v2.insert({ from: 1514764800000 })._id;
      let k3 = db.v3.insert({ start: 1619827200000 })._id;

      db.e1.insert({ _from: k2, _to: k1 });
      db.e2.insert({ _from: k3, _to: k2 });

      const query = `
WITH v1, v2, v3
FOR doc1 IN v1 FILTER doc1._id == '${k1}'
FOR doc2 IN 1..1 INBOUND doc1 e1
FOR doc3 IN 1..1 INBOUND doc2 e2
FILTER doc3.start < doc1.from
RETURN { s: doc3.start, f: doc1.from }`;
      
      let result = db._query(query).toArray();
      assertEqual(1, result.length);
      assertEqual({ s: 1619827200000, f: 1704067200000 }, result[0]);
    },
    
    testOptimizeProjectionsTraversalPruneRegression: function () {
      let k1 = db.v1.insert({ from: 1704067200000 })._id;
      let k2 = db.v2.insert({ from: 1514764800000 })._id;
      let k3 = db.v3.insert({ start: 1619827200000 })._id;

      db.e1.insert({ _from: k2, _to: k1 });
      db.e2.insert({ _from: k3, _to: k2 });

      const query = `
WITH v1, v2, v3
FOR doc1 IN v1 FILTER doc1._id == '${k1}'
FOR doc2 IN 1..1 INBOUND doc1 e1
FOR doc3 IN 1..1 INBOUND doc2 e2
PRUNE doc3.start > doc1.from
RETURN { s: doc3.start, f: doc1.from }`;
      
      let result = db._query(query).toArray();
      assertEqual(1, result.length);
      assertEqual({ s: 1619827200000, f: 1704067200000 }, result[0]);
    },
    
    testOptimizeProjectionsKShortestPathsRegression: function () {
      let k1 = db.v1.insert({})._id;
      let k2 = db.v1.insert({})._id;
      let k3 = db.v1.insert({})._id;

      db.e1.insert({ _from: k2, _to: k1, value: 1 });
      db.e1.insert({ _from: k3, _to: k2, value: 1 });

      const query = `
WITH v1
FOR doc1 IN v1 FILTER doc1._id == '${k3}'
FOR doc2 IN v1 FILTER doc2._id == '${k1}'
FOR p IN OUTBOUND K_SHORTEST_PATHS doc1._id TO doc2._id e1
FILTER p.edges[*].value ALL == 1
RETURN p.vertices[*]._id`;

      let result = db._query(query).toArray(); 
      assertEqual(1, result.length);
      assertEqual(3, result[0].length);
      assertEqual([k3, k2, k1], result[0]);
    },
    
    testOptimizeProjectionsKPathsRegression: function () {
      let k1 = db.v1.insert({})._id;
      let k2 = db.v1.insert({})._id;
      let k3 = db.v1.insert({})._id;

      db.e1.insert({ _from: k2, _to: k1, value: 1 });
      db.e1.insert({ _from: k3, _to: k2, value: 1 });

      const query = `
WITH v1
FOR doc1 IN v1 FILTER doc1._id == '${k3}'
FOR doc2 IN v1 FILTER doc2._id == '${k1}'
FOR p IN 1..5 OUTBOUND K_PATHS doc1._id TO doc2._id e1
FILTER p.edges[*].value ALL == 1
RETURN p.vertices[*]._id`;

      let result = db._query(query).toArray(); 
      assertEqual(1, result.length);
      assertEqual(3, result[0].length);
      assertEqual([k3, k2, k1], result[0]);
    },
    
    testOptimizeProjectionsShortestPathRegression: function () {
      let k1 = db.v1.insert({})._id;
      let k2 = db.v1.insert({})._id;
      let k3 = db.v1.insert({})._id;

      db.e1.insert({ _from: k2, _to: k1 });
      db.e1.insert({ _from: k3, _to: k2 });

      const query = `
WITH v1
FOR doc1 IN v1 FILTER doc1._id == '${k3}'
FOR doc2 IN v1 FILTER doc2._id == '${k1}'
FOR v, e IN OUTBOUND SHORTEST_PATH doc1._id TO doc2._id e1
RETURN v._id`;

      let result = db._query(query).toArray();
      assertEqual(3, result.length);
      assertEqual([k3, k2, k1], result);
    },

  };
}

jsunity.run(optimizerRuleTestSuite);
jsunity.run(optimizerRuleRegressionsTestSuite);

return jsunity.done();
