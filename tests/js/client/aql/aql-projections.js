/*jshint globalstrict:false, strict:false, maxlen: 500 */
/*global assertEqual, assertTrue, assertFalse, assertNotEqual */

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
const db = require("@arangodb").db;
const normalize = require("@arangodb/aql-helper").normalizeProjections;
const ruleName = "reduce-extraction-to-projection";
const cn = "UnitTestsOptimizer";
const internal = require('internal');
let IM = global.instanceManager;

function projectionsPlansTestSuite () {
  let c = null;

  return {
    setUpAll : function () {
      db._createView(cn + "View", "arangosearch", {});
    },
    
    tearDownAll : function () {
      db._dropView(cn + "View");
      if (IM.debugCanUseFailAt()) {
        IM.debugClearFailAt();
      }
    },

    setUp : function () {
      db._drop(cn);
      c = db._createEdgeCollection(cn, { numberOfShards: 4 });

      let docs = [];
      for (let i = 0; i < 1000; ++i) {
        docs.push({ _from: "v/test" + i, _to: "v/test" + i, _key: "test" + i, value1: i, value2: "test" + i, value3: i, foo: { bar: i, baz: i, bat: i } });
      }
      c.insert(docs);
    },

    tearDown : function () {
      db._drop(cn);
    },

    testPrimary : function () {
      let queries = [
        [`FOR doc IN ${cn} RETURN doc._key`, 'primary', ['_key'], "covering" ],
        [`FOR doc IN ${cn} FILTER doc._key == 'abc' RETURN doc._key`, 'primary', ['_key'], "covering" ],
        [`FOR doc IN ${cn} FILTER doc._key == 'abc' RETURN doc._id`, 'primary', ['_id'], "covering" ],
        [`FOR doc IN ${cn} FILTER doc._key == 'abc' RETURN 1`, 'primary', [], "no result" ],
        [`FOR doc IN ${cn} RETURN doc._id`, 'primary', ['_id'], "covering" ],
        [`FOR doc IN ${cn} FILTER doc._id == '${cn}/abc' RETURN doc._id`, 'primary', ['_id'], "covering" ],
        [`FOR doc IN ${cn} FILTER doc._id == '${cn}/abc' RETURN doc._key`, 'primary', ['_key'], "covering" ],
        [`FOR doc IN ${cn} FILTER doc._id == '${cn}/abc' RETURN 1`, 'primary', [], "no result" ],
        [`FOR doc IN ${cn} RETURN [doc._key, doc._id]`, 'primary', ['_id', '_key'], "covering" ],
        [`FOR doc IN ${cn} FILTER doc._id == '${cn}/abc' RETURN [doc._key, doc._id]`, 'primary', ['_id', '_key'], "covering" ],
        [`FOR doc IN ${cn} REMOVE doc IN ${cn}`, 'primary', ['_key'], "covering" ],
        [`FOR doc IN ${cn} REMOVE doc._key IN ${cn}`, 'primary', ['_key'], "covering" ],
        [`FOR doc IN ${cn} REMOVE { _id: doc._id } IN ${cn}`, 'primary', ['_id'], "covering" ],
        [`FOR doc IN ${cn} UPDATE doc WITH {} IN ${cn}`, 'primary', ['_key'], "covering" ],
        [`FOR doc IN ${cn} UPDATE doc._key WITH {} IN ${cn}`, 'primary', ['_key'], "covering" ],
        [`FOR doc IN ${cn} UPDATE { _id: doc._id } WITH {} IN ${cn}`, 'primary', ['_id'], "covering" ],
        [`FOR doc IN ${cn} REPLACE doc WITH {} IN ${cn}`, 'primary', ['_key'], "covering" ],
        [`FOR doc IN ${cn} REPLACE doc._key WITH {} IN ${cn}`, 'primary', ['_key'], "covering" ],
        [`FOR doc IN ${cn} REPLACE { _id: doc._id } WITH {} IN ${cn}`, 'primary', ['_id'], "covering" ],
        [`FOR doc IN ${cn} FILTER doc._key == 'abc' FOR v IN ${cn}View SEARCH v.id == doc._key RETURN doc._key`, 'primary', ['_key'], "covering" ],
        [`FOR doc IN ${cn} FILTER doc._key == 'abc' FOR v IN ${cn}View SEARCH v.id == doc._key && v.other == doc._id RETURN doc._key`, 'primary', ['_key', '_id'], "covering" ],
      ];

      queries.forEach(function(query) {
        let plan = db._createStatement({query: query[0], options: { optimizer: { rules: ["-optimize-cluster-single-document-operations"] } }}).explain().plan;
        let nodes = plan.nodes.filter(function(node) { return node.type === 'IndexNode'; });
        assertEqual(1, nodes.length, query);
        assertEqual(1, nodes[0].indexes.length, query);
        assertEqual(query[1], nodes[0].indexes[0].type, query);
        assertEqual(normalize(query[2]), normalize(nodes[0].projections), query);
        if (query[2].length) {
          assertTrue(nodes[0].indexCoversProjections, query);
          assertNotEqual(-1, plan.rules.indexOf(ruleName));
        } else {
          assertFalse(nodes[0].indexCoversProjections, query);
          assertEqual(-1, plan.rules.indexOf(ruleName));
        }
        assertEqual(query[3], nodes[0].strategy, query);
      });
    },
    
    testEdge : function () {
      let queries = [
        [`FOR doc IN ${cn} FILTER doc._key == 'abc' RETURN doc._from`, 'primary', ['_from'], false, "document" ],
        [`FOR doc IN ${cn} FILTER doc._key == 'abc' RETURN doc._to`, 'primary', ['_to'], false, "document" ],
        [`FOR doc IN ${cn} FILTER doc._key == 'abc' RETURN [doc._from, doc._to]`, 'primary', ['_from', '_to'], false, "document" ],
        [`FOR doc IN ${cn} FILTER doc._from == 'v/abc' RETURN 1`, 'edge', [], false, "no result" ],
        [`FOR doc IN ${cn} FILTER doc._to == 'v/abc' RETURN 1`, 'edge', [], false, "no result" ],
        [`FOR doc IN ${cn} FILTER doc._from == '${cn}/abc' RETURN doc._from`, 'edge', ['_from'], true, "covering" ],
        [`FOR doc IN ${cn} FILTER doc._from == '${cn}/abc' RETURN doc._to`, 'edge', ['_to'], true, "covering" ],
        [`FOR doc IN ${cn} FILTER doc._from == '${cn}/abc' RETURN [doc._from, doc._to]`, 'edge', ['_from', '_to'], true, "covering" ],
        [`FOR doc IN ${cn} FILTER doc._from == '${cn}/abc' FOR v IN ${cn}View SEARCH v.id == doc._from RETURN doc._from`, 'edge', ['_from'], true, "covering" ],
        [`FOR doc IN ${cn} FILTER doc._to == '${cn}/abc' FOR v IN ${cn}View SEARCH v.id == doc._from && v.other == doc._to RETURN [doc._from, doc._to]`, 'edge', ['_from', '_to'], true, "covering" ],
        [`FOR doc IN ${cn} FILTER doc._to == '${cn}/abc' FOR v IN ${cn}View SEARCH v.id == doc._from && v.other == doc._to RETURN [doc._from, doc._to, doc.meow]`, 'edge', ['_from', '_to', 'meow'], false, "document" ],
      ];

      queries.forEach(function(query) {
        let plan = db._createStatement({query: query[0], options: { optimizer: { rules: ["-optimize-cluster-single-document-operations"] } }}).explain().plan;
        let nodes = plan.nodes.filter(function(node) { return node.type === 'IndexNode'; });
        assertEqual(1, nodes.length, query);
        assertEqual(1, nodes[0].indexes.length, query);
        assertEqual(query[1], nodes[0].indexes[0].type, query);
        assertEqual(normalize(query[2]), normalize(nodes[0].projections), query);
        assertEqual(query[3], nodes[0].indexCoversProjections, query);
        if (query[2].length) {
          assertNotEqual(-1, plan.rules.indexOf(ruleName));
        } else {
          assertEqual(-1, plan.rules.indexOf(ruleName));
        }
        assertEqual(query[4], nodes[0].strategy, query);
      });

      queries = [
        [`FOR doc IN ${cn} RETURN doc._from`, ['_from'] ],
        [`FOR doc IN ${cn} RETURN doc._to`, ['_to'] ],
        [`FOR doc IN ${cn} RETURN [doc._from, doc._to]`, ['_from', '_to'] ],
      ];
      
      queries.forEach(function(query) {
        let plan = db._createStatement({query: query[0]}).explain().plan;
        let nodes = plan.nodes.filter(function(node) { return node.type === 'EnumerateCollectionNode'; });
        assertEqual(1, nodes.length, query);
        assertEqual(normalize(query[1]), normalize(nodes[0].projections), query);
        assertNotEqual(-1, plan.rules.indexOf(ruleName));
      });
    },

    testPersistentSingleAttribute : function () {
      c.ensureIndex({ type: "persistent", fields: ["value1"] });
      let queries = [
        [`FOR doc IN ${cn} RETURN doc.value1`, 'persistent', ['value1'], true, "covering"],
        [`FOR doc IN ${cn} FILTER doc.value1 == 93 RETURN doc.value1`, 'persistent', ['value1'], true, "covering"],
        [`FOR doc IN ${cn} FILTER doc.value1 == 93 RETURN [doc.value1, doc.value2]`, 'persistent', ['value1', 'value2'], false, "document"],
        [`FOR doc IN ${cn} FILTER doc.value1 == 93 RETURN doc.value2`, 'persistent', ['value2'], false, "document"],
        [`FOR doc IN ${cn} FILTER doc.value1 == 93 RETURN [doc.value1, doc.value2]`, 'persistent', ['value1', 'value2'], false, "document"],
        [`FOR doc IN ${cn} FILTER doc.value1 == 93 RETURN 1`, 'persistent', [], false, "no result"],
        [`FOR doc IN ${cn} FILTER doc._key == 'abc' RETURN doc.value1`, 'primary', ['value1'], false, "document"],
        [`FOR doc IN ${cn} FILTER doc._key == 'abc' RETURN doc.value2`, 'primary', ['value2'], false, "document"],
        [`FOR doc IN ${cn} FILTER doc.value1 == 'abc' RETURN doc.value2[*].abc`, 'persistent', ['value2'], false, "document"],
        [`FOR doc IN ${cn} FILTER doc.value1 == 'abc' RETURN doc.value2[**].abc`, 'persistent', ['value2'], false, "document"],
        [`FOR doc IN ${cn} FILTER doc.value1 == 'abc' RETURN doc[*]`, 'persistent', [], false, "document"],
        [`FOR doc IN ${cn} FILTER doc.value1 == 'abc' RETURN doc[**]`, 'persistent', [], false, "document"],
        [`FOR doc IN ${cn} FILTER doc.value1 == 'abc' RETURN doc[*].abc`, 'persistent', [], false, "document"],
        [`FOR doc IN ${cn} FILTER doc.value1 == 'abc' RETURN doc[**].abc`, 'persistent', [], false, "document"],
        [`FOR doc IN ${cn} FILTER doc.value1 == 'abc' RETURN doc[*].value2`, 'persistent', [], false, "document"],
        [`FOR doc IN ${cn} FILTER doc.value1 == 'abc' RETURN doc[**].value2`, 'persistent', [], false, "document"],
        [`FOR doc IN ${cn} FILTER doc.value1 == 'abc' RETURN doc[*].foo.bar`, 'persistent', [], false, "document"],
        [`FOR doc IN ${cn} FILTER doc.value1 > 0 RETURN doc`, 'persistent', [], false, "late materialized"],
        [`FOR doc IN ${cn} FILTER doc.value1 > 0 RETURN doc.value1`, 'persistent', ["value1"], true, "covering"],
        [`FOR doc IN ${cn} FILTER doc.value1 > 0 RETURN doc[*]`, 'persistent', [], false, "late materialized"],
        [`FOR doc IN ${cn} FILTER doc.value1 == 'abc' RETURN doc.value2[* FILTER CURRENT > 1].abc`, 'persistent', ['value2'], false, "document"],
        [`FOR doc IN ${cn} FILTER doc.value1 == 'abc' RETURN doc.value2[* RETURN CURRENT].abc`, 'persistent', ['value2'], false, "document"],
        [`FOR doc IN ${cn} FILTER doc.value1 == 'abc' RETURN doc.value2[* FILTER CURRENT > 1 RETURN CURRENT].abc`, 'persistent', ['value2'], false, "document"],
        [`FOR doc IN ${cn} FILTER doc.value1 == 'abc' RETURN doc.value2[* FILTER doc.value2 == 1].abc`, 'persistent', ['value2'], false, "document"],
        [`FOR doc IN ${cn} FILTER doc.value1 == 'abc' RETURN doc.value2[* RETURN doc.value2 == 1].abc`, 'persistent', ['value2'], false, "document"],
        [`FOR doc IN ${cn} FILTER doc.value1 == 'abc' RETURN doc.value2[* FILTER doc.value2 == 1 RETURN doc.value2].abc`, 'persistent', ['value2'], false, "document"],
        [`FOR doc IN ${cn} FILTER doc.value1 == 'abc' RETURN doc.value2[* FILTER doc.value1 == 1].abc`, 'persistent', ['value1', 'value2'], false, "document"],
        [`FOR doc IN ${cn} FILTER doc.value1 == 'abc' RETURN doc.value2[* RETURN doc.value1 == 1].abc`, 'persistent', ['value1', 'value2'], false, "document"],
        [`FOR doc IN ${cn} FILTER doc.value1 == 'abc' RETURN doc.value2[* FILTER doc.value1 == 1 RETURN doc.value1].abc`, 'persistent', ['value1', 'value2'], false, "document"],
        [`FOR doc IN ${cn} FILTER doc.value1 == 'abc' RETURN doc.value1[* FILTER doc.value1 == 1 RETURN doc.value1].abc`, 'persistent', ['value1'], true, "covering"],
        [`FOR doc IN ${cn} FILTER doc.value1 == 'abc' RETURN doc.value1[* FILTER doc.value1 == 1 RETURN doc.value2].abc`, 'persistent', ['value1', 'value2'], false, "document"],
        [`FOR doc IN ${cn} FILTER doc.value1 == 'abc' FOR v IN ${cn}View SEARCH v.value == doc.value1 RETURN doc.value2`, 'persistent', ['value1', 'value2'], false, "document"],
      ];

      queries.forEach(function(query) {
        let plan = db._createStatement({query: query[0], options: { optimizer: { rules: ["-optimize-cluster-single-document-operations"] } }}).explain().plan;
        let nodes = plan.nodes.filter(function(node) { return node.type === 'IndexNode'; });
        assertEqual(1, nodes.length, query);
        assertEqual(1, nodes[0].indexes.length, query);
        assertEqual(query[1], nodes[0].indexes[0].type, query);
        assertEqual(normalize(query[2]), normalize(nodes[0].projections), query);
        assertEqual(query[3], nodes[0].indexCoversProjections, query);
        assertEqual(query[4], nodes[0].strategy, query);
        
        if (query[2].length) {
          assertNotEqual(-1, plan.rules.indexOf(ruleName));
        } else {
          assertEqual(-1, plan.rules.indexOf(ruleName));
        }
      });
      
      queries = [
        [`FOR doc IN ${cn} RETURN [doc.value1, doc.value2]`, ['value1', 'value2'] ],
        [`FOR doc IN ${cn} RETURN doc.value2`, ['value2'] ],
        [`FOR doc IN ${cn} RETURN [doc.value2, doc.value3]`, ['value2', 'value3'] ],
      ];

      queries.forEach(function(query) {
        let plan = db._createStatement({query: query[0]}).explain().plan;
        let nodes = plan.nodes.filter(function(node) { return node.type === 'EnumerateCollectionNode'; });
        assertEqual(1, nodes.length, query);
        assertEqual(normalize(query[1]), normalize(nodes[0].projections), query);
        assertNotEqual(-1, plan.rules.indexOf(ruleName));
      });
    },
    
    testPersistentMultiAttribute : function () {
      c.ensureIndex({ type: "persistent", fields: ["value1", "value2"] });
      let queries = [
        [`FOR doc IN ${cn} RETURN doc.value1`, 'persistent', ['value1'], true, [], "covering"],
        [`FOR doc IN ${cn} RETURN doc.value2`, 'persistent', ['value2'], true, [], "covering"],
        [`FOR doc IN ${cn} FILTER doc.value1 == 93 RETURN doc.value1`, 'persistent', ['value1'], true, [], "covering"],
        [`FOR doc IN ${cn} FILTER doc.value2 == 93 RETURN doc.value1`, 'persistent', ['value1'], true, ['value2'], "covering"],
        [`FOR doc IN ${cn} FILTER doc.value1 == 93 RETURN doc.value2`, 'persistent', ['value2'], true, [], "covering"],
        [`FOR doc IN ${cn} FILTER doc.value2 == 93 RETURN doc.value2`, 'persistent', ['value2'], true, ['value2'], "covering"],
        [`FOR doc IN ${cn} FILTER doc._key == 'abc' RETURN doc.value1`, 'primary', ['value1'], false, [], "document"],
        [`FOR doc IN ${cn} FILTER doc._key == 'abc' RETURN doc.value2`, 'primary', ['value2'], false, [], "document"],
        [`FOR doc IN ${cn} FILTER doc.value1 == 'abc' RETURN doc.value2[*].abc`, 'persistent', ['value2'], true, [], "covering"],
        [`FOR doc IN ${cn} FILTER doc.value1 == 'abc' RETURN doc.value2[**].abc`, 'persistent', ['value2'], true, [], "covering"],
        [`FOR doc IN ${cn} FILTER doc.value1 == 'abc' RETURN doc[*]`, 'persistent', [], false, [], "document"],
        [`FOR doc IN ${cn} FILTER doc.value1 == 'abc' RETURN doc[**]`, 'persistent', [], false, [], "document"],
        [`FOR doc IN ${cn} FILTER doc.value1 == 'abc' RETURN doc[*].abc`, 'persistent', [], false, [], "document"],
        [`FOR doc IN ${cn} FILTER doc.value1 == 'abc' RETURN doc[**].abc`, 'persistent', [], false, [], "document"],
        [`FOR doc IN ${cn} FILTER doc.value1 == 'abc' RETURN doc[*].value2`, 'persistent', [], false, [], "document"],
        [`FOR doc IN ${cn} FILTER doc.value1 == 'abc' RETURN doc[**].value2`, 'persistent', [], false, [], "document"],
        [`FOR doc IN ${cn} FILTER doc.value1 == 'abc' RETURN doc[*].foo.bar`, 'persistent', [], false, [], "document"],
        [`FOR doc IN ${cn} FILTER doc.value1 == 'abc' RETURN doc.value2[* FILTER CURRENT > 1].abc`, 'persistent', ['value2'], true, [], "covering"],
        [`FOR doc IN ${cn} FILTER doc.value1 == 'abc' RETURN doc.value2[* RETURN CURRENT].abc`, 'persistent', ['value2'], true, [], "covering"],
        [`FOR doc IN ${cn} FILTER doc.value1 == 'abc' RETURN doc.value2[* FILTER CURRENT > 1 RETURN CURRENT].abc`, 'persistent', ['value2'], true, [], "covering"],
        [`FOR doc IN ${cn} FILTER doc.value1 == 'abc' RETURN doc.value2[* FILTER doc.value2 == 1].abc`, 'persistent', ['value2'], true, [], "covering"],
        [`FOR doc IN ${cn} FILTER doc.value1 == 'abc' RETURN doc.value2[* RETURN doc.value2 == 1].abc`, 'persistent', ['value2'], true, [], "covering"],
        [`FOR doc IN ${cn} FILTER doc.value1 == 'abc' RETURN doc.value2[* FILTER doc.value2 == 1 RETURN doc.value2].abc`, 'persistent', ['value2'], true, [], "covering"],
        [`FOR doc IN ${cn} FILTER doc.value1 == 'abc' RETURN doc.value2[* FILTER doc.value1 == 1].abc`, 'persistent', ['value1', 'value2'], true, [], "covering"],
        [`FOR doc IN ${cn} FILTER doc.value1 == 'abc' RETURN doc.value2[* RETURN doc.value1 == 1].abc`, 'persistent', ['value1', 'value2'], true, [], "covering"],
        [`FOR doc IN ${cn} FILTER doc.value1 == 'abc' RETURN doc.value2[* FILTER doc.value1 == 1 RETURN doc.value1].abc`, 'persistent', ['value1', 'value2'], true, [], "covering"],
        [`FOR doc IN ${cn} FILTER doc.value1 == 'abc' RETURN doc.value2[* FILTER doc.value1 == 1 && doc.value2 == 2].abc`, 'persistent', ['value1', 'value2'], true, [], "covering"],
        [`FOR doc IN ${cn} FILTER doc.value1 == 'abc' RETURN doc.value2[* FILTER doc.value3 == 1 && doc.value4 == 2].abc`, 'persistent', ['value2', 'value3', 'value4'], false, [], "document"],
        [`FOR doc IN ${cn} FILTER doc.value1 == 'abc' FOR v IN ${cn}View SEARCH v.value1 == doc.value1 && v.value2 == doc.value2 RETURN doc.value2`, 'persistent', ['value1', 'value2'], true, [], "covering"],
        [`FOR doc IN ${cn} FILTER doc.value1 == 'abc' FILTER doc.meow == 2 FOR v IN ${cn}View SEARCH v.value1 == doc.value1 && v.value2 == doc.value2 RETURN doc.value2`, 'persistent', ['value1', 'value2'], false, ['meow'], "document"],
      ];

      queries.forEach(function(query) {
        let plan = db._createStatement({query: query[0], options: { optimizer: { rules: ["-optimize-cluster-single-document-operations"] } }}).explain().plan;
        let nodes = plan.nodes.filter(function(node) { return node.type === 'IndexNode'; });
        assertEqual(1, nodes.length, query);
        assertEqual(1, nodes[0].indexes.length, query);
        assertEqual(query[1], nodes[0].indexes[0].type, query);
        assertEqual(normalize(query[2]), normalize(nodes[0].projections), query);
        assertEqual(query[3], nodes[0].indexCoversProjections, query);
        assertEqual(normalize(query[4]), normalize(nodes[0].filterProjections), query);
        if (query[2].length) {
          assertNotEqual(-1, plan.rules.indexOf(ruleName));
        } else {
          assertEqual(-1, plan.rules.indexOf(ruleName));
        }
        assertEqual(query[5], nodes[0].strategy, query);
      });
      
      queries = [
        [`FOR doc IN ${cn} RETURN [doc.value1, doc.value3]`, ['value1', 'value3'] ],
        [`FOR doc IN ${cn} RETURN [doc.value2, doc.value3]`, ['value2', 'value3'] ],
        [`FOR doc IN ${cn} RETURN doc.value3`, ['value3'] ],
        [`FOR doc IN ${cn} RETURN [doc.value3, doc.value4]`, ['value3', 'value4'] ],
      ];

      queries.forEach(function(query) {
        let plan = db._createStatement({query: query[0]}).explain().plan;
        let nodes = plan.nodes.filter(function(node) { return node.type === 'EnumerateCollectionNode'; });
        assertEqual(1, nodes.length, query);
        assertEqual(normalize(query[1]), normalize(nodes[0].projections), query);
        assertNotEqual(-1, plan.rules.indexOf(ruleName));
      });
    },
    
    testPersistentSubAttribute : function () {
      c.ensureIndex({ type: "persistent", fields: ["foo.bar"] });
      let queries = [
        [`FOR doc IN ${cn} RETURN doc.foo.bar`, 'persistent', [['foo', 'bar']], true, [], "covering" ],
        [`FOR doc IN ${cn} RETURN doc.foo.bar.baz`, 'persistent', [['foo', 'bar', 'baz']], true, [], "covering" ],
        [`FOR doc IN ${cn} RETURN [doc.foo.bar.baz, doc.foo.bar.bat]`, 'persistent', [['foo', 'bar', 'bat'], ['foo', 'bar', 'baz']], true, [], "covering" ],
        [`FOR doc IN ${cn} RETURN [doc.foo.bar, doc.foo.bar.baz]`, 'persistent', [['foo', 'bar']], true, [], "covering" ],
        [`FOR doc IN ${cn} FILTER doc.foo.bar == 1 RETURN doc.foo.bar`, 'persistent', [['foo', 'bar']], true, [], "covering" ],
        [`FOR doc IN ${cn} FILTER doc.foo.bar == 1 RETURN 1`, 'persistent', [], false, [], "no result" ],
        [`FOR doc IN ${cn} FILTER doc.foo.bar[*].sub == 1 RETURN 1`, 'persistent', [], false, [['foo', 'bar']], "covering, filter only, scan only" ],
        [`FOR doc IN ${cn} FILTER doc.foo.bar[* FILTER RAND()].sub == 1 RETURN 1`, 'persistent', [['foo', 'bar']], true, [], "covering" ],
        [`FOR doc IN ${cn} FILTER doc.foo.bar[* RETURN RAND()].sub == 1 RETURN 1`, 'persistent', [['foo', 'bar']], true, [], "covering" ],
        [`FOR doc IN ${cn} FILTER doc.foo.bar[* FILTER RAND() RETURN RAND()].sub == 1 RETURN 1`, 'persistent', [['foo', 'bar']], true, [], "covering" ],
        [`FOR doc IN ${cn} FILTER doc.foo.bar[* FILTER doc.foo.bar == 1].sub == 1 RETURN 1`, 'persistent', [], false, [['foo', 'bar']], "covering, filter only, scan only" ],
        [`FOR doc IN ${cn} FILTER doc.foo.bar[* RETURN doc.foo.bar == 1].sub == 1 RETURN 1`, 'persistent', [], false, [['foo', 'bar']], "covering, filter only, scan only" ],
        [`FOR doc IN ${cn} FILTER doc.foo.bar[* FILTER doc.foo.bar == 1 RETURN doc.foo.bar == 1].sub == 1 RETURN 1`, 'persistent', [], false, [['foo', 'bar']], "covering, filter only, scan only" ],
        [`FOR doc IN ${cn} FILTER doc.foo.bar == 3 FOR v IN ${cn}View SEARCH v.id == doc.foo.bar && v.other == doc.x RETURN doc.test`, 'persistent', ['x', 'test', ['foo', 'bar']], false, [], "document" ],
      ];

      queries.forEach(function(query) {
        let plan = db._createStatement({query: query[0], options: { optimizer: { rules: ["-optimize-cluster-single-document-operations"] } }}).explain().plan;
        let nodes = plan.nodes.filter(function(node) { return node.type === 'IndexNode'; });
        assertEqual(1, nodes.length, query);
        assertEqual(1, nodes[0].indexes.length, query);
        assertEqual(query[1], nodes[0].indexes[0].type, query);
        assertEqual(normalize(query[2]), normalize(nodes[0].projections), query);
        assertEqual(query[3], nodes[0].indexCoversProjections, query);
        assertEqual(normalize(query[4]), normalize(nodes[0].filterProjections), query);
        assertEqual(query[5], nodes[0].strategy, query);
      });
      
      queries = [
        [`FOR doc IN ${cn} RETURN doc.boo`, ['boo'], [], "document" ],
        [`FOR doc IN ${cn} RETURN doc.foo`, ['foo'], [], "document" ],
        [`FOR doc IN ${cn} FILTER doc.foo == 1 RETURN doc`, [], ['foo'], "document" ],
        [`FOR doc IN ${cn} FILTER doc.foo == 1 RETURN doc.foo`, ['foo'], ['foo'], "document" ],
        [`FOR doc IN ${cn} RETURN doc.foo.baz`, [['foo', 'baz']], [], "document" ],
        [`FOR doc IN ${cn} RETURN doc.boo.far`, [['boo', 'far']], [], "covering" ],
        [`FOR doc IN ${cn} RETURN [doc.foo.bar, doc.unrelated]`, [['foo', 'bar'], 'unrelated'], [], "document" ],
        [`FOR doc IN ${cn} FILTER doc.foo.bar[* FILTER doc.foo == 1].sub == 1 RETURN 1`, [], ['foo'], "document" ],
        [`FOR doc IN ${cn} FILTER doc.foo.bar[* FILTER doc.foo.baz == 1 RETURN doc.foo.bar == 1].sub == 1 RETURN 1`, [], [['foo', 'bar'], ['foo', 'baz']], "document" ],
        [`FOR doc IN ${cn} FILTER doc.foo.bar[* FILTER doc.foo.bar == 1 RETURN doc.foo.baz == 1].sub == 1 RETURN 1`, [], [['foo', 'bar'], ['foo', 'baz']], "document" ],
        [`FOR doc IN ${cn} FILTER doc.foo.bar[* FILTER doc.value1 == 1 RETURN doc.value2 == 1].sub == 1 RETURN 1`, [], [['foo', 'bar'], 'value1', 'value2'], "document" ],
      ];

      queries.forEach(function(query) {
        let plan = db._createStatement({query: query[0]}).explain().plan;
        let nodes = plan.nodes.filter(function(node) { return node.type === 'EnumerateCollectionNode'; });
        assertEqual(1, nodes.length, query);
        assertEqual(normalize(query[1]), normalize(nodes[0].projections), query);
        assertEqual(normalize(query[2]), normalize(nodes[0].filterProjections), query);
      });
    },
    
    testPersistentSubAttributesMulti : function () {
      c.ensureIndex({ type: "persistent", fields: ["foo.bar", "foo.baz"] });
      let queries = [
        [`FOR doc IN ${cn} RETURN doc.foo.bar`, 'persistent', [['foo', 'bar']], true, [], "covering" ],
        [`FOR doc IN ${cn} RETURN doc.foo.baz`, 'persistent', [['foo', 'baz']], true, [], "covering" ],
        [`FOR doc IN ${cn} RETURN doc.foo.bar.baz`, 'persistent', [['foo', 'bar', 'baz']], true, [], "covering" ],
        [`FOR doc IN ${cn} RETURN doc.foo.baz.bar`, 'persistent', [['foo', 'baz', 'bar']], true, [], "covering" ],
        [`FOR doc IN ${cn} RETURN [doc.foo.bar.baz, doc.foo.bar.bat]`, 'persistent', [['foo', 'bar', 'bat'], ['foo', 'bar', 'baz']], true, [], "covering" ],
        [`FOR doc IN ${cn} RETURN [doc.foo.baz.bar, doc.foo.baz.bat]`, 'persistent', [['foo', 'baz', 'bar'], ['foo', 'baz', 'bat']], true, [], "covering" ],
        [`FOR doc IN ${cn} FILTER doc.foo.bar == 1 RETURN doc.foo.bar`, 'persistent', [['foo', 'bar']], true, [], "covering" ],
        [`FOR doc IN ${cn} FILTER doc.foo.bar == 1 RETURN doc.foo.baz`, 'persistent', [['foo', 'baz']], true, [], "covering" ],
        [`FOR doc IN ${cn} FILTER doc.foo.baz == 1 RETURN doc.foo.baz`, 'persistent', [['foo', 'baz']], true, [['foo', 'baz']], "covering" ],
        [`FOR doc IN ${cn} FILTER doc.foo.bar == 1 RETURN 1`, 'persistent', [], false, [], "no result" ],
        [`FOR doc IN ${cn} FILTER doc.foo.baz == 1 RETURN 1`, 'persistent', [], false, [['foo', 'baz']], "covering, filter only, scan only" ],
        [`FOR doc IN ${cn} RETURN [doc.foo.bar, doc.foo.baz]`, 'persistent', [['foo', 'bar'], ['foo', 'baz']], true, [], "covering" ],
        [`FOR doc IN ${cn} RETURN [doc.foo.bar, doc.foo.baz.bar]`, 'persistent', [['foo', 'bar'], ['foo', 'baz', 'bar']], true, [], "covering" ],
        [`FOR doc IN ${cn} FILTER doc.foo.baz == 1 RETURN doc.foo.bar`, 'persistent', [['foo', 'bar']], true, [['foo', 'baz']], "covering" ],
      ];

      queries.forEach(function(query) {
        let plan = db._createStatement({query: query[0], options: { optimizer: { rules: ["-optimize-cluster-single-document-operations"] } }}).explain().plan;
        let nodes = plan.nodes.filter(function(node) { return node.type === 'IndexNode'; });
        assertEqual(1, nodes.length, query);
        assertEqual(1, nodes[0].indexes.length, query);
        assertEqual(query[1], nodes[0].indexes[0].type, query);
        assertEqual(normalize(query[2]), normalize(nodes[0].projections), query);
        assertEqual(query[3], nodes[0].indexCoversProjections, query);
        assertEqual(normalize(query[4]), normalize(nodes[0].filterProjections), query);
        assertEqual(query[5], nodes[0].strategy, query);
      });
      
      queries = [
        [`FOR doc IN ${cn} RETURN doc.boo`, ['boo'] ],
        [`FOR doc IN ${cn} RETURN doc.foo`, ['foo'] ],
        [`FOR doc IN ${cn} RETURN doc.foo.bat`, [['foo', 'bat']] ],
        [`FOR doc IN ${cn} RETURN doc.foo.moo`, [['foo', 'moo']] ],
        [`FOR doc IN ${cn} RETURN doc.bar`, ['bar'] ],
        [`FOR doc IN ${cn} RETURN doc.baz`, ['baz'] ],
        [`FOR doc IN ${cn} RETURN [doc.foo.bar, doc.unrelated]`, [['foo', 'bar'], 'unrelated'] ],
        [`FOR doc IN ${cn} RETURN [doc.foo.baz, doc.unrelated]`, [['foo', 'baz'], 'unrelated'] ],
        [`FOR doc IN ${cn} RETURN [doc.foo.bar, doc.foo.baz, doc.unrelated]`, [['foo', 'bar'], ['foo', 'baz'], 'unrelated'] ],
      ];

      queries.forEach(function(query) {
        let plan = db._createStatement({query: query[0]}).explain().plan;
        let nodes = plan.nodes.filter(function(node) { return node.type === 'EnumerateCollectionNode'; });
        assertEqual(1, nodes.length, query);
        assertEqual(normalize(query[1]), normalize(nodes[0].projections), query);
        assertEqual(normalize(query[2]), normalize(nodes[0].filterProjections), query);
        assertNotEqual(-1, plan.rules.indexOf(ruleName));
      });
    },
    
    testPersistentSubAttributesMultiSamePrefix : function () {
      c.ensureIndex({ type: "persistent", fields: ["foo.bar.a", "foo.bar.b"] });
      let queries = [
        [`FOR doc IN ${cn} RETURN doc.foo.bar.a`, 'persistent', [['foo', 'bar', 'a']], true, "covering" ],
        [`FOR doc IN ${cn} RETURN doc.foo.bar.b`, 'persistent', [['foo', 'bar', 'b']], true, "covering" ],
        [`FOR doc IN ${cn} RETURN [doc.foo.bar.a, doc.foo.bar.b]`, 'persistent', [['foo', 'bar', 'a'], ['foo', 'bar', 'b']], true, "covering" ],
      ];

      queries.forEach(function(query) {
        let plan = db._createStatement({query: query[0], options: { optimizer: { rules: ["-optimize-cluster-single-document-operations"] } }}).explain().plan;
        let nodes = plan.nodes.filter(function(node) { return node.type === 'IndexNode'; });
        assertEqual(1, nodes.length, query);
        assertEqual(1, nodes[0].indexes.length, query);
        assertEqual(query[1], nodes[0].indexes[0].type, query);
        assertEqual(normalize(query[2]), normalize(nodes[0].projections), query);
        assertEqual(query[3], nodes[0].indexCoversProjections, query);
        if (query[2].length) {
          assertNotEqual(-1, plan.rules.indexOf(ruleName));
        } else {
          assertEqual(-1, plan.rules.indexOf(ruleName));
        }
        assertEqual(query[4], nodes[0].strategy, query);
      });
      
      queries = [
        [`FOR doc IN ${cn} RETURN doc.foo`, ['foo'] ],
        [`FOR doc IN ${cn} RETURN doc.foo.bar`, [['foo', 'bar']] ],
        [`FOR doc IN ${cn} RETURN doc.foo.bar.c`, [['foo', 'bar', 'c']] ],
      ];

      queries.forEach(function(query) {
        let plan = db._createStatement({query: query[0]}).explain().plan;
        let nodes = plan.nodes.filter(function(node) { return node.type === 'EnumerateCollectionNode'; });
        assertEqual(1, nodes.length, query);
        assertEqual(normalize(query[1]), normalize(nodes[0].projections), query);
        assertNotEqual(-1, plan.rules.indexOf(ruleName));
      });
    },
    
    testPersistentOnKeyAndOtherAttribute : function () {
      c.ensureIndex({ type: "persistent", fields: ["_key", "value1"] });
      let queries = [
        [`FOR doc IN ${cn} RETURN doc.value1`, 'persistent', ['value1'], true, [], "covering" ],
        [`FOR doc IN ${cn} RETURN doc._key`, 'primary', ['_key'], true, [], "covering" ],
        [`FOR doc IN ${cn} FILTER doc._key == 'abc' RETURN doc.value1`, 'primary', ['value1'], false, [], "document" ],
        [`FOR doc IN ${cn} FILTER doc._key == 'abc' RETURN doc.value2`, 'primary', ['value2'], false, [], "document" ],
        [`FOR doc IN ${cn} FILTER doc.value1 == 93 RETURN doc.value1`, 'persistent', ['value1'], true, ['value1'], "covering" ],
        [`FOR doc IN ${cn} FILTER doc.value1 == 93 RETURN doc._key`, 'persistent', ['_key'], true, ['value1'], "covering" ],
        [`FOR doc IN ${cn} FILTER doc._key == 'abc' FILTER doc.value1 == 93 RETURN doc._key`, 'persistent', ['_key'], true, [], "covering" ],
        [`FOR doc IN ${cn} FILTER doc._key == 'abc' FILTER doc.value1 == 93 FILTER doc.value2 > 0 RETURN doc._key`, 'persistent', ['_key'], false, ['value2'], "document" ],
      ];

      queries.forEach(function(query) {
        let plan = db._createStatement({query: query[0], options: { optimizer: { rules: ["-optimize-cluster-single-document-operations"] } }}).explain().plan;
        let nodes = plan.nodes.filter(function(node) { return node.type === 'IndexNode'; });
        assertEqual(1, nodes.length, query);
        assertEqual(1, nodes[0].indexes.length, query);
        assertEqual(query[1], nodes[0].indexes[0].type, query);
        assertEqual(normalize(query[2]), normalize(nodes[0].projections), query);
        assertEqual(query[3], nodes[0].indexCoversProjections, query);
        assertEqual(normalize(query[4]), normalize(nodes[0].filterProjections), query);
        assertEqual(query[5], nodes[0].strategy, query);
      });
    },
    
    testPersistentOnOtherAttributeAndKey : function () {
      c.ensureIndex({ type: "persistent", fields: ["value1", "_key"] });
      let queries = [
        [`FOR doc IN ${cn} RETURN doc.value1`, 'persistent', ['value1'], true, "covering" ],
        [`FOR doc IN ${cn} RETURN doc._key`, 'primary', ['_key'], true, "covering" ],
        [`FOR doc IN ${cn} FILTER doc._key == 'abc' RETURN doc.value1`, 'primary', ['value1'], false, "document" ],
        [`FOR doc IN ${cn} FILTER doc._key == 'abc' RETURN doc.value2`, 'primary', ['value2'], false, "document" ],
        [`FOR doc IN ${cn} FILTER doc.value1 == 93 RETURN doc.value1`, 'persistent', ['value1'], true, "covering" ],
        [`FOR doc IN ${cn} FILTER doc.value1 == 93 RETURN doc._key`, 'persistent', ['_key'], true, "covering" ],
      ];

      queries.forEach(function(query) {
        let plan = db._createStatement({query: query[0], options: { optimizer: { rules: ["-optimize-cluster-single-document-operations"] } }}).explain().plan;
        let nodes = plan.nodes.filter(function(node) { return node.type === 'IndexNode'; });
        assertEqual(1, nodes.length, query);
        assertEqual(1, nodes[0].indexes.length, query);
        assertEqual(query[1], nodes[0].indexes[0].type, query);
        assertEqual(normalize(query[2]), normalize(nodes[0].projections), query);
        assertEqual(query[3], nodes[0].indexCoversProjections, query);
        
        if (query[2].length) {
          assertNotEqual(-1, plan.rules.indexOf(ruleName));
        } else {
          assertEqual(-1, plan.rules.indexOf(ruleName));
        }
        assertEqual(query[4], nodes[0].strategy, query);
      });
    },
    
    testMaterialize : function () {
      if (!IM.debugCanUseFailAt()) {
        // cant execute this test as failure points are not available here
        return;
      }

      IM.debugSetFailAt('batch-materialize-no-estimation');
      c.ensureIndex({ type: "persistent", fields: ["value1", "value2", "value3"] });
      let queries = [
        [`FOR doc IN ${cn} FILTER doc.value1 == 93 SORT doc.value3 LIMIT 5 RETURN doc`, 'persistent', ["value3"], true, [ { _key: "test93", value1: 93, value2: "test93", value3: 93 } ] ],
        [`FOR doc IN ${cn} FILTER doc.value1 >= 93 && doc.value1 <= 95 SORT doc.value3 LIMIT 5 RETURN doc`, 'persistent', ["value3"], true, [ { _key: "test93", value1: 93, value2: "test93", value3: 93 }, { _key: "test94", value1: 94, value2: "test94", value3: 94 }, { _key: "test95", value1: 95, value2: "test95", value3: 95 } ] ],
        [`FOR doc IN ${cn} FILTER doc.value1 >= 93 && doc.value1 <= 95 SORT doc.value3 DESC LIMIT 5 RETURN doc`, 'persistent', ["value3"], true, [ { _key: "test95", value1: 95, value2: "test95", value3: 95 }, { _key: "test94", value1: 94, value2: "test94", value3: 94 }, { _key: "test93", value1: 93, value2: "test93", value3: 93 } ] ],
        [`FOR doc IN ${cn} FILTER doc.value1 >= 93 && doc.value1 <= 95 && doc.value2 != 23 SORT doc.value3 LIMIT 5 RETURN doc`, 'persistent', ["value3"], true, [ { _key: "test93", value1: 93, value2: "test93", value3: 93 }, { _key: "test94", value1: 94, value2: "test94", value3: 94 }, { _key: "test95", value1: 95, value2: "test95", value3: 95 } ] ],
        [`FOR doc IN ${cn} FILTER doc.value1 >= 93 && doc.value1 <= 95 && doc.value2 != 23 SORT doc.value3 DESC LIMIT 5 RETURN doc`, 'persistent', ["value3"], true, [ { _key: "test95", value1: 95, value2: "test95", value3: 95 }, { _key: "test94", value1: 94, value2: "test94", value3: 94 }, { _key: "test93", value1: 93, value2: "test93", value3: 93 } ] ],
      ];

      queries.forEach(function(query) {
        let plan = db._createStatement({query: query[0], options: { optimizer: { rules: ["-optimize-cluster-single-document-operations"] } }}).explain().plan;
        let nodes = plan.nodes.filter(function(node) { return node.type === 'IndexNode'; });
        assertEqual(1, nodes.length, query);
        assertEqual(1, nodes[0].indexes.length, query);
        assertEqual(query[1], nodes[0].indexes[0].type, query);
        assertEqual(normalize(query[2]), normalize(nodes[0].projections), query);
        assertEqual(query[3], nodes[0].indexCoversProjections, query);
        assertEqual("late materialized", nodes[0].strategy, query);

        let results = db._query(query[0], null, { optimizer: { rules: ["-optimize-cluster-single-document-operations"] } }).toArray();
        assertEqual(query[4].length, results.length);
        results = results.map(function(doc) {
          return { _key: doc._key, value1: doc.value1, value2: doc.value2, value3: doc.value3 };
        });
        assertEqual(query[4], results);
      });
    },
    
    testMaterializeSubAttributes : function () {
      c.ensureIndex({ type: "persistent", fields: ["foo.bar", "foo.baz", "foo.bat"] });
      let queries = [
        [`FOR doc IN ${cn} FILTER doc.foo.bar == 93 SORT doc.foo.bat LIMIT 5 RETURN doc`, 'persistent', [["foo","bat"]], true, [ { _key: "test93", value1: 93, value2: "test93", value3: 93 } ] ],
        [`FOR doc IN ${cn} FILTER doc.foo.bar >= 93 && doc.foo.bar <= 95 SORT doc.foo.bat LIMIT 5 RETURN doc`, 'persistent', [["foo","bat"]], true, [ { _key: "test93", value1: 93, value2: "test93", value3: 93 }, { _key: "test94", value1: 94, value2: "test94", value3: 94 }, { _key: "test95", value1: 95, value2: "test95", value3: 95 } ] ],
        [`FOR doc IN ${cn} FILTER doc.foo.bar >= 93 && doc.foo.bar <= 95 SORT doc.foo.bat DESC LIMIT 5 RETURN doc`, 'persistent', [["foo","bat"]], true, [ { _key: "test95", value1: 95, value2: "test95", value3: 95 }, { _key: "test94", value1: 94, value2: "test94", value3: 94 }, { _key: "test93", value1: 93, value2: "test93", value3: 93 } ] ],
        [`FOR doc IN ${cn} FILTER doc.foo.bar >= 93 && doc.foo.bar <= 95 && doc.foo.baz != 23 SORT doc.foo.bat LIMIT 5 RETURN doc`, 'persistent', [["foo","bat"]], true, [ { _key: "test93", value1: 93, value2: "test93", value3: 93 }, { _key: "test94", value1: 94, value2: "test94", value3: 94 }, { _key: "test95", value1: 95, value2: "test95", value3: 95 } ] ],
        [`FOR doc IN ${cn} FILTER doc.foo.bar >= 93 && doc.foo.bar <= 95 && doc.foo.baz != 23 SORT doc.foo.bat DESC LIMIT 5 RETURN doc`, 'persistent', [["foo","bat"]], true, [ { _key: "test95", value1: 95, value2: "test95", value3: 95 }, { _key: "test94", value1: 94, value2: "test94", value3: 94 }, { _key: "test93", value1: 93, value2: "test93", value3: 93 } ] ],
      ];

      queries.forEach(function(query) {
        let plan = db._createStatement({query: query[0], options: { optimizer: { rules: ["-optimize-cluster-single-document-operations"] } }}).explain().plan;
        let nodes = plan.nodes.filter(function(node) { return node.type === 'IndexNode'; });
        assertEqual(1, nodes.length, query);
        assertEqual(1, nodes[0].indexes.length, query);
        assertEqual(query[1], nodes[0].indexes[0].type, query);
        assertEqual(normalize(query[2]), normalize(nodes[0].projections), query);
        assertEqual(query[3], nodes[0].indexCoversProjections, query);
        assertEqual("late materialized", nodes[0].strategy, query);
        
        let results = db._query(query[0], null, { optimizer: { rules: ["-optimize-cluster-single-document-operations"] } }).toArray();
        assertEqual(query[4].length, results.length);
        results = results.map(function(doc) {
          return { _key: doc._key, value1: doc.value1, value2: doc.value2, value3: doc.value3 };
        });
        assertEqual(query[4], results);
      });
    },
    
    testMaterializeMaxProjections : function () {
      if (!IM.debugCanUseFailAt()) {
        // cant execute this test as failure points are not available here
        return;
      }

      IM.debugSetFailAt('batch-materialize-no-estimation');
      c.ensureIndex({ type: "persistent", fields: ["value1"] });
      let queries = [
        [`FOR doc IN ${cn} FILTER doc.value1 == 93 RETURN [doc.a, doc.b]`, 'persistent', [], false, ["a", "b"] ],
        [`FOR doc IN ${cn} FILTER doc.value1 == 93 RETURN [doc.a, doc.b, doc.c]`, 'persistent', [], false, ["a", "b", "c"] ],
        [`FOR doc IN ${cn} OPTIONS { maxProjections: 3 } FILTER doc.value1 == 93 RETURN [doc.a, doc.b, doc.c]`, 'persistent', [], false, ["a", "b", "c"] ],
        [`FOR doc IN ${cn} OPTIONS { maxProjections: 2 } FILTER doc.value1 == 93 RETURN [doc.a, doc.b]`, 'persistent', [], false, ["a", "b"] ],
        [`FOR doc IN ${cn} OPTIONS { maxProjections: 1 } FILTER doc.value1 == 93 RETURN [doc.a, doc.b]`, 'persistent', [], false, [] ],
        [`FOR doc IN ${cn} OPTIONS { maxProjections: 1 } FILTER doc.value1 == 93 RETURN [doc.a]`, 'persistent', [], false, ["a"] ],
      ];

      queries.forEach(function(query) {
        let plan = db._createStatement({query: query[0], options: { optimizer: { rules: ["-optimize-cluster-single-document-operations"] } }}).explain().plan;
        let nodes = plan.nodes.filter(function(node) { return node.type === 'IndexNode' || node.type === 'MaterializeNode'; });
        assertEqual(2, nodes.length, query);
        assertEqual("IndexNode", nodes[0].type);
        assertEqual("MaterializeNode", nodes[1].type);

        assertEqual(1, nodes[0].indexes.length, query);
        assertEqual(query[1], nodes[0].indexes[0].type, query);
        assertEqual(normalize(query[2]), normalize(nodes[0].projections), query);
        assertEqual(query[3], nodes[0].indexCoversProjections, query);
        assertEqual("late materialized", nodes[0].strategy, query);
        assertEqual(normalize(query[4]), normalize(nodes[1].projections), query);
      });
    },
    
    testSparseIndex : function () {
      c.ensureIndex({ type: "persistent", fields: ["value1"], sparse: true });
      let queries = [
        [`FOR doc IN ${cn} FILTER doc.value1 == 93 RETURN doc.value1`, 'persistent', ['value1'], true, "covering" ],
      ];

      queries.forEach(function(query) {
        let plan = db._createStatement({query: query[0], options: { optimizer: { rules: ["-optimize-cluster-single-document-operations"] } }}).explain().plan;
        let nodes = plan.nodes.filter(function(node) { return node.type === 'IndexNode'; });
        assertEqual(1, nodes.length, query);
        assertEqual(1, nodes[0].indexes.length, query);
        assertEqual(query[1], nodes[0].indexes[0].type, query);
        assertEqual(normalize(query[2]), normalize(nodes[0].projections), query);
        assertEqual(query[3], nodes[0].indexCoversProjections, query);
        assertEqual(query[4], nodes[0].strategy, query);
        
        if (query[2].length) {
          assertNotEqual(-1, plan.rules.indexOf(ruleName));
        } else {
          assertEqual(-1, plan.rules.indexOf(ruleName));
        }
      });
      
      queries = [
        [`FOR doc IN ${cn} RETURN doc.value1`, ['value1'] ],
        [`FOR doc IN ${cn} FILTER doc.value1 < 99 RETURN doc.value1`, ['value1'] ],
      ];

      queries.forEach(function(query) {
        let plan = db._createStatement({query: query[0]}).explain().plan;
        let nodes = plan.nodes.filter(function(node) { return node.type === 'EnumerateCollectionNode'; });
        assertEqual(1, nodes.length, query);
        assertEqual(normalize(query[1]), normalize(nodes[0].projections), query);
        assertNotEqual(-1, plan.rules.indexOf(ruleName));
      });
    },
    
    testSparseIndexSubAttribute : function () {
      c.ensureIndex({ type: "persistent", fields: ["a.b"], sparse: true });
      let queries = [
        [`FOR doc IN ${cn} FILTER doc.a.b == 93 RETURN doc.a.b`, 'persistent', [['a', 'b']], true, "covering" ],
        [`FOR doc IN ${cn} FILTER doc.a.b == 93 RETURN doc.a`, 'persistent', ['a'], false, "late materialized" ],
      ];

      queries.forEach(function(query) {
        let plan = db._createStatement({query: query[0], options: { optimizer: { rules: ["-optimize-cluster-single-document-operations"] } }}).explain().plan;
        let nodes = plan.nodes.filter(function(node) { return node.type === 'IndexNode'; });
        assertEqual(1, nodes.length, query);
        assertEqual(1, nodes[0].indexes.length, query);
        assertEqual(query[1], nodes[0].indexes[0].type, query);
        let matNodes = plan.nodes.filter(function(node) { return node.type === 'MaterializeNode'; });
        if (matNodes.length === 0) {
          assertEqual(query[2], normalize(nodes[0].projections), query);
        } else {
          assertEqual([], normalize(nodes[0].projections), query);
          assertEqual(normalize(query[2]), normalize(matNodes[0].projections), query);
        }
        assertEqual(query[3], nodes[0].indexCoversProjections, query);
        assertEqual(query[4], nodes[0].strategy, query);
        
        if (query[2].length) {
          assertNotEqual(-1, plan.rules.indexOf(ruleName));
        } else {
          assertEqual(-1, plan.rules.indexOf(ruleName));
        }
      });
      
      queries = [
        [`FOR doc IN ${cn} RETURN doc.a.b`, [['a', 'b']] ],
        [`FOR doc IN ${cn} FILTER doc.a.b < 99 RETURN doc.a.b`, [['a', 'b']] ],
      ];

      queries.forEach(function(query) {
        let plan = db._createStatement({query: query[0]}).explain().plan;
        let nodes = plan.nodes.filter(function(node) { return node.type === 'EnumerateCollectionNode'; });
        assertEqual(1, nodes.length, query);
        assertEqual(normalize(query[1]), normalize(nodes[0].projections), query);
        assertNotEqual(-1, plan.rules.indexOf(ruleName));
      });
    },
    
    testNoIndex : function () {
      let queries = [
        [`FOR doc IN ${cn} RETURN doc.foo`, ['foo'], [] ],
        [`FOR doc IN ${cn} FILTER doc.foo == 1 RETURN doc.foo`, ['foo'], ['foo'] ],
        [`FOR doc IN ${cn} FILTER doc.foo == 1 RETURN doc.foo.bar`, [['foo', 'bar']], ['foo'] ],
        [`FOR doc IN ${cn} FILTER doc.moo == 1 RETURN doc.foo`, ['foo'], ['moo'] ],
        [`FOR doc IN ${cn} RETURN doc.foo.bar`, [['foo', 'bar']], [] ],
        [`FOR doc IN ${cn} FILTER doc.foo.bar == 1 RETURN doc.foo.bar`, [['foo', 'bar']], [['foo', 'bar']] ],
        [`FOR doc IN ${cn} FILTER doc.moo == 1 RETURN doc.foo.bar`, [['foo', 'bar']], ['moo'] ],
        [`FOR doc IN ${cn} FILTER doc.moo == 1 RETURN doc.moo.bar`, [['moo', 'bar']], ['moo'] ],
        [`FOR doc IN ${cn} RETURN [doc.foo.a, doc.foo.b, doc.moo]`, [['foo', 'a'], ['foo', 'b'], 'moo'], [] ],
        [`FOR doc IN ${cn} RETURN [doc.a, doc.b, doc.moo]`, ['a', 'b', 'moo'], [] ],
        [`FOR doc IN ${cn} RETURN [doc.a, doc.b, doc.moo.far]`, ['a', 'b', ['moo', 'far']], [] ],
      ];

      queries.forEach(function(query) {
        let plan = db._createStatement({query: query[0]}).explain().plan;
        let nodes = plan.nodes.filter(function(node) { return node.type === 'EnumerateCollectionNode'; });
        assertEqual(1, nodes.length, query);
        assertEqual(normalize(query[1]), normalize(nodes[0].projections), query);
        assertEqual(normalize(query[2]), normalize(nodes[0].filterProjections), query);
        assertNotEqual(-1, plan.rules.indexOf(ruleName));
      });
    },

  };
}

function projectionsExtractionTestSuite () {
  let c = null;

  return {
    setUp : function () {
      db._drop(cn);
      c = db._create(cn, { numberOfShards: 1 });

      let docs = [];
      for (let i = 0; i < 100; ++i) {
        docs.push({ _key: "test" + i, value1: i, value2: i, foo: { bar: { a: i, b: i }, baz: i } });
      }
      c.insert(docs);
    },

    tearDown : function () {
      db._drop(cn);
    },

    testExtractMultiAttribute : function () {
      c.ensureIndex({ type: "persistent", fields: ["value1", "value2"] });
      let one = [];
      for (let i = 0; i < c.count(); ++i) {
        one.push(i);
      }
      let two = [ 93 ];

      let queries = [
        [`FOR doc IN ${cn} RETURN doc.value1`, 'persistent', ['value1'], true, one, [] ],
        [`FOR doc IN ${cn} RETURN doc.value2`, 'persistent', ['value2'], true, one, [] ],
        [`FOR doc IN ${cn} FILTER doc.value1 == 93 RETURN doc.value1`, 'persistent', ['value1'], true, two, [] ],
        [`FOR doc IN ${cn} FILTER doc.value2 == 93 RETURN doc.value1`, 'persistent', ['value1'], true, two, ['value2'] ],
        [`FOR doc IN ${cn} FILTER doc.value1 == 93 RETURN doc.value2`, 'persistent', ['value2'], true, two, [] ],
        [`FOR doc IN ${cn} FILTER doc.value2 == 93 RETURN doc.value2`, 'persistent', ['value2'], true, two, ['value2'] ],
        [`FOR doc IN ${cn} FILTER doc._key == 'test93' RETURN doc.value1`, 'primary', ['value1'], false, two, [] ],
        [`FOR doc IN ${cn} FILTER doc._key == 'test93' RETURN doc.value2`, 'primary', ['value2'], false, two, [] ],
      ];

      queries.forEach(function(query) {
        let plan = db._createStatement({query: query[0], options: { optimizer: { rules: ["-optimize-cluster-single-document-operations"] } }}).explain().plan;
        let nodes = plan.nodes.filter(function(node) { return node.type === 'IndexNode'; });
        assertEqual(1, nodes.length, query);
        assertEqual(1, nodes[0].indexes.length, query);
        assertEqual(query[1], nodes[0].indexes[0].type, query);
        assertEqual(normalize(query[2]), normalize(nodes[0].projections), query);
        assertEqual(normalize(query[5]), normalize(nodes[0].filterProjections), query);
        assertEqual(query[3], nodes[0].indexCoversProjections, query);
        assertNotEqual(-1, plan.rules.indexOf(ruleName));
        let results = db._query(query[0], null, { optimizer: { rules: ["-optimize-cluster-single-document-operations"] } }).toArray();
        assertEqual(query[4], results);
      });
    },
    
    testExtractSingleAttribute : function () {
      c.ensureIndex({ type: "persistent", fields: ["foo"] });
      let foo = [], bar = [], barA = [], baz = [], barAB = [], bazBarA = [];
      for (let i = 0; i < c.count(); ++i) {
        foo.push({ bar: { a: i, b: i }, baz: i });
        bar.push({ a: i, b: i });
        barA.push(i);
        barAB.push([i, i]);
        bazBarA.push([i, i]);
        baz.push(i);
      }

      let queries = [
        [`FOR doc IN ${cn} RETURN doc.foo`, ['foo'], true, foo ],
        [`FOR doc IN ${cn} RETURN doc.foo.bar`, [['foo', 'bar']], true, bar ],
        [`FOR doc IN ${cn} RETURN doc.foo.bar.a`, [['foo', 'bar', 'a']], true, barA ],
        [`FOR doc IN ${cn} RETURN doc.foo.baz`, [['foo', 'baz']], true, baz ],
        [`FOR doc IN ${cn} RETURN [doc.foo.bar.a, doc.foo.bar.b]`, [['foo', 'bar', 'a'], ['foo', 'bar', 'b']], true, barAB ],
        [`FOR doc IN ${cn} RETURN [doc.foo.baz, doc.foo.bar.a]`, [['foo', 'bar', 'a'], ['foo', 'baz']], true, bazBarA ],
      ];

      queries.forEach(function(query) {
        let plan = db._createStatement({query: query[0], options: { optimizer: { rules: ["-optimize-cluster-single-document-operations"] } }}).explain().plan;
        let nodes = plan.nodes.filter(function(node) { return node.type === 'IndexNode' || node.type === 'EnumerateCollectionNode'; });
        assertEqual(1, nodes.length, query);
        assertEqual(normalize(query[1]), normalize(nodes[0].projections), query);
        assertEqual(query[2] ? 'IndexNode' : 'EnumerateCollectionNode', nodes[0].type, query);
        assertNotEqual(-1, plan.rules.indexOf(ruleName));
        let results = db._query(query[0], null, { optimizer: { rules: ["-optimize-cluster-single-document-operations"] } }).toArray();
        assertEqual(query[3], results);
      });
    },
    
    testExtractSubAttribute : function () {
      c.ensureIndex({ type: "persistent", fields: ["foo.bar"] });
      let foo = [], bar = [], baz = [], barA = [], barBaz = [];
      for (let i = 0; i < c.count(); ++i) {
        foo.push({ bar: { a: i, b: i }, baz: i });
        bar.push({ a: i, b: i });
        barA.push(i);
        baz.push(i);
        barBaz.push([i, i]);
      }

      let queries = [
        [`FOR doc IN ${cn} RETURN doc.foo`, ['foo'], false, foo ],
        [`FOR doc IN ${cn} RETURN doc.foo.bar`, [['foo', 'bar']], true, bar ],
        [`FOR doc IN ${cn} RETURN doc.foo.baz`, [['foo', 'baz']], false, baz ],
        [`FOR doc IN ${cn} RETURN doc.foo.bar.a`, [['foo', 'bar', 'a']], true, barA ],
        [`FOR doc IN ${cn} RETURN [doc.foo.bar.a, doc.foo.baz]`, [['foo', 'bar', 'a'], ['foo', 'baz']], false, barBaz ],
        [`FOR doc IN ${cn} RETURN [doc.foo.bar.a, doc.foo.bar.b]`, [['foo', 'bar', 'a'], ['foo', 'bar', 'b']], true, barBaz ],
      ];

      queries.forEach(function(query) {
        let plan = db._createStatement({query: query[0], options: { optimizer: { rules: ["-optimize-cluster-single-document-operations"] } }}).explain().plan;
        let nodes = plan.nodes.filter(function(node) { return node.type === 'IndexNode' || node.type === 'EnumerateCollectionNode'; });
        assertEqual(1, nodes.length, query);
        assertEqual(normalize(query[1]), normalize(nodes[0].projections), query);
        assertEqual(query[2] ? 'IndexNode' : 'EnumerateCollectionNode', nodes[0].type, query);
        assertNotEqual(-1, plan.rules.indexOf(ruleName));
        let results = db._query(query[0], null, { optimizer: { rules: ["-optimize-cluster-single-document-operations"] } }).toArray();
        assertEqual(query[3], results);
      });
    },

    testExtractSecondSubAttribute : function () {
      // While the index is created on [p.s, p.k], the query filters by p.k but only extracts p.s.
      c.ensureIndex({ type: "persistent", fields: ["p.s", "p.k"]});
      c.insert({"p": {"s": 1234, "k": "hund"}});
      c.insert({"p": {"s": 1235, "k": "katze"}});
      c.insert({"p": {"s": 1236, "k": "schnecke"}});
      c.insert({"p": {"s": 1237, "k": "kuh"}});
      let bindVars = {'@coll': cn};
      let q = "" +
        "FOR doc in @@coll " +
        "FILTER doc.p.k == 'hund' " +
        "SORT doc.p.s DESC " +
        "RETURN doc.p.s";
      let result = db._createStatement({query: q, bindVars}).execute();
      assertEqual(result.toArray(), [1234]);
    },
  
    testRemoveUnnecessaryProjectionsIndex : function () {
      // While the index is created on [p.s, p.k], the query filters by p.k but only extracts p.s.
      c.ensureIndex({ type: "persistent", fields: ["x", "y"], storedValues: ["z", "w"]});
      let q = `for doc1 in ${cn} filter doc1.x == 5 AND doc1.z == 8 return doc1.w`;
      let plan = db._createStatement(q).explain().plan;
      const index = plan.nodes[1];
      assertEqual(index.type, "IndexNode");
      assertEqual(normalize(index.projections), normalize(["w"]));
      assertEqual(normalize(index.filterProjections), normalize(["z"]));
    },

    testRemoveUnnecessaryProjectionsCollection : function () {
      // While the index is created on [p.s, p.k], the query filters by p.k but only extracts p.s.
      let q = `for doc1 in ${cn} filter doc1.x == 5 AND doc1.z == 8 return doc1.w`;
      let plan = db._createStatement(q).explain().plan;
      const ec = plan.nodes[1];
      assertEqual(ec.type, "EnumerateCollectionNode");
      assertEqual(normalize(ec.projections), normalize(["w"]));
      assertEqual(normalize(ec.filterProjections), normalize(["x", "z"]));
    },
  };
}

function projectionsMaxProjectionsTestSuite () {
  let c = null;

  return {
    setUp : function () {
      db._drop(cn);
      c = db._create(cn, { numberOfShards: 1 });

      let docs = [];
      for (let i = 0; i < 100; ++i) {
        docs.push({ _key: "test" + i, value1: i, value2: i, value3: i, value4: i, value5: i, value6: i, value7: i });
      }
      c.insert(docs);
    },

    tearDown : function () {
      db._drop(cn);
    },

    testDefaultMaxProjections : function () {
      let queries = [];
      for (let i = 1; i < 11; ++i) {
        let fields = [];
        let projections = [];
        for (let j = 0; j < i; ++j) {
          fields.push(`doc.value${j}`);
          if (i <= 5) {
            projections.push(`value${j}`);
          }
        }
        let query = `FOR doc IN ${cn} RETURN [ ${fields.join(', ')} ]`;
        queries.push([ query, projections ]);
      }

      queries.forEach(function(query) {
        let plan = db._createStatement({query: query[0], options: { optimizer: { rules: ["-optimize-cluster-single-document-operations"] } }}).explain().plan;
        let nodes = plan.nodes.filter(function(node) { return node.type === 'EnumerateCollectionNode'; });
        assertEqual(1, nodes.length, query);
        assertEqual(normalize(query[1]), normalize(nodes[0].projections), query);
        if (query[1].length) {
          assertNotEqual(-1, plan.rules.indexOf(ruleName));
        } else {
          assertEqual(-1, plan.rules.indexOf(ruleName));
        }
      });
    },
    
    testAdjustedMaxProjections : function () {
      let queries = [];
      [ 1, 2, 3, 5, 6, 10, 12 ].forEach((maxProjections) => {
        for (let i = 1; i < 13; ++i) {
          let fields = [];
          let projections = [];
          for (let j = 0; j < i; ++j) {
            fields.push(`doc.value${j}`);
            if (i <= maxProjections) {
              projections.push(`value${j}`);
            }
          }
          let query = `FOR doc IN ${cn} OPTIONS { maxProjections: ${maxProjections} } RETURN [ ${fields.join(', ')} ]`;
          queries.push([ query, projections ]);
        }
      });

      queries.forEach(function(query) {
        let plan = db._createStatement({query: query[0], options: { optimizer: { rules: ["-optimize-cluster-single-document-operations"] } }}).explain().plan;
        let nodes = plan.nodes.filter(function(node) { return node.type === 'EnumerateCollectionNode'; });
        assertEqual(1, nodes.length, query);
        assertEqual(normalize(query[1].sort()), normalize(nodes[0].projections.sort()), query);
        if (query[1].length) {
          assertNotEqual(-1, plan.rules.indexOf(ruleName));
        } else {
          assertEqual(-1, plan.rules.indexOf(ruleName));
        }
      });
    },
    
  };
}

jsunity.run(projectionsPlansTestSuite);
jsunity.run(projectionsExtractionTestSuite);
jsunity.run(projectionsMaxProjectionsTestSuite);

return jsunity.done();
