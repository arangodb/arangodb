/*jshint globalstrict:false, strict:false */
/*global arango, assertEqual, assertTrue, assertFalse, assertEqual, assertNotEqual, fail */

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
/// @author Copyright 2013, triAGENS GmbH, Cologne, Germany
// //////////////////////////////////////////////////////////////////////////////

const jsunity = require("jsunity");
const arangodb = require("@arangodb");
const internal = require("internal");
const db = arangodb.db;
const ERRORS = arangodb.errors;
const globalProperties = internal.ttlProperties();
const { versionHas } = require('@arangodb/test-helper');
const isCluster = require("internal").isCluster();
let { instanceRole } = require('@arangodb/testutils/instance');
let IM = global.instanceManager;

let deltaTimeout = 30;
let divisor = 1;
if (versionHas('coverage')) {
  deltaTimeout *= 5;
  divisor = 10;
}

function TtlSuite () {
  'use strict';
  const cn = "UnitTestsTtl";

  const waitUntil = function(tries, predicate) {
    let stats = internal.ttlStatistics();
    while (!predicate(stats) && tries-- > 0) {
      internal.wait(0.2, false);    
      stats = internal.ttlStatistics();
    }
    return stats;
  };

  return {

    setUp : function () {
      if (isCluster) {
        IM.debugSetFailAt("allow-low-ttl-frequency", instanceRole.dbServer);
      } else {
        IM.debugSetFailAt("allow-low-ttl-frequency");
      }
      db._drop(cn);
      internal.ttlProperties(globalProperties);
    },

    tearDown : function () {
      if (isCluster) {
        IM.debugClearFailAt();
      } else {
        IM.debugClearFailAt();
      }
      db._drop(cn);
      internal.ttlProperties(globalProperties);
    },
    
    testStatistics : function () {
      let stats = internal.ttlStatistics();
      assertTrue(stats.hasOwnProperty("runs"));
      assertEqual("number", typeof stats.runs);
      assertTrue(stats.hasOwnProperty("documentsRemoved"));
      assertEqual("number", typeof stats.documentsRemoved);
      assertTrue(stats.hasOwnProperty("limitReached"));
      assertEqual("number", typeof stats.limitReached);
    },
    
    testProperties : function () {
      let properties = internal.ttlProperties();
      assertTrue(properties.hasOwnProperty("active"));
      assertEqual("boolean", typeof properties.active);
      assertTrue(properties.hasOwnProperty("frequency"));
      assertEqual("number", typeof properties.frequency);
      assertTrue(properties.hasOwnProperty("maxCollectionRemoves"));
      assertEqual("number", typeof properties.maxCollectionRemoves);
      assertTrue(properties.hasOwnProperty("maxTotalRemoves"));
      assertEqual("number", typeof properties.maxTotalRemoves);
    },
    
    testSetProperties : function () {
      let values = [
        [ { active: null }, false ],
        [ { active: "foo" }, false ],
        [ { active: 404 }, false ],
        [ { active: false }, true ],
        [ { active: true }, true ],
        [ { frequency: "foobar" }, false ],
        [ { frequency: null }, false ],
        [ { frequency: 1000 }, true ],
        [ { frequency: 100000 / divisor }, true ],
        [ { maxCollectionRemoves: true }, false ],
        [ { maxCollectionRemoves: "1000" }, false ],
        [ { maxCollectionRemoves: 1000 }, true ],
        [ { maxCollectionRemoves: 100000 / divisor }, true ],
        [ { maxTotalRemoves: true }, false ],
        [ { maxTotalRemoves: "5000" }, false ],
        [ { maxTotalRemoves: 100 }, true ],
        [ { maxTotalRemoves: 5000 }, true ],
        [ { maxTotalRemoves: 500000 / divisor }, true ],
      ];

      values.forEach(function(value) {
        if (value[1]) {
          let properties = internal.ttlProperties(value[0]);
          let keys = Object.keys(value[0]);
          keys.forEach(function(k) {
            assertEqual(properties[k], value[0][k]);
          });
          properties = internal.ttlProperties();
          keys.forEach(function(k) {
            assertEqual(properties[k], value[0][k]);
          });
        } else {
          let properties = internal.ttlProperties();
          try {
            internal.ttlProperties(value[0]);
            fail();
          } catch (err) {
            assertEqual(err.errorNum, ERRORS.ERROR_BAD_PARAMETER.code, value);
          }
          let properties2 = internal.ttlProperties();
          let keys = Object.keys(value[0]);
          keys.forEach(function(k) {
            assertEqual(properties[k], properties2[k]);
            assertNotEqual(properties[k], value[0][k]);
          });
        }
      });
    },
    
    testActive : function () {
      // disable first
      internal.ttlProperties({ active: false });

      let stats = internal.ttlStatistics();
      const oldRuns = stats.runs;
      
      // reenable first
      internal.ttlProperties({ active: true, frequency: 100 });

      let tries = 0;
      while (tries++ < 10) {
        internal.wait(0.1, false);
      
        stats = internal.ttlStatistics();
        if (stats.runs > oldRuns) {
          break;
        }
      }

      // wait for the number of runs to increase
      assertTrue(stats.runs > oldRuns);
    },
    
    testInactive : function () {
      // disable first
      internal.ttlProperties({ active: false });

      let stats = internal.ttlStatistics();
      const oldRuns = stats.runs;
      
      // set properties, but keep disabled
      internal.ttlProperties({ active: false, frequency: 100 });

      let tries = 0;
      while (tries++ < 10) {
        internal.wait(0.1, false);
      
        stats = internal.ttlStatistics();
        if (stats.runs > oldRuns) {
          break;
        }
      }

      // number of runs must not have changed
      assertEqual(stats.runs, oldRuns);
    },
    
    testCreateIndexSubAttribute : function () {
      let c = db._create(cn, { numberOfShards: 2 });
      try {
        c.ensureIndex({ type: "ttl", fields: ["date.created"], expireAfter: 10, unique: true });
        fail();
      } catch (err) {
        assertEqual(ERRORS.ERROR_BAD_PARAMETER.code, err.errorNum);
      }
    },

    testCreateIndexUnique : function () {
      let c = db._create(cn, { numberOfShards: 2 });
      try {
        c.ensureIndex({ type: "ttl", fields: ["dateCreated"], expireAfter: 10, unique: true });
        fail();
      } catch (err) {
        assertEqual(ERRORS.ERROR_BAD_PARAMETER.code, err.errorNum);
      }
    },
    
    testCreateIndexMultipleTimesDifferentField : function () {
      let c = db._create(cn, { numberOfShards: 2 });
      c.ensureIndex({ type: "ttl", fields: ["test"], expireAfter: 10 });
      try {
        c.ensureIndex({ type: "ttl", fields: ["dateCreated"], expireAfter: 10 });
        fail();
      } catch (err) {
        assertEqual(ERRORS.ERROR_BAD_PARAMETER.code, err.errorNum);
      }
    },
    
    testCreateIndexMultipleTimesDifferentExpire : function () {
      let c = db._create(cn, { numberOfShards: 2 });
      c.ensureIndex({ type: "ttl", fields: ["test"], expireAfter: 10 });
      try {
        c.ensureIndex({ type: "ttl", fields: ["test"], expireAfter: 11 });
        fail();
      } catch (err) {
        assertEqual(ERRORS.ERROR_BAD_PARAMETER.code, err.errorNum);
      }
    },
    
    testCreateIndexMultipleTimesSameAttributes : function () {
      let c = db._create(cn, { numberOfShards: 2 });
      let idx1 = c.ensureIndex({ type: "ttl", fields: ["test"], expireAfter: 10 });
      let idx2 = c.ensureIndex({ type: "ttl", fields: ["test"], expireAfter: 10 });

      assertTrue(idx1.isNewlyCreated);
      assertFalse(idx2.isNewlyCreated);
      assertEqual(idx1.id, idx2.id);
    },
    
    testCreateIndexOnMultipleAttributes : function () {
      let c = db._create(cn, { numberOfShards: 2 });
      try {
        c.ensureIndex({ type: "ttl", fields: ["dateCreated", "foobar"], expireAfter: 10 });
        fail();
      } catch (err) {
        assertEqual(ERRORS.ERROR_BAD_PARAMETER.code, err.errorNum);
      }
    },
    
    testCreateIndexWithoutExpireAfter : function () {
      let c = db._create(cn, { numberOfShards: 2 });
      try {
        c.ensureIndex({ type: "ttl", fields: ["dateCreated"] });
        fail();
      } catch (err) {
        assertEqual(ERRORS.ERROR_BAD_PARAMETER.code, err.errorNum);
      }
    },
    
    testCreateIndexAttributes : function () {
      let c = db._create(cn, { numberOfShards: 2 });
      let idx = c.ensureIndex({ type: "ttl", fields: ["dateCreated"], expireAfter: 10 });
      assertTrue(idx.isNewlyCreated);
      assertEqual("ttl", idx.type);
      assertEqual(["dateCreated"], idx.fields);
      assertEqual(10, idx.expireAfter);
      assertFalse(idx.estimates);

      // fetch index data yet again via API
      idx = c.indexes()[1];
      assertEqual("ttl", idx.type);
      assertEqual(["dateCreated"], idx.fields);
      assertEqual(10, idx.expireAfter);
      assertFalse(idx.estimates);
    },
    
    testCreateIndexWithEstimates : function () {
      let c = db._create(cn, { numberOfShards: 2 });
      let idx = c.ensureIndex({ type: "ttl", fields: ["dateCreated"], expireAfter: 10, estimates: true });
      assertTrue(idx.isNewlyCreated);
      assertFalse(idx.estimates);
    },
    
    testCreateIndexWithInvalidExpireAfter : function () {
      const values = [ 
        null,
        false,
        true,
        -10,
        "10",
        "abc",
        [],
        {}
      ];

      let c = db._create(cn, { numberOfShards: 2 });
      
      values.forEach(function(v) {
        try {
          c.ensureIndex({ type: "ttl", fields: ["dateCreated"], expireAfter: v });
          fail();
        } catch (err) {
          assertEqual(ERRORS.ERROR_BAD_PARAMETER.code, err.errorNum);
        }
      });
    },
    
    testIndexUsed : function() {
      let c = db._create(cn, { numberOfShards: 2 });
      c.ensureIndex({ type: "ttl", fields: ["dateCreated"], expireAfter: 1 });

      let query = "FOR doc IN @@collection FILTER doc.@indexAttribute >= 0 && doc.@indexAttribute <= @stamp SORT doc.@indexAttribute LIMIT @limit REMOVE doc IN @@collection OPTIONS { ignoreErrors: true }";
      
      let bindVars = { "@collection": cn, indexAttribute: "dateCreated", stamp: Date.now() / 1000, limit: 100 };

      let stmt = db._createStatement({ query, bindVars });
      let plan = stmt.explain().plan;
      let rules = plan.rules;
      assertNotEqual(-1, rules.indexOf("use-indexes"));
      assertNotEqual(-1, rules.indexOf("use-index-for-sort"));

      plan.nodes.forEach(function(node) {
        assertNotEqual("EnumerateCollectionNode", node.type);
        assertNotEqual("SortNode", node.type);
      });
    },
    
    testIndexNotUsedForFiltering : function() {
      let c = db._create(cn, { numberOfShards: 2 });
      c.ensureIndex({ type: "ttl", fields: ["dateCreated"], expireAfter: 1 });

      let queries = [
        "FOR doc IN @@collection RETURN doc.@indexAttribute",
        "FOR doc IN @@collection SORT doc.@indexAttribute RETURN doc",
        "FOR doc IN @@collection SORT doc.@indexAttribute RETURN doc.@indexAttribute",
        "FOR doc IN @@collection SORT doc.@indexAttribute DESC RETURN doc",
        "FOR doc IN @@collection SORT doc.@indexAttribute DESC RETURN doc.@indexAttribute",
        "FOR doc IN @@collection FILTER doc.@indexAttribute >= '2019-01-01' RETURN doc",
        "FOR doc IN @@collection FILTER doc.@indexAttribute <= '2019-01-31' RETURN doc",
        "FOR doc IN @@collection FILTER doc.@indexAttribute >= '2019-01-01' && doc.@indexAttribute <= '2019-01-31' RETURN doc",
      ];
      
      let bindVars = { "@collection": cn, indexAttribute: "dateCreated" };

      queries.forEach(function(query) {
        let stmt = db._createStatement({ query, bindVars });
        let plan = stmt.explain().plan;
        let rules = plan.rules;
        assertEqual(-1, rules.indexOf("use-indexes"), query);
        assertEqual(-1, rules.indexOf("use-index-for-sort"), query);

        plan.nodes.forEach(function(node) {
          assertNotEqual("IndexNode", node.type);
        });
      });
    },
    
    testIndexUsedForSorting : function() {
      let c = db._create(cn, { numberOfShards: 2 });
      c.ensureIndex({ type: "ttl", fields: ["dateCreated"], expireAfter: 1 });

      let queries = [
        "FOR doc IN @@collection FILTER doc.@indexAttribute >= '2019-01-01' SORT doc.@indexAttribute RETURN doc",
        "FOR doc IN @@collection FILTER doc.@indexAttribute <= '2019-01-31' && doc.@indexAttribute != null SORT doc.@indexAttribute RETURN doc",
        "FOR doc IN @@collection FILTER doc.@indexAttribute >= '2019-01-01' && doc.@indexAttribute <= '2019-01-31' SORT doc.@indexAttribute RETURN doc",
      ];
      
      let bindVars = { "@collection": cn, indexAttribute: "dateCreated" };

      queries.forEach(function(query) {
        let stmt = db._createStatement({ query, bindVars });
        let plan = stmt.explain().plan;
        let rules = plan.rules;
        assertEqual(-1, rules.indexOf("use-indexes"), query);
        assertNotEqual(-1, rules.indexOf("use-index-for-sort"), query);
      });
    },
    
    testRemovalsExpireAfterNotPresent : function () {
      internal.ttlProperties({ active: false });

      let c = db._create(cn, { numberOfShards: 2 });
      c.ensureIndex({ type: "ttl", fields: ["dateCreated"], expireAfter: 1 });

      let docs = [];
      for (let i = 0; i < 1000; ++i) {
        docs.push({ value: i });
      }
      c.insert(docs);

      const oldStats = internal.ttlStatistics();
      
      // reenable
      internal.ttlProperties({ active: true, frequency: 100, maxTotalRemoves: 100000 / divisor, maxCollectionRemoves: 100000 / divisor });

      let stats = waitUntil(20 * deltaTimeout, stats => stats.runs > oldStats.runs);

      // number of runs must have changed, number of deletions must not
      assertNotEqual(stats.runs, oldStats.runs);
      assertTrue(stats.documentsRemoved === oldStats.documentsRemoved, { stats, oldStats });
      
      assertEqual(1000, db._collection(cn).count());
    },
    
    testRemovalsAllNumeric : function () {
      internal.ttlProperties({ active: false });

      let c = db._create(cn, { numberOfShards: 2 });
      c.ensureIndex({ type: "ttl", fields: ["dateCreated"], expireAfter: 1 });

      // dt is one minute in the past
      const dt = new Date((new Date()).getTime() - 1000 * 60).getTime();
      assertTrue(new Date(dt).toISOString() >= "2019-01-");

      let docs = [];
      for (let i = 0; i < 1000; ++i) {
        docs.push({ dateCreated: dt / 1000, value: i });
      }
      c.insert(docs);

      const oldStats = internal.ttlStatistics();
      
      // reenable
      internal.ttlProperties({ active: true, frequency: 100, maxTotalRemoves: 100000 / divisor, maxCollectionRemoves: 100000 / divisor });
      
      let stats = waitUntil(20 * deltaTimeout, stats => stats.documentsRemoved >= oldStats.documentsRemoved + docs.length);

      // both number of runs and deletions must have changed
      assertNotEqual(stats.runs, oldStats.runs);
      assertTrue(stats.documentsRemoved >= oldStats.documentsRemoved + docs.length, { stats, oldStats });

      assertEqual(0, db._collection(cn).count());
    },
    
    testRemovalsAllDate : function () {
      internal.ttlProperties({ active: false });

      let c = db._create(cn, { numberOfShards: 2 });
      c.ensureIndex({ type: "ttl", fields: ["dateCreated"], expireAfter: 1 });
      c.ensureIndex({ type: "persistent", fields: ["dateCreated"], unique: false, sparse: false});

      // dt is one minute in the past
      const dt = new Date((new Date()).getTime() - 1000 * 60).toISOString();
      assertTrue(dt >= "2019-01-");

      let docs = [];
      for (let i = 0; i < 1000; ++i) {
        docs.push({ dateCreated: dt, value: i });
      }
      c.insert(docs);

      const oldStats = internal.ttlStatistics();
      
      // reenable
      internal.ttlProperties({ active: true, frequency: 100, maxTotalRemoves: 100000 / divisor, maxCollectionRemoves: 100000 / divisor });
      
      let stats = waitUntil(20 * deltaTimeout, stats => stats.documentsRemoved >= oldStats.documentsRemoved + docs.length);

      // both number of runs and deletions must have changed
      assertNotEqual(stats.runs, oldStats.runs);
      assertTrue(stats.documentsRemoved >= oldStats.documentsRemoved + docs.length, { stats, oldStats });

      assertEqual(0, db._collection(cn).count());
    },
    
    testRemovalsAllMixed : function () {
      internal.ttlProperties({ active: false });

      let c = db._create(cn, { numberOfShards: 2 });
      c.ensureIndex({ type: "ttl", fields: ["dateCreated"], expireAfter: 1 });

      // dt is one minute in the past
      let dt = new Date((new Date()).getTime() - 1000 * 60).toISOString();
      assertTrue(dt >= "2019-01-");

      let docs = [];
      for (let i = 0; i < 1000; ++i) {
        docs.push({ dateCreated: dt, value: i });
      }
      c.insert(docs);
      docs = [];
      dt = new Date((new Date()).getTime() - 1000 * 60).getTime();
      for (let i = 0; i < 1000; ++i) {
        docs.push({ dateCreated: dt / 1000, value: i });
      }
      c.insert(docs);

      const oldStats = internal.ttlStatistics();
      
      // reenable
      internal.ttlProperties({ active: true, frequency: 100, maxTotalRemoves: 100000 / divisor, maxCollectionRemoves: 100000 / divisor });
      
      let stats = waitUntil(20 * deltaTimeout, stats => stats.documentsRemoved >= oldStats.documentsRemoved + 2 * docs.length / divisor);

      // both number of runs and deletions must have changed
      assertNotEqual(stats.runs, oldStats.runs);
      assertTrue(stats.documentsRemoved >= oldStats.documentsRemoved + 2 * docs.length / divisor, { stats, oldStats });

      assertEqual(0, db._collection(cn).count());
    },
    
    testRemovalsAllButOneNumeric : function () {
      internal.ttlProperties({ active: false });

      let c = db._create(cn, { numberOfShards: 2 });
      c.ensureIndex({ type: "ttl", fields: ["dateCreated"], expireAfter: 1 });

      // dt is one minute in the past
      const dt = new Date((new Date()).getTime() - 1000 * 60).getTime();
      assertTrue(new Date(dt).toISOString() >= "2019-01-");

      let docs = [];
      for (let i = 0; i < 1000; ++i) {
        docs.push({ dateCreated: dt / 1000, value: i });
      }
      c.insert(docs);
      c.insert({ dateCreated: dt }); // intentionally not divided by 1000

      const oldStats = internal.ttlStatistics();
      
      // reenable
      internal.ttlProperties({ active: true, frequency: 100, maxTotalRemoves: 100000 / divisor, maxCollectionRemoves: 100000 / divisor });

      let stats = waitUntil(20 * deltaTimeout, stats => stats.documentsRemoved >= oldStats.documentsRemoved + docs.length / divisor);

      // both number of runs and deletions must have changed
      assertNotEqual(stats.runs, oldStats.runs);
      assertTrue(stats.documentsRemoved >= oldStats.documentsRemoved + docs.length / divisor, { stats, oldStats }); 

      assertEqual(1, db._collection(cn).count());
      assertEqual(dt, db._collection(cn).any().dateCreated);
    },
    
    testRemovalsAllButOneDate : function () {
      internal.ttlProperties({ active: false });

      let c = db._create(cn, { numberOfShards: 2 });
      c.ensureIndex({ type: "ttl", fields: ["dateCreated"], expireAfter: 1 });

      // dt is one minute in the past
      const dt = new Date((new Date()).getTime() - 1000 * 60).toISOString();
      assertTrue(dt >= "2019-01-");

      let docs = [];
      for (let i = 0; i < 1000; ++i) {
        docs.push({ dateCreated: dt, value: i });
      }
      c.insert(docs);
      // insert a date in the future
      const dt2 = new Date((new Date()).getTime() + 1000 * 60).toISOString();
      c.insert({ dateCreated: dt2 });

      const oldStats = internal.ttlStatistics();
      
      // reenable
      internal.ttlProperties({ active: true, frequency: 100, maxTotalRemoves: 100000 / divisor, maxCollectionRemoves: 100000 / divisor });

      let stats = waitUntil(20 * deltaTimeout, stats => stats.documentsRemoved >= oldStats.documentsRemoved + docs.length / divisor);

      // both number of runs and deletions must have changed
      assertNotEqual(stats.runs, oldStats.runs);
      assertTrue(stats.documentsRemoved >= oldStats.documentsRemoved + docs.length / divisor, { stats, oldStats });

      assertEqual(1, db._collection(cn).count());
      assertEqual(dt2, db._collection(cn).any().dateCreated);
    },
    
    testRemovalsAllBiggerNumeric : function () {
      internal.ttlProperties({ active: false });

      let c = db._create(cn, { numberOfShards: 2 });
      c.ensureIndex({ type: "ttl", fields: ["dateCreated"], expireAfter: 1 });

      // dt is one minute in the past
      const dt = new Date((new Date()).getTime() - 1000 * 60).getTime();
      assertTrue(new Date(dt).toISOString() >= "2019-01-");

      let docs = [];
      for (let i = 0; i < 10000; ++i) {
        docs.push({ dateCreated: dt / 1000, value: i });
      }
      c.insert(docs);

      const oldStats = internal.ttlStatistics();
      
      // reenable
      internal.ttlProperties({ active: true, frequency: 100, maxTotalRemoves: 100000 / divisor, maxCollectionRemoves: 100000 / divisor });

      let stats = waitUntil(20 * deltaTimeout, stats => stats.documentsRemoved >= oldStats.documentsRemoved + docs.length / divisor);
      
      // both number of runs and deletions must have changed
      assertNotEqual(stats.runs, oldStats.runs);
      assertTrue(stats.documentsRemoved >= oldStats.documentsRemoved + docs.length / divisor, { stats, oldStats });

      assertEqual(0, db._collection(cn).count());
    },
    
    testRemovalsAllBiggerDate : function () {
      internal.ttlProperties({ active: false });

      let c = db._create(cn, { numberOfShards: 2 });
      c.ensureIndex({ type: "ttl", fields: ["dateCreated"], expireAfter: 1 });

      // dt is one minute in the past
      const dt = new Date((new Date()).getTime() - 1000 * 60).toISOString();
      assertTrue(dt >= "2019-01-");

      let docs = [];
      for (let i = 0; i < 10000 / divisor; ++i) {
        docs.push({ dateCreated: dt, value: i });
      }
      c.insert(docs);

      const oldStats = internal.ttlStatistics();
      
      // reenable
      internal.ttlProperties({ active: true, frequency: 100, maxTotalRemoves: 100000 / divisor, maxCollectionRemoves: 100000 / divisor });

      let stats = waitUntil(20 * deltaTimeout, stats => stats.documentsRemoved >= oldStats.documentsRemoved + docs.length / divisor);

      // both number of runs and deletions must have changed
      assertNotEqual(stats.runs, oldStats.runs);
      assertTrue(stats.documentsRemoved >= oldStats.documentsRemoved + docs.length / divisor, { stats, oldStats });

      assertEqual(0, db._collection(cn).count());
    },
    
    testRemovalsExpireAfterInTheFutureNumeric : function () {
      internal.ttlProperties({ active: false });

      let c = db._create(cn, { numberOfShards: 2 });
      c.ensureIndex({ type: "ttl", fields: ["dateCreated"], expireAfter: 1 });

      // dt is one day in the future
      const dt = new Date((new Date()).getTime() + 1000 * 86400).getTime();
      assertTrue(new Date(dt).toISOString() >= "2019-01-");

      let docs = [];
      for (let i = 0; i < 1000; ++i) {
        docs.push({ dateCreated: dt / 1000, value: i });
      }
      c.insert(docs);

      const oldStats = internal.ttlStatistics();
      
      // reenable
      internal.ttlProperties({ active: true, frequency: 100, maxTotalRemoves: 100000 / divisor, maxCollectionRemoves: 100000 / divisor });

      let stats = waitUntil(20 * deltaTimeout, stats => stats.runs > oldStats.runs);

      // number of runs must have changed, number of deletions must not
      assertNotEqual(stats.runs, oldStats.runs);
      assertTrue(stats.documentsRemoved === oldStats.documentsRemoved, { stats, oldStats });

      assertEqual(1000, db._collection(cn).count());
    },
    
    testRemovalsExpireAfterInTheFutureDate : function () {
      internal.ttlProperties({ active: false });

      let c = db._create(cn, { numberOfShards: 2 });
      c.ensureIndex({ type: "ttl", fields: ["dateCreated"], expireAfter: 1 });

      // dt is one day in the future
      const dt = new Date((new Date()).getTime() + 1000 * 86400).toISOString();
      assertTrue(dt >= "2019-01-");

      let docs = [];
      for (let i = 0; i < 1000; ++i) {
        docs.push({ dateCreated: dt, value: i });
      }
      c.insert(docs);

      const oldStats = internal.ttlStatistics();
      
      // reenable
      internal.ttlProperties({ active: true, frequency: 100, maxTotalRemoves: 100000 / divisor, maxCollectionRemoves: 100000 / divisor });

      let stats = waitUntil(20 * deltaTimeout, stats => stats.runs > oldStats.runs);

      // number of runs must have changed, number of deletions must not
      assertNotEqual(stats.runs, oldStats.runs);
      assertTrue(stats.documentsRemoved === oldStats.documentsRemoved, { stats, oldStats });

      assertEqual(1000, db._collection(cn).count());
    },
    
    testRemovalsExpireAfterSomeInTheFutureNumeric : function () {
      internal.ttlProperties({ active: false });

      let c = db._create(cn, { numberOfShards: 2 });
      c.ensureIndex({ type: "ttl", fields: ["dateCreated"], expireAfter: 1 });

      // dt is one day in the future
      let dt = new Date((new Date()).getTime() + 1000 * 86400).getTime();
      assertTrue(new Date(dt).toISOString() >= "2019-01-");

      let docs = [];
      for (let i = 0; i < 1000; ++i) {
        docs.push({ dateCreated: dt / 1000, value: i });
      }
      c.insert(docs);
      
      // dt is a minute in the past
      dt = new Date((new Date()).getTime() - 1000 * 60).getTime();
      assertTrue(new Date(dt).toISOString() >= "2019-01-");
      docs = [];
      for (let i = 0; i < 1000; ++i) {
        docs.push({ dateCreated: dt / 1000, value: i });
      }
      c.insert(docs);

      assertEqual(2000, db._collection(cn).count());

      const oldStats = internal.ttlStatistics();
      
      // reenable
      let props = internal.ttlProperties({ active: true, frequency: 100, maxTotalRemoves: 100000 / divisor, maxCollectionRemoves: 100000 / divisor });
      assertTrue(props.active);
      assertEqual(100, props.frequency);
      assertEqual(100000 / divisor, props.maxTotalRemoves);
      assertEqual(100000 / divisor, props.maxCollectionRemoves);

      let stats = waitUntil(20 * deltaTimeout, stats => stats.documentsRemoved > oldStats.documentsRemoved);

      // both number of runs and deletions must have changed
      assertNotEqual(stats.runs, oldStats.runs);
      assertTrue(stats.documentsRemoved > oldStats.documentsRemoved);

      let tries = 0;
      while (++tries < 20 && db._collection(cn).count() !== 1000 / divisor) {
        internal.wait(0.1, false);
      }
      
      assertEqual(1000, db._collection(cn).count());
    },
    
    testRemovalsExpireAfterSomeInTheFutureDate : function () {
      internal.ttlProperties({ active: false });

      let c = db._create(cn, { numberOfShards: 2 });
      c.ensureIndex({ type: "ttl", fields: ["dateCreated"], expireAfter: 1 });

      // dt is one day in the future
      let dt = new Date((new Date()).getTime() + 1000 * 86400).toISOString();
      assertTrue(dt >= "2019-01-");

      let docs = [];
      for (let i = 0; i < 1000; ++i) {
        docs.push({ dateCreated: dt, value: i });
      }
      c.insert(docs);

      // dt is a minute in the past
      dt = new Date((new Date()).getTime() - 1000 * 60).toISOString();
      assertTrue(dt >= "2019-01-");
      docs = [];
      for (let i = 0; i < 1000; ++i) {
        docs.push({ dateCreated: dt, value: i });
      }
      c.insert(docs);

      assertEqual(2000, db._collection(cn).count());

      const oldStats = internal.ttlStatistics();
      
      // reenable
      let props = internal.ttlProperties({ active: true, frequency: 100, maxTotalRemoves: 100000 / divisor, maxCollectionRemoves: 100000 / divisor });
      assertTrue(props.active);
      assertEqual(100, props.frequency);
      assertEqual(100000 / divisor, props.maxTotalRemoves);
      assertEqual(100000 / divisor, props.maxCollectionRemoves);

      let stats = waitUntil(20 * deltaTimeout, stats => stats.documentsRemoved > oldStats.documentsRemoved);

      // both number of runs and deletions must have changed
      assertNotEqual(stats.runs, oldStats.runs);
      assertTrue(stats.documentsRemoved > oldStats.documentsRemoved);
      
      let tries = 0;
      while (++tries < 20 && db._collection(cn).count() !== 1000 / divisor) {
        internal.wait(0.1, false);
      }
      
      assertEqual(1000, db._collection(cn).count());
    },
    
    testRemovalsLimitsHitGlobalNumeric : function () {
      internal.ttlProperties({ active: false });

      let c = db._create(cn, { numberOfShards: 2 });
      c.ensureIndex({ type: "ttl", fields: ["dateCreated"], expireAfter: 1 });

      // dt is one minute in the past
      const dt = new Date((new Date()).getTime() - 1000 * 60).getTime();
      assertTrue(new Date(dt).toISOString() >= "2019-01-");

      let docs = [];
      for (let i = 0; i < 10000; ++i) {
        docs.push({ dateCreated: dt / 1000, value: i });
      }
      c.insert(docs);

      let oldStats = internal.ttlStatistics();
      let oldCount = 10000;  
      
      // reenable
      internal.ttlProperties({ active: true, frequency: 100, maxTotalRemoves: 10, maxCollectionRemoves: 100000 / divisor });
    
      let stats = waitUntil(20 * deltaTimeout,
        stats => stats.documentsRemoved > oldStats.documentsRemoved && stats.limitReached > oldStats.limitReached);

      // number of runs, deletions and limitReached must have changed
      assertNotEqual(stats.runs, oldStats.runs);
      assertTrue(stats.limitReached > oldStats.limitReached);
      assertTrue(stats.documentsRemoved > oldStats.documentsRemoved);
      assertTrue(db._collection(cn).count() < oldCount);
      oldCount = db._collection(cn).count();
     
      // wait again for next removal 
      oldStats = stats;
      stats = waitUntil(20 * deltaTimeout,
        stats => stats.documentsRemoved > oldStats.documentsRemoved && stats.limitReached > oldStats.limitReached);

      assertNotEqual(stats.runs, oldStats.runs);
      assertTrue(stats.limitReached > oldStats.limitReached);
      assertTrue(stats.documentsRemoved > oldStats.documentsRemoved);
      // wait again, as fetching the stats and acquiring the collection count is not atomic
      waitUntil(20 * deltaTimeout, _ => db._collection(cn).count() < oldCount || db._collection(cn).count() === 0);
      assertTrue(db._collection(cn).count() < oldCount || db._collection(cn).count() === 0);
    },
    
    testRemovalsLimitsHitGlobalDate : function () {
      internal.ttlProperties({ active: false });

      let c = db._create(cn, { numberOfShards: 2 });
      c.ensureIndex({ type: "ttl", fields: ["dateCreated"], expireAfter: 1 });

      // dt is one minute in the past
      const dt = new Date((new Date()).getTime() - 1000 * 60).toISOString();
      assertTrue(dt >= "2019-01-");

      let docs = [];
      for (let i = 0; i < 10000; ++i) {
        docs.push({ dateCreated: dt, value: i });
      }
      c.insert(docs);

      let oldStats = internal.ttlStatistics();
      let oldCount = 10000;  
      
      // reenable
      internal.ttlProperties({ active: true, frequency: 100, maxTotalRemoves: 10, maxCollectionRemoves: 100000 / divisor });
    
      let stats = waitUntil(20 * deltaTimeout,
        stats => stats.documentsRemoved > oldStats.documentsRemoved && stats.limitReached > oldStats.limitReached);

      // number of runs, deletions and limitReached must have changed
      assertNotEqual(stats.runs, oldStats.runs);
      assertTrue(stats.limitReached > oldStats.limitReached);
      assertTrue(stats.documentsRemoved > oldStats.documentsRemoved);
      assertTrue(db._collection(cn).count() < oldCount);
      oldCount = db._collection(cn).count();
     
      // wait again for next removal 
      oldStats = stats;
      stats = waitUntil(20 * deltaTimeout,
        stats => stats.documentsRemoved > oldStats.documentsRemoved && stats.limitReached > oldStats.limitReached);

      assertNotEqual(stats.runs, oldStats.runs);
      assertTrue(stats.limitReached > oldStats.limitReached);
      assertTrue(stats.documentsRemoved > oldStats.documentsRemoved);
      // wait again, as fetching the stats and acquiring the collection count is not atomic
      waitUntil(20 * deltaTimeout, _ => db._collection(cn).count() < oldCount || db._collection(cn).count() === 0);
      assertTrue(db._collection(cn).count() < oldCount || db._collection(cn).count() === 0);
    },
    
    testRemovalsLimitsHitCollectionNumeric : function () {
      internal.ttlProperties({ active: false });

      let c = db._create(cn, { numberOfShards: 2 });
      c.ensureIndex({ type: "ttl", fields: ["dateCreated"], expireAfter: 1 });

      // dt is one minute in the past
      const dt = new Date((new Date()).getTime() - 1000 * 60).getTime();
      assertTrue(new Date(dt).toISOString() >= "2019-01-");

      let docs = [];
      for (let i = 0; i < 10000; ++i) {
        docs.push({ dateCreated: dt / 1000, value: i });
      }
      c.insert(docs);

      let oldStats = internal.ttlStatistics();
      let oldCount = 10000;  
      
      // reenable
      internal.ttlProperties({ active: true, frequency: 100, maxTotalRemoves: 1000, maxCollectionRemoves: 2000 });
    
      let stats = waitUntil(20 * deltaTimeout,
        stats => stats.documentsRemoved > oldStats.documentsRemoved && stats.limitReached > oldStats.limitReached);

      // number of runs, deletions and limitReached must have changed
      assertNotEqual(stats.runs, oldStats.runs);
      assertTrue(stats.limitReached > oldStats.limitReached);
      assertTrue(stats.documentsRemoved > oldStats.documentsRemoved);
      assertTrue(db._collection(cn).count() < oldCount);
      oldCount = db._collection(cn).count();
     
      // wait again for next removal 
      oldStats = stats;
      stats = waitUntil(20 * deltaTimeout,
        stats => stats.documentsRemoved > oldStats.documentsRemoved && stats.limitReached > oldStats.limitReached);

      assertNotEqual(stats.runs, oldStats.runs);
      assertTrue(stats.limitReached > oldStats.limitReached);
      assertTrue(stats.documentsRemoved > oldStats.documentsRemoved);
      // wait again, as fetching the stats and acquiring the collection count is not atomic
      waitUntil(20 * deltaTimeout, _ => db._collection(cn).count() < oldCount || db._collection(cn).count() === 0);
      assertTrue(db._collection(cn).count() < oldCount || db._collection(cn).count() === 0);
    },
    
    testRemovalsLimitsHitCollectionDate : function () {
      internal.ttlProperties({ active: false });

      let c = db._create(cn, { numberOfShards: 2 });
      c.ensureIndex({ type: "ttl", fields: ["dateCreated"], expireAfter: 1 });

      // dt is one minute in the past
      const dt = new Date((new Date()).getTime() - 1000 * 60).toISOString();
      assertTrue(dt >= "2019-01-");

      let docs = [];
      for (let i = 0; i < 10000; ++i) {
        docs.push({ dateCreated: dt, value: i });
      }
      c.insert(docs);

      let oldStats = internal.ttlStatistics();
      let oldCount = 10000;
      
      // reenable
      internal.ttlProperties({ active: true, frequency: 100, maxTotalRemoves: 1000, maxCollectionRemoves: 2000 });
    
      let stats = waitUntil(20 * deltaTimeout,
        stats => stats.documentsRemoved > oldStats.documentsRemoved && stats.limitReached > oldStats.limitReached);

      // number of runs, deletions and limitReached must have changed
      assertNotEqual(stats.runs, oldStats.runs);
      assertTrue(stats.limitReached > oldStats.limitReached);
      assertTrue(stats.documentsRemoved > oldStats.documentsRemoved);
      assertTrue(db._collection(cn).count() < oldCount);
      oldCount = db._collection(cn).count();
     
      // wait again for next removal 
      oldStats = stats;
      stats = waitUntil(20 * deltaTimeout,
        stats => stats.documentsRemoved > oldStats.documentsRemoved && stats.limitReached > oldStats.limitReached);

      assertNotEqual(stats.runs, oldStats.runs);
      assertTrue(stats.limitReached > oldStats.limitReached);
      assertTrue(stats.documentsRemoved > oldStats.documentsRemoved);
      // wait again, as fetching the stats and acquiring the collection count is not atomic
      waitUntil(20 * deltaTimeout, _ => db._collection(cn).count() < oldCount || db._collection(cn).count() === 0);
      assertTrue(db._collection(cn).count() < oldCount || db._collection(cn).count() === 0);
    },
    
    testRemovalsSmartGraphVertex : function () {
      if (!internal.isCluster() || !internal.isEnterprise()) {
        return;
      }

      const vn = "UnitTestsVertex";
      const en = "UnitTestsEdge";
      const gn = "UnitTestsGraph";

      const graphs = require("@arangodb/smart-graph");
      graphs._create(gn, [graphs._relation(en, vn, vn)], null, { numberOfShards: 3, replicationFactor: 2, smartGraphAttribute: "testi" });

      try {
        internal.ttlProperties({ active: false });

        // populate smart vertex collection
        db[vn].ensureIndex({ type: "ttl", fields: ["dateCreated"], expireAfter: 1 });

        // dt is one minute in the past
        let dt = new Date((new Date()).getTime() - 1000 * 60).toISOString();
        assertTrue(dt >= "2019-01-");

        let docs = [];
        for (let i = 0; i < 50000; ++i) {
          docs.push({ _key: 'test' + (i % 10) + ':test' + i, testi: 'test' + (i % 10), dateCreated: dt, value: i });
          if (docs.length === 5000) {
            db[vn].insert(docs);
            docs = [];
          }
        }

        let oldStats = internal.ttlStatistics();
        let oldCount = 50000;  
        assertEqual(db[vn].count(), oldCount);

        // reenable
        internal.ttlProperties({ active: true, frequency: 1000, maxTotalRemoves: 1000, maxCollectionRemoves: 2000 });
    
        let stats = waitUntil(30 * deltaTimeout, stats => stats.runs > oldStats.runs + 3);

        // number of runs, deletions and limitReached must have changed
        assertNotEqual(stats.runs, oldStats.runs);
        assertTrue(stats.limitReached > oldStats.limitReached);
        assertTrue(stats.documentsRemoved > oldStats.documentsRemoved);
        assertTrue(db._collection(vn).count() < oldCount);
        oldCount = db._collection(vn).count();
    
        if (oldCount > 0) {
          // wait again for next removal 
          oldStats = stats;
          stats = waitUntil(30 * deltaTimeout, stats => stats.runs > oldStats.runs + 3);

          assertNotEqual(stats.runs, oldStats.runs);
          assertTrue(stats.limitReached > oldStats.limitReached);
          assertTrue(stats.documentsRemoved > oldStats.documentsRemoved);
          // wait again, as fetching the stats and acquiring the collection count is not atomic
          oldStats = stats;
          stats = waitUntil(30 * deltaTimeout, stats => stats.runs > oldStats.runs + 3);
          assertTrue(db._collection(vn).count() < oldCount || db._collection(vn).count() === 0);
        }
      } finally {
        graphs._drop(gn, true);
      }
    },
    
    testRemovalsSmartGraphEdge : function () {
      if (!internal.isCluster() || !internal.isEnterprise()) {
        return;
      }

      const vn = "UnitTestsVertex";
      const en = "UnitTestsEdge";
      const gn = "UnitTestsGraph";

      const graphs = require("@arangodb/smart-graph");
      graphs._create(gn, [graphs._relation(en, vn, vn)], null, { numberOfShards: 3, replicationFactor: 2, smartGraphAttribute: "testi" });

      try {
        internal.ttlProperties({ active: false });

        // populate smart edge collection
        let dt = new Date((new Date()).getTime() - 1000 * 60).toISOString();
        db[en].ensureIndex({ type: "ttl", fields: ["dateCreated"], expireAfter: 1 });
        
        let docs = [];
        for (let i = 0; i < 60000; ++i) {
          docs.push({ _from: vn + '/test' + i + ':test' + (i % 10), _to: vn + '/test' + ((i + 1) % 100) + ':test' + (i % 10), testi: i % 10, dateCreated: dt, value: i });
          if (docs.length === 5000) {
            db[en].insert(docs);
            docs = [];
          }
        }
              
        let oldStats = internal.ttlStatistics();
        let oldCount = 60000;
        assertEqual(db[en].count(), oldCount);
      
        // reenable
        internal.ttlProperties({ active: true, frequency: 1000, maxTotalRemoves: 4000, maxCollectionRemoves: 2000 });
    
        let stats = waitUntil(30 * deltaTimeout, stats => stats.runs > oldStats.runs + 3);

        // number of runs, deletions and limitReached must have changed
        assertNotEqual(stats.runs, oldStats.runs);
        assertTrue(stats.limitReached > oldStats.limitReached);
        assertTrue(stats.documentsRemoved > oldStats.documentsRemoved);
        assertTrue(db._collection(en).count() < oldCount);
        oldCount = db._collection(en).count();
    
        if (oldCount > 0) {
          // wait again for next removal 
          oldStats = stats;
          stats = waitUntil(30 * deltaTimeout, stats => stats.runs > oldStats.runs + 3);

          assertNotEqual(stats.runs, oldStats.runs);
          assertTrue(stats.limitReached > oldStats.limitReached);
          assertTrue(stats.documentsRemoved > oldStats.documentsRemoved);
          // wait again, as fetching the stats and acquiring the collection count is not atomic
          oldStats = stats;
          stats = waitUntil(30 * deltaTimeout, stats => stats.runs > oldStats.runs);
          assertTrue(db._collection(en).count() < oldCount || db._collection(en).count() === 0);
        }
      } finally {
        graphs._drop(gn, true);
      }
    },
  
  };
}

jsunity.run(TtlSuite);

return jsunity.done();
