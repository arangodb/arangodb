/*jshint globalstrict:false, strict:false, maxlen: 500 */
/*global assertEqual, assertNotEqual, AQL_EXPLAIN */

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
        [`FOR doc IN ${cn} FILTER doc._from == '${cn}/abc' && doc._to != '${cn}/abc' RETURN doc`, 'edge', ['_to'], [] ],
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
        if (nodes[0].projections.length > 0) {
          assertNotEqual(-1, plan.rules.indexOf(ruleName));
        }
        
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
        [`FOR doc IN ${cn} FILTER doc.value1 == 93 FILTER doc.value2 == 1 RETURN doc`, 'persistent', [] ],
      ];

      queries.forEach(function(query) {
        let plan = AQL_EXPLAIN(query[0], null, { optimizer: { rules: ["-optimize-cluster-single-document-operations"] } }).plan;
        let nodes = plan.nodes.filter(function(node) { return node.type === 'IndexNode'; });
        assertEqual(1, nodes.length, query);
        assertEqual(1, nodes[0].indexes.length, query);
        assertEqual(query[1], nodes[0].indexes[0].type, query);
        assertEqual(query[2], nodes[0].projections, query);
        assertEqual([], nodes[0].filterProjections, query);
        if (nodes[0].projections.length > 0) {
          assertNotEqual(-1, plan.rules.indexOf(ruleName));
        }
        
        // query must not contain any FILTER nodes
        nodes = plan.nodes.filter(function(node) { return node.type === 'FilterNode'; });
        assertEqual(0, nodes.length);
      });
    },

    testPersistentMultiAttribute : function () {
      c.ensureIndex({ type: "persistent", fields: ["value1", "value2"] });
      let queries = [
        [`FOR doc IN ${cn} FILTER doc.value1 == 93 FILTER doc.value2 != 7 RETURN doc.value3`, 'persistent', ['value2'], ['value3'] ],
        [`FOR doc IN ${cn} FILTER doc.value1 == 93 FILTER doc.value2 != 7 RETURN doc`, 'persistent', ['value2'], [] ],
      ];

      queries.forEach(function(query) {
        let plan = AQL_EXPLAIN(query[0], null, { optimizer: { rules: ["-optimize-cluster-single-document-operations"] } }).plan;
        let nodes = plan.nodes.filter(function(node) { return node.type === 'IndexNode'; });
        assertEqual(1, nodes.length, query);
        assertEqual(1, nodes[0].indexes.length, query);
        assertEqual(query[1], nodes[0].indexes[0].type, query);
        assertEqual(query[2], nodes[0].filterProjections, query);
        assertEqual(query[3], nodes[0].projections, query);
        if (nodes[0].projections.length > 0) {
          assertNotEqual(-1, plan.rules.indexOf(ruleName));
        }
       
        // query must not contain any FILTER nodes
        nodes = plan.nodes.filter(function(node) { return node.type === 'FilterNode'; });
        assertEqual(0, nodes.length);
      });
    },
    
    testPersistentSubAttribute : function () {
      // no filter projections will be used for these queries
      c.ensureIndex({ type: "persistent", fields: ["foo.bar"] });
      let queries = [
        [`FOR doc IN ${cn} FILTER doc.foo.bar == 1 RETURN doc.foo.bar`, 'persistent', [['foo', 'bar']] ],
        [`FOR doc IN ${cn} FILTER doc.foo.bar == 1 RETURN doc.value`, 'persistent', ['value'] ],
        [`FOR doc IN ${cn} FILTER doc.foo.bar == 1 RETURN doc`, 'persistent', [] ],
      ];

      queries.forEach(function(query) {
        let plan = AQL_EXPLAIN(query[0], null, { optimizer: { rules: ["-optimize-cluster-single-document-operations"] } }).plan;
        let nodes = plan.nodes.filter(function(node) { return node.type === 'IndexNode'; });
        assertEqual(1, nodes.length, query);
        assertEqual(1, nodes[0].indexes.length, query);
        assertEqual(query[1], nodes[0].indexes[0].type, query);
        assertEqual(query[2], nodes[0].projections, query);
        assertEqual([], nodes[0].filterProjections, query);
        if (nodes[0].projections.length > 0) {
          assertNotEqual(-1, plan.rules.indexOf(ruleName));
        }
      });
    },
    
    testPersistentSubAttributesMulti : function () {
      c.ensureIndex({ type: "persistent", fields: ["foo.bar", "foo.baz"] });
      let queries = [
        [`FOR doc IN ${cn} FILTER doc.foo.bar == 1 FILTER doc.foo.baz != 1 RETURN doc.value`, 'persistent', [['foo', 'baz']], ['value'] ],
        [`FOR doc IN ${cn} FILTER doc.foo.bar == 1 FILTER doc.foo.baz != 1 RETURN [doc.foo.bar, doc.value]`, 'persistent', [['foo', 'baz']], [['foo', 'bar'], 'value'] ],
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
        [`FOR doc IN ${cn} FILTER doc.value1 == 93 FILTER doc.value2 == 1 RETURN doc.value3`, 'persistent', ['value2'], ['value3'] ],
        [`FOR doc IN ${cn} FILTER doc.value1 == 93 FILTER doc.value2 == 1 RETURN [doc.value3, doc.value4]`, 'persistent', ['value2'], ['value3', 'value4'] ],
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
    
    testPersistentStoredValuesSubAttributesMulti : function () {
      c.ensureIndex({ type: "persistent", fields: ["foo.bar"], storedValues: ["moo"] });
      let queries = [
        [`FOR doc IN ${cn} FILTER doc.foo.bar == 1 FILTER doc.moo.baz != 1 FILTER doc.moo.qux == 2 RETURN doc.value`, 'persistent', ['moo'], ['value'] ],
        [`FOR doc IN ${cn} FILTER doc.foo.bar == 1 FILTER doc.moo.baz != 1 FILTER doc.moo.qux != 3 RETURN [doc.foo.bar, doc.value]`, 'persistent', ['moo'], [['foo', 'bar'], 'value'] ],
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
    
    testPersistentSubAttributesMixBetweenIndexFieldsAndStoredValues : function () {
      c.ensureIndex({ type: "persistent", fields: ["foo.bar"], storedValues: ["foo.bark"] });
      
      let queries = [
        [`FOR doc IN ${cn} FILTER doc.foo.bar == 1 FILTER doc.foo.bark != 1 RETURN doc.value`, 'persistent', [['foo', 'bark']], ['value'] ],
        [`FOR doc IN ${cn} FILTER doc.foo.bar == 1 FILTER doc.foo.bark != 1 RETURN [doc.foo.bar, doc.value]`, 'persistent', [['foo', 'bark']], [['foo', 'bar'], 'value'] ],
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
    
  };
}

function filterProjectionsResultsTestSuite () {
  let c = null;

  return {
    setUp : function () {
      db._drop(cn);
      c = db._create(cn, { numberOfShards: 4 });

      let docs = [];
      for (let i = 0; i < 10000; ++i) {
        docs.push({ _key: "test" + i, value1: (i % 100), value2: "test" + i, value3: "test" + i, value4: i });
      }
      c.insert(docs);
    },

    tearDown : function () {
      db._drop(cn);
    },
    
    testPersistentMultiAttributeResults : function () {
      c.ensureIndex({ type: "persistent", fields: ["value1", "value2", "value3"] });
        
      let queries = [
        [ `FOR doc IN ${cn} FILTER doc.value1 == 93 FILTER doc.value3 == 'test93' RETURN doc.value4`, [ 93 ] ],
        [ `FOR doc IN ${cn} FILTER doc.value1 == 93 FILTER doc.value3 == 'test93' RETURN [doc.value2, doc.value4]`, [ [ 'test93', 93 ] ] ],
      ];

      queries.forEach(function(query) {
        let plan = AQL_EXPLAIN(query[0], null, { optimizer: { rules: ["-optimize-cluster-single-document-operations"] } }).plan;
        let nodes = plan.nodes.filter(function(node) { return node.type === 'IndexNode'; });
        assertEqual(1, nodes.length, query);
        assertEqual(1, nodes[0].indexes.length, query);
        assertEqual("persistent", nodes[0].indexes[0].type, query);
        assertNotEqual([], nodes[0].filterProjections, query);
        assertNotEqual(-1, plan.rules.indexOf(ruleName));
        
        // query must not contain any FILTER nodes
        nodes = plan.nodes.filter(function(node) { return node.type === 'FilterNode'; });
        assertEqual(0, nodes.length);

        let res = db._query(query[0]).toArray();
        assertEqual(query[1], res, query);
      });
    },
    
    testPersistentStoredValuesResults : function () {
      c.ensureIndex({ type: "persistent", fields: ["value1"], storedValues: ["value2"] });
        
      let queries = [
        [ `FOR doc IN ${cn} FILTER doc.value1 == 93 FILTER doc.value2 == 'test93' RETURN doc.value4`, [ 93 ] ],
        [ `FOR doc IN ${cn} FILTER doc.value1 == 93 FILTER doc.value2 == 'test93' RETURN [doc.value2, doc.value4]`, [ [ 'test93', 93 ] ] ],
      ];

      queries.forEach(function(query) {
        let plan = AQL_EXPLAIN(query[0], null, { optimizer: { rules: ["-optimize-cluster-single-document-operations"] } }).plan;
        let nodes = plan.nodes.filter(function(node) { return node.type === 'IndexNode'; });
        assertEqual(1, nodes.length, query);
        assertEqual(1, nodes[0].indexes.length, query);
        assertEqual("persistent", nodes[0].indexes[0].type, query);
        assertNotEqual([], nodes[0].filterProjections, query);
        assertNotEqual(-1, plan.rules.indexOf(ruleName));
        
        // query must not contain any FILTER nodes
        nodes = plan.nodes.filter(function(node) { return node.type === 'FilterNode'; });
        assertEqual(0, nodes.length);

        let res = db._query(query[0]).toArray();
        assertEqual(query[1], res, query);
      });
    },

  };
}

jsunity.run(filterProjectionsPlansTestSuite);
jsunity.run(filterProjectionsResultsTestSuite);

return jsunity.done();
