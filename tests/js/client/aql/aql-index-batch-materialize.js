/*jshint globalstrict:false, strict:false, maxlen: 500 */
/*global fail, assertEqual, assertNotEqual, assertTrue, assertFalse */

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
/// @author Wilfried Goesgens
// //////////////////////////////////////////////////////////////////////////////

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
    const c = db._create(name, {numberOfShards: 3});
    c.ensureIndex({type: "persistent", fields: ["x"], storedValues: ["b", "z"]});
    c.ensureIndex({type: "persistent", fields: ["y"]});
    c.ensureIndex({type: "persistent", fields: ["z", "w"]});
    c.ensureIndex({type: "persistent", fields: ["u", "_key"], unique: true});
    c.ensureIndex({type: "persistent", fields: ["p", "values[*].y"], unique: false});

    return c;
  }

  function fillCollection(c, n) {
    let docs = [];
    for (let i = 0; i < n; i++) {
      docs.push({x: i, y: 2 * i, z: 2 * i + 1, w: i % 10, u: i, b: i + 1, p: i, q: i, r: i});
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

  function checkResult(query, resultIsSorted = false, options = {}) {
    let expected = db._createStatement({query, options: { optimizer: {rules: [`-${batchMaterializeRule}`]}}}).execute().toArray();
    let actual = db._createStatement({query, options}).execute().toArray();

    if (!resultIsSorted) {
      expected = _.sortBy(expected, ['_key']);
      actual = _.sortBy(expected, ['_key']);
    }

    assertEqual(actual, expected);
  }

  function expectOptimization(query, options = {}) {
    try {
      const plan = db._createStatement({query, options}).explain().plan;
      assertNotEqual(plan.rules.indexOf(batchMaterializeRule), -1);
      const nodes = plan.nodes.map(n => n.type);

      assertNotEqual(nodes.indexOf("MaterializeNode"), -1);
      assertNotEqual(nodes.indexOf("IndexNode"), -1);

      const indexNode = plan.nodes[nodes.indexOf("IndexNode")];
      assertEqual(indexNode.strategy, "late materialized");

      const materializeNode = plan.nodes[nodes.indexOf("MaterializeNode")];
      assertEqual(indexNode.outNmDocId.id, materializeNode.inNmDocId.id);

      checkResult(query, false, options);
      return {plan, nodes, indexNode, materializeNode};
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
      fillCollection(c, 5000);
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
    
    testMaterializeIndexScanSkip: function () {
      const query = `
        FOR doc IN ${collection}
          FILTER doc.x > 5
          SORT doc.x 
          LIMIT 20, 10
          RETURN doc
      `;

      expectOptimization(query);
    },
    
    testMaterializeIndexScanSkip2: function () {
      const query = `
        FOR doc IN ${collection}
          FILTER doc.p > 5
          LIMIT 20, 10
          RETURN doc
      `;

      expectOptimization(query);
    },
    
    testMaterializeIndexScanSkip3: function () {
      const query = `
        FOR doc IN ${collection}
          FILTER doc.p > 5
          FILTER NOOPT(doc.q > 0)
          LIMIT 20, 10
          RETURN doc
      `;

      expectOptimization(query);
    },

    testMaterializeIndexScanFullCount: function () {
      const query = `
        FOR doc IN ${collection}
          FILTER doc.x > 5
          SORT doc.x 
          LIMIT 0, 20
          RETURN doc
      `;

      expectOptimization(query, {fullCount: true});
    },
    
    testMaterializeIndexScanFullCount2: function () {
      const query = `
        FOR doc IN ${collection}
          FILTER doc.p > 5
          LIMIT 0, 20
          RETURN doc
      `;

      expectOptimization(query, {fullCount: true});
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

    testMaterializeSortStoredValues: function () {
      const query = `
        FOR doc IN ${collection}
          FILTER doc.x > 5
          SORT doc.b
          RETURN doc
      `;
      const {materializeNode, indexNode, nodes} = expectOptimization(query);
      assertEqual(normalize(indexNode.projections), [["b"]]);
      assertEqual(normalize(materializeNode.projections), []);

      // expect `MATERIALIZE` to be after `SORT`
      assertNotEqual(nodes.indexOf('SortNode'), -1);
      assertNotEqual(nodes.indexOf('MaterializeNode'), -1);
      assertTrue(nodes.indexOf('SortNode') < nodes.indexOf('MaterializeNode'));
    },

    testMaterializeFilterStoredValues: function () {
      const query = `
        FOR doc IN ${collection}
          FILTER doc.x > 5
          FILTER NOOPT(doc.b > 7)
          RETURN doc
      `;
      const {materializeNode, indexNode, nodes} = expectOptimization(query);
      assertEqual(normalize(indexNode.projections), [["b"]]);
      assertEqual(normalize(materializeNode.projections), []);

      // expect `MATERIALIZE` to be after `FILTER`
      assertNotEqual(nodes.indexOf('FilterNode'), -1);
      assertNotEqual(nodes.indexOf('MaterializeNode'), -1);
      assertTrue(nodes.indexOf('FilterNode') < nodes.indexOf('MaterializeNode'));
    },

    testMaterializeDoubleFilterStoredValues: function () {
      const query = `
        FOR d1 IN ${collection}
          FILTER d1.x > 5
          SORT d1.b
          LIMIT 100
          FILTER d1.z < 18
          RETURN d1.w`;
      const {materializeNode, indexNode, nodes} = expectOptimization(query);
      assertEqual(normalize(indexNode.projections), [["b"], ["z"]]);
      assertEqual(normalize(materializeNode.projections), [["w"]]);
      assertNotEqual(nodes.indexOf('SortNode'), -1);
      assertNotEqual(nodes.indexOf('MaterializeNode'), -1);
      assertTrue(nodes.indexOf('SortNode') < nodes.indexOf('MaterializeNode'));
      if (!internal.isCluster()) {
        assertNotEqual(nodes.indexOf('FilterNode'), -1);
        assertTrue(nodes.indexOf('FilterNode') < nodes.indexOf('MaterializeNode'));
      }
    },

    testMaterializeIndexScanSubquery: function() {
      const query = `
        FOR d1 IN ${collection} 
          FILTER d1.x > 5
          LET e = SUM(FOR c IN ${collection} LET p = d1.b + c.x LIMIT 10 RETURN p)
          SORT e
          LIMIT 10
          RETURN d1
      `;

      const {materializeNode, indexNode, nodes} = expectOptimization(query);
      if (!internal.isCluster()) {
        assertEqual(normalize(indexNode.projections), [["b"]]);
        assertEqual(normalize(materializeNode.projections), []);
        assertNotEqual(nodes.indexOf('SortNode'), -1);
        assertNotEqual(nodes.indexOf('LimitNode'), -1);
        assertNotEqual(nodes.indexOf('MaterializeNode'), -1);
        assertNotEqual(nodes.indexOf('SubqueryEndNode'), -1);
        assertTrue(nodes.indexOf('LimitNode') < nodes.indexOf('MaterializeNode'));
        assertTrue(nodes.indexOf('SortNode') < nodes.indexOf('MaterializeNode'));
        assertTrue(nodes.indexOf('SubqueryEndNode') < nodes.indexOf('MaterializeNode'));
      } else {
        // Subqueries Start nodes are always on the coordinator, thus the materialize node will stay
        // right below the IndexNode.
        assertEqual(normalize(indexNode.projections), []);
        assertEqual(normalize(materializeNode.projections), []);
      }
    },

    testMaterializeIndexScanSubqueryFullDoc: function() {
      const query = `
        FOR d1 IN ${collection} 
          FILTER d1.x > 5
          LET e = SUM(FOR c IN ${collection} LET p = d1 LIMIT 10 RETURN p)
          SORT e
          LIMIT 10
          RETURN d1
      `;

      const {materializeNode, indexNode, nodes} = expectOptimization(query);
      assertEqual(normalize(indexNode.projections), []);
      assertEqual(normalize(materializeNode.projections), []);
      assertNotEqual(nodes.indexOf('SortNode'), -1);
      assertNotEqual(nodes.indexOf('LimitNode'), -1);
      assertNotEqual(nodes.indexOf('MaterializeNode'), -1);
      assertNotEqual(nodes.indexOf('SubqueryEndNode'), -1);
      assertTrue(nodes.indexOf('LimitNode') > nodes.indexOf('MaterializeNode'));
      assertTrue(nodes.indexOf('SortNode') > nodes.indexOf('MaterializeNode'));
      assertTrue(nodes.indexOf('SubqueryEndNode') > nodes.indexOf('MaterializeNode'));
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
    
    testMaterializeSkipIndexScanPostFilterCovered: function () {
      const query = `
        FOR doc IN ${collection}
          FILTER doc.x > 5 AND doc.b < 7
          LIMIT 10, 20
          RETURN doc
      `;

      expectOptimization(query);
    },

    testMaterializeSkipIndexScanPostFilterNotCovered: function () {
      const query = `
        FOR doc IN ${collection}
          FILTER doc.x > 5 AND doc.c < 7
          LIMIT 10, 20
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
    
    testMaterializeIndexScanNoProjectionOptimization: function () {
      const query = `
        FOR doc IN ${collection}
          FILTER doc.x > 5
          RETURN doc.r
      `;

      expectOptimization(query, {optimizer: {rules: ["-optimize-projections"] } });

      let results = db._query(query).toArray();
      results = _.sortBy(results); 
      for (let i = 0; i < results.length; ++i) {
        assertEqual(results[i], i + 6);
      }
    },
  };
}

jsunity.run(IndexBatchMaterializeTestSuite);
return jsunity.done();
