/*jshint globalstrict:false, strict:false, maxlen: 500 */
/*global assertEqual, assertNotEqual, assertTrue, assertFalse */

const jsunity = require("jsunity");
const db = require("@arangodb").db;
const normalize = require("@arangodb/aql-helper").normalizeProjections;
const ruleName = "reduce-extraction-to-projection";
const cn = "UnitTestsProjectionsOr";

function query_explain(query, bindVars = null, options = {}) {
  return db._createStatement({query, bindVars, options}).explain();
}

function orFilterProjectionsPlansSuite() {
  let c = null;

  return {
    setUp: function () {
      db._drop(cn);
      c = db._create(cn, {numberOfShards: 4});

      let docs = [];
      for (let i = 0; i < 1000; ++i) {
        docs.push({
          _key: "test" + i,
          price: i,
          name: "P" + (i % 50),
          category: "C" + (i % 20),
          description: "desc" + i,
          extra: "extra" + i
        });
      }
      c.insert(docs);

      c.ensureIndex({type: "persistent", fields: ["price"]});
      c.ensureIndex({type: "persistent", fields: ["name"]});
    },

    tearDown: function () {
      db._drop(cn);
    },

    testOrTwoIndexesProjections: function () {
      // The core bug: OR across two indexes should still have projections.
      // Projections include all attributes accessed downstream: filter (price, name)
      // and return (description), since the filter stays as a separate CalculationNode.
      let query = `FOR p IN ${cn} FILTER p.price > 20 OR p.name == "P1" RETURN p.description`;
      let plan = query_explain(query).plan;
      let nodes = plan.nodes.filter(function (node) { return node.type === 'IndexNode'; });
      assertEqual(1, nodes.length);
      assertEqual(2, nodes[0].indexes.length);
      assertEqual(normalize(["description", "name", "price"]), normalize(nodes[0].projections), query);
      assertEqual("document", nodes[0].strategy);
      assertNotEqual(-1, plan.rules.indexOf(ruleName));
    },

    testOrTwoIndexesMultipleReturnAttributes: function () {
      let query = `FOR p IN ${cn} FILTER p.price > 20 OR p.name == "P1" RETURN [p.description, p.extra]`;
      let plan = query_explain(query).plan;
      let nodes = plan.nodes.filter(function (node) { return node.type === 'IndexNode'; });
      assertEqual(1, nodes.length);
      assertEqual(2, nodes[0].indexes.length);
      assertEqual(normalize(["description", "extra", "name", "price"]), normalize(nodes[0].projections), query);
      assertEqual("document", nodes[0].strategy);
      assertNotEqual(-1, plan.rules.indexOf(ruleName));
    },

    testOrTwoIndexesReturnFilterAttribute: function () {
      // return an attribute that is also used in the filter
      let query = `FOR p IN ${cn} FILTER p.price > 20 OR p.name == "P1" RETURN p.price`;
      let plan = query_explain(query).plan;
      let nodes = plan.nodes.filter(function (node) { return node.type === 'IndexNode'; });
      assertEqual(1, nodes.length);
      assertEqual(2, nodes[0].indexes.length);
      assertEqual(normalize(["name", "price"]), normalize(nodes[0].projections), query);
      assertEqual("document", nodes[0].strategy);
      assertNotEqual(-1, plan.rules.indexOf(ruleName));
    },

    testOrThreeIndexes: function () {
      c.ensureIndex({type: "persistent", fields: ["category"]});
      let query = `FOR p IN ${cn} FILTER p.price > 20 OR p.name == "P1" OR p.category == "C5" RETURN p.description`;
      let plan = query_explain(query).plan;
      let nodes = plan.nodes.filter(function (node) { return node.type === 'IndexNode'; });
      assertEqual(1, nodes.length);
      assertTrue(nodes[0].indexes.length >= 2);
      assertEqual(normalize(["category", "description", "name", "price"]), normalize(nodes[0].projections), query);
      assertEqual("document", nodes[0].strategy);
      assertNotEqual(-1, plan.rules.indexOf(ruleName));
    },

    testOrReturnFullDocument: function () {
      // returning the full document should result in empty projections
      let query = `FOR p IN ${cn} FILTER p.price > 20 OR p.name == "P1" RETURN p`;
      let plan = query_explain(query).plan;
      let nodes = plan.nodes.filter(function (node) { return node.type === 'IndexNode'; });
      assertEqual(1, nodes.length);
      assertEqual(2, nodes[0].indexes.length);
      assertEqual(normalize([]), normalize(nodes[0].projections), query);
      assertEqual("document", nodes[0].strategy);
    },

    testSingleIndexOrStillWorks: function () {
      // OR on the same index field should still work (single index case)
      let query = `FOR p IN ${cn} FILTER p.price == 10 OR p.price == 20 RETURN p.description`;
      let plan = query_explain(query).plan;
      let nodes = plan.nodes.filter(function (node) { return node.type === 'IndexNode'; });
      assertEqual(1, nodes.length);
      // same index used for both conditions
      assertEqual(1, nodes[0].indexes.length);
      assertEqual(normalize(["description"]), normalize(nodes[0].projections), query);
    },
  };
}

function orFilterProjectionsResultsSuite() {
  let c = null;

  return {
    setUp: function () {
      db._drop(cn);
      c = db._create(cn, {numberOfShards: 4});

      c.insert([
        {_key: "a", price: 100, name: "P1", category: "C1", description: "alpha", extra: "e1"},
        {_key: "b", price: 5, name: "P1", category: "C2", description: "beta", extra: "e2"},
        {_key: "c", price: 50, name: "P2", category: "C1", description: "gamma", extra: "e3"},
        {_key: "d", price: 10, name: "P3", category: "C3", description: "delta", extra: "e4"},
        {_key: "e", price: 200, name: "P1", category: "C2", description: "epsilon", extra: "e5"},
      ]);

      c.ensureIndex({type: "persistent", fields: ["price"]});
      c.ensureIndex({type: "persistent", fields: ["name"]});
    },

    tearDown: function () {
      db._drop(cn);
    },

    testOrTwoIndexesCorrectResults: function () {
      // price > 20: a(100), c(50), e(200)
      // name == "P1": a, b, e
      // union: a, b, c, e
      let query = `FOR p IN ${cn} FILTER p.price > 20 OR p.name == "P1" SORT p._key RETURN p.description`;
      let res = db._query(query).toArray();
      assertEqual(["alpha", "beta", "gamma", "epsilon"], res);
    },

    testOrTwoIndexesMultipleReturnResults: function () {
      let query = `FOR p IN ${cn} FILTER p.price > 20 OR p.name == "P1" SORT p._key RETURN [p.description, p.extra]`;
      let res = db._query(query).toArray();
      assertEqual([["alpha", "e1"], ["beta", "e2"], ["gamma", "e3"], ["epsilon", "e5"]], res);
    },

    testOrReturnFilterAttribute: function () {
      let query = `FOR p IN ${cn} FILTER p.price > 20 OR p.name == "P1" SORT p._key RETURN p.price`;
      let res = db._query(query).toArray();
      assertEqual([100, 5, 50, 200], res);
    },

    testOrThreeIndexesCorrectResults: function () {
      c.ensureIndex({type: "persistent", fields: ["category"]});
      // price > 20: a, c, e
      // name == "P1": a, b, e
      // category == "C3": d
      // union: a, b, c, d, e (all of them)
      let query = `FOR p IN ${cn} FILTER p.price > 20 OR p.name == "P1" OR p.category == "C3" SORT p._key RETURN p.description`;
      let res = db._query(query).toArray();
      assertEqual(["alpha", "beta", "gamma", "delta", "epsilon"], res);
    },

    testOrNoMatchReturnsEmpty: function () {
      let query = `FOR p IN ${cn} FILTER p.price > 9999 OR p.name == "NONE" RETURN p.description`;
      let res = db._query(query).toArray();
      assertEqual([], res);
    },
  };
}

function perIndexCoveringPlansSuite() {
  let c = null;

  return {
    setUp: function () {
      db._drop(cn);
      c = db._create(cn, {numberOfShards: 4});
      let docs = [];
      for (let i = 0; i < 100; ++i) {
        docs.push({
          _key: "test" + i,
          price: i,
          name: "P" + (i % 50),
          description: "desc" + i,
        });
      }
      c.insert(docs);
    },

    tearDown: function () {
      db._drop(cn);
    },

    testBothIndexesCoverPerIndex: function () {
      // Each index must cover ALL projections (filter attrs + return attrs).
      // Projections here: [description, name, price]
      c.ensureIndex({type: "persistent", fields: ["price"], storedValues: ["name", "description"]});
      c.ensureIndex({type: "persistent", fields: ["name"], storedValues: ["price", "description"]});
      let query = `FOR p IN ${cn} FILTER p.price > 20 OR p.name == "P1" RETURN p.description`;
      let plan = query_explain(query).plan;
      let nodes = plan.nodes.filter(n => n.type === 'IndexNode');
      assertEqual(1, nodes.length);
      assertEqual(2, nodes[0].indexes.length);
      assertTrue(nodes[0].hasOwnProperty("perIndexCovering"));
      assertEqual([true, true], nodes[0].perIndexCovering);
    },

    testOneIndexCoversPerIndex: function () {
      c.ensureIndex({type: "persistent", fields: ["price"], storedValues: ["name", "description"]});
      c.ensureIndex({type: "persistent", fields: ["name"]});
      let query = `FOR p IN ${cn} FILTER p.price > 20 OR p.name == "P1" RETURN p.description`;
      let plan = query_explain(query).plan;
      let nodes = plan.nodes.filter(n => n.type === 'IndexNode');
      assertEqual(1, nodes.length);
      assertEqual(2, nodes[0].indexes.length);
      assertTrue(nodes[0].hasOwnProperty("perIndexCovering"));
      let covering = nodes[0].perIndexCovering;
      assertEqual(2, covering.length);
      // one covers, one doesn't (order depends on optimizer)
      assertEqual(1, covering.filter(x => x === true).length);
      assertEqual(1, covering.filter(x => x === false).length);
    },

    testNeitherCoversPerIndex: function () {
      c.ensureIndex({type: "persistent", fields: ["price"]});
      c.ensureIndex({type: "persistent", fields: ["name"]});
      let query = `FOR p IN ${cn} FILTER p.price > 20 OR p.name == "P1" RETURN p.description`;
      let plan = query_explain(query).plan;
      let nodes = plan.nodes.filter(n => n.type === 'IndexNode');
      assertEqual(1, nodes.length);
      assertEqual(2, nodes[0].indexes.length);
      assertTrue(nodes[0].hasOwnProperty("perIndexCovering"));
      assertEqual([false, false], nodes[0].perIndexCovering);
    },

    testSingleIndexCoveringUnchanged: function () {
      // single-index covering should still use the existing strategy
      c.ensureIndex({type: "persistent", fields: ["price"], storedValues: ["description"]});
      let query = `FOR p IN ${cn} FILTER p.price > 20 RETURN p.description`;
      let plan = query_explain(query).plan;
      let nodes = plan.nodes.filter(n => n.type === 'IndexNode');
      assertEqual(1, nodes.length);
      assertEqual(1, nodes[0].indexes.length);
      assertEqual("covering", nodes[0].strategy);
      assertTrue(nodes[0].indexCoversProjections);
      // perIndexCovering should NOT be present for single index
      assertFalse(nodes[0].hasOwnProperty("perIndexCovering"));
    },

    testPerIndexCoveringWithIdProjection: function () {
      // _id can't be covered by persistent indexes, so perIndexCovering
      // should show [false, false] when _id is in the projections
      c.ensureIndex({type: "persistent", fields: ["price"], storedValues: ["name"]});
      c.ensureIndex({type: "persistent", fields: ["name"], storedValues: ["price"]});
      let query = `FOR p IN ${cn} FILTER p.price > 20 OR p.name == "P1" RETURN p._id`;
      let plan = query_explain(query).plan;
      let nodes = plan.nodes.filter(n => n.type === 'IndexNode');
      assertEqual(1, nodes.length);
      assertEqual(2, nodes[0].indexes.length);
      assertTrue(nodes[0].hasOwnProperty("perIndexCovering"));
      assertEqual([false, false], nodes[0].perIndexCovering);
    },

    testPerIndexCoveringNotShownForFullDocReturn: function () {
      // returning full document means empty projections, so no perIndexCovering
      c.ensureIndex({type: "persistent", fields: ["price"], storedValues: ["name"]});
      c.ensureIndex({type: "persistent", fields: ["name"], storedValues: ["price"]});
      let query = `FOR p IN ${cn} FILTER p.price > 20 OR p.name == "P1" RETURN p`;
      let plan = query_explain(query).plan;
      let nodes = plan.nodes.filter(n => n.type === 'IndexNode');
      assertEqual(1, nodes.length);
      assertFalse(nodes[0].hasOwnProperty("perIndexCovering"));
    },
  };
}

function perIndexCoveringResultsSuite() {
  let c = null;

  return {
    setUp: function () {
      db._drop(cn);
      c = db._create(cn, {numberOfShards: 4});
      c.insert([
        {_key: "a", price: 100, name: "P1", description: "alpha"},
        {_key: "b", price: 5, name: "P1", description: "beta"},
        {_key: "c", price: 50, name: "P2", description: "gamma"},
        {_key: "d", price: 10, name: "P3", description: "delta"},
        {_key: "e", price: 200, name: "P1", description: "epsilon"},
      ]);
    },

    tearDown: function () {
      db._drop(cn);
    },

    testBothCoverCorrectResults: function () {
      c.ensureIndex({type: "persistent", fields: ["price"], storedValues: ["name", "description"]});
      c.ensureIndex({type: "persistent", fields: ["name"], storedValues: ["price", "description"]});
      // price > 20: a(100), c(50), e(200)
      // name == "P1": a, b, e
      // union: a, b, c, e
      let query = `FOR p IN ${cn} FILTER p.price > 20 OR p.name == "P1" SORT p._key RETURN p.description`;
      let res = db._query(query).toArray();
      assertEqual(["alpha", "beta", "gamma", "epsilon"], res);
    },

    testMixedCoveringCorrectResults: function () {
      // one index covers, the other doesn't
      c.ensureIndex({type: "persistent", fields: ["price"], storedValues: ["name", "description"]});
      c.ensureIndex({type: "persistent", fields: ["name"]});
      let query = `FOR p IN ${cn} FILTER p.price > 20 OR p.name == "P1" SORT p._key RETURN p.description`;
      let res = db._query(query).toArray();
      assertEqual(["alpha", "beta", "gamma", "epsilon"], res);
    },

    testBothCoverReturnFilterAttribute: function () {
      c.ensureIndex({type: "persistent", fields: ["price"], storedValues: ["name", "description"]});
      c.ensureIndex({type: "persistent", fields: ["name"], storedValues: ["price", "description"]});
      let query = `FOR p IN ${cn} FILTER p.price > 20 OR p.name == "P1" SORT p._key RETURN p.price`;
      let res = db._query(query).toArray();
      assertEqual([100, 5, 50, 200], res);
    },

    testEmptyResultWithCovering: function () {
      c.ensureIndex({type: "persistent", fields: ["price"], storedValues: ["name", "description"]});
      c.ensureIndex({type: "persistent", fields: ["name"], storedValues: ["price", "description"]});
      let query = `FOR p IN ${cn} FILTER p.price > 9999 OR p.name == "NONE" RETURN p.description`;
      let res = db._query(query).toArray();
      assertEqual([], res);
    },

    testNoCoveringStillWorks: function () {
      // neither index covers — should still produce correct results (kDocument for both)
      c.ensureIndex({type: "persistent", fields: ["price"]});
      c.ensureIndex({type: "persistent", fields: ["name"]});
      let query = `FOR p IN ${cn} FILTER p.price > 20 OR p.name == "P1" SORT p._key RETURN p.description`;
      let res = db._query(query).toArray();
      assertEqual(["alpha", "beta", "gamma", "epsilon"], res);
    },

    testBothCoverMultipleReturnAttributes: function () {
      // exercises the projectionsForRegisters path (individual registers)
      c.ensureIndex({type: "persistent", fields: ["price"], storedValues: ["name", "description"]});
      c.ensureIndex({type: "persistent", fields: ["name"], storedValues: ["price", "description"]});
      let query = `FOR p IN ${cn} FILTER p.price > 20 OR p.name == "P1" SORT p._key RETURN [p.description, p.name]`;
      let res = db._query(query).toArray();
      assertEqual([["alpha", "P1"], ["beta", "P1"], ["gamma", "P2"], ["epsilon", "P1"]], res);
    },

    testThreeIndexesMixedCovering: function () {
      c.insert({_key: "f", price: 1, name: "P9", description: "zeta", category: "CX"});
      c.ensureIndex({type: "persistent", fields: ["price"], storedValues: ["name", "description", "category"]});
      c.ensureIndex({type: "persistent", fields: ["name"], storedValues: ["price", "description", "category"]});
      // category index does NOT cover all projections (missing price/name in storedValues)
      c.ensureIndex({type: "persistent", fields: ["category"]});
      // price > 20: a, c, e; name == "P1": a, b, e; category == "CX": f
      // union: a, b, c, e, f
      let query = `FOR p IN ${cn} FILTER p.price > 20 OR p.name == "P1" OR p.category == "CX" SORT p._key RETURN p.description`;
      let res = db._query(query).toArray();
      assertEqual(["alpha", "beta", "gamma", "epsilon", "zeta"], res);
    },

    testCoveringWithSubquery: function () {
      // exercises cursor reuse: IndexNode runs once per outer row
      c.ensureIndex({type: "persistent", fields: ["price"], storedValues: ["name", "description"]});
      c.ensureIndex({type: "persistent", fields: ["name"], storedValues: ["price", "description"]});
      let query = `FOR x IN ["P1", "P2"]
                     LET sub = (FOR p IN ${cn} FILTER p.price > 20 OR p.name == x SORT p._key RETURN p.description)
                     RETURN sub`;
      let res = db._query(query).toArray();
      // x == "P1": price>20 = a,c,e; name=="P1" = a,b,e → union: a,b,c,e
      // x == "P2": price>20 = a,c,e; name=="P2" = c → union: a,c,e
      assertEqual([["alpha", "beta", "gamma", "epsilon"], ["alpha", "gamma", "epsilon"]], res);
    },

    testCoveringWithLimit: function () {
      c.ensureIndex({type: "persistent", fields: ["price"], storedValues: ["name", "description"]});
      c.ensureIndex({type: "persistent", fields: ["name"], storedValues: ["price", "description"]});
      // price > 20: a(100), c(50), e(200); name == "P1": a, b, e → union: a, b, c, e
      let query = `FOR p IN ${cn} FILTER p.price > 20 OR p.name == "P1" SORT p._key LIMIT 2 RETURN p.description`;
      let res = db._query(query).toArray();
      assertEqual(["alpha", "beta"], res);
    },

    testCoveringWithLimitOffset: function () {
      c.ensureIndex({type: "persistent", fields: ["price"], storedValues: ["name", "description"]});
      c.ensureIndex({type: "persistent", fields: ["name"], storedValues: ["price", "description"]});
      let query = `FOR p IN ${cn} FILTER p.price > 20 OR p.name == "P1" SORT p._key LIMIT 1, 2 RETURN p.description`;
      let res = db._query(query).toArray();
      assertEqual(["beta", "gamma"], res);
    },

    testCoveringWithIdProjection: function () {
      // _id is a virtual attribute that indexes can't cover — should fall back to kDocument
      c.ensureIndex({type: "persistent", fields: ["price"], storedValues: ["name"]});
      c.ensureIndex({type: "persistent", fields: ["name"], storedValues: ["price"]});
      let query = `FOR p IN ${cn} FILTER p.price > 20 OR p.name == "P1" SORT p._key RETURN p._id`;
      let res = db._query(query).toArray();
      assertEqual([`${cn}/a`, `${cn}/b`, `${cn}/c`, `${cn}/e`], res);
    },
  };
}

jsunity.run(orFilterProjectionsPlansSuite);
jsunity.run(orFilterProjectionsResultsSuite);
jsunity.run(perIndexCoveringPlansSuite);
jsunity.run(perIndexCoveringResultsSuite);

return jsunity.done();
