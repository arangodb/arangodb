/*jshint globalstrict:false, strict:false */
/*global arango, assertTrue, assertEqual, assertNotEqual, assertMatch, assertNull, fail */

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
/// @author Jan Christoph Uhde
/// @author Copyright 2019, triAGENS GmbH, Cologne, Germany
// //////////////////////////////////////////////////////////////////////////////

const jsunity = require("jsunity");
const arangodb = require("@arangodb");
const db = arangodb.db;
const internal = require('internal');
const ERRORS = arangodb.errors;
const isEnterprise = internal.isEnterprise();
const isCluster = internal.isCluster();
const request = require('@arangodb/request');

const defaultReplicationFactor = db._properties().replicationFactor;
const replication2Enabled = require('internal').db._version(true).details['replication2-enabled'] === 'true';

let {
  getMaxReplicationFactor,
  getMinReplicationFactor,
  agency,
} = require("@arangodb/test-helper");

const maxReplicationFactor = getMaxReplicationFactor();
const minReplicationFactor = getMinReplicationFactor(); 
        
function assertNoDatabasesInPlan () {
  if (!isCluster) {
    return;
  }
  const prefix = arango.POST("/_admin/execute", `return global.ArangoAgency.prefix()`);
  const paths = [
      "Plan/Databases/",
    ].map(p => `${prefix}/${p}`);
  let result = agency.call("read", [ paths ]);
  assertTrue(Array.isArray(result), result);
  assertEqual(1, result.length);
  result = result[0];
  
  let parts = paths[0].split("/").filter((p) => p.trim() !== "");
  parts.forEach((part) => {
    assertTrue(result.hasOwnProperty(part), { result, part });
    result = result[part];
  });
  let keys = Object.keys(result);
  assertEqual(["_system"], keys, result);
}

function getEndpointsByType(type) {
  const isType = (d) => (d.instanceRole === type);
  const toEndpoint = (d) => (d.endpoint);
  const endpointToURL = (endpoint) => {
    if (endpoint.substr(0, 6) === 'ssl://') {
      return 'https://' + endpoint.substr(6);
    }
    let pos = endpoint.indexOf('://');
    if (pos === -1) {
      return 'http://' + endpoint;
    }
    return 'http' + endpoint.substr(pos);
  };

  const instanceManager = JSON.parse(internal.env.INSTANCEINFO);
  return instanceManager.arangods.filter(isType)
                              .map(toEndpoint)
                              .map(endpointToURL);
}

function checkDBServerSharding(db, expected) {
  // connect to all db servers and check if they picked up the
  // "sharding" attribute correctly
  if (!require('@arangodb').isServer) {
    // request module can only be used inside arangosh tests
    let endpoints = getEndpointsByType("dbserver");
    assertTrue(endpoints.length > 0);
    endpoints.forEach((ep) => {
      let res = request.get({ url: ep + "/_db/" + encodeURIComponent(db) + "/_api/database/current" });
      assertEqual(200, res.status);
      assertEqual(expected, res.json.result.sharding);
    });
  }
}

function OneShardPropertiesSuite () {
  var dn = "UnitTestsDB";

  return {
    setUp: function () {
      try {
        db._useDatabase("_system");
        db._dropDatabase(dn);
      } catch (ex) {
      }
    },

    tearDown: function () {
      try {
        db._useDatabase("_system");
        db._dropDatabase(dn);
      } catch (ex) {
      }
    },

    testDefaultValues : function () {
      assertTrue(db._createDatabase(dn));
      db._useDatabase(dn);
      let props = db._properties();
      if (isCluster) {
        assertEqual(props.sharding, "");
        assertEqual(props.replicationFactor, defaultReplicationFactor);
      } else {
        assertEqual(props.sharding, undefined);
        assertEqual(props.replicationFactor, undefined);
      }
    },
    
    testDefaultValuesOverridden : function () {
      assertTrue(db._createDatabase(dn, { replicationFactor: 2, writeConcern: 2, sharding: "single" }));
      db._useDatabase(dn);
      let props = db._properties();
      if (isCluster) {
        assertEqual(props.sharding, "single");
        assertEqual(props.replicationFactor, 2);
        assertEqual(props.writeConcern, 2);
        if (props.replicationVersion === "2") {
          try {
            // With Replication2 the writeConcern is defined by the CollectionGroup
            // it cannot be individual by collection anymore.
            db._create("test", {writeConcern: 1});
            fail();
          } catch (err) {
            assertEqual(ERRORS.ERROR_BAD_PARAMETER.code, err.errorNum);
          }
          // for compatibility, the other parameters should still be ignored - this behavior is now deprecated, though.
          let c = db._create("test", { replicationFactor: 1, numberOfShards: 2, distributeShardsLike: "" });
          props = c.properties();
          assertEqual(2, props.writeConcern);
          assertEqual(2, props.replicationFactor);
          assertEqual(1, props.numberOfShards);
        } else {
          let c = db._create("test", { writeConcern: 1, replicationFactor: 1, numberOfShards: 2, distributeShardsLike: "" });
          props = c.properties();
          assertEqual(1, props.writeConcern);
          assertEqual(2, props.replicationFactor);
          assertEqual(1, props.numberOfShards);
        }


        /* Expected implementation, if we can drop backwards compatibility with 3.11
        try {
          // Disallow using a different distributeShardsLike
          db._create("test", {distributeShardsLike: ""});
          fail();
        } catch (err) {
          assertEqual(ERRORS.ERROR_BAD_PARAMETER.code, err.errorNum);
        }
        try {
          // Disallow using a different numberOfShards
          db._create("test", {numberOfShards: 2});
          fail();
        } catch (err) {
          assertEqual(ERRORS.ERROR_BAD_PARAMETER.code, err.errorNum);
        }
        try {
          // Disallow using a different replicationFactor
          db._create("test", {replicationFactor: 1});
          fail();
        } catch (err) {
          assertEqual(ERRORS.ERROR_BAD_PARAMETER.code, err.errorNum);
        }
        if (replication2Enabled) {
          try {
            // With Replication2 the writeConcern is defined by the CollectionGroup
            // it cannot be individual by collection anymore.
            db._create("test", {writeConcern: 1});
            fail();
          } catch (err) {
            assertEqual(ERRORS.ERROR_BAD_PARAMETER.code, err.errorNum);
          }
        } else {
          try {
            // Allow using a different writeConcern
            db._create("test", {writeConcern: 1});
            // For Replication1 this should be allowed.
            // WriteConcern is per Collection basis
          } catch (err) {
            fail();
          } finally {
            db._drop("test");
          }
        }

        // Allow creation where all values match
        let c = db._create("test", {writeConcern: 2, replicationFactor: 2, numberOfShards: 1});
        props = c.properties();
        assertEqual(1, props.writeConcern);
        assertEqual(2, props.replicationFactor);
        assertEqual(1, props.numberOfShards);
        */

        checkDBServerSharding(dn, "single");
      } else {
        assertEqual(props.sharding, undefined);
        assertEqual(props.replicationFactor, undefined);
        assertEqual(props.writeConcern, undefined);
      }
    },

    testCreationWithDistributeShardsLike : function () {
      if (isCluster) {
        assertTrue(db._createDatabase(dn, { replicationFactor: 2, writeConcern: 2, sharding: "single" }));
        db._useDatabase(dn);
        // We need to create a new collection that we can use as a leader
        db._create("leading");
        try {
          const c = db._create("test", {distributeShardsLike: "leading"});
          // We can create an illegal distribute shards like. For one shard
          // it will just be ignored
          const props = c.properties();
          assertEqual(2, props.writeConcern);
          assertEqual(2, props.replicationFactor);
          assertEqual(1, props.numberOfShards);
          assertEqual("_graphs", props.distributeShardsLike);
        } finally {
          db._drop("test");
        }
        /* Expected Test Code as soon as we drop compatibility with 3.11
        try {
          db._create("test", {distributeShardsLike: "leading"});
          fail();
        } catch (err) {
          // We cannot create the collection, as the leader is not allowed! We will have a chain of distributeShardsLike
          assertEqual(ERRORS.ERROR_CLUSTER_CHAIN_OF_DISTRIBUTESHARDSLIKE.code, err.errorNum);
        }
        */
        // It should be allowed to create a collection following the OneShard leader (_graphs for every non _system db)
        const c = db._create("test", {distributeShardsLike: "_graphs"});
        const props = c.properties();
        assertEqual(2, props.writeConcern);
        assertEqual(2, props.replicationFactor);
        assertEqual(1, props.numberOfShards);

        checkDBServerSharding(dn, "single");
      }
    },
    
    testShardingFlexible : function () {
      assertTrue(db._createDatabase(dn, { sharding: "flexible" }));
      db._useDatabase(dn);
      let props = db._properties();
      if (isCluster) {
        assertEqual(props.sharding, "");
        assertEqual(props.replicationFactor, defaultReplicationFactor);
        
        checkDBServerSharding(dn, "");
      } else {
        assertEqual(props.sharding, undefined);
        assertEqual(props.replicationFactor, undefined);
      }
    },

    testDeviatingWriteConcernAndMinReplicationFactorForDatabase : function () {
      if (!isCluster) {
        return;
      }
      try {
        db._createDatabase(dn, { replicationFactor: 2, minReplicationFactor: 1, writeConcern: 2, sharding: "flexible" });
        fail();
      } catch (err) {
        assertEqual(ERRORS.ERROR_BAD_PARAMETER.code, err.errorNum);
      }
        
      assertTrue(db._createDatabase(dn, { replicationFactor: 2, minReplicationFactor: 2, writeConcern: 2, sharding: "flexible" }));
    },
    
    testNormalDBAndTooManyServers : function () {
      if (!isCluster) {
        return;
      }
      try {
        db._createDatabase(dn, { replicationFactor : 5 });
        fail();
      } catch (err) {
        assertEqual(ERRORS.ERROR_CLUSTER_INSUFFICIENT_DBSERVERS.code, err.errorNum);
      }
      try {
        db._createDatabase(dn, { writeConcern : 5 });
        fail();
      } catch (err) {
        assertEqual(ERRORS.ERROR_CLUSTER_INSUFFICIENT_DBSERVERS.code, err.errorNum);
      }
    },
    
    testNormalDBAndTooManyServers2 : function () {
      if (!isCluster) {
        return;
      }
      db._createDatabase(dn, { replicationFactor : 2 });
      db._useDatabase(dn);
      try {
        db._create("oneshardcol", { replicationFactor : 5 });
        fail();
      } catch (err) {
        assertEqual(ERRORS.ERROR_CLUSTER_INSUFFICIENT_DBSERVERS.code, err.errorNum);
      }
      try {
        db._create("oneshardcol", { writeConcern : 5 });
        fail();
      } catch (err) {
        assertEqual(ERRORS.ERROR_BAD_PARAMETER.code, err.errorNum);
      }
    },
    
    testNormalDBAndTooManyServers3 : function () {
      if (!isCluster) {
        return;
      }
      db._createDatabase(dn, { writeConcern: 2, replicationFactor : 2 });
      db._useDatabase(dn);
      try {
        db._create("oneshardcol", { replicationFactor : 5 });
        fail();
      } catch (err) {
        assertEqual(ERRORS.ERROR_CLUSTER_INSUFFICIENT_DBSERVERS.code, err.errorNum);
      }
      try {
        db._create("oneshardcol", { writeConcern : 5 });
        fail();
      } catch (err) {
        assertEqual(ERRORS.ERROR_BAD_PARAMETER.code, err.errorNum);
      }
    },
    
    testOneShardDBAndTooManyServers : function () {
      if (!isCluster) {
        return;
      }
      try {
        db._createDatabase(dn, { sharding : "single", replicationFactor : 5 });
        fail();
      } catch (err) {
        assertEqual(ERRORS.ERROR_CLUSTER_INSUFFICIENT_DBSERVERS.code, err.errorNum);
      }
      try {
        db._createDatabase(dn, { sharding : "single", writeConcern : 5 });
        fail();
      } catch (err) {
        assertEqual(ERRORS.ERROR_CLUSTER_INSUFFICIENT_DBSERVERS.code, err.errorNum);
      }
    },
    
    testOneShardDBAndTooManyServers2 : function () {
      if (!isCluster) {
        return;
      }
      db._createDatabase(dn, { sharding : "single", replicationFactor : 2 });
      db._useDatabase(dn);
      try {
        // WriteConcern is specific to collection and is now greater than replicationFactor, which is disallowed
        db._create("oneshardcol", { writeConcern : 5 });
        fail();
      } catch (err) {
        assertEqual(ERRORS.ERROR_BAD_PARAMETER.code, err.errorNum);
      }
    },
    
    testOneShardDBAndTooManyServers3 : function () {
      if (!isCluster) {
        return;
      }
      db._createDatabase(dn, { sharding : "single", writeConcern: 2, replicationFactor : 2 });
      db._useDatabase(dn);
      try {
        // WriteConcern is specific to collection and is now greater than replicationFactor, which is disallowed
        db._create("oneshardcol", { writeConcern : 5 });
        fail();
      } catch (err) {
        assertEqual(ERRORS.ERROR_BAD_PARAMETER.code, err.errorNum);
      }
    },
    
    testOneShardDBAndOverrides : function () {
      assertTrue(db._createDatabase(dn, { sharding : "single", replicationFactor : 2 }));

      db._useDatabase(dn);
      let props = db._properties();
      if (!isCluster) {
        assertEqual(props.sharding, undefined);
        assertEqual(props.replicationFactor, undefined);
      } else {
        assertEqual(props.sharding, "single", props);
        assertEqual(props.replicationFactor, 2, props);

        {
          let col = db._create("oneshardcol");
          let colProperties = col.properties();
          let graphsProperties = db._collection("_graphs").properties();

          if (isCluster && isEnterprise) {
            assertEqual(colProperties.distributeShardsLike, "_graphs");
            assertEqual(colProperties.replicationFactor, graphsProperties.replicationFactor);
          }
        }

        {
          // we want to create a normal collection
          let col = db._create("overrideOneShardCollection", { distributeShardsLike: ""});
          let colProperties = col.properties();

          if (isCluster) {
            assertEqual(colProperties.distributeShardsLike, "_graphs");
            assertEqual(colProperties.replicationFactor, db._properties().replicationFactor);
          }
        }
        /* Expected Test Code as soon as we drop compatibility with 3.11
        if (isCluster) {
          try {
            // We set distributeShardsLike to an illegal value
            db._create("overrideOneShardCollection", {distributeShardsLike: ""});
            fail();
          } catch (err) {
            assertEqual(ERRORS.ERROR_BAD_PARAMETER.code, err.errorNum);
          }
        } else {
          // Should be allowed on single server
          db._create("overrideOneShardCollection", {distributeShardsLike: ""});
        }
        */
        {
          // we want to create a normal collection and have a different replication factor
          let nonDefaultReplicationFactor = 1;
          assertNotEqual(db._properties.ReplicationFactor, nonDefaultReplicationFactor);
          let col = db._create("overrideOneShardAndReplicationFactor", { distributeShardsLike: "", replicationFactor: nonDefaultReplicationFactor });
          let colProperties = col.properties();
          let graphsProperties = db._collection("_graphs").properties();

          if (isCluster) {
            assertEqual(colProperties.distributeShardsLike, "_graphs");
            assertEqual(colProperties.replicationFactor, db._properties().replicationFactor);
          }
        }

        /* Expected Test Code as soon as we drop compatibility with 3.11
        {
          // we want to create a normal collection and have a different replication factor
          let nonDefaultReplicationFactor = 1;
          assertNotEqual(db._properties().replicationFactor, nonDefaultReplicationFactor);
          try {
            // This is not allowed, as the OneShardDatabase as a feature enforces all replicationFactors to
            // be identical
            db._create("overrideOneShardAndReplicationFactor", {replicationFactor: nonDefaultReplicationFactor});
            fail();
          } catch (err) {
            assertEqual(ERRORS.ERROR_BAD_PARAMETER.code, err.errorNum);
          }
        }
        */
      }
    },
    
    testReplicationFactorAndOverrides : function () {
      assertTrue(db._createDatabase(dn, { replicationFactor : 2, writeConcern : 2 }));

      db._useDatabase(dn);
      let props = db._properties();
      if (!isCluster) {
        assertEqual(props.sharding, undefined);
        assertEqual(props.replicationFactor, undefined);
        assertEqual(props.minReplicationFactor, undefined);  // deprecated
        assertEqual(props.writeConcern, undefined);
      } else {
        assertEqual(props.sharding, "");
        assertEqual(props.replicationFactor, 2);
        assertEqual(props.writeConcern, 2);

        {
          let col = db._create("somecollection");
          let colProperties = col.properties();
          assertEqual(colProperties.replicationFactor, 2);
          assertEqual(colProperties.minReplicationFactor, 2);  // deprecated
          assertEqual(colProperties.writeConcern, 2);
        }

        {
          let col = db._create("overrideCollection1", {writeConcern: 1});
          let colProperties = col.properties();
          assertEqual(colProperties.minReplicationFactor, 1);  // deprecated
          assertEqual(colProperties.writeConcern, 1);
        }

        {
          let col = db._create("overrideCollection2", {writeConcern: 1, replicationFactor: 1, numberOfShards: 3});
          let colProperties = col.properties();
          assertEqual(colProperties.minReplicationFactor, 1);  // deprecated
          assertEqual(colProperties.writeConcern, 1);
        }

        if (replication2Enabled) {
          // we want to create a normal collection and have a different replication factor
          try {
            // Write concern is defined by collection group and cannot be changed here
            db._create("overrideCollection3", {distributeShardsLike: "overrideCollection2", writeConcern: 2});
            fail();
          } catch (err) {
            assertEqual(ERRORS.ERROR_BAD_PARAMETER.code, err.errorNum);
          }

          let col2 = db._collection("overrideCollection2");
          // Inheriting the writeConcern from original collection should be allowed, even if default is higher
          let col3 = db._create("overrideCollection3", {distributeShardsLike: "overrideCollection2"});
          let col2Properties = col2.properties();
          let col3Properties = col3.properties();

          assertEqual(col3Properties.distributeShardsLike, col2.name());
          assertEqual(col3Properties.replicationFactor, col2Properties.replicationFactor);
          assertEqual(col3Properties.writeConcern, col2Properties.writeConcern);

          // Explicitly setting the same writeConcern as original collection should be allowed, even if default is higher
          let col4 = db._create("overrideCollection4", {distributeShardsLike: "overrideCollection2", writeConcern: 1});
          let col4Properties = col4.properties();

          assertEqual(col4Properties.distributeShardsLike, col2.name());
          assertEqual(col4Properties.replicationFactor, col2Properties.replicationFactor);
          assertEqual(col4Properties.writeConcern, col2Properties.writeConcern);
        } else {
          // we want to create a normal collection and have a different replication factor
          try {
            // Write concern is not inherited, the replicationFactor is. The default for the database is 2
            // we inherit replication factor 1 by distribute shards like, with default wc 2. This is forbidden,
            // as writeConcern is higher than replicationFactor.
            db._create("overrideCollection3", {distributeShardsLike: "overrideCollection2"});
            fail();
          } catch (err) {
            assertEqual(ERRORS.ERROR_BAD_PARAMETER.code, err.errorNum);
          }

          let col2 = db._collection("overrideCollection2");
          // Giving correct write concern we can do distributeShardsLike properly
          let col3 = db._create("overrideCollection3", {distributeShardsLike: "overrideCollection2", writeConcern: 1});
          let col2Properties = col2.properties();
          let col3Properties = col3.properties();

          assertEqual(col3Properties.distributeShardsLike, col2.name());
          assertEqual(col3Properties.replicationFactor, col2Properties.replicationFactor);
        }
      }
    },

    testSatelliteDB : function () {
      assertTrue(db._createDatabase(dn, { replicationFactor : "satellite" }));
      db._useDatabase(dn);
      let props = db._properties();
      if (!isCluster) {
        assertEqual(props.sharding, undefined);
      } else  {
        assertEqual(props.sharding, "");
        if (isEnterprise) {
          assertEqual(props.replicationFactor, "satellite");
        } else {
          //without enterprise we can not have a replication factor of 1
          assertEqual(props.replicationFactor, defaultReplicationFactor);
        }
      }
    },

    testSatelliteDBWithTooHighWriteConcern : function () {
      if (isEnterprise && isCluster) {
        try {
          db._createDatabase(dn, { replicationFactor : "satellite", sharding: "single", writeConcern: 2 });
          fail();
        } catch (err) {
          assertEqual(ERRORS.ERROR_FAILED.code, err.errorNum);
        }

        assertNoDatabasesInPlan();
      }
    },
    
    testSatelliteDBWithOneShard : function () {
      if (isEnterprise && isCluster) {
        try {
          db._createDatabase(dn, { replicationFactor : "satellite", sharding: "single" });
          fail();
        } catch (err) {
          assertEqual(ERRORS.ERROR_FAILED.code, err.errorNum);
        }
        
        assertNoDatabasesInPlan();
      }
    },

    testValuesBelowMinReplicationFactor : function () {
      let min = minReplicationFactor;
      if (min > 1) {
        try {
          db._createDatabase(dn, { replicationFactor : min - 1 });
          fail();
        } catch (err) {
          assertEqual(ERRORS.ERROR_BAD_PARAMETER.code, err.errorNum);
        }
        
        assertNoDatabasesInPlan();
      }
    },

    testValuesAboveMaxReplicationFactor : function () {
      let max = maxReplicationFactor;
      if (max > 0) {
        try {
          db._createDatabase(dn, { replicationFactor : max + 1 });
          fail();
        } catch (err) {
          assertEqual(ERRORS.ERROR_BAD_PARAMETER.code, err.errorNum);
        }
        
        assertNoDatabasesInPlan();
      }
    },
  
  };
}

jsunity.run(OneShardPropertiesSuite);
return jsunity.done();
