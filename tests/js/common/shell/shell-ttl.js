/*jshint globalstrict:false, strict:false */
/*global arango, assertEqual, assertTrue, assertFalse, assertEqual, assertNotEqual, fail */

////////////////////////////////////////////////////////////////////////////////
/// @brief test ttl configuration
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
/// @author Copyright 2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

const jsunity = require("jsunity");
const arangodb = require("@arangodb");
const internal = require("internal");
const db = arangodb.db;
const ERRORS = arangodb.errors;
const globalProperties = internal.ttlProperties();

function TtlSuite () {
  'use strict';
  const cn = "UnitTestsTtl";
      
  const waitForNextRun = function(collection, oldStats, tries) {
    const oldRuns = oldStats.runs;
    let numServers;
    try {
      // create a unique list of servers
      numServers = Object.values(collection.shards(true)).filter(function(value, index, self) {
        return self.indexOf(value) === index;
      }).length;
      // if there are multiple servers involved, we increase by 3, in order to avoid continuing
      // in case the the *same* server reports multiple times. this would break the simple
      // "how many times did it run check" in case the jobs are executed in the following order:
      // - job run on server 1
      // - job run on server 2
      // - job run on server 1 (3 executions, but not from 3 different servers!)
      // - job run on server 3
      // - job run on server 2
      // - job run on server 3
      numServers *= 3;
    } catch (err) {
      // collection.shards() will throw when not running in cluster mode
      numServers = 1;
    }

    let stats;
    while (tries-- > 0) {
      internal.wait(1, false);
    
      stats = internal.ttlStatistics();
      if (stats.runs - oldRuns >= numServers) {
        break;
      }
    }
    return stats;
  };

  return {

    setUp : function () {
      db._drop(cn);
      internal.ttlProperties(globalProperties);
    },

    tearDown : function () {
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
        [ { frequency: 100000 }, true ],
        [ { maxCollectionRemoves: true }, false ],
        [ { maxCollectionRemoves: "1000" }, false ],
        [ { maxCollectionRemoves: 1000 }, true ],
        [ { maxCollectionRemoves: 100000 }, true ],
        [ { maxTotalRemoves: true }, false ],
        [ { maxTotalRemoves: "5000" }, false ],
        [ { maxTotalRemoves: 100 }, true ],
        [ { maxTotalRemoves: 5000 }, true ],
        [ { maxTotalRemoves: 500000 }, true ],
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
      internal.ttlProperties({ active: true, frequency: 1000 });

      let tries = 0;
      while (tries++ < 10) {
        internal.wait(1, false);
      
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
      internal.ttlProperties({ active: false, frequency: 1000 });

      let tries = 0;
      while (tries++ < 10) {
        internal.wait(1, false);
      
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

      for (let i = 0; i < 1000; ++i) {
        c.insert({ value: i });
      }

      const oldStats = internal.ttlStatistics();
      
      // reenable
      internal.ttlProperties({ active: true, frequency: 1000, maxTotalRemoves: 100000, maxCollectionRemoves: 100000 });

      let stats = waitForNextRun(c, oldStats, 10);

      // number of runs must have changed, number of deletions must not
      assertNotEqual(stats.runs, oldStats.runs);
      assertEqual(stats.documentsRemoved, oldStats.documentsRemoved);
      
      assertEqual(1000, db._collection(cn).count());
    },
    
    testRemovalsAllNumeric : function () {
      internal.ttlProperties({ active: false });

      let c = db._create(cn, { numberOfShards: 2 });
      c.ensureIndex({ type: "ttl", fields: ["dateCreated"], expireAfter: 1 });

      // dt is one minute in the past
      const dt = new Date((new Date()).getTime() - 1000 * 60).getTime();
      assertTrue(new Date(dt).toISOString() >= "2019-01-");

      for (let i = 0; i < 1000; ++i) {
        c.insert({ dateCreated: dt / 1000, value: i });
      }

      const oldStats = internal.ttlStatistics();
      
      // reenable
      internal.ttlProperties({ active: true, frequency: 1000, maxTotalRemoves: 100000, maxCollectionRemoves: 100000 });
      
      let stats = waitForNextRun(c, oldStats, 10);

      // both number of runs and deletions must have changed
      assertNotEqual(stats.runs, oldStats.runs);
      assertEqual(stats.documentsRemoved, oldStats.documentsRemoved + 1000);

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

      for (let i = 0; i < 1000; ++i) {
        c.insert({ dateCreated: dt, value: i });
      }

      const oldStats = internal.ttlStatistics();
      
      // reenable
      internal.ttlProperties({ active: true, frequency: 1000, maxTotalRemoves: 100000, maxCollectionRemoves: 100000 });
      
      let stats = waitForNextRun(c, oldStats, 10);

      // both number of runs and deletions must have changed
      assertNotEqual(stats.runs, oldStats.runs);
      assertEqual(stats.documentsRemoved, oldStats.documentsRemoved + 1000);

      assertEqual(0, db._collection(cn).count());
    },
    
    testRemovalsAllMixed : function () {
      internal.ttlProperties({ active: false });

      let c = db._create(cn, { numberOfShards: 2 });
      c.ensureIndex({ type: "ttl", fields: ["dateCreated"], expireAfter: 1 });

      // dt is one minute in the past
      let dt = new Date((new Date()).getTime() - 1000 * 60).toISOString();
      assertTrue(dt >= "2019-01-");

      for (let i = 0; i < 1000; ++i) {
        c.insert({ dateCreated: dt, value: i });
      }
      dt = new Date((new Date()).getTime() - 1000 * 60).getTime();
      for (let i = 0; i < 1000; ++i) {
        c.insert({ dateCreated: dt / 1000, value: i });
      }

      const oldStats = internal.ttlStatistics();
      
      // reenable
      internal.ttlProperties({ active: true, frequency: 1000, maxTotalRemoves: 100000, maxCollectionRemoves: 100000 });
      
      let stats = waitForNextRun(c, oldStats, 10);

      // both number of runs and deletions must have changed
      assertNotEqual(stats.runs, oldStats.runs);
      assertEqual(stats.documentsRemoved, oldStats.documentsRemoved + 2000);

      assertEqual(0, db._collection(cn).count());
    },
    
    testRemovalsAllButOneNumeric : function () {
      internal.ttlProperties({ active: false });

      let c = db._create(cn, { numberOfShards: 2 });
      c.ensureIndex({ type: "ttl", fields: ["dateCreated"], expireAfter: 1 });

      // dt is one minute in the past
      const dt = new Date((new Date()).getTime() - 1000 * 60).getTime();
      assertTrue(new Date(dt).toISOString() >= "2019-01-");

      for (let i = 0; i < 1000; ++i) {
        c.insert({ dateCreated: dt / 1000, value: i });
      }
      c.insert({ dateCreated: dt }); // intentionally not divided by 1000

      const oldStats = internal.ttlStatistics();
      
      // reenable
      internal.ttlProperties({ active: true, frequency: 1000, maxTotalRemoves: 100000, maxCollectionRemoves: 100000 });

      let stats = waitForNextRun(c, oldStats, 10);

      // both number of runs and deletions must have changed
      assertNotEqual(stats.runs, oldStats.runs);
      assertEqual(stats.documentsRemoved, oldStats.documentsRemoved + 1000); 

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

      for (let i = 0; i < 1000; ++i) {
        c.insert({ dateCreated: dt, value: i });
      }
      // insert a date in the futue
      const dt2 = new Date((new Date()).getTime() + 1000 * 60).toISOString();
      c.insert({ dateCreated: dt2 });

      const oldStats = internal.ttlStatistics();
      
      // reenable
      internal.ttlProperties({ active: true, frequency: 1000, maxTotalRemoves: 100000, maxCollectionRemoves: 100000 });

      let stats = waitForNextRun(c, oldStats, 10);

      // both number of runs and deletions must have changed
      assertNotEqual(stats.runs, oldStats.runs);
      assertEqual(stats.documentsRemoved, oldStats.documentsRemoved + 1000);

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

      for (let i = 0; i < 10000; ++i) {
        c.insert({ dateCreated: dt / 1000, value: i });
      }

      const oldStats = internal.ttlStatistics();
      
      // reenable
      internal.ttlProperties({ active: true, frequency: 1000, maxTotalRemoves: 100000, maxCollectionRemoves: 100000 });

      let stats = waitForNextRun(c, oldStats, 10);
      
      // both number of runs and deletions must have changed
      assertNotEqual(stats.runs, oldStats.runs);
      assertEqual(stats.documentsRemoved, oldStats.documentsRemoved + 10000);

      assertEqual(0, db._collection(cn).count());
    },
    
    testRemovalsAllBiggerDate : function () {
      internal.ttlProperties({ active: false });

      let c = db._create(cn, { numberOfShards: 2 });
      c.ensureIndex({ type: "ttl", fields: ["dateCreated"], expireAfter: 1 });

      // dt is one minute in the past
      const dt = new Date((new Date()).getTime() - 1000 * 60).toISOString();
      assertTrue(dt >= "2019-01-");

      for (let i = 0; i < 10000; ++i) {
        c.insert({ dateCreated: dt, value: i });
      }

      const oldStats = internal.ttlStatistics();
      
      // reenable
      internal.ttlProperties({ active: true, frequency: 1000, maxTotalRemoves: 100000, maxCollectionRemoves: 100000 });

      let stats = waitForNextRun(c, oldStats, 10);
      
      // both number of runs and deletions must have changed
      assertNotEqual(stats.runs, oldStats.runs);
      assertEqual(stats.documentsRemoved, oldStats.documentsRemoved + 10000);

      assertEqual(0, db._collection(cn).count());
    },
    
    testRemovalsExpireAfterInTheFutureNumeric : function () {
      internal.ttlProperties({ active: false });

      let c = db._create(cn, { numberOfShards: 2 });
      c.ensureIndex({ type: "ttl", fields: ["dateCreated"], expireAfter: 1 });

      // dt is one day in the future
      const dt = new Date((new Date()).getTime() + 1000 * 86400).getTime();
      assertTrue(new Date(dt).toISOString() >= "2019-01-");

      for (let i = 0; i < 1000; ++i) {
        c.insert({ dateCreated: dt / 1000, value: i });
      }

      const oldStats = internal.ttlStatistics();
      
      // reenable
      internal.ttlProperties({ active: true, frequency: 1000, maxTotalRemoves: 100000, maxCollectionRemoves: 100000 });

      let stats = waitForNextRun(c, oldStats, 10);

      // number of runs must have changed, number of deletions must not
      assertNotEqual(stats.runs, oldStats.runs);
      assertEqual(stats.documentsRemoved, oldStats.documentsRemoved);

      assertEqual(1000, db._collection(cn).count());
    },
    
    testRemovalsExpireAfterInTheFutureDate : function () {
      internal.ttlProperties({ active: false });

      let c = db._create(cn, { numberOfShards: 2 });
      c.ensureIndex({ type: "ttl", fields: ["dateCreated"], expireAfter: 1 });

      // dt is one day in the future
      const dt = new Date((new Date()).getTime() + 1000 * 86400).toISOString();
      assertTrue(dt >= "2019-01-");

      for (let i = 0; i < 1000; ++i) {
        c.insert({ dateCreated: dt, value: i });
      }

      const oldStats = internal.ttlStatistics();
      
      // reenable
      internal.ttlProperties({ active: true, frequency: 1000, maxTotalRemoves: 100000, maxCollectionRemoves: 100000 });

      let stats = waitForNextRun(c, oldStats, 10);

      // number of runs must have changed, number of deletions must not
      assertNotEqual(stats.runs, oldStats.runs);
      assertEqual(stats.documentsRemoved, oldStats.documentsRemoved);

      assertEqual(1000, db._collection(cn).count());
    },
    
    testRemovalsExpireAfterSomeInTheFutureNumeric : function () {
      internal.ttlProperties({ active: false });

      let c = db._create(cn, { numberOfShards: 2 });
      c.ensureIndex({ type: "ttl", fields: ["dateCreated"], expireAfter: 1 });

      // dt is one day in the future
      let dt = new Date((new Date()).getTime() + 1000 * 86400).getTime();
      assertTrue(new Date(dt).toISOString() >= "2019-01-");

      for (let i = 0; i < 1000; ++i) {
        c.insert({ dateCreated: dt / 1000, value: i });
      }
      
      // dt is a minute in the past
      dt = new Date((new Date()).getTime() - 1000 * 60).getTime();
      assertTrue(new Date(dt).toISOString() >= "2019-01-");
      
      for (let i = 0; i < 1000; ++i) {
        c.insert({ dateCreated: dt / 1000, value: i });
      }

      assertEqual(2000, db._collection(cn).count());

      const oldStats = internal.ttlStatistics();
      
      // reenable
      let props = internal.ttlProperties({ active: true, frequency: 1000, maxTotalRemoves: 100000, maxCollectionRemoves: 100000 });
      assertTrue(props.active);
      assertEqual(1000, props.frequency);
      assertEqual(100000, props.maxTotalRemoves);
      assertEqual(100000, props.maxCollectionRemoves);

      let stats = waitForNextRun(c, oldStats, 10);

      // both number of runs and deletions must have changed
      assertNotEqual(stats.runs, oldStats.runs);
      assertTrue(stats.documentsRemoved > oldStats.documentsRemoved);

      let tries = 0;
      while (++tries < 20 && db._collection(cn).count() !== 1000) {
        internal.wait(1, false);
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

      for (let i = 0; i < 1000; ++i) {
        c.insert({ dateCreated: dt, value: i });
      }
      
      // dt is a minute in the past
      dt = new Date((new Date()).getTime() - 1000 * 60).toISOString();
      assertTrue(dt >= "2019-01-");
      
      for (let i = 0; i < 1000; ++i) {
        c.insert({ dateCreated: dt, value: i });
      }

      assertEqual(2000, db._collection(cn).count());

      const oldStats = internal.ttlStatistics();
      
      // reenable
      let props = internal.ttlProperties({ active: true, frequency: 1000, maxTotalRemoves: 100000, maxCollectionRemoves: 100000 });
      assertTrue(props.active);
      assertEqual(1000, props.frequency);
      assertEqual(100000, props.maxTotalRemoves);
      assertEqual(100000, props.maxCollectionRemoves);

      let stats = waitForNextRun(c, oldStats, 10);

      // both number of runs and deletions must have changed
      assertNotEqual(stats.runs, oldStats.runs);
      assertTrue(stats.documentsRemoved > oldStats.documentsRemoved);
      
      let tries = 0;
      while (++tries < 20 && db._collection(cn).count() !== 1000) {
        internal.wait(1, false);
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

      for (let i = 0; i < 10000; ++i) {
        c.insert({ dateCreated: dt / 1000, value: i });
      }

      let oldStats = internal.ttlStatistics();
      let oldCount = 10000;  
      
      // reenable
      internal.ttlProperties({ active: true, frequency: 1000, maxTotalRemoves: 10, maxCollectionRemoves: 100000 });
    
      let stats = waitForNextRun(c, oldStats, 10);

      // number of runs, deletions and limitReached must have changed
      assertNotEqual(stats.runs, oldStats.runs);
      assertTrue(stats.limitReached > oldStats.limitReached);
      assertTrue(stats.documentsRemoved > oldStats.documentsRemoved);
      assertTrue(db._collection(cn).count() < oldCount);
      oldCount = db._collection(cn).count();
     
      // wait again for next removal 
      oldStats = stats;
      stats = waitForNextRun(c, oldStats, 10);

      assertNotEqual(stats.runs, oldStats.runs);
      assertTrue(stats.limitReached > oldStats.limitReached);
      assertTrue(stats.documentsRemoved > oldStats.documentsRemoved);
      // wait again, as fetching the stats and acquiring the collection count is not atomic
      oldStats = stats;
      stats = waitForNextRun(c, oldStats, 10);
      assertTrue(db._collection(cn).count() < oldCount || db._collection(cn).count() === 0);
    },
    
    testRemovalsLimitsHitGlobalDate : function () {
      internal.ttlProperties({ active: false });

      let c = db._create(cn, { numberOfShards: 2 });
      c.ensureIndex({ type: "ttl", fields: ["dateCreated"], expireAfter: 1 });

      // dt is one minute in the past
      const dt = new Date((new Date()).getTime() - 1000 * 60).toISOString();
      assertTrue(dt >= "2019-01-");

      for (let i = 0; i < 10000; ++i) {
        c.insert({ dateCreated: dt, value: i });
      }

      let oldStats = internal.ttlStatistics();
      let oldCount = 10000;  
      
      // reenable
      internal.ttlProperties({ active: true, frequency: 1000, maxTotalRemoves: 10, maxCollectionRemoves: 100000 });
    
      let stats = waitForNextRun(c, oldStats, 10);

      // number of runs, deletions and limitReached must have changed
      assertNotEqual(stats.runs, oldStats.runs);
      assertTrue(stats.limitReached > oldStats.limitReached);
      assertTrue(stats.documentsRemoved > oldStats.documentsRemoved);
      assertTrue(db._collection(cn).count() < oldCount);
      oldCount = db._collection(cn).count();
     
      // wait again for next removal 
      oldStats = stats;
      stats = waitForNextRun(c, oldStats, 10);

      assertNotEqual(stats.runs, oldStats.runs);
      assertTrue(stats.limitReached > oldStats.limitReached);
      assertTrue(stats.documentsRemoved > oldStats.documentsRemoved);
      // wait again, as fetching the stats and acquiring the collection count is not atomic
      oldStats = stats;
      stats = waitForNextRun(c, oldStats, 10);
      assertTrue(db._collection(cn).count() < oldCount || db._collection(cn).count() === 0);
    },
    
    testRemovalsLimitsHitCollectionNumeric : function () {
      internal.ttlProperties({ active: false });

      let c = db._create(cn, { numberOfShards: 2 });
      c.ensureIndex({ type: "ttl", fields: ["dateCreated"], expireAfter: 1 });

      // dt is one minute in the past
      const dt = new Date((new Date()).getTime() - 1000 * 60).getTime();
      assertTrue(new Date(dt).toISOString() >= "2019-01-");

      for (let i = 0; i < 10000; ++i) {
        c.insert({ dateCreated: dt / 1000, value: i });
      }

      let oldStats = internal.ttlStatistics();
      let oldCount = 10000;  
      
      // reenable
      internal.ttlProperties({ active: true, frequency: 1000, maxTotalRemoves: 1000, maxCollectionRemoves: 2000 });
    
      let stats = waitForNextRun(c, oldStats, 10);

      // number of runs, deletions and limitReached must have changed
      assertNotEqual(stats.runs, oldStats.runs);
      assertTrue(stats.limitReached > oldStats.limitReached);
      assertTrue(stats.documentsRemoved > oldStats.documentsRemoved);
      assertTrue(db._collection(cn).count() < oldCount);
      oldCount = db._collection(cn).count();
     
      // wait again for next removal 
      oldStats = stats;
      stats = waitForNextRun(c, oldStats, 10);

      assertNotEqual(stats.runs, oldStats.runs);
      assertTrue(stats.limitReached > oldStats.limitReached);
      assertTrue(stats.documentsRemoved > oldStats.documentsRemoved);
      // wait again, as fetching the stats and acquiring the collection count is not atomic
      oldStats = stats;
      stats = waitForNextRun(c, oldStats, 10);
      assertTrue(db._collection(cn).count() < oldCount || db._collection(cn).count() === 0);
    },
    
    testRemovalsLimitsHitCollectionDate : function () {
      internal.ttlProperties({ active: false });

      let c = db._create(cn, { numberOfShards: 2 });
      c.ensureIndex({ type: "ttl", fields: ["dateCreated"], expireAfter: 1 });

      // dt is one minute in the past
      const dt = new Date((new Date()).getTime() - 1000 * 60).toISOString();
      assertTrue(dt >= "2019-01-");

      for (let i = 0; i < 10000; ++i) {
        c.insert({ dateCreated: dt, value: i });
      }

      let oldStats = internal.ttlStatistics();
      let oldCount = 10000;  
      
      // reenable
      internal.ttlProperties({ active: true, frequency: 1000, maxTotalRemoves: 1000, maxCollectionRemoves: 2000 });
    
      let stats = waitForNextRun(c, oldStats, 10);

      // number of runs, deletions and limitReached must have changed
      assertNotEqual(stats.runs, oldStats.runs);
      assertTrue(stats.limitReached > oldStats.limitReached);
      assertTrue(stats.documentsRemoved > oldStats.documentsRemoved);
      assertTrue(db._collection(cn).count() < oldCount);
      oldCount = db._collection(cn).count();
     
      // wait again for next removal 
      oldStats = stats;
      stats = waitForNextRun(c, oldStats, 10);

      assertNotEqual(stats.runs, oldStats.runs);
      assertTrue(stats.limitReached > oldStats.limitReached);
      assertTrue(stats.documentsRemoved > oldStats.documentsRemoved);
      // wait again, as fetching the stats and acquiring the collection count is not atomic
      oldStats = stats;
      stats = waitForNextRun(c, oldStats, 10);
      assertTrue(db._collection(cn).count() < oldCount || db._collection(cn).count() === 0);
    },
  
  };
}

jsunity.run(TtlSuite);

return jsunity.done();
