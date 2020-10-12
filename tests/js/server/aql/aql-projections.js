/*jshint globalstrict:false, strict:false, maxlen: 500 */
/*global assertEqual, assertTrue, assertFalse, assertNotEqual, AQL_EXPLAIN */

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

let jsunity = require("jsunity");
let db = require("@arangodb").db;
const ruleName = "reduce-extraction-to-projection";
const cn = "UnitTestsOptimizer";

function projectionsPlansTestSuite () {
  let c = null;

  return {
    setUp : function () {
      db._drop(cn);
      c = db._createEdgeCollection(cn, { numberOfShards: 4 });

      let docs = [];
      for (var i = 0; i < 1000; ++i) {
        docs.push({ _from: "v/test" + i, _to: "v/test" + i, _key: "test" + i, value1: i, value2: "test" + i, value3: i, foo: { bar: i, baz: i, bat: i } });
      }
      c.insert(docs);
    },

    tearDown : function () {
      db._drop(cn);
    },

    testPrimary : function () {
      let queries = [
        [`FOR doc IN ${cn} RETURN doc._key`, 'primary', ['_key'] ],
        [`FOR doc IN ${cn} FILTER doc._key == 'abc' RETURN doc._key`, 'primary', ['_key'] ],
        [`FOR doc IN ${cn} FILTER doc._key == 'abc' RETURN doc._id`, 'primary', ['_id'] ],
        [`FOR doc IN ${cn} FILTER doc._key == 'abc' RETURN 1`, 'primary', [] ],
        [`FOR doc IN ${cn} RETURN doc._id`, 'primary', ['_id'] ],
        [`FOR doc IN ${cn} FILTER doc._id == '${cn}/abc' RETURN doc._id`, 'primary', ['_id'] ],
        [`FOR doc IN ${cn} FILTER doc._id == '${cn}/abc' RETURN doc._key`, 'primary', ['_key'] ],
        [`FOR doc IN ${cn} FILTER doc._id == '${cn}/abc' RETURN 1`, 'primary', [] ],
        [`FOR doc IN ${cn} RETURN [doc._key, doc._id]`, 'primary', ['_id', '_key'] ],
        [`FOR doc IN ${cn} FILTER doc._id == '${cn}/abc' RETURN [doc._key, doc._id]`, 'primary', ['_id', '_key'] ],
        [`FOR doc IN ${cn} REMOVE doc IN ${cn}`, 'primary', ['_key'] ],
        [`FOR doc IN ${cn} REMOVE doc._key IN ${cn}`, 'primary', ['_key'] ],
        [`FOR doc IN ${cn} REMOVE { _id: doc._id } IN ${cn}`, 'primary', ['_id'] ],
        [`FOR doc IN ${cn} UPDATE doc WITH {} IN ${cn}`, 'primary', ['_key'] ],
        [`FOR doc IN ${cn} UPDATE doc._key WITH {} IN ${cn}`, 'primary', ['_key'] ],
        [`FOR doc IN ${cn} UPDATE { _id: doc._id } WITH {} IN ${cn}`, 'primary', ['_id'] ],
        [`FOR doc IN ${cn} REPLACE doc WITH {} IN ${cn}`, 'primary', ['_key'] ],
        [`FOR doc IN ${cn} REPLACE doc._key WITH {} IN ${cn}`, 'primary', ['_key'] ],
        [`FOR doc IN ${cn} REPLACE { _id: doc._id } WITH {} IN ${cn}`, 'primary', ['_id'] ],
      ];

      queries.forEach(function(query) {
        let plan = AQL_EXPLAIN(query[0], null, { optimizer: { rules: ["-optimize-cluster-single-document-operations"] } }).plan;
        let nodes = plan.nodes.filter(function(node) { return node.type === 'IndexNode'; });
        assertEqual(1, nodes.length, query);
        assertEqual(1, nodes[0].indexes.length, query);
        assertEqual(query[1], nodes[0].indexes[0].type, query);
        assertEqual(query[2], nodes[0].projections, query);
        if (query[2].length) {
          assertTrue(nodes[0].indexCoversProjections, query);
          assertNotEqual(-1, plan.rules.indexOf(ruleName));
        } else {
          assertFalse(nodes[0].indexCoversProjections, query);
          assertEqual(-1, plan.rules.indexOf(ruleName));
        }
      });
    },
    
    testEdge : function () {
      let queries = [
        [`FOR doc IN ${cn} FILTER doc._key == 'abc' RETURN doc._from`, 'primary', ['_from'], false ],
        [`FOR doc IN ${cn} FILTER doc._key == 'abc' RETURN doc._to`, 'primary', ['_to'], false ],
        [`FOR doc IN ${cn} FILTER doc._key == 'abc' RETURN [doc._from, doc._to]`, 'primary', ['_from', '_to'], false ],
        [`FOR doc IN ${cn} FILTER doc._from == 'v/abc' RETURN 1`, 'edge', [], false ],
        [`FOR doc IN ${cn} FILTER doc._to == 'v/abc' RETURN 1`, 'edge', [], false ],
        [`FOR doc IN ${cn} FILTER doc._from == '${cn}/abc' RETURN doc._from`, 'edge', ['_from'], true ],
        [`FOR doc IN ${cn} FILTER doc._from == '${cn}/abc' RETURN doc._to`, 'edge', ['_to'], true ],
        [`FOR doc IN ${cn} FILTER doc._from == '${cn}/abc' RETURN [doc._from, doc._to]`, 'edge', ['_from', '_to'], true ],
      ];

      queries.forEach(function(query) {
        let plan = AQL_EXPLAIN(query[0], null, { optimizer: { rules: ["-optimize-cluster-single-document-operations"] } }).plan;
        let nodes = plan.nodes.filter(function(node) { return node.type === 'IndexNode'; });
        assertEqual(1, nodes.length, query);
        assertEqual(1, nodes[0].indexes.length, query);
        assertEqual(query[1], nodes[0].indexes[0].type, query);
        assertEqual(query[2], nodes[0].projections, query);
        assertEqual(query[3], nodes[0].indexCoversProjections, query);
        if (query[2].length) {
          assertNotEqual(-1, plan.rules.indexOf(ruleName));
        } else {
          assertEqual(-1, plan.rules.indexOf(ruleName));
        }
      });

      queries = [
        [`FOR doc IN ${cn} RETURN doc._from`, ['_from'] ],
        [`FOR doc IN ${cn} RETURN doc._to`, ['_to'] ],
        [`FOR doc IN ${cn} RETURN [doc._from, doc._to]`, ['_from', '_to'] ],
      ];
      
      queries.forEach(function(query) {
        let plan = AQL_EXPLAIN(query[0]).plan;
        let nodes = plan.nodes.filter(function(node) { return node.type === 'EnumerateCollectionNode'; });
        assertEqual(1, nodes.length, query);
        assertEqual(query[1], nodes[0].projections, query);
        assertNotEqual(-1, plan.rules.indexOf(ruleName));
      });
    },

    testPersistentSingleAttribute : function () {
      c.ensureIndex({ type: "persistent", fields: ["value1"] });
      let queries = [
        [`FOR doc IN ${cn} RETURN doc.value1`, 'persistent', ['value1'], true ],
        [`FOR doc IN ${cn} FILTER doc.value1 == 93 RETURN doc.value1`, 'persistent', ['value1'], true ],
        [`FOR doc IN ${cn} FILTER doc.value1 == 93 RETURN [doc.value1, doc.value2]`, 'persistent', ['value1', 'value2'], false ],
        [`FOR doc IN ${cn} FILTER doc.value1 == 93 RETURN doc.value2`, 'persistent', ['value2'], false ],
        [`FOR doc IN ${cn} FILTER doc.value1 == 93 RETURN [doc.value1, doc.value2]`, 'persistent', ['value1', 'value2'], false ],
        [`FOR doc IN ${cn} FILTER doc.value1 == 93 RETURN 1`, 'persistent', [], false ],
        [`FOR doc IN ${cn} FILTER doc._key == 'abc' RETURN doc.value1`, 'primary', ['value1'], false ],
        [`FOR doc IN ${cn} FILTER doc._key == 'abc' RETURN doc.value2`, 'primary', ['value2'], false ],
      ];

      queries.forEach(function(query) {
        let plan = AQL_EXPLAIN(query[0], null, { optimizer: { rules: ["-optimize-cluster-single-document-operations"] } }).plan;
        let nodes = plan.nodes.filter(function(node) { return node.type === 'IndexNode'; });
        assertEqual(1, nodes.length, query);
        assertEqual(1, nodes[0].indexes.length, query);
        assertEqual(query[1], nodes[0].indexes[0].type, query);
        assertEqual(query[2], nodes[0].projections, query);
        assertEqual(query[3], nodes[0].indexCoversProjections, query);
        
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
        let plan = AQL_EXPLAIN(query[0]).plan;
        let nodes = plan.nodes.filter(function(node) { return node.type === 'EnumerateCollectionNode'; });
        assertEqual(1, nodes.length, query);
        assertEqual(query[1], nodes[0].projections, query);
        assertNotEqual(-1, plan.rules.indexOf(ruleName));
      });
    },
    
    testPersistentMultiAttribute : function () {
      c.ensureIndex({ type: "persistent", fields: ["value1", "value2"] });
      let queries = [
        [`FOR doc IN ${cn} RETURN doc.value1`, 'persistent', ['value1'], true ],
        [`FOR doc IN ${cn} RETURN doc.value2`, 'persistent', ['value2'], true ],
        [`FOR doc IN ${cn} FILTER doc.value1 == 93 RETURN doc.value1`, 'persistent', ['value1'], true ],
        [`FOR doc IN ${cn} FILTER doc.value2 == 93 RETURN doc.value1`, 'persistent', ['value1', 'value2'], true ],
        [`FOR doc IN ${cn} FILTER doc.value1 == 93 RETURN doc.value2`, 'persistent', ['value2'], true ],
        [`FOR doc IN ${cn} FILTER doc.value2 == 93 RETURN doc.value2`, 'persistent', ['value2'], true ],
        [`FOR doc IN ${cn} FILTER doc._key == 'abc' RETURN doc.value1`, 'primary', ['value1'], false ],
        [`FOR doc IN ${cn} FILTER doc._key == 'abc' RETURN doc.value2`, 'primary', ['value2'], false ],
      ];

      queries.forEach(function(query) {
        let plan = AQL_EXPLAIN(query[0], null, { optimizer: { rules: ["-optimize-cluster-single-document-operations"] } }).plan;
        let nodes = plan.nodes.filter(function(node) { return node.type === 'IndexNode'; });
        assertEqual(1, nodes.length, query);
        assertEqual(1, nodes[0].indexes.length, query);
        assertEqual(query[1], nodes[0].indexes[0].type, query);
        assertEqual(query[2], nodes[0].projections, query);
        assertEqual(query[3], nodes[0].indexCoversProjections, query);
        assertNotEqual(-1, plan.rules.indexOf(ruleName));
      });
      
      queries = [
        [`FOR doc IN ${cn} RETURN [doc.value1, doc.value3]`, ['value1', 'value3'] ],
        [`FOR doc IN ${cn} RETURN [doc.value2, doc.value3]`, ['value2', 'value3'] ],
        [`FOR doc IN ${cn} RETURN doc.value3`, ['value3'] ],
        [`FOR doc IN ${cn} RETURN [doc.value3, doc.value4]`, ['value3', 'value4'] ],
      ];

      queries.forEach(function(query) {
        let plan = AQL_EXPLAIN(query[0]).plan;
        let nodes = plan.nodes.filter(function(node) { return node.type === 'EnumerateCollectionNode'; });
        assertEqual(1, nodes.length, query);
        assertEqual(query[1], nodes[0].projections, query);
        assertNotEqual(-1, plan.rules.indexOf(ruleName));
      });
    },
    
    testPersistentSubAttribute : function () {
      c.ensureIndex({ type: "persistent", fields: ["foo.bar"] });
      let queries = [
        [`FOR doc IN ${cn} RETURN doc.foo.bar`, 'persistent', [['foo', 'bar']], true ],
        [`FOR doc IN ${cn} RETURN doc.foo.bar.baz`, 'persistent', [['foo', 'bar', 'baz']], true ],
        [`FOR doc IN ${cn} RETURN [doc.foo.bar.baz, doc.foo.bar.bat]`, 'persistent', [['foo', 'bar']], true ],
        [`FOR doc IN ${cn} RETURN [doc.foo.bar, doc.foo.bar.baz]`, 'persistent', [['foo', 'bar']], true ],
        [`FOR doc IN ${cn} FILTER doc.foo.bar == 1 RETURN doc.foo.bar`, 'persistent', [['foo', 'bar']], true ],
        [`FOR doc IN ${cn} FILTER doc.foo.bar == 1 RETURN 1`, 'persistent', [], false ],
      ];

      queries.forEach(function(query) {
        let plan = AQL_EXPLAIN(query[0], null, { optimizer: { rules: ["-optimize-cluster-single-document-operations"] } }).plan;
        let nodes = plan.nodes.filter(function(node) { return node.type === 'IndexNode'; });
        assertEqual(1, nodes.length, query);
        assertEqual(1, nodes[0].indexes.length, query);
        assertEqual(query[1], nodes[0].indexes[0].type, query);
        assertEqual(query[2], nodes[0].projections, query);
        assertEqual(query[3], nodes[0].indexCoversProjections, query);
        if (query[2].length) {
          assertNotEqual(-1, plan.rules.indexOf(ruleName));
        } else {
          assertEqual(-1, plan.rules.indexOf(ruleName));
        }
      });
      
      queries = [
        [`FOR doc IN ${cn} RETURN doc.boo`, ['boo'] ],
        [`FOR doc IN ${cn} RETURN doc.foo`, ['foo'] ],
        [`FOR doc IN ${cn} RETURN doc.foo.baz`, [['foo', 'baz']] ],
        [`FOR doc IN ${cn} RETURN doc.boo.far`, [['boo', 'far']] ],
        [`FOR doc IN ${cn} RETURN [doc.foo.bar, doc.unrelated]`, [['foo', 'bar'], 'unrelated'] ],
      ];

      queries.forEach(function(query) {
        let plan = AQL_EXPLAIN(query[0]).plan;
        let nodes = plan.nodes.filter(function(node) { return node.type === 'EnumerateCollectionNode'; });
        assertEqual(1, nodes.length, query);
        assertEqual(query[1], nodes[0].projections, query);
        assertNotEqual(-1, plan.rules.indexOf(ruleName));
      });
    },
    
    testPersistentSubAttributesMulti : function () {
      c.ensureIndex({ type: "persistent", fields: ["foo.bar", "foo.baz"] });
      let queries = [
        [`FOR doc IN ${cn} RETURN doc.foo.bar`, 'persistent', [['foo', 'bar']], true ],
        [`FOR doc IN ${cn} RETURN doc.foo.baz`, 'persistent', [['foo', 'baz']], true ],
        [`FOR doc IN ${cn} RETURN doc.foo.bar.baz`, 'persistent', [['foo', 'bar', 'baz']], true ],
        [`FOR doc IN ${cn} RETURN doc.foo.baz.bar`, 'persistent', [['foo', 'baz', 'bar']], true ],
        [`FOR doc IN ${cn} RETURN [doc.foo.bar.baz, doc.foo.bar.bat]`, 'persistent', [['foo', 'bar']], true ],
        [`FOR doc IN ${cn} RETURN [doc.foo.baz.bar, doc.foo.baz.bat]`, 'persistent', [['foo', 'baz']], true ],
        [`FOR doc IN ${cn} FILTER doc.foo.bar == 1 RETURN doc.foo.bar`, 'persistent', [['foo', 'bar']], true ],
        [`FOR doc IN ${cn} FILTER doc.foo.bar == 1 RETURN doc.foo.baz`, 'persistent', [['foo', 'baz']], true ],
        [`FOR doc IN ${cn} FILTER doc.foo.baz == 1 RETURN doc.foo.baz`, 'persistent', [['foo', 'baz']], true ],
        [`FOR doc IN ${cn} FILTER doc.foo.bar == 1 RETURN 1`, 'persistent', [], false ],
        [`FOR doc IN ${cn} FILTER doc.foo.baz == 1 RETURN 1`, 'persistent', [['foo', 'baz']], true ],
      ];

      queries.forEach(function(query) {
        let plan = AQL_EXPLAIN(query[0], null, { optimizer: { rules: ["-optimize-cluster-single-document-operations"] } }).plan;
        let nodes = plan.nodes.filter(function(node) { return node.type === 'IndexNode'; });
        assertEqual(1, nodes.length, query);
        assertEqual(1, nodes[0].indexes.length, query);
        assertEqual(query[1], nodes[0].indexes[0].type, query);
        assertEqual(query[2], nodes[0].projections, query);
        assertEqual(query[3], nodes[0].indexCoversProjections, query);
        if (query[2].length) {
          assertNotEqual(-1, plan.rules.indexOf(ruleName));
        } else {
          assertEqual(-1, plan.rules.indexOf(ruleName));
        }
      });
      
      queries = [
        [`FOR doc IN ${cn} RETURN doc.boo`, ['boo'] ],
        [`FOR doc IN ${cn} RETURN doc.foo`, ['foo'] ],
        [`FOR doc IN ${cn} RETURN doc.foo.bat`, [['foo', 'bat']] ],
        [`FOR doc IN ${cn} RETURN doc.foo.moo`, [['foo', 'moo']] ],
        [`FOR doc IN ${cn} RETURN doc.bar`, ['bar'] ],
        [`FOR doc IN ${cn} RETURN doc.baz`, ['baz'] ],
        [`FOR doc IN ${cn} RETURN [doc.foo.bar, doc.foo.baz]`, ['foo'] ],
        [`FOR doc IN ${cn} RETURN [doc.foo.bar, doc.foo.baz.bar]`, ['foo'] ],
        [`FOR doc IN ${cn} RETURN [doc.foo.bar, doc.unrelated]`, [['foo', 'bar'], 'unrelated'] ],
        [`FOR doc IN ${cn} RETURN [doc.foo.baz, doc.unrelated]`, [['foo', 'baz'], 'unrelated'] ],
        [`FOR doc IN ${cn} RETURN [doc.foo.bar, doc.foo.baz, doc.unrelated]`, ['foo', 'unrelated'] ],
        [`FOR doc IN ${cn} FILTER doc.foo.baz == 1 RETURN doc.foo.bar`, ['foo'] ],
      ];

      queries.forEach(function(query) {
        let plan = AQL_EXPLAIN(query[0]).plan;
        let nodes = plan.nodes.filter(function(node) { return node.type === 'EnumerateCollectionNode'; });
        assertEqual(1, nodes.length, query);
        assertEqual(query[1], nodes[0].projections, query);
        assertNotEqual(-1, plan.rules.indexOf(ruleName));
      });
    },
    
    testPersistentOnKeyAndOtherAttribute : function () {
      c.ensureIndex({ type: "persistent", fields: ["_key", "value1"] });
      let queries = [
        [`FOR doc IN ${cn} RETURN doc.value1`, 'persistent', ['value1'], true ],
        [`FOR doc IN ${cn} RETURN doc._key`, 'primary', ['_key'], true ],
        [`FOR doc IN ${cn} FILTER doc._key == 'abc' RETURN doc.value1`, 'primary', ['value1'], false ],
        [`FOR doc IN ${cn} FILTER doc._key == 'abc' RETURN doc.value2`, 'primary', ['value2'], false ],
        [`FOR doc IN ${cn} FILTER doc.value1 == 93 RETURN doc.value1`, 'persistent', ['value1'], true ],
        [`FOR doc IN ${cn} FILTER doc.value1 == 93 RETURN doc._key`, 'persistent', ['_key', 'value1'], true ],
      ];

      queries.forEach(function(query) {
        let plan = AQL_EXPLAIN(query[0], null, { optimizer: { rules: ["-optimize-cluster-single-document-operations"] } }).plan;
        let nodes = plan.nodes.filter(function(node) { return node.type === 'IndexNode'; });
        assertEqual(1, nodes.length, query);
        assertEqual(1, nodes[0].indexes.length, query);
        assertEqual(query[1], nodes[0].indexes[0].type, query);
        assertEqual(query[2], nodes[0].projections, query);
        assertEqual(query[3], nodes[0].indexCoversProjections, query);
        
        if (query[2].length) {
          assertNotEqual(-1, plan.rules.indexOf(ruleName));
        } else {
          assertEqual(-1, plan.rules.indexOf(ruleName));
        }
      });
    },
    
    testPersistentOnOtherAttributeAndKey : function () {
      c.ensureIndex({ type: "persistent", fields: ["value1", "_key"] });
      let queries = [
        [`FOR doc IN ${cn} RETURN doc.value1`, 'persistent', ['value1'], true ],
        [`FOR doc IN ${cn} RETURN doc._key`, 'primary', ['_key'], true ],
        [`FOR doc IN ${cn} FILTER doc._key == 'abc' RETURN doc.value1`, 'primary', ['value1'], false ],
        [`FOR doc IN ${cn} FILTER doc._key == 'abc' RETURN doc.value2`, 'primary', ['value2'], false ],
        [`FOR doc IN ${cn} FILTER doc.value1 == 93 RETURN doc.value1`, 'persistent', ['value1'], true ],
        [`FOR doc IN ${cn} FILTER doc.value1 == 93 RETURN doc._key`, 'persistent', ['_key'], true ],
      ];

      queries.forEach(function(query) {
        let plan = AQL_EXPLAIN(query[0], null, { optimizer: { rules: ["-optimize-cluster-single-document-operations"] } }).plan;
        let nodes = plan.nodes.filter(function(node) { return node.type === 'IndexNode'; });
        assertEqual(1, nodes.length, query);
        assertEqual(1, nodes[0].indexes.length, query);
        assertEqual(query[1], nodes[0].indexes[0].type, query);
        assertEqual(query[2], nodes[0].projections, query);
        assertEqual(query[3], nodes[0].indexCoversProjections, query);
        
        if (query[2].length) {
          assertNotEqual(-1, plan.rules.indexOf(ruleName));
        } else {
          assertEqual(-1, plan.rules.indexOf(ruleName));
        }
      });
    },
    
    testMaterialize : function () {
      c.ensureIndex({ type: "persistent", fields: ["value1", "value2", "value3"] });
      let queries = [
        [`FOR doc IN ${cn} FILTER doc.value1 == 93 SORT doc.value3 LIMIT 5 RETURN doc`, 'persistent', [], false, [ { _key: "test93", value1: 93, value2: "test93", value3: 93 } ] ],
        [`FOR doc IN ${cn} FILTER doc.value1 >= 93 && doc.value1 <= 95 SORT doc.value3 LIMIT 5 RETURN doc`, 'persistent', [], false, [ { _key: "test93", value1: 93, value2: "test93", value3: 93 }, { _key: "test94", value1: 94, value2: "test94", value3: 94 }, { _key: "test95", value1: 95, value2: "test95", value3: 95 } ] ],
        [`FOR doc IN ${cn} FILTER doc.value1 >= 93 && doc.value1 <= 95 SORT doc.value3 DESC LIMIT 5 RETURN doc`, 'persistent', [], false, [ { _key: "test95", value1: 95, value2: "test95", value3: 95 }, { _key: "test94", value1: 94, value2: "test94", value3: 94 }, { _key: "test93", value1: 93, value2: "test93", value3: 93 } ] ],
        [`FOR doc IN ${cn} FILTER doc.value1 >= 93 && doc.value1 <= 95 && doc.value2 != 23 SORT doc.value3 LIMIT 5 RETURN doc`, 'persistent', [], false, [ { _key: "test93", value1: 93, value2: "test93", value3: 93 }, { _key: "test94", value1: 94, value2: "test94", value3: 94 }, { _key: "test95", value1: 95, value2: "test95", value3: 95 } ] ],
        [`FOR doc IN ${cn} FILTER doc.value1 >= 93 && doc.value1 <= 95 && doc.value2 != 23 SORT doc.value3 DESC LIMIT 5 RETURN doc`, 'persistent', [], false, [ { _key: "test95", value1: 95, value2: "test95", value3: 95 }, { _key: "test94", value1: 94, value2: "test94", value3: 94 }, { _key: "test93", value1: 93, value2: "test93", value3: 93 } ] ],
      ];

      queries.forEach(function(query) {
        let plan = AQL_EXPLAIN(query[0], null, { optimizer: { rules: ["-optimize-cluster-single-document-operations"] } }).plan;
        let nodes = plan.nodes.filter(function(node) { return node.type === 'IndexNode'; });
        assertEqual(1, nodes.length, query);
        assertEqual(1, nodes[0].indexes.length, query);
        assertEqual(query[1], nodes[0].indexes[0].type, query);
        assertEqual(query[2], nodes[0].projections, query);
        assertEqual(query[3], nodes[0].indexCoversProjections, query);
        
        if (query[2].length) {
          assertNotEqual(-1, plan.rules.indexOf(ruleName));
        } else {
          assertEqual(-1, plan.rules.indexOf(ruleName));
        }
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
        [`FOR doc IN ${cn} FILTER doc.foo.bar == 93 SORT doc.foo.bat LIMIT 5 RETURN doc`, 'persistent', [], false, [ { _key: "test93", value1: 93, value2: "test93", value3: 93 } ] ],
        [`FOR doc IN ${cn} FILTER doc.foo.bar >= 93 && doc.foo.bar <= 95 SORT doc.foo.bat LIMIT 5 RETURN doc`, 'persistent', [], false, [ { _key: "test93", value1: 93, value2: "test93", value3: 93 }, { _key: "test94", value1: 94, value2: "test94", value3: 94 }, { _key: "test95", value1: 95, value2: "test95", value3: 95 } ] ],
        [`FOR doc IN ${cn} FILTER doc.foo.bar >= 93 && doc.foo.bar <= 95 SORT doc.foo.bat DESC LIMIT 5 RETURN doc`, 'persistent', [], false, [ { _key: "test95", value1: 95, value2: "test95", value3: 95 }, { _key: "test94", value1: 94, value2: "test94", value3: 94 }, { _key: "test93", value1: 93, value2: "test93", value3: 93 } ] ],
        [`FOR doc IN ${cn} FILTER doc.foo.bar >= 93 && doc.foo.bar <= 95 && doc.foo.baz != 23 SORT doc.foo.bat LIMIT 5 RETURN doc`, 'persistent', [], false, [ { _key: "test93", value1: 93, value2: "test93", value3: 93 }, { _key: "test94", value1: 94, value2: "test94", value3: 94 }, { _key: "test95", value1: 95, value2: "test95", value3: 95 } ] ],
        [`FOR doc IN ${cn} FILTER doc.foo.bar >= 93 && doc.foo.bar <= 95 && doc.foo.baz != 23 SORT doc.foo.bat DESC LIMIT 5 RETURN doc`, 'persistent', [], false, [ { _key: "test95", value1: 95, value2: "test95", value3: 95 }, { _key: "test94", value1: 94, value2: "test94", value3: 94 }, { _key: "test93", value1: 93, value2: "test93", value3: 93 } ] ],
      ];

      queries.forEach(function(query) {
        let plan = AQL_EXPLAIN(query[0], null, { optimizer: { rules: ["-optimize-cluster-single-document-operations"] } }).plan;
        let nodes = plan.nodes.filter(function(node) { return node.type === 'IndexNode'; });
        assertEqual(1, nodes.length, query);
        assertEqual(1, nodes[0].indexes.length, query);
        assertEqual(query[1], nodes[0].indexes[0].type, query);
        assertEqual(query[2], nodes[0].projections, query);
        assertEqual(query[3], nodes[0].indexCoversProjections, query);
        
        if (query[2].length) {
          assertNotEqual(-1, plan.rules.indexOf(ruleName));
        } else {
          assertEqual(-1, plan.rules.indexOf(ruleName));
        }
        let results = db._query(query[0], null, { optimizer: { rules: ["-optimize-cluster-single-document-operations"] } }).toArray();
        assertEqual(query[4].length, results.length);
        results = results.map(function(doc) {
          return { _key: doc._key, value1: doc.value1, value2: doc.value2, value3: doc.value3 };
        });
        assertEqual(query[4], results);
      });
    },
    
    testSparseIndex : function () {
      c.ensureIndex({ type: "persistent", fields: ["value1"], sparse: true });
      let queries = [
        [`FOR doc IN ${cn} FILTER doc.value1 == 93 RETURN doc.value1`, 'persistent', ['value1'], true ],
      ];

      queries.forEach(function(query) {
        let plan = AQL_EXPLAIN(query[0], null, { optimizer: { rules: ["-optimize-cluster-single-document-operations"] } }).plan;
        let nodes = plan.nodes.filter(function(node) { return node.type === 'IndexNode'; });
        assertEqual(1, nodes.length, query);
        assertEqual(1, nodes[0].indexes.length, query);
        assertEqual(query[1], nodes[0].indexes[0].type, query);
        assertEqual(query[2], nodes[0].projections, query);
        assertEqual(query[3], nodes[0].indexCoversProjections, query);
        
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
        let plan = AQL_EXPLAIN(query[0]).plan;
        let nodes = plan.nodes.filter(function(node) { return node.type === 'EnumerateCollectionNode'; });
        assertEqual(1, nodes.length, query);
        assertEqual(query[1], nodes[0].projections, query);
        assertNotEqual(-1, plan.rules.indexOf(ruleName));
      });
    },
    
    testSparseIndexSubAttribute : function () {
      c.ensureIndex({ type: "persistent", fields: ["a.b"], sparse: true });
      let queries = [
        [`FOR doc IN ${cn} FILTER doc.a.b == 93 RETURN doc.a.b`, 'persistent', [['a', 'b']], true ],
        [`FOR doc IN ${cn} FILTER doc.a.b == 93 RETURN doc.a`, 'persistent', ['a'], false ],
      ];

      queries.forEach(function(query) {
        let plan = AQL_EXPLAIN(query[0], null, { optimizer: { rules: ["-optimize-cluster-single-document-operations"] } }).plan;
        let nodes = plan.nodes.filter(function(node) { return node.type === 'IndexNode'; });
        assertEqual(1, nodes.length, query);
        assertEqual(1, nodes[0].indexes.length, query);
        assertEqual(query[1], nodes[0].indexes[0].type, query);
        assertEqual(query[2], nodes[0].projections, query);
        assertEqual(query[3], nodes[0].indexCoversProjections, query);
        
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
        let plan = AQL_EXPLAIN(query[0]).plan;
        let nodes = plan.nodes.filter(function(node) { return node.type === 'EnumerateCollectionNode'; });
        assertEqual(1, nodes.length, query);
        assertEqual(query[1], nodes[0].projections, query);
        assertNotEqual(-1, plan.rules.indexOf(ruleName));
      });
    },
    
    testNoIndex : function () {
      let queries = [
        [`FOR doc IN ${cn} RETURN doc.foo`, ['foo'] ],
        [`FOR doc IN ${cn} FILTER doc.foo == 1 RETURN doc.foo`, ['foo'] ],
        [`FOR doc IN ${cn} FILTER doc.foo == 1 RETURN doc.foo.bar`, ['foo'] ],
        [`FOR doc IN ${cn} FILTER doc.moo == 1 RETURN doc.foo`, ['foo', 'moo'] ],
        [`FOR doc IN ${cn} RETURN doc.foo.bar`, [['foo', 'bar']] ],
        [`FOR doc IN ${cn} FILTER doc.foo.bar == 1 RETURN doc.foo.bar`, [['foo', 'bar']] ],
        [`FOR doc IN ${cn} FILTER doc.moo == 1 RETURN doc.foo.bar`, [['foo', 'bar'], 'moo'] ],
        [`FOR doc IN ${cn} FILTER doc.moo == 1 RETURN doc.moo.bar`, ['moo'] ],
        [`FOR doc IN ${cn} RETURN [doc.foo.a, doc.foo.b, doc.moo]`, ['foo', 'moo'] ],
        [`FOR doc IN ${cn} RETURN [doc.a, doc.b, doc.moo]`, ['a', 'b', 'moo'] ],
        [`FOR doc IN ${cn} RETURN [doc.a, doc.b, doc.moo.far]`, ['a', 'b', ['moo', 'far']] ],
      ];

      queries.forEach(function(query) {
        let plan = AQL_EXPLAIN(query[0]).plan;
        let nodes = plan.nodes.filter(function(node) { return node.type === 'EnumerateCollectionNode'; });
        assertEqual(1, nodes.length, query);
        assertEqual(query[1], nodes[0].projections, query);
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
      for (var i = 0; i < 100; ++i) {
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
        [`FOR doc IN ${cn} RETURN doc.value1`, 'persistent', ['value1'], true, one ],
        [`FOR doc IN ${cn} RETURN doc.value2`, 'persistent', ['value2'], true, one ],
        [`FOR doc IN ${cn} FILTER doc.value1 == 93 RETURN doc.value1`, 'persistent', ['value1'], true, two ],
        [`FOR doc IN ${cn} FILTER doc.value2 == 93 RETURN doc.value1`, 'persistent', ['value1', 'value2'], true, two ],
        [`FOR doc IN ${cn} FILTER doc.value1 == 93 RETURN doc.value2`, 'persistent', ['value2'], true, two ],
        [`FOR doc IN ${cn} FILTER doc.value2 == 93 RETURN doc.value2`, 'persistent', ['value2'], true, two ],
        [`FOR doc IN ${cn} FILTER doc._key == 'test93' RETURN doc.value1`, 'primary', ['value1'], false, two ],
        [`FOR doc IN ${cn} FILTER doc._key == 'test93' RETURN doc.value2`, 'primary', ['value2'], false, two ],
      ];

      queries.forEach(function(query) {
        let plan = AQL_EXPLAIN(query[0], null, { optimizer: { rules: ["-optimize-cluster-single-document-operations"] } }).plan;
        let nodes = plan.nodes.filter(function(node) { return node.type === 'IndexNode'; });
        assertEqual(1, nodes.length, query);
        assertEqual(1, nodes[0].indexes.length, query);
        assertEqual(query[1], nodes[0].indexes[0].type, query);
        assertEqual(query[2], nodes[0].projections, query);
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
        [`FOR doc IN ${cn} RETURN [doc.foo.bar.a, doc.foo.bar.b]`, [['foo', 'bar']], true, barAB ],
        [`FOR doc IN ${cn} RETURN [doc.foo.baz, doc.foo.bar.a]`, ['foo'], true, bazBarA ],
      ];

      queries.forEach(function(query) {
        let plan = AQL_EXPLAIN(query[0], null, { optimizer: { rules: ["-optimize-cluster-single-document-operations"] } }).plan;
        let nodes = plan.nodes.filter(function(node) { return node.type === 'IndexNode' || node.type === 'EnumerateCollectionNode'; });
        assertEqual(1, nodes.length, query);
        assertEqual(query[1], nodes[0].projections, query);
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
        [`FOR doc IN ${cn} RETURN [doc.foo.bar.a, doc.foo.baz]`, ['foo'], false, barBaz ],
        [`FOR doc IN ${cn} RETURN [doc.foo.bar.a, doc.foo.bar.b]`, [['foo', 'bar']], true, barBaz ],
      ];

      queries.forEach(function(query) {
        let plan = AQL_EXPLAIN(query[0], null, { optimizer: { rules: ["-optimize-cluster-single-document-operations"] } }).plan;
        let nodes = plan.nodes.filter(function(node) { return node.type === 'IndexNode' || node.type === 'EnumerateCollectionNode'; });
        assertEqual(1, nodes.length, query);
        assertEqual(query[1], nodes[0].projections, query);
        assertEqual(query[2] ? 'IndexNode' : 'EnumerateCollectionNode', nodes[0].type, query);
        assertNotEqual(-1, plan.rules.indexOf(ruleName));
        let results = db._query(query[0], null, { optimizer: { rules: ["-optimize-cluster-single-document-operations"] } }).toArray();
        assertEqual(query[3], results);
      });
    },
  };
}

jsunity.run(projectionsPlansTestSuite);
jsunity.run(projectionsExtractionTestSuite);

return jsunity.done();
