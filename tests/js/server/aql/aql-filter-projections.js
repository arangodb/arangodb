/*jshint globalstrict:false, strict:false, maxlen: 500 */
/*global assertEqual, assertTrue, assertFalse, assertNotEqual, AQL_EXPLAIN */

////////////////////////////////////////////////////////////////////////////////
/// @brief tests for optimizer rules
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

const jsunity = require("jsunity");
const db = require("@arangodb").db;
const ruleName = "reduce-extraction-to-projection";
const cn = "UnitTestsOptimizer";

function filterProjectionsPlansTestSuite () {
  let c = null;

  return {
    setUp : function () {
      db._drop(cn);
      c = db._createEdgeCollection(cn, { numberOfShards: 4 });

      let docs = [];
      for (let i = 0; i < 1000; ++i) {
        docs.push({ _from: "v/test" + i, _to: "v/test" + (i % 10), _key: "test" + i, value1: i, value2: "test" + i, value3: i, foo: { bar: i, baz: i, bat: i } });
      }
      c.insert(docs);
    },

    tearDown : function () {
      db._drop(cn);
    },
    
    testEdge : function () {
      let queries = [
        [`FOR doc IN ${cn} FILTER doc._from == '${cn}/abc' && doc._to == '${cn}/abc' RETURN doc.value`, 'edge', ['_to'], ['value'] ],
        [`FOR doc IN ${cn} FILTER doc._from == '${cn}/abc' && doc._to != '${cn}/abc' RETURN doc.value`, 'edge', ['_to'], ['value'] ],
        [`FOR doc IN ${cn} FILTER doc._from == '${cn}/abc' && doc._to == '${cn}/abc' RETURN [doc.value1, doc.value2]`, 'edge', ['_to'], ['value1', 'value2'] ],
        [`FOR doc IN ${cn} FILTER doc._from == '${cn}/abc' && doc._to != '${cn}/abc' RETURN [doc.value1, doc.value2]`, 'edge', ['_to'], ['value1', 'value2'] ],
        [`FOR doc IN ${cn} FILTER doc._from == '${cn}/abc' && doc._to == NOEVAL('${cn}/abc') RETURN doc.value`, 'edge', ['_to'], ['value'] ],
        [`FOR doc IN ${cn} FILTER doc._from == '${cn}/abc' && doc._to == NOEVAL('${cn}/abc') RETURN [doc.value, doc._to]`, 'edge', ['_to'], ['_to', 'value'] ],
        [`FOR doc IN ${cn} FILTER doc._from == '${cn}/abc' && doc._to == NOEVAL('${cn}/abc') RETURN [doc.value1, doc.value2]`, 'edge', ['_to'], ['value1', 'value2'] ],
      ];

      queries.forEach(function(query) {
        let plan = AQL_EXPLAIN(query[0], null, { optimizer: { rules: ["-optimize-cluster-single-document-operations"] } }).plan;
        let nodes = plan.nodes.filter(function(node) { return node.type === 'IndexNode'; });
        assertEqual(1, nodes.length, query);
        assertEqual(1, nodes[0].indexes.length, query);
        assertEqual(query[1], nodes[0].indexes[0].type, query);
        assertEqual(query[2], nodes[0].filterProjections, query);
        assertEqual(query[3], nodes[0].projections, query);
        assertNotEqual(-1, plan.rules.indexOf(ruleName));
        
        // query must not contain any FILTER nodes
        nodes = plan.nodes.filter(function(node) { return node.type === 'FilterNode'; });
        assertEqual(0, nodes.length);
      });
    },

    testPersistentSingleAttribute : function () {
      // no filter projections will be used for these queries
      c.ensureIndex({ type: "persistent", fields: ["value1"] });

      let queries = [
        [`FOR doc IN ${cn} FILTER doc.value1 == 93 FILTER doc.value2 == 1 RETURN doc.value1`, 'persistent', ['value1', 'value2'] ],
        [`FOR doc IN ${cn} FILTER doc.value1 == 93 FILTER doc.value2 == 1 RETURN [doc.value1, doc.value2]`, 'persistent', ['value1', 'value2'] ],
      ];

      queries.forEach(function(query) {
        let plan = AQL_EXPLAIN(query[0], null, { optimizer: { rules: ["-optimize-cluster-single-document-operations"] } }).plan;
        let nodes = plan.nodes.filter(function(node) { return node.type === 'IndexNode'; });
        assertEqual(1, nodes.length, query);
        assertEqual(1, nodes[0].indexes.length, query);
        assertEqual(query[1], nodes[0].indexes[0].type, query);
        assertEqual(query[2], nodes[0].projections, query);
        assertEqual([], nodes[0].filterProjections, query);
        assertNotEqual(-1, plan.rules.indexOf(ruleName));
        
        // query must not contain any FILTER nodes
        nodes = plan.nodes.filter(function(node) { return node.type === 'FilterNode'; });
        assertEqual(0, nodes.length);
      });
    },

    testPersistentMultiAttribute : function () {
      c.ensureIndex({ type: "persistent", fields: ["value1", "value2"] });
      let queries = [
        [`FOR doc IN ${cn} FILTER doc.value1 == 93 FILTER doc.value2 != 7 RETURN doc.value3`, 'persistent', ['value2'], ['value3'] ],
      ];

      queries.forEach(function(query) {
        let plan = AQL_EXPLAIN(query[0], null, { optimizer: { rules: ["-optimize-cluster-single-document-operations"] } }).plan;
        let nodes = plan.nodes.filter(function(node) { return node.type === 'IndexNode'; });
        assertEqual(1, nodes.length, query);
        assertEqual(1, nodes[0].indexes.length, query);
        assertEqual(query[1], nodes[0].indexes[0].type, query);
        assertEqual(query[2], nodes[0].filterProjections, query);
        assertEqual(query[3], nodes[0].projections, query);
        assertNotEqual(-1, plan.rules.indexOf(ruleName));
       
        // query must not contain any FILTER nodes
        nodes = plan.nodes.filter(function(node) { return node.type === 'FilterNode'; });
        assertEqual(0, nodes.length);
      });
    },
    
    testPersistentSubAttribute : function () {
      // no filter projections will be used for these queries
      c.ensureIndex({ type: "persistent", fields: ["foo.bar"] });
      let queries = [
        [`FOR doc IN ${cn} RETURN doc.foo.bar`, 'persistent', [['foo', 'bar']] ],
        [`FOR doc IN ${cn} RETURN doc.foo.bar.baz`, 'persistent', [['foo', 'bar', 'baz']] ],
        [`FOR doc IN ${cn} RETURN [doc.foo.bar.baz, doc.foo.bar.bat]`, 'persistent', [['foo', 'bar']] ],
        [`FOR doc IN ${cn} RETURN [doc.foo.bar, doc.foo.bar.baz]`, 'persistent', [['foo', 'bar']] ],
        [`FOR doc IN ${cn} FILTER doc.foo.bar == 1 RETURN doc.foo.bar`, 'persistent', [['foo', 'bar']] ],
      ];

      queries.forEach(function(query) {
        let plan = AQL_EXPLAIN(query[0], null, { optimizer: { rules: ["-optimize-cluster-single-document-operations"] } }).plan;
        let nodes = plan.nodes.filter(function(node) { return node.type === 'IndexNode'; });
        assertEqual(1, nodes.length, query);
        assertEqual(1, nodes[0].indexes.length, query);
        assertEqual(query[1], nodes[0].indexes[0].type, query);
        assertEqual(query[2], nodes[0].projections, query);
        assertEqual([], nodes[0].filterProjections, query);
        assertNotEqual(-1, plan.rules.indexOf(ruleName));
      });
    },
    
    testPersistentSubAttributesMulti : function () {
      c.ensureIndex({ type: "persistent", fields: ["foo.bar", "foo.baz"] });
      let queries = [
        [`FOR doc IN ${cn} FILTER doc.foo.bar == 1 FILTER doc.foo.baz != 1 RETURN doc.value`, 'persistent', [['foo', 'baz']], ['value'] ],
      ];

      queries.forEach(function(query) {
        let plan = AQL_EXPLAIN(query[0], null, { optimizer: { rules: ["-optimize-cluster-single-document-operations"] } }).plan;
        let nodes = plan.nodes.filter(function(node) { return node.type === 'IndexNode'; });
        assertEqual(1, nodes.length, query);
        assertEqual(1, nodes[0].indexes.length, query);
        assertEqual(query[1], nodes[0].indexes[0].type, query);
        assertEqual(query[2], nodes[0].filterProjections, query);
        assertEqual(query[3], nodes[0].projections, query);
        assertNotEqual(-1, plan.rules.indexOf(ruleName));
      });
    },
    
    testPersistentStoredValues : function () {
      c.ensureIndex({ type: "persistent", fields: ["value1"], storedValues: ["value2"] });

      let queries = [
        [`FOR doc IN ${cn} FILTER doc.value1 == 93 FILTER doc.value2 == 1 RETURN doc.value1`, 'persistent', ['value1', 'value2'] ],
        [`FOR doc IN ${cn} FILTER doc.value1 == 93 FILTER doc.value2 == 1 RETURN [doc.value1, doc.value2]`, 'persistent', ['value1', 'value2'] ],
      ];

      queries.forEach(function(query) {
        let plan = AQL_EXPLAIN(query[0], null, { optimizer: { rules: ["-optimize-cluster-single-document-operations"] } }).plan;
        let nodes = plan.nodes.filter(function(node) { return node.type === 'IndexNode'; });
        assertEqual(1, nodes.length, query);
        assertEqual(1, nodes[0].indexes.length, query);
        assertEqual(query[1], nodes[0].indexes[0].type, query);
        assertEqual(query[2], nodes[0].projections, query);
        assertEqual([], nodes[0].filterProjections, query);
        assertNotEqual(-1, plan.rules.indexOf(ruleName));
        
        // query must not contain any FILTER nodes
        nodes = plan.nodes.filter(function(node) { return node.type === 'FilterNode'; });
        assertEqual(0, nodes.length);
      });
    },
    
  };
}

jsunity.run(filterProjectionsPlansTestSuite);

return jsunity.done();
