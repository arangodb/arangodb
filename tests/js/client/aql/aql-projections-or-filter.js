/*jshint globalstrict:false, strict:false, maxlen: 500 */
/*global assertEqual, assertNotEqual, assertTrue */

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
      // The core bug: OR across two indexes should still have projections
      let query = `FOR p IN ${cn} FILTER p.price > 20 OR p.name == "P1" RETURN p.description`;
      let plan = query_explain(query).plan;
      let nodes = plan.nodes.filter(function (node) { return node.type === 'IndexNode'; });
      assertEqual(1, nodes.length);
      assertEqual(2, nodes[0].indexes.length);
      // projections should include description (what we return)
      assertEqual(normalize(["description"]), normalize(nodes[0].projections), query);
      assertEqual("document", nodes[0].strategy);
      assertNotEqual(-1, plan.rules.indexOf(ruleName));
    },

    testOrTwoIndexesMultipleReturnAttributes: function () {
      let query = `FOR p IN ${cn} FILTER p.price > 20 OR p.name == "P1" RETURN [p.description, p.extra]`;
      let plan = query_explain(query).plan;
      let nodes = plan.nodes.filter(function (node) { return node.type === 'IndexNode'; });
      assertEqual(1, nodes.length);
      assertEqual(2, nodes[0].indexes.length);
      assertEqual(normalize(["description", "extra"]), normalize(nodes[0].projections), query);
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
      assertEqual(normalize(["price"]), normalize(nodes[0].projections), query);
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
      assertEqual(normalize(["description"]), normalize(nodes[0].projections), query);
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

jsunity.run(orFilterProjectionsPlansSuite);
jsunity.run(orFilterProjectionsResultsSuite);

return jsunity.done();
