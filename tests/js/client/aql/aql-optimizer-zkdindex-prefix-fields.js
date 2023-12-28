////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2021-2021 ArangoDB GmbH, Cologne, Germany
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
///
/// @author Tobias Gödderz
////////////////////////////////////////////////////////////////////////////////

'use strict';

const jsunity = require("jsunity");
const arangodb = require("@arangodb");
const db = arangodb.db;
const aql = arangodb.aql;
const {assertTrue, assertFalse, assertEqual} = jsunity.jsUnity.assertions;
const _ = require("lodash");
const normalize = require("@arangodb/aql-helper").normalizeProjections;

function optimizerRuleZkd2dIndexTestSuite() {
  const colName = 'UnitTestZkdIndexCollection';
  let col;

  return {

    tearDown: function () {
      db[colName].drop();
    },

    testSimplePrefix: function () {
      col = db._create(colName);
      col.ensureIndex({
        type: 'zkd',
        name: 'zkdIndex',
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
        type: 'zkd',
        name: 'zkdIndex',
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
      const index = col.index("zkdIndex");
      assertTrue(index.estimates);
      assertTrue(index.hasOwnProperty("selectivityEstimate"));
    },

    testMultiPrefix: function () {
      col = db._create(colName);
      col.ensureIndex({
        type: 'zkd',
        name: 'zkdIndex',
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
        type: 'zkd',
        name: 'zkdIndex',
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
        type: 'zkd',
        name: 'zkdIndex',
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
  };
}

const gm = require('@arangodb/enterprise-graph');
const waitForEstimatorSync = require('@arangodb/test-helper').waitForEstimatorSync;

function optimizerRuleZkdTraversal() {
  const database = "MyTestZkdTraversalDB";
  const graph = "mygraph";
  const vertexCollection = "v";
  const edgeCollection = "e";
  const indexName = "myZkdIndex";
  const levelIndexName = "myZkdIndexLevel";

  return {
    setUpAll: function () {
      db._createDatabase(database);
      db._useDatabase(database);

      gm._create(graph,
          [{"collection": edgeCollection, "to": [vertexCollection], "from": [vertexCollection]}],
          [],
          {numberOfShards: 3, isSmart: true});

      db[edgeCollection].ensureIndex({
        type: 'zkd',
        name: indexName,
        fields: ["x", "y"],
        prefixFields: ["_from"],
        fieldValueTypes: 'double',
      });
      db[edgeCollection].ensureIndex({
        type: 'zkd',
        name: levelIndexName,
        fields: ["x", "y", "w"],
        storedValues: ["foo"],
        prefixFields: ["_from"],
        fieldValueTypes: 'double',
      });
      db._query(`
        for i in 0..99
          insert {_key: CONCAT("${vertexCollection}", i)} into ${vertexCollection}
          for j in 1..100
            let x = RAND() * 10
            let y = RAND() * 10
            let w = RAND() * 10
            let from = CONCAT("${vertexCollection}/v", i)
            let to = CONCAT("${vertexCollection}/v", (j+1)%100)
            insert {_from: from, _to: to, x, y, w} into ${edgeCollection}
      `);

      waitForEstimatorSync();
    },

    tearDownAll: function () {
      db._useDatabase("_system");
      db._dropDatabase(database);
    },

    testZkdTraversal: function () {
      const query = `
        for v, e, p in 0..3 outbound "${vertexCollection}/v1" graph "${graph}"
        options {bfs: true, uniqueVertices: "path"}
          filter p.edges[*].x all >= 5 and p.edges[*].y all <= 7
          filter p.edges[2].w >= 5 and p.edges[2].y <= 8 and p.edges[2].foo == "bar"
          return p
      `;
      //db._explain(query);

      const res = db._createStatement(query).explain();
      const traversalNodes = res.plan.nodes.filter(n => n.type === "TraversalNode");

      traversalNodes.forEach(function (node) {
        const baseIndexes = node.indexes.base;
        assertEqual(1, baseIndexes.length);
        assertEqual(baseIndexes[0].name, indexName);

        assertEqual(["2"], Object.keys(node.indexes.levels));
        const levelIndexes = node.indexes.levels["2"];
        assertEqual(1, levelIndexes.length);
        assertEqual(levelIndexes[0].name, levelIndexName);
      });
    },

    testZkdTraversalOnlyOneLevel: function () {
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

jsunity.run(optimizerRuleZkd2dIndexTestSuite);
jsunity.run(optimizerRuleZkdTraversal);

return jsunity.done();
