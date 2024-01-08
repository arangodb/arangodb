/*jshint globalstrict:false, strict:false, maxlen: 5000 */
/* global assertTrue, assertFalse, assertEqual, assertUndefined, fail */

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
          if (p[s].replicationFactor === 3) {
            ++found;
          }
        });
        if (found === keys.length) {
          break;
        }
        internal.sleep(1);
      }
          
      keys.forEach((s) => {
        assertEqual(3, p[s].replicationFactor, {s, p});
      });

      p = c.properties();
      assertEqual(proto, p.distributeShardsLike);
      let expectedRF;
      if (db._properties().replicationVersion === "2") {
        // with replication 2, the collection's replicationFactor is tied to the replicationFactor
        // of the collection group, so it will report the correct value#
        assertEqual(2, p.replicationFactor, p);
      } else {
        // with replication1, dependent collection won't see the replicationFactor update
        assertEqual(3, p.replicationFactor, p);
      }
      
      // the individual shards currently do not report the updated replicationFactor,
      // neither for replication 1 nor 2
      p = propertiesOnDBServers(c);
      keys = Object.keys(p);
      assertTrue(keys.length > 0);
      keys.forEach((s) => {
        assertEqual(3, p[s].replicationFactor, {s, p});
      });
    },
    
    testCreateOtherChangeWriteConcernForProto : function () {
      let c = db._create(other, { distributeShardsLike: proto });

      db[proto].properties({ writeConcern: 1 });
      
      let p = db[proto].properties();
      assertEqual(1, p.writeConcern);
      
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
      p = c.properties();
      assertEqual(proto, p.distributeShardsLike);
      assertEqual(2, p.writeConcern);

      // nor do its shards
      p = propertiesOnDBServers(c);
      keys = Object.keys(p);
      assertTrue(keys.length > 0);
      keys.forEach((s) => {
        assertEqual(2, p[s].writeConcern, {s, p});
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

      try {
        c.properties({ writeConcern: 1 });
        fail();
      } catch (err) {
        assertEqual(internal.errors.ERROR_FORBIDDEN.code, err.errorNum);
      }
    },
  };
}

jsunity.run(CollectionPropertiesPropagationSuite);

return jsunity.done();
