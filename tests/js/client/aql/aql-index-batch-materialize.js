/*jshint globalstrict:false, strict:false, maxlen: 500 */
/*global fail, assertEqual, assertNotEqual, assertTrue, assertFalse */

////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2010-2016 ArangoDB GmbH, Cologne, Germany
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
////////////////////////////////////////////////////////////////////////////////

const jsunity = require("jsunity");
const db = require("@arangodb").db;
const normalize = require("@arangodb/aql-helper").normalizeProjections;
const internal = require('internal');
const _ = require('lodash');

const database = "BatchMaterializeIndexDB";
const collection = "MyTestCollection";
const emptyCollection = "MyTestCollectionEmpty";
const smallCollection = "MyTestCollectionSmall";

const batchMaterializeRule = "batch-materialize-documents";

function IndexBatchMaterializeTestSuite() {

  function makeCollection(name) {
    const c = db._create(name);
    c.ensureIndex({type: "persistent", fields: ["x"], storedValues: ["b"]});
    c.ensureIndex({type: "persistent", fields: ["y"]});
    c.ensureIndex({type: "persistent", fields: ["z", "w"]});
    c.ensureIndex({type: "persistent", fields: ["u"], unique: true});
    return c;
  }

  function fillCollection(c, n) {
    let docs = [];
    for (let i = 0; i < n; i++) {
      docs.push({x: i, y: 2 * i, z: 2 * i + 1, w: i % 10, u: i, b: i + 1});
    }
    c.insert(docs);
  }

  function expectNoOptimization(query) {
    try {
      const plan = db._createStatement({query}).explain().plan;
      assertEqual(plan.rules.indexOf(batchMaterializeRule), -1);
      const nodes = plan.nodes.map(n => n.type);

      assertEqual(nodes.indexOf("MaterializeNode"), -1);
      assertNotEqual(nodes.indexOf("IndexNode"), -1);

      const indexNode = plan.nodes[nodes.indexOf("IndexNode")];
      assertNotEqual(indexNode.strategy, "late materialized");

      return plan;
    } catch (e) {
      db._explain(query);
      throw e;
    }
  }

  function checkResult(query) {
    const expected = db._createStatement({query, optimizer: {rules: [`-${batchMaterializeRule}`]}}).execute().toArray();

    const actual = db._createStatement({query}).execute().toArray();

    assertEqual(actual, expected);
  }

  function expectOptimization(query) {
    try {
      const plan = db._createStatement({query}).explain().plan;
      assertNotEqual(plan.rules.indexOf(batchMaterializeRule), -1);
      const nodes = plan.nodes.map(n => n.type);

      assertNotEqual(nodes.indexOf("MaterializeNode"), -1);
      assertNotEqual(nodes.indexOf("IndexNode"), -1);

      const indexNode = plan.nodes[nodes.indexOf("IndexNode")];
      assertEqual(indexNode.strategy, "late materialized");

      const materializeNode = plan.nodes[nodes.indexOf("MaterializeNode")];
      assertEqual(indexNode.outNmDocId.id, materializeNode.inNmDocId.id);

      checkResult(query);
      return {plan, indexNode, materializeNode};
    } catch (e) {
      db._explain(query);
      throw e;
    }
  }

  return {
    setUpAll: function () {
      db._createDatabase(database);
      db._useDatabase(database);
      makeCollection(emptyCollection);
      const s = makeCollection(smallCollection);
      const c = makeCollection(collection);

      fillCollection(s, 10);
      fillCollection(c, 1000);
    },

    tearDownAll: function () {
      db._useDatabase("_system");
      db._dropDatabase(database);
    },

    testNoMaterializeForSmallCollections: function () {
      const query = `
        FOR doc IN ${smallCollection}
          FILTER doc.x > 5
          RETURN doc
      `;

      expectNoOptimization(query);
    },

    testNoMaterializeSmallUniqueIndex: function () {
      const query = `
      FOR i IN 1..10
        FOR doc IN ${collection}
          FILTER doc.x == i
          RETURN doc
      `;

      expectNoOptimization(query);
    },


    testMaterializeUniqueIndex: function () {
      const query = `
      FOR i IN 1..120
        FOR doc IN ${collection}
          FILTER doc.x == i
          RETURN doc
      `;

      expectOptimization(query);
    },

    testMaterializeUniqueIndexSmallRange: function () {
      const query = `
      FOR i IN 1..10
        FOR doc IN ${collection}
          FILTER doc.x == i
          RETURN doc
      `;

      expectNoOptimization(query);
    },

    testMaterializeIndexScan: function () {
      const query = `
        FOR doc IN ${collection}
          FILTER doc.x > 5
          RETURN doc
      `;

      expectOptimization(query);
    },

    testMaterializeMultiIndexScanSameIndex: function () {
      const query = `
        FOR doc IN ${collection}
          FILTER doc.x > 5 or doc.x < 8
          RETURN doc
      `;
      expectOptimization(query);
    },

    testMaterializeMultiIndexScanMultiIndex: function () {
      const query = `
        FOR doc IN ${collection}
          FILTER doc.x > 5 or doc.y < 8
          RETURN doc
      `;
      expectNoOptimization(query);
    },

    testMaterializeIndexScanProjections: function () {
      const query = `
        FOR doc IN ${collection}
          FILTER doc.x > 5
          RETURN [doc.y, doc.z, doc.a]
      `;
      const {materializeNode, indexNode} = expectOptimization(query);
      assertEqual(normalize(indexNode.projections), []);
      assertEqual(normalize(materializeNode.projections), [["a"], ["y"], ["z"]]);
    },

    testMaterializeIndexScanCoveringProjections: function () {
      const query = `
        FOR doc IN ${collection}
          FILTER doc.x > 5
          RETURN [doc.x, doc.b]
      `;
      expectNoOptimization(query);
    },

    testMaterializeIndexScanPostFilterCovered: function () {
      const query = `
        FOR doc IN ${collection}
          FILTER doc.x > 5 AND doc.b < 7
          RETURN doc
      `;

      expectOptimization(query);
    },

    testMaterializeIndexScanPostFilterNotCovered: function () {
      const query = `
        FOR doc IN ${collection}
          FILTER doc.x > 5 AND doc.c < 7
          RETURN doc
      `;

      expectNoOptimization(query);
    },

    testMaterializeIndexScanPostFilterDependentVar: function () {
      const query = `
        FOR i IN 1..5
          FOR doc IN ${collection}
            FILTER doc.x > 5 AND doc.b < i
            RETURN doc
      `;

      const {indexNode} = expectOptimization(query);
      assertTrue(indexNode.indexCoversFilterProjections);
      assertEqual(normalize(indexNode.filterProjections), [["b"]]);
    },
  };
}

jsunity.run(IndexBatchMaterializeTestSuite);
return jsunity.done();
