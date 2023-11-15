/*jshint globalstrict:false, strict:false, maxlen: 500 */
/*global fail, assertEqual, assertNotEqual, assertTrue, assertFalse */

////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2010-2016 ArangoDB GmbH, Cologne, Germany
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
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

const jsunity = require("jsunity");
const db = require("@arangodb").db;
const internal = require('internal');
const _ = require('lodash');

const database = "ReplaceEqualAccessDatabase";

function optimizerRuleReplaceEqualAttributeAccess() {
  return {

    setUpAll: function () {
      db._createDatabase(database);
      db._useDatabase(database);

      for (const name of ["A", "B", "C", "D"]) {
        const coll = db._create(name);
        coll.ensureIndex({type: "persistent", fields: ["x"]});
      }
    },

    tearDownAll: function () {
      db._useDatabase("_system");
      db._dropDatabase(database);
    },

    testReturnEqualPairs: function () {
      const query = `
        for x in 1..10
          for y in 1..20
            filter x == y
            return [x, y]
      `;

      const stmt = db._createStatement({query});
      const plan = stmt.explain().plan;
      assertNotEqual(plan.rules.indexOf("replace-equal-attribute-accesses"), -1);

      const nodes = plan.nodes.map(x => x.type);
      assertNotEqual(nodes.indexOf("CalculationNode"), -1);

      const calc = plan.nodes[nodes.length - 2];
      const expr = calc.expression;
      assertEqual(expr.type, "array");
      assertEqual(expr.subNodes.length, 2);

      const vars = expr.subNodes.map(function (expr) {
        assertTrue(expr.type, "reference");
        return expr.name;
      });

      assertEqual(_.uniq(vars).length, 1);
    },

    testReturnEqualPairsIndex: function () {
      const query = `
        for x in A
          for y in B
            filter x.x == y.x
            return [x.x, y.x]
      `;

      const stmt = db._createStatement({query, options: {optimizer: {rules: ["-join-index-nodes"]}}});
      const plan = stmt.explain().plan;
      assertNotEqual(plan.rules.indexOf("replace-equal-attribute-accesses"), -1);

      // only one index should have a projection. The other index is scan only.
      const indexNodes = plan.nodes.filter(x => x.type === "IndexNode");
      assertEqual(indexNodes.length, 2);

      const [first, second] = indexNodes;
      assertEqual(first.producesResult, true);
      assertEqual(first.projections.length, 1, first.projections);
      assertEqual(second.producesResult, false);
      assertEqual(second.projections.length, 0);

      const nodes = plan.nodes.map(x => x.type);
      assertNotEqual(nodes.indexOf("CalculationNode"), -1);

      const calc = plan.nodes[nodes.indexOf("CalculationNode")];
      const expr = calc.expression;
      assertEqual(expr.type, "array");
      assertEqual(expr.subNodes.length, 2);

      const vars = expr.subNodes.map(function (expr) {
        assertTrue(expr.type, "reference");
        return expr.name;
      });

      assertEqual(_.uniq(vars).length, 1);
    },

    testTransitiveJoin1: function () {
      const query = `
        for a in A
          for b in B
            for c in C
            filter a.x == b.x
            filter b.x == c.x
            return [a.x, b.x, c.x]
      `;

      const stmt = db._createStatement({query});
      const plan = stmt.explain().plan;
      assertNotEqual(plan.rules.indexOf("replace-equal-attribute-accesses"), -1);
      assertNotEqual(plan.rules.indexOf("join-index-nodes"), -1);

      const nodes = plan.nodes.map(x => x.type);
      assertNotEqual(nodes.indexOf("CalculationNode"), -1);

      const calc = plan.nodes[nodes.indexOf("CalculationNode")];
      const expr = calc.expression;
      assertEqual(expr.type, "array");
      assertEqual(expr.subNodes.length, 3);

      const vars = expr.subNodes.map(function (expr) {
        assertTrue(expr.type, "reference");
        return expr.name;
      });

      assertEqual(_.uniq(vars).length, 1);
    },

    testTransitiveJoin2: function () {
      const query = `
        for a in A
          for b in B
            for c in C
            filter a.x == b.x
            filter a.x == c.x
            return [a.x, b.x, c.x]
      `;

      const stmt = db._createStatement({query});
      const plan = stmt.explain().plan;
      assertNotEqual(plan.rules.indexOf("replace-equal-attribute-accesses"), -1);
      assertNotEqual(plan.rules.indexOf("join-index-nodes"), -1);

      const nodes = plan.nodes.map(x => x.type);
      assertNotEqual(nodes.indexOf("CalculationNode"), -1);

      const calc = plan.nodes[nodes.indexOf("CalculationNode")];
      const expr = calc.expression;
      assertEqual(expr.type, "array");
      assertEqual(expr.subNodes.length, 3);

      const vars = expr.subNodes.map(function (expr) {
        assertTrue(expr.type, "reference");
        return expr.name;
      });

      assertEqual(_.uniq(vars).length, 1);
    },
  };
}

jsunity.run(optimizerRuleReplaceEqualAttributeAccess);

return jsunity.done();
