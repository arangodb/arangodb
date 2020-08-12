/*jshint globalstrict:false, strict:false, maxlen: 500 */
/*global assertEqual, assertTrue, AQL_EXPLAIN */

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

function projectionsTestSuite () {
  let c = null;
  const cn = "UnitTestsOptimizer";

  return {

    setUp : function () {
      db._drop(cn);
      c = db._create(cn, { numberOfShards: 4 });

      let docs = [];
      for (var i = 0; i < 1000; ++i) {
        docs.push({ value1: i, value2: "test" + i, foo: { bar: i } });
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
        assertEqual(query[2],nodes[0].projections, query);
        if (query[2].length) {
          assertTrue(nodes[0].indexCoversProjections, query);
        }
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
        assertEqual(query[2],nodes[0].projections, query);
        assertEqual(query[3], nodes[0].indexCoversProjections, query);
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
        assertEqual(query[1],nodes[0].projections, query);
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
        assertEqual(query[2],nodes[0].projections, query);
        assertEqual(query[3], nodes[0].indexCoversProjections, query);
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
        assertEqual(query[1],nodes[0].projections, query);
      });
    },
    
    testPersistentSubAttribute : function () {
      c.ensureIndex({ type: "persistent", fields: ["foo.bar"] });
      let queries = [
        [`FOR doc IN ${cn} RETURN doc.foo.bar`, 'persistent', [['foo', 'bar']], true ],
        [`FOR doc IN ${cn} RETURN doc.foo.bar.baz`, 'persistent', [['foo', 'bar', 'baz']], true ],
        [`FOR doc IN ${cn} RETURN [doc.foo.bar.baz, doc.foo.bar.bat]`, 'persistent', [['foo', 'bar']], true ],
        [`FOR doc IN ${cn} FILTER doc.foo.bar == 1 RETURN doc.foo.bar`, 'persistent', [['foo', 'bar']], true ],
        [`FOR doc IN ${cn} FILTER doc.foo.bar == 1 RETURN 1`, 'persistent', [], false ],
      ];

      queries.forEach(function(query) {
        let plan = AQL_EXPLAIN(query[0], null, { optimizer: { rules: ["-optimize-cluster-single-document-operations"] } }).plan;
        let nodes = plan.nodes.filter(function(node) { return node.type === 'IndexNode'; });
        assertEqual(1, nodes.length, query);
        assertEqual(1, nodes[0].indexes.length, query);
        assertEqual(query[1], nodes[0].indexes[0].type, query);
        assertEqual(query[2],nodes[0].projections, query);
        assertEqual(query[3], nodes[0].indexCoversProjections, query);
      });
      
      queries = [
        [`FOR doc IN ${cn} RETURN doc.boo`, ['boo'] ],
        [`FOR doc IN ${cn} RETURN doc.foo`, ['foo'] ],
        [`FOR doc IN ${cn} RETURN doc.foo.baz`, [['foo', 'baz']] ],
        [`FOR doc IN ${cn} RETURN doc.boo.far`, [['boo', 'far']] ],
      ];

      queries.forEach(function(query) {
        let plan = AQL_EXPLAIN(query[0]).plan;
        let nodes = plan.nodes.filter(function(node) { return node.type === 'EnumerateCollectionNode'; });
        assertEqual(1, nodes.length, query);
        assertEqual(query[1],nodes[0].projections, query);
      });
    },

  };
}

jsunity.run(projectionsTestSuite);

return jsunity.done();
