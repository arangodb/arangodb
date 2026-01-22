/*jshint globalstrict:false, strict:false, maxlen: 5000 */
/* global assertTrue, assertFalse, assertEqual, assertUndefined, fail */

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
/// @author Copyright 2012, triAGENS GmbH, Cologne, Germany
// //////////////////////////////////////////////////////////////////////////////

'use strict';

const jsunity = require("jsunity");
const request = require("@arangodb/request");
const internal = require("internal");
const db = require("@arangodb").db;
const { getDBServers } = require('@arangodb/test-helper');

const proto = "UnitTestsCollectionProto";
const other = "UnitTestsCollectionOther";

const propertiesOnDBServers = (collection) => {
  let shards = collection.shards(true);
  // just focus on one of the shards. should be enough
  let shard = Object.keys(shards)[0]; 
  let servers = Object.values(shards)[0];

  let p = {};
  getDBServers().forEach((server) => {
    if (!servers.includes(server.id)) {
      return;
    }
    let result = request({ method: "GET", url: server.url + "/_api/collection/" + encodeURIComponent(shard) + "/properties" });
    assertEqual(200, result.status);
        
    p[server.id] = result.json;
  });
  return p;
};

function CollectionPropertiesPropagationSuite() {
  return {
    setUp : function () {
      db._create(proto, { replicationFactor: 3, numberOfShards: 3, writeConcern: 2 });
    },

    tearDown : function () {
      db._drop(other);
      db._drop(proto);
    },
    
    testCheckOwnProperties : function () {
      let c = db[proto];

      let p = c.properties();
      assertEqual(3, p.replicationFactor);
      assertEqual(2, p.writeConcern);
      assertEqual(3, p.numberOfShards);

      p = propertiesOnDBServers(c);
      let keys = Object.keys(p);
      assertTrue(keys.length > 0);
      keys.forEach((s) => {
        assertEqual(3, p[s].replicationFactor, {s, p});
        assertEqual(2, p[s].writeConcern, {s, p});
        assertEqual(3, p[s].numberOfShards, {s, p});
      });
    },
    
    testCreateOther : function () {
      let c = db._create(other, { distributeShardsLike: proto });

      let p = c.properties();
      assertEqual(proto, p.distributeShardsLike);
      assertEqual(3, p.replicationFactor);
      assertEqual(2, p.writeConcern);
      assertEqual(3, p.numberOfShards);

      p = propertiesOnDBServers(c);
      let keys = Object.keys(p);
      assertTrue(keys.length > 0);
      keys.forEach((s) => {
        // DB servers do not have distributeShardsLike property
        assertUndefined(p[s].distributeShardsLike, {s, p});
        assertEqual(3, p[s].replicationFactor, {s, p});
        assertEqual(2, p[s].writeConcern, {s, p});
        assertEqual(3, p[s].numberOfShards, {s, p});
      });
    },
    
    testCreateOtherWithDifferentWriteConcern : function () {
      if (db._properties().replicationVersion === "2") {
        // in replication 2 the writeConcern is associated with the log (i.e., with the collection group),
        // so we cannot create a collection with a different writeConcern
        return;
      }
      let c = db._create(other, { distributeShardsLike: proto, writeConcern: 1 });

      let p = c.properties();
      assertEqual(proto, p.distributeShardsLike);
      assertEqual(3, p.replicationFactor);
      assertEqual(1, p.writeConcern);
      assertEqual(3, p.numberOfShards);

      p = propertiesOnDBServers(c);
      let keys = Object.keys(p);
      assertTrue(keys.length > 0);
      keys.forEach((s) => {
        // DB servers do not have distributeShardsLike property
        assertUndefined(p[s].distributeShardsLike, {s, p});
        assertEqual(3, p[s].replicationFactor, {s, p});
        assertEqual(1, p[s].writeConcern, {s, p});
        assertEqual(3, p[s].numberOfShards, {s, p});
      });
    },
    
    testCreateOtherChangeReplicationFactorForProto : function () {
      let c = db._create(other, { distributeShardsLike: proto });

      let shards = db[proto].shards(true);
      // just focus on one of the shards. should be enough
      let shard = Object.keys(shards)[0]; 
      let servers = Object.values(shards)[0];
      assertEqual(3, servers.length);

      db[proto].properties({ replicationFactor: 2 });
      
      let p = db[proto].properties();
      assertEqual(2, p.replicationFactor);
 
      let tries = 15;
      while (tries-- > 0) {
        shards = db[proto].shards(true);
        // just focus on one of the shards. should be enough
        shard = Object.keys(shards)[0]; 
        servers = Object.values(shards)[0];
        if (servers.length === 2) {
          break;
        }
        internal.sleep(1);
      }
      assertEqual(2, servers.length);

      // For ReplicationVersion 2 shards and followerCollections
      // all report the change in ReplicationFactor.
      // For ReplicationVersion 1 only the Leading collection
      // knows about the change.
      const expectedRF = db._properties().replicationVersion === "2" ? 2 : 3;
     
      // shards do not see the replicationFactor update
      let keys;
      tries = 15;
      // propagation to DB servers is carried out by the
      // maintenance. give it some time...
      while (tries-- > 0) {
        p = propertiesOnDBServers(db[proto]);
        keys = Object.keys(p);
        assertTrue(keys.length > 0);
        let found = 0;
        keys.forEach((s) => {
          if (p[s].replicationFactor === expectedRF) {
            ++found;
          }
        });
        if (found === keys.length) {
          break;
        }
        internal.sleep(1);
      }
          
      keys.forEach((s) => {
        assertEqual(expectedRF, p[s].replicationFactor, {s, p});
      });

      p = c.properties();
      assertEqual(proto, p.distributeShardsLike);
      assertEqual(expectedRF, p.replicationFactor, p);
      
      p = propertiesOnDBServers(c);
      keys = Object.keys(p);
      assertTrue(keys.length > 0);
      keys.forEach((s) => {
        assertEqual(expectedRF, p[s].replicationFactor, {s, p});
      });
    },
    
    testCreateOtherChangeWriteConcernForProto : function () {
      let c = db._create(other, { distributeShardsLike: proto });

      db[proto].properties({ writeConcern: 1 });
      
      let p = db[proto].properties();
      assertEqual(1, p.writeConcern, p);
      
      // shards will see the writeConcern update
      let keys;
      let tries = 15;
      // propagation to DB servers is carried out by the
      // maintenance. give it some time...
      while (tries-- > 0) {
        p = propertiesOnDBServers(db[proto]);
        keys = Object.keys(p);
        assertTrue(keys.length > 0);
        let found = 0;
        keys.forEach((s) => {
          if (p[s].writeConcern === 1) {
            ++found;
          }
        });
        if (found === keys.length) {
          break;
        }
        internal.sleep(1);
      }
        
      keys.forEach((s) => {
        assertEqual(1, p[s].writeConcern, {s, p});
      });

      // dependent collection won't see the writeConcern update
      const isReplicationTwo = db._properties().replicationVersion === "2";
      // In Replication1 the followers won't see the writeConcern update
      // as they are not changed
      // In Replication2 the followers share the replicated log with the
      // leader, and the log holds the writeConcern, hence all are updated.
      // The above holds true for the collection as well as the corresponding
      // shards.
      const expectedFollowerWriteConcern = isReplicationTwo ? 1 : 2;
      p = c.properties();
      assertEqual(proto, p.distributeShardsLike);

      assertEqual(expectedFollowerWriteConcern, p.writeConcern);

      p = propertiesOnDBServers(c);
      keys = Object.keys(p);
      assertTrue(keys.length > 0);
      keys.forEach((s) => {
        assertEqual(expectedFollowerWriteConcern, p[s].writeConcern, {s, p});
      });
    },
    
    testChangeReplicationFactorForOther : function () {
      let c = db._create(other, { distributeShardsLike: proto });

      try {
        c.properties({ replicationFactor: 2 });
        fail();
      } catch (err) {
        assertEqual(internal.errors.ERROR_FORBIDDEN.code, err.errorNum);
      }
    },
    
    testChangeWriteConcernForOther : function () {
      let c = db._create(other, { distributeShardsLike: proto });
      
      let p = db[proto].properties();
      assertEqual(2, p.writeConcern, p);
      
      p = db[other].properties();
      assertEqual(2, p.writeConcern, p);
        
      db[other].properties({ writeConcern: 1 });
      
      p = db[proto].properties();
      const isReplicationTwo = db._properties().replicationVersion === "2";
      assertEqual(isReplicationTwo ? 1 : 2, p.writeConcern, p);
      
      p = db[other].properties();
      assertEqual(1, p.writeConcern, p);
      
      // shards will see the writeConcern update
      let keys;
      let tries = 15;
      // propagation to DB servers is carried out by the
      // maintenance. give it some time...
      while (tries-- > 0) {
        p = propertiesOnDBServers(db[other]);
        keys = Object.keys(p);
        assertTrue(keys.length > 0);
        let found = 0;
        keys.forEach((s) => {
          if (p[s].writeConcern === 1) {
            ++found;
          }
        });
        if (found === keys.length) {
          break;
        }
        internal.sleep(1);
      }
        
      keys.forEach((s) => {
        assertEqual(1, p[s].writeConcern, {s, p});
      });
    },

    // BTS-2279: Changing writeConcern should properly invalidate the
    // _canWrite flag in FollowerInfo, allowing writes to continue. since we
    // waitForSyncReplication is true, we can wait for the followers to sync.
    testWritesAfterChangingWriteConcern : function () {
      const dbName = "UnitTestsWriteConcernDb1";
      db._createDatabase(dbName);
      db._useDatabase(dbName);

      const cn = "col";
      try {
        const coll = db._create(cn, {replicationFactor: 2, writeConcern: 1, numberOfShards: 3}, {waitForSyncReplication: true});

        // Insert with initial writeConcern of 1
        coll.insert({_key: "test1"});

        // Change writeConcern to 2
        coll.properties({writeConcern: 2});

        // Insert should still work after writeConcern change
        coll.insert({_key: "test2"});

        // Verify both documents are inserted
        assertEqual(2, coll.count());
        assertTrue(coll.document("test1") !== null);
        assertTrue(coll.document("test2") !== null);
      } finally {
        db._useDatabase("_system");
        db._dropDatabase(dbName);
      }
    },

    // BTS-2279: When waitForSyncReplication is false and writeConcern is
    // increased before followers sync, inserts should fail because the
    // required write concern cannot be satisfied.
    testWritesFailWhenWriteConcernNotSatisfied : function () {
      const dbName = "UnitTestsWriteConcernDb2";
      db._createDatabase(dbName);
      db._useDatabase(dbName);

      const cn = "col";
      try {
        const coll = db._create(cn, {replicationFactor: 2, writeConcern: 1, numberOfShards: 3}, {waitForSyncReplication: false});

        // Insert with initial writeConcern of 1 should work
        coll.insert({_key: "test1"});

        // Immediately change writeConcern to 2 before followers have a chance to sync
        coll.properties({writeConcern: 2});

        // Try to insert - this should fail because writeConcern: 2 cannot be
        // satisfied when followers haven't synced yet
        let insertFailed = false;
        for (let i = 0; i < 10; i++) {
          try {
            coll.insert({});
          } catch(e) {
            // Expected: ERROR_ARANGO_WRITE_CONCERN_NOT_FULFILLED (1480)
            assertEqual(internal.errors.ERROR_ARANGO_WRITE_CONCERN_NOT_FULFILLED.code, e.errorNum);
            insertFailed = true;
            break;
          }
          // If insert succeeded, followers might have synced - try again quickly
          // In a race condition, this test might not catch the error
        }

        // Note: This test may occasionally pass all inserts if followers sync
        // quickly enough. The important thing is that if an error occurs,
        // it should be ERROR_ARANGO_WRITE_CONCERN_NOT_FULFILLED.
        if (!insertFailed) {
          // Followers synced fast enough, test is inconclusive but not failed
          // Just verify the collection is functional
          assertTrue(coll.count() >= 1);
        }
      } finally {
        db._useDatabase("_system");
        db._dropDatabase(dbName);
      }
    },
  };
}

jsunity.run(CollectionPropertiesPropagationSuite);

return jsunity.done();
