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
  };
}

// BTS-2279: Test suite for writeConcern validation and _canWrite flag behavior
function WriteConcernValidationSuite() {
  const dbName = "UnitTestsWriteConcernDb";
  const cn = "col";

  return {
    setUp : function () {
      db._createDatabase(dbName);
      db._useDatabase(dbName);
    },

    tearDown : function () {
      global.instanceManager.debugClearFailAt();
      db._useDatabase("_system");
      db._dropDatabase(dbName);
    },

    // BTS-2279: Changing writeConcern should properly invalidate the
    // _canWrite flag in FollowerInfo, allowing writes to continue.
    testWritesAfterChangingWriteConcern : function () {
      const coll = db._create(cn, {replicationFactor: 2, writeConcern: 1, numberOfShards: 3}, {waitForSyncReplication: true});

      coll.insert({_key: "test1"});

      coll.properties({writeConcern: 2});

      coll.insert({_key: "test2"});

      assertEqual(2, coll.count());
      assertTrue(coll.document("test1") !== null);
      assertTrue(coll.document("test2") !== null);
    },

    // BTS-2279: When followers are down and writeConcern cannot be satisfied,
    // inserts should fail because the shard becomes read-only.
    testWritesFailWhenWriteConcernNotSatisfied : function () {
      const coll = db._create(cn, {replicationFactor: 2, writeConcern: 2, numberOfShards: 1}, {waitForSyncReplication: true});

      coll.insert({_key: "test1"});
      assertEqual(1, coll.count());

      const [_, [__, follower]] = Object.entries(coll.shards(true))[0];

      try {
        // Use fail points to block inserts on the follower and prevent re-sync
        // This is more reliable than suspending the server (which triggers supervision)
        global.instanceManager.debugSetFailAt("LogicalCollection::insert", '', follower);
        global.instanceManager.debugSetFailAt("SynchronizeShard::disable", '', follower);

        // First insert after fail points: drops the follower but succeeds
        coll.insert({_key: "test2"});

        // Second insert should fail because writeConcern: 2 cannot be satisfied
        // with only the leader (follower was dropped from in-sync list).
        // The shard becomes read-only when there are fewer in-sync replicas than writeConcern.
        try {
          coll.insert({_key: "test3"});
          fail("Expected insert to fail because shard is read-only");
        } catch(e) {
          // Expected: ERROR_ARANGO_READ_ONLY (1004) - "collection is read-only"
          // This happens when the shard has fewer in-sync replicas than writeConcern
          assertEqual(internal.errors.ERROR_ARANGO_READ_ONLY.code, e.errorNum,
            "Expected ERROR_ARANGO_READ_ONLY but got: " + JSON.stringify(e));
        }
      } finally {
        // Always clear fail points, even if the test fails
        global.instanceManager.debugClearFailAt();
      }
    },
  };
}

jsunity.run(CollectionPropertiesPropagationSuite);
jsunity.run(WriteConcernValidationSuite);

return jsunity.done();
