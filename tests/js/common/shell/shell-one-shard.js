/*jshint globalstrict:false, strict:false */
/*global assertTrue, assertEqual, assertNotEqual, assertMatch, assertNull, fail */

////////////////////////////////////////////////////////////////////////////////
/// @brief test the collection interface
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
/// @author Jan Christoph Uhde
/// @author Copyright 2019, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

const jsunity = require("jsunity");
const arangodb = require("@arangodb");
const db = arangodb.db;
const internal = require('internal');
const ERRORS = arangodb.errors;
const isEnterprise = internal.isEnterprise();
const isCluster = internal.isCluster();

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
        assertEqual(props.replicationFactor, 1);
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

        let c = db._create("test", { writeConcern: 1, replicationFactor: 1, numberOfShards: 2, distributeShardsLike: "" });
        props = c.properties();
        assertEqual(2, props.writeConcern);
        assertEqual(2, props.replicationFactor);
        assertEqual(1, props.numberOfShards);
      } else {
        assertEqual(props.sharding, undefined);
        assertEqual(props.replicationFactor, undefined);
        assertEqual(props.writeConcern, undefined);
      }
    },
    
    testShardingFlexible : function () {
      assertTrue(db._createDatabase(dn, { sharding: "flexible" }));
      db._useDatabase(dn);
      let props = db._properties();
      if (isCluster) {
        assertEqual(props.sharding, "");
        assertEqual(props.replicationFactor, 1);
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
    
    testDeviatingWriteConcernAndMinReplicationFactorForCollection : function () {
      if (!isCluster) {
        return;
      }
      assertTrue(db._createDatabase(dn, { sharding: "flexible" }));
      db._useDatabase(dn);
      try {
        db._create("test", { replicationFactor: 2, minReplicationFactor: 1, writeConcern: 2 });
        fail();
      } catch (err) {
        assertEqual(ERRORS.ERROR_BAD_PARAMETER.code, err.errorNum);
      }
        
      db._create("test", { replicationFactor: 2, minReplicationFactor: 2, writeConcern: 2 });
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
        assertEqual(ERRORS.ERROR_CLUSTER_INSUFFICIENT_DBSERVERS.code, err.errorNum);
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
        assertEqual(ERRORS.ERROR_CLUSTER_INSUFFICIENT_DBSERVERS.code, err.errorNum);
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
        db._create("oneshardcol", { replicationFactor : 5 });
        fail();
      } catch (err) {
        assertEqual(ERRORS.ERROR_CLUSTER_INSUFFICIENT_DBSERVERS.code, err.errorNum);
      }
      try {
        db._create("oneshardcol", { writeConcern : 5 });
        fail();
      } catch (err) {
        assertEqual(ERRORS.ERROR_CLUSTER_INSUFFICIENT_DBSERVERS.code, err.errorNum);
      }
    },
    
    testOneShardDBAndTooManyServers3 : function () {
      if (!isCluster) {
        return;
      }
      db._createDatabase(dn, { sharding : "single", writeConcern: 2, replicationFactor : 2 });
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
        assertEqual(ERRORS.ERROR_CLUSTER_INSUFFICIENT_DBSERVERS.code, err.errorNum);
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
          let graphsProperties = db._collection("_graphs").properties();

          if (isCluster) {
            assertEqual(colProperties.distributeShardsLike, "_graphs");
            assertEqual(colProperties.replicationFactor, db._properties().replicationFactor);
          }
        }

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
          let col = db._create("overrideCollection1", { writeConcern : 1});
          let colProperties = col.properties();
          assertEqual(colProperties.minReplicationFactor, 1);  // deprecated
          assertEqual(colProperties.writeConcern, 1);
        }

        {
          let col = db._create("overrideCollection2", { writeConcern : 1, replicationFactor : 1, numberOfShards : 3});
          let colProperties = col.properties();
          assertEqual(colProperties.minReplicationFactor, 1);  // deprecated
          assertEqual(colProperties.writeConcern, 1);
        }

        {
          // we want to create a normal collection and have a different replication factor
          let col2 = db._collection("overrideCollection2");
          let col3 = db._create("overrideCollection3", { distributeShardsLike: "overrideCollection2"});
          let col2Properties = col2.properties();
          let col3Properties = col3.properties();

          assertEqual(col3Properties.distributeShardsLike, col2.name());
          assertEqual(col3Properties.replicationFactor, col2Properties.replicationFactor);
        }
      }
    },

    testSatelliteDB : function () {
      assertTrue(db._createDatabase(dn, { replicationFactor : "satellite"}));
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
          assertEqual(props.replicationFactor, 1);
        }
      }
    },
    
    testValuesBelowMinReplicationFactor : function () {
      let min = internal.minReplicationFactor;
      if (min > 0) {
        try {
          db._createDatabase(dn, { replicationFactor : min - 1 });
        } catch (err) {
          assertEqual(ERRORS.ERROR_BAD_PARAMETER.code, err.errorNum);
        }
      }
    },

    testValuesAboveMaxReplicationFactor : function () {
      let max = internal.maxReplicationFactor;
      if (max > 0) {
        try {
          db._createDatabase(dn, { replicationFactor : max + 1 });
        } catch (err) {
          assertEqual(ERRORS.ERROR_BAD_PARAMETER.code, err.errorNum);
        }
      }
    },
  
  };
}

jsunity.run(OneShardPropertiesSuite);
return jsunity.done();
