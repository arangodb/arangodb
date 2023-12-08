/*jshint globalstrict:false, strict:false, maxlen: 500 */
/*global assertEqual, assertNotEqual, assertTrue, assertFalse, assertNull, assertMatch, fail */

////////////////////////////////////////////////////////////////////////////////
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

const internal = require("internal");
const db = require("@arangodb").db;
const jsunity = require("jsunity");
const errors = require("internal").errors;
const isCluster = require("internal").isCluster();
const isEnterprise = require("internal").isEnterprise();
const cn = "UnitTestsCollection";

function readOwnWritesSuite() {
  const fillCollection = () => {
    const docs = [];
    for (let i = 1; i <= 2001; ++i) {
      docs.push({ _key: "test" + i, value: i });
    }
    db[cn].insert(docs);
  };
  
  return {
    setUp: function () {
      let c = db._create(cn, { numberOfShards: 1 });
      c.ensureIndex({ type: "persistent", fields: ["value"] });
    },

    tearDown: function () {
      db._drop(cn);
    },

    testReadOwnWritesNotSpecified: function () {
      fillCollection();

      const q = `FOR doc IN ${cn} OPTIONS { disableIndex: true } UPSERT FILTER $CURRENT.value == doc.value INSERT { _key: 'nono' } UPDATE { updated: true } IN ${cn}`;
      const explain = db._createStatement(q).explain();
      if (!isCluster || isEnterprise) {
        assertEqual([], explain.warnings);
      } else {
        assertEqual(1, explain.warnings.length);
        assertEqual(errors.ERROR_QUERY_INVALID_OPTIONS_ATTRIBUTE.code, explain.warnings[0].code);
      }
      const plan = explain.plan;

      // default value for readOwnWrites is true
      let nodes = plan.nodes.filter((n) => n.type === 'UpsertNode');
      assertEqual(1, nodes.length);
      assertTrue(nodes[0].readOwnWrites);
      
      nodes = plan.nodes.filter((n) => n.type === 'IndexNode');
      assertEqual(1, nodes.length);
      assertTrue(nodes[0].readOwnWrites);

      let res = db._query(q);
      assertEqual(2001, res.getExtra().stats.writesExecuted);

      res = db._query(`FOR doc IN ${cn} FILTER doc.updated == true RETURN 1`).toArray();
      assertEqual(2001, res.length);

      assertFalse(db[cn].exists('nono'));
    },
    
    testReadOwnWritesSetToTrue: function () {
      fillCollection();

      const q = `FOR doc IN ${cn} OPTIONS { disableIndex: true } UPSERT FILTER $CURRENT.value == doc.value INSERT { _key: 'nono' } UPDATE { updated: true } IN ${cn} OPTIONS { readOwnWrites: true }`;
      const explain = db._createStatement(q).explain();
      if (!isCluster || isEnterprise) {
        assertEqual([], explain.warnings);
      } else {
        assertEqual(1, explain.warnings.length);
        assertEqual(errors.ERROR_QUERY_INVALID_OPTIONS_ATTRIBUTE.code, explain.warnings[0].code);
      }
      const plan = explain.plan;

      let nodes = plan.nodes.filter((n) => n.type === 'UpsertNode');
      assertEqual(1, nodes.length);
      assertTrue(nodes[0].readOwnWrites);
      
      nodes = plan.nodes.filter((n) => n.type === 'IndexNode');
      assertEqual(1, nodes.length);
      assertTrue(nodes[0].readOwnWrites);

      let res = db._query(q);
      assertEqual(2001, res.getExtra().stats.writesExecuted);
      if (!isCluster || isEnterprise) {
        assertEqual([], explain.warnings);
      } else {
        assertEqual(1, explain.warnings.length);
        assertEqual(errors.ERROR_QUERY_INVALID_OPTIONS_ATTRIBUTE.code, explain.warnings[0].code);
      }

      res = db._query(`FOR doc IN ${cn} FILTER doc.updated == true RETURN 1`).toArray();
      assertEqual(2001, res.length);

      assertFalse(db[cn].exists('nono'));
    },
    
    testReadOwnWritesSetToFalse: function () {
      fillCollection();

      const q = `FOR doc IN ${cn} OPTIONS { disableIndex: true } UPSERT FILTER $CURRENT.value == doc.value INSERT { _key: 'nono' } UPDATE { updated: true } IN ${cn} OPTIONS { readOwnWrites: false }`;
      const explain = db._createStatement(q).explain();
      assertEqual([], explain.warnings);
      const plan = explain.plan;

      let nodes = plan.nodes.filter((n) => n.type === 'UpsertNode');
      assertEqual(1, nodes.length);
      assertFalse(nodes[0].readOwnWrites);
      
      nodes = plan.nodes.filter((n) => n.type === 'IndexNode');
      assertEqual(1, nodes.length);
      assertFalse(nodes[0].readOwnWrites);

      let res = db._query(q);
      assertEqual(2001, res.getExtra().stats.writesExecuted);
      assertEqual([], res.getExtra().warnings);

      res = db._query(`FOR doc IN ${cn} FILTER doc.updated == true RETURN 1`).toArray();
      assertEqual(2001, res.length);

      assertFalse(db[cn].exists('nono'));
    },
    
    testReadOwnWritesTrueOverlappingInputs: function () {
      if (isCluster && !isEnterprise) {
        // must bail out here because we need the cluster's OneShard optimization
        // to achieve deterministic behavior
        return;
      }
      const q = `FOR i IN 1..1000 UPSERT FILTER $CURRENT.value == 1 INSERT { inserted: true, value: 1 } UPDATE { updated: true } IN ${cn} OPTIONS { readOwnWrites: true }`;
      const explain = db._createStatement(q).explain();
      assertEqual([], explain.warnings);
      const plan = explain.plan;

      let nodes = plan.nodes.filter((n) => n.type === 'UpsertNode');
      assertEqual(1, nodes.length);
      assertTrue(nodes[0].readOwnWrites);
      
      nodes = plan.nodes.filter((n) => n.type === 'IndexNode');
      assertEqual(1, nodes.length);
      assertTrue(nodes[0].readOwnWrites);

      let res = db._query(q);
      assertEqual(1000, res.getExtra().stats.writesExecuted);
      assertEqual([], res.getExtra().warnings);

      res = db._query(`FOR doc IN ${cn} FILTER doc.updated == true RETURN 1`).toArray();
      assertEqual(1, res.length);
      
      res = db._query(`FOR doc IN ${cn} FILTER doc.inserted == true RETURN 1`).toArray();
      assertEqual(1, res.length);
    },
    
    testReadOwnWritesFalseOverlappingInputs: function () {
      // the following query will only trigger the insert case,
      // as the readOwnWrites: false will lead to our own inserts
      // not being observed
      const q = `FOR i IN 1..1000 UPSERT FILTER $CURRENT.value == 1 INSERT { inserted: true, value: 1 } UPDATE { updated: true } IN ${cn} OPTIONS { readOwnWrites: false }`;
      const explain = db._createStatement(q).explain();
      assertEqual([], explain.warnings);
      const plan = explain.plan;

      let nodes = plan.nodes.filter((n) => n.type === 'UpsertNode');
      assertEqual(1, nodes.length);
      assertFalse(nodes[0].readOwnWrites);
      
      nodes = plan.nodes.filter((n) => n.type === 'IndexNode');
      assertEqual(1, nodes.length);
      assertFalse(nodes[0].readOwnWrites);

      let res = db._query(q);
      assertEqual(1000, res.getExtra().stats.writesExecuted);
      assertEqual([], res.getExtra().warnings);

      res = db._query(`FOR doc IN ${cn} FILTER doc.updated == true RETURN 1`).toArray();
      assertEqual(0, res.length);
      
      res = db._query(`FOR doc IN ${cn} FILTER doc.inserted == true RETURN 1`).toArray();
      assertEqual(1000, res.length);
    },
    
    testReadOwnWritesTrueRepeatedUpdates: function () {
      if (isCluster && !isEnterprise) {
        // must bail out here because we need the cluster's OneShard optimization
        // to achieve deterministic behavior
        return;
      }
      const q = `FOR j IN 1..10 FOR i IN 1..200 UPSERT FILTER $CURRENT.value == j INSERT { inserted: true, value: j, hits: 1 } UPDATE { updated: true, hits: OLD.hits + 1 } IN ${cn} OPTIONS { readOwnWrites: true }`;
      const explain = db._createStatement(q).explain();
      assertEqual([], explain.warnings);
      const plan = explain.plan;

      let nodes = plan.nodes.filter((n) => n.type === 'UpsertNode');
      assertEqual(1, nodes.length);
      assertTrue(nodes[0].readOwnWrites);
      
      nodes = plan.nodes.filter((n) => n.type === 'IndexNode');
      assertEqual(1, nodes.length);
      assertTrue(nodes[0].readOwnWrites);

      let res = db._query(q);
      assertEqual(2000, res.getExtra().stats.writesExecuted);
      assertEqual([], res.getExtra().warnings);

      res = db._query(`FOR doc IN ${cn} SORT doc.value RETURN {value: doc.value, hits: doc.hits}`).toArray();
      assertEqual(10, res.length);

      res.forEach((row, i) => {
        assertEqual(i + 1, row.value);
        assertEqual(200, row.hits);
      });
      
      res = db._query(`FOR doc IN ${cn} FILTER doc.inserted == true RETURN 1`).toArray();
      assertEqual(10, res.length);
    },
    
    testReadOwnWritesFalseRepeatedUpdates: function () {
      const q = `FOR j IN 1..10 FOR i IN 1..200 UPSERT FILTER $CURRENT.value == j INSERT { inserted: true, value: j, hits: 1 } UPDATE { updated: true, hits: OLD.hits + 1 } IN ${cn} OPTIONS { readOwnWrites: false }`;
      const explain = db._createStatement(q).explain();
      assertEqual([], explain.warnings);
      const plan = explain.plan;

      let nodes = plan.nodes.filter((n) => n.type === 'UpsertNode');
      assertEqual(1, nodes.length);
      assertFalse(nodes[0].readOwnWrites);
      
      nodes = plan.nodes.filter((n) => n.type === 'IndexNode');
      assertEqual(1, nodes.length);
      assertFalse(nodes[0].readOwnWrites);

      let res = db._query(q);
      assertEqual(2000, res.getExtra().stats.writesExecuted);
      assertEqual([], res.getExtra().warnings);

      res = db._query(`FOR doc IN ${cn} SORT doc.value RETURN {value: doc.value, hits: doc.hits}`).toArray();
      assertEqual(2000, res.length);

      res.forEach((row, i) => {
        assertEqual(1, row.hits);
      });
      
      res = db._query(`FOR doc IN ${cn} FILTER doc.inserted == true RETURN 1`).toArray();
      assertEqual(2000, res.length);
    },
    
  };
}

function readOwnWritesMultipleShardsSuite() {
  return {
    tearDown: function () {
      db._drop(cn);
    },

    testMultipleShardsReadOwnWritesFalse: function () {
      [2, 5, 10].forEach((numberOfShards) => {
        let c = db._create(cn, { numberOfShards });

        try {
          const q = `FOR doc IN ${cn} UPSERT FILTER $CURRENT.value == doc.value INSERT { _key: 'nono' } UPDATE { updated: true } IN ${cn} OPTIONS { readOwnWrites: false }`;
          const explain = db._createStatement(q).explain();
          assertEqual([], explain.warnings);
        } finally {
          db._drop(cn);
        }
      });
    },
    
    testMultipleShardsReadOwnWritesTrue: function () {
      [2, 5, 10].forEach((numberOfShards) => {
        let c = db._create(cn, { numberOfShards });

        try {
          const q = `FOR doc IN ${cn} UPSERT FILTER $CURRENT.value == doc.value INSERT { _key: 'nono' } UPDATE { updated: true } IN ${cn} OPTIONS { readOwnWrites: true }`;
          const explain = db._createStatement(q).explain();
          assertEqual(1, explain.warnings.length);
          assertEqual(errors.ERROR_QUERY_INVALID_OPTIONS_ATTRIBUTE.code, explain.warnings[0].code);
        } finally {
          db._drop(cn);
        }
      });
    },
    
    testSingleShardReadOwnWritesTrue: function () {
      if (isEnterprise) {
        // does not apply in enterprise edition
        return;
      }

      let c = db._create(cn, { numberOfShards: 1 });

      const q = `FOR doc IN ${cn} UPSERT FILTER $CURRENT.value == doc.value INSERT { _key: 'nono' } UPDATE { updated: true } IN ${cn} OPTIONS { readOwnWrites: true }`;
      const explain = db._createStatement(q).explain();
      assertEqual(1, explain.warnings.length);
      assertEqual(errors.ERROR_QUERY_INVALID_OPTIONS_ATTRIBUTE.code, explain.warnings[0].code);
    },
    
    testOneShardReadOwnWritesTrue: function () {
      if (!isEnterprise) {
        // does not apply in community edition
        return;
      }

      db._createDatabase(cn);
      try {
        db._useDatabase(cn);

        let c = db._create(cn, { numberOfShards: 1 });

        const q = `FOR doc IN ${cn} UPSERT FILTER $CURRENT.value == doc.value INSERT { _key: 'nono' } UPDATE { updated: true } IN ${cn} OPTIONS { readOwnWrites: true }`;
        const explain = db._createStatement(q).explain();
        assertEqual([], explain.warnings);
      } finally {
        db._useDatabase("_system");
        db._dropDatabase(cn);
      }
    },
    
  };
}

jsunity.run(readOwnWritesSuite);
if (isCluster) {
  jsunity.run(readOwnWritesMultipleShardsSuite);
}

return jsunity.done();
