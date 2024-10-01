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
/// @author Tobias Gödderz
// //////////////////////////////////////////////////////////////////////////////

'use strict';

const jsunity = require("jsunity");
const arangodb = require("@arangodb");
const db = arangodb.db;
const aql = arangodb.aql;
const {assertTrue, assertFalse, assertEqual, assertNotEqual} = jsunity.jsUnity.assertions;
const _ = require("lodash");
const normalize = require("@arangodb/aql-helper").normalizeProjections;

function optimizerRuleMdi2dIndexTestSuite() {
  const colName = 'UnitTestMdiIndexCollection';
  let col;

  return {
    tearDown: function () {
      db._drop(colName);
    },

    testSimplePrefix: function () {
      col = db._create(colName);
      col.ensureIndex({
        type: 'mdi-prefixed',
        name: 'mdiIndex',
        fields: ['x', 'y'],
        fieldValueTypes: 'double',
        storedValues: ["z"],
        prefixFields: ["stringValue"],
      });

      db._query(aql`
        FOR str IN ["foo", "bar", "baz"]
          FOR i IN 1..100
            INSERT {x: i, y: i, z: i, stringValue: str} INTO ${col}
      `);

      const res = db._query(aql`
        FOR doc IN ${col}
          FILTER doc.x >= 5 && doc.y <= 7 && doc.stringValue == "foo"
          return [doc.z, doc.stringValue]
      `);

      const result = res.toArray();
      assertEqual(result.length, 3);
      assertEqual(_.uniq(result.map(([a, b]) => b)), ["foo"]);
      assertEqual(result.map(([a, b]) => a).sort(), [5, 6, 7]);
    },

    testEstimates: function () {
      col = db._create(colName);
      col.ensureIndex({
        type: 'mdi-prefixed',
        name: 'mdiIndex',
        fields: ['x', 'y'],
        fieldValueTypes: 'double',
        storedValues: ["z"],
        prefixFields: ["stringValue"],
      });

      db._query(aql`
        FOR str IN ["foo", "bar", "baz"]
          FOR i IN 1..2
            INSERT {x: i, y: i, z: i, stringValue: str} INTO ${col}
      `);
      const index = col.index("mdiIndex");
      assertTrue(index.estimates);
      assertTrue(index.hasOwnProperty("selectivityEstimate"));
    },

    testMultiPrefix: function () {
      col = db._create(colName);
      col.ensureIndex({
        type: 'mdi-prefixed',
        name: 'mdiIndex',
        fields: ['x', 'y'],
        fieldValueTypes: 'double',
        storedValues: ["z"],
        prefixFields: ["stringValue", "value"],
      });

      db._query(aql`
        FOR str IN ["foo", "bar", "baz"]
        FOR v IN [1, 19, -2]
          FOR i IN 1..100
            INSERT {x: i, y: i, z: i, stringValue: str, value: v} INTO ${col}
      `);

      const res = db._query(aql`
        FOR doc IN ${col}
          FILTER doc.x >= 5 && doc.y <= 7 && doc.stringValue == "foo" && doc.value == -2
          return [doc.z, doc.stringValue, doc.value]
      `);

      const result = res.toArray();
      assertEqual(result.length, 3);
      assertEqual(_.uniq(result.map(([a, b, c]) => b)), ["foo"]);
      assertEqual(_.uniq(result.map(([a, b, c]) => c)), [-2]);
      assertEqual(result.map(([a, b, c]) => a).sort(), [5, 6, 7]);
    },

    testProjections: function () {
      col = db._create(colName);
      col.ensureIndex({
        type: 'mdi-prefixed',
        name: 'mdiIndex',
        fields: ['x', 'y'],
        fieldValueTypes: 'double',
        storedValues: ["z"],
        prefixFields: ["stringValue", "value"],
      });

      db._query(aql`
        FOR str IN ["foo", "bar", "baz"]
        FOR v IN [1, 19, -2]
          FOR i IN 1..100
            INSERT {x: i, y: i, z: i, stringValue: str, value: v} INTO ${col}
      `);

      const query = aql`
        FOR doc IN ${col}
          FILTER doc.x >= 5 && doc.y <= 7 && doc.stringValue == "foo" && doc.value == -2
          return [doc.z, doc.stringValue, doc.value]
      `;

      const res = db._createStatement({query: query.query, bindVars: query.bindVars}).explain();
      const indexNodes = res.plan.nodes.filter(n => n.type === "IndexNode");
      assertEqual(indexNodes.length, 1);
      const index = indexNodes[0];
      assertTrue(index.indexCoversProjections, true);
      assertEqual(normalize(index.projections), normalize(["z", "stringValue", "value"]));

      const result = db._createStatement({query: query.query, bindVars: query.bindVars}).execute().toArray();
      for (const [a, b, c] of result) {
        assertEqual(b, "foo");
        assertEqual(c, -2);
        assertTrue(5 <= a && a <= 7);
      }
    },

    testNoStoredValues: function () {
      col = db._create(colName);
      col.ensureIndex({
        type: 'mdi-prefixed',
        name: 'mdiIndex',
        fields: ['x', 'y'],
        fieldValueTypes: 'double',
        prefixFields: ["stringValue", "value"],
      });

      db._query(aql`
        FOR str IN ["foo", "bar", "baz"]
        FOR v IN [1, 19, -2]
          FOR i IN 1..100
            INSERT {x: i, y: i, z: i, stringValue: str, value: v} INTO ${col}
      `);

      const query = aql`
        FOR doc IN ${col}
          FILTER doc.x >= 5 && doc.y <= 7 && doc.stringValue == "foo" && doc.value == -2
          return [doc.stringValue, doc.value]
      `;

      const res = db._createStatement({query: query.query, bindVars: query.bindVars}).explain();
      const indexNodes = res.plan.nodes.filter(n => n.type === "IndexNode");
      assertEqual(indexNodes.length, 1);
      const index = indexNodes[0];
      assertTrue(index.indexCoversProjections, true);
      assertEqual(normalize(index.projections), normalize(["stringValue", "value"]));

      const result = db._createStatement({query: query.query, bindVars: query.bindVars}).execute().toArray();
      for (const [b, c] of result) {
        assertEqual(b, "foo");
        assertEqual(c, -2);
      }
    },

    testTruncate: function () {
      let col = db._create(colName);
      col.ensureIndex({
        type: 'mdi-prefixed',
        name: 'mdiIndex',
        fields: ['x', 'y'],
        fieldValueTypes: 'double',
        prefixFields: ["z"]
      });

      db._query(aql`
          FOR i IN 1..100
            INSERT {x: i, y: i, z: 0} INTO ${col}
      `);

      let res = db._query(aql`
        FOR doc in ${col}
          FILTER doc.x > -100
          FILTER doc.z == 0
          RETURN doc
      `).toArray();
      assertEqual(100, res.length);

      col.truncate();

      res = db._query(aql`
        FOR doc in ${col}
          FILTER doc.x > -100
          FILTER doc.z == 0
          RETURN doc
      `).toArray();
      assertEqual(0, res.length);
    },

    testCompareIndex: function () {
      let col = db._create(colName + "4");
      const idx1 = col.ensureIndex({
        type: 'mdi-prefixed',
        prefixFields: ["attr1"],
        fields: ['x', 'y'],
        fieldValueTypes: 'double',
      });

      const idx2 = col.ensureIndex({
        type: 'mdi-prefixed',
        prefixFields: ["attr1"],
        fields: ['x', 'y'],
        fieldValueTypes: 'double',
      });

      const idx3 = col.ensureIndex({
        type: 'mdi-prefixed',
        prefixFields: ["attr1"],
        fields: ['y', 'x'],
        fieldValueTypes: 'double',
      });

      const idx4 = col.ensureIndex({
        type: 'mdi-prefixed',
        prefixFields: ["attr2"],
        fields: ['y', 'x'],
        fieldValueTypes: 'double',
      });

      assertEqual(idx1.id, idx2.id);
      assertNotEqual(idx3.id, idx2.id);
      assertNotEqual(idx4.id, idx2.id);
      assertNotEqual(idx4.id, idx3.id);
      col.drop();
    },
  };
}

const gm = require("@arangodb/general-graph");
const waitForEstimatorSync = require('@arangodb/test-helper').waitForEstimatorSync;

function optimizerRuleMdiTraversal() {
  const database = "MyTestMdiTraversalDB";
  const graph = "mygraph";
  const vertexCollection = "v";
  const edgeCollection = "e";
  const indexName = "myMdiIndex";
  const levelIndexName = "myMdiIndexLevel";
  const sparseIndexName = "myMdiIndexSparse";

  return {
    setUpAll: function () {
      db._createDatabase(database);
      db._useDatabase(database);

      gm._create(graph,
          [{"collection": edgeCollection, "to": [vertexCollection], "from": [vertexCollection]}],
          [],
          {numberOfShards: 2});

      db[edgeCollection].ensureIndex({
        type: 'mdi-prefixed',
        name: indexName,
        fields: ["x", "y"],
        prefixFields: ["_from"],
        fieldValueTypes: 'double',
      });
      db[edgeCollection].ensureIndex({
        type: 'mdi-prefixed',
        name: levelIndexName,
        fields: ["x", "y", "w"],
        storedValues: ["foo"],
        prefixFields: ["_from"],
        fieldValueTypes: 'double',
      });
      db[edgeCollection].ensureIndex({
        type: 'mdi-prefixed',
        name: sparseIndexName,
        fields: ["a", "b"],
        storedValues: ["foo"],
        prefixFields: ["_from"],
        sparse: true,
        fieldValueTypes: 'double',
      });
      db._query(`
        for i in 0..99
          insert {_key: CONCAT("${vertexCollection}", i)} into ${vertexCollection}
          for j in 1..200
            let x = RAND() * 10
            let y = RAND() * 10
            let w = RAND() * 10
            let a = RAND() * 10
            let b = RAND() * 10
            let from = CONCAT("${vertexCollection}/v", i)
            let to = CONCAT("${vertexCollection}/v", (j+1)%100)
            insert {_from: from, _to: to, x, y, w, a, b} into ${edgeCollection}
      `);

      waitForEstimatorSync();
    },

    tearDownAll: function () {
      db._useDatabase("_system");
      db._dropDatabase(database);
    },

    testMdiTraversal: function () {
      const query = `
        for v, e, p in 0..3 outbound "${vertexCollection}/v1" graph "${graph}"
        options {bfs: true, uniqueVertices: "path"}
          filter p.edges[*].x all >= 5 and p.edges[*].y all <= 7
          filter p.edges[2].w >= 5 and p.edges[2].y <= 8 and p.edges[2].foo == "bar"
          return p
      `;

      const res = db._createStatement(query).explain();
      const traversalNodes = res.plan.nodes.filter(n => n.type === "TraversalNode");

      traversalNodes.forEach(function (node) {
        node.indexes.base.forEach(function (idx) {
          assertEqual(idx.name, indexName, node.indexes);
        });

        assertEqual(["2"], Object.keys(node.indexes.levels), node.indexes);
        node.indexes.levels["2"].forEach(function (idx) {
          assertEqual(idx.name, levelIndexName, node.indexes);
        });

      });
    },

    testMdiTraversalSparse: function () {
      const query = `
        for v, e, p in 0..3 outbound "${vertexCollection}/v1" graph "${graph}"
        options {bfs: true, uniqueVertices: "path"}
          filter p.edges[*].a all >= 5 and p.edges[*].b all <= 7 and p.edges[*].b all != NULL
          filter p.edges[2].w >= 5 and p.edges[2].y <= 8 and p.edges[2].y != NULL and p.edges[2].foo == "bar"
          return p
      `;

      const res = db._createStatement(query).explain();
      const traversalNodes = res.plan.nodes.filter(n => n.type === "TraversalNode");
      db._explain(query);
      traversalNodes.forEach(function (node) {
        node.indexes.base.forEach(function (idx) {
          assertEqual(idx.name, sparseIndexName, node.indexes);
        });

        assertEqual(["2"], Object.keys(node.indexes.levels), node.indexes);
        node.indexes.levels["2"].forEach(function (idx) {
          assertEqual(idx.name, sparseIndexName, node.indexes);
        });

      });
    },

    testMdiTraversalOnlyOneLevel: function () {
      const query = `
        for v, e, p in 0..3 outbound "${vertexCollection}/v1" graph "${graph}"
        options {bfs: true, uniqueVertices: "path"}
          filter p.edges[2].x >= 5 and p.edges[2].w >= 5 and p.edges[2].y <= 8 and p.edges[2].foo == "bar"
          return p
      `;

      const res = db._createStatement(query).explain();
      const traversalNodes = res.plan.nodes.filter(n => n.type === "TraversalNode");

      traversalNodes.forEach(function (node) {
        const baseIndexes = node.indexes.base;
        assertEqual(1, baseIndexes.length);
        assertEqual(baseIndexes[0].name, "edge");

        assertEqual(["2"], Object.keys(node.indexes.levels));
        const levelIndexes = node.indexes.levels["2"];
        assertEqual(1, levelIndexes.length);
        assertEqual(levelIndexes[0].name, levelIndexName);
      });
    },

  };
}

jsunity.run(optimizerRuleMdi2dIndexTestSuite);
jsunity.run(optimizerRuleMdiTraversal);

return jsunity.done();
