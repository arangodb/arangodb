/*jshint globalstrict:false, strict:false, maxlen: 500 */
/*global fail, assertEqual, assertNotEqual, assertTrue, assertFalse */

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
/// @author Wilfried Goesgens
// //////////////////////////////////////////////////////////////////////////////

const jsunity = require("jsunity");
const db = require("@arangodb").db;
const internal = require('internal');
const _ = require('lodash');

const database = "ReplaceEqualAccessDatabase";

function optimizerRuleReplaceEqualAttributeAccess() {
  const options = {optimizer: {rules: ["+replace-equal-attribute-accesses"]}};

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

      const stmt = db._createStatement({query, options});
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

    testReturnEqualPairsOr: function () {
      const query = `
        for x in 1..10
          for y in 1..20
            for z in 1..20
            filter x == y || x == z
            return [x, y, z]
      `;

      const stmt = db._createStatement({query, options});
      const plan = stmt.explain().plan;
      assertEqual(plan.rules.indexOf("replace-equal-attribute-accesses"), -1);
    },

    testReturnEqualPairsMixed: function () {
      const query = `
        for x in 1..10
          for y in 1..20
            for z in 1..20
              for w in 1..20
            filter (x == w) && ( x == y || x == z )
            return [x, y, z, w]
      `;
      // TODO make the rule more robust and detect more cases
      const stmt = db._createStatement({query, options});
      const plan = stmt.explain().plan;
      assertEqual(plan.rules.indexOf("replace-equal-attribute-accesses"), -1);
    },

    testReturnEqualPairsIndex: function () {
      const query = `
        for x in A
          for y in B
            filter x.x == y.x
            return [x.x, y.x]
      `;

      const stmt = db._createStatement({
        query,
        options: {optimizer: {rules: ["+replace-equal-attribute-accesses", "-join-index-nodes"]}}
      });
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

      const stmt = db._createStatement({query, options});
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

      const stmt = db._createStatement({query, options});
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

    testPropagateIntoSubquery: function () {
      const query = `
          for i in 1..5
            for j in 1..5
              filter i == j
              limit 10        // prevent subquery being moved above the filter
              let c = (return [i, j])
              return [c, i, j]
      `;

      const stmt = db._createStatement({query, options});
      const plan = stmt.explain().plan;
      assertNotEqual(plan.rules.indexOf("replace-equal-attribute-accesses"), -1);

      const nodes = plan.nodes.map(x => x.type);
      assertNotEqual(nodes.indexOf("SubqueryStartNode"), -1);
      
      const calc = plan.nodes[nodes.indexOf("SubqueryStartNode") + 1];
      assertEqual(calc.type, "CalculationNode");

      const returnNode = plan.nodes[nodes.indexOf("SubqueryStartNode") + 2];
      assertEqual(returnNode.type, "SubqueryEndNode");

      const inVar = returnNode.inVariable.id;

      {
        const calc = plan.nodes.filter(x => x.type === "CalculationNode" && x.outVariable.id === inVar)[0];
        const expr = calc.expression;
        assertEqual(expr.type, "array");
        assertEqual(expr.subNodes.length, 2);

        const vars = expr.subNodes.map(function (expr) {
          assertTrue(expr.type, "reference");
          return expr.name;
        });

        assertEqual(_.uniq(vars).length, 1);
      }
    },

    testDoNotPropagateOutOfSubquery: function () {
      const query = `
        for i in 1..5
          for j in 1..5
            for k in 1..5
              for m in 1..5
                filter i == j
                filter k == m
                let x = (FILTER j == k RETURN 1)
                limit 10
                return [x, i, j, k, m]`;

      const stmt = db._createStatement({query, options});
      const plan = stmt.explain().plan;
      assertNotEqual(plan.rules.indexOf("replace-equal-attribute-accesses"), -1);

      const calc = plan.nodes[plan.nodes.length - 2];
      assertEqual(calc.type, "CalculationNode");

      const expr = calc.expression;
      assertEqual(expr.type, "array");
      assertEqual(expr.subNodes.length, 5);

      const vars = expr.subNodes.map(function (expr) {
        assertTrue(expr.type, "reference");
        return expr.name;
      });

      assertEqual(_.uniq(vars).length, 3);
    }
  };
}

jsunity.run(optimizerRuleReplaceEqualAttributeAccess);

return jsunity.done();
