/*jshint globalstrict:false, strict:false */

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
/// @author Copyright 2014, triAGENS GmbH, Cologne, Germany
// //////////////////////////////////////////////////////////////////////////////

const jsunity = require("jsunity");
const {fail, assertEqual, assertTrue, assertFalse, assertNull, assertTypeOf} = jsunity.jsUnity.assertions;
const arangodb = require("@arangodb");
const ERRORS = arangodb.errors;
const db = arangodb.db;
const internal = require("internal");
const isServer = typeof internal.arango === 'undefined';
const console = require('console');
const request = require('@arangodb/request');
const ArangoError = require("@arangodb").ArangoError;
let {
  getMaxNumberOfShards,
  getMaxReplicationFactor,
  getMinReplicationFactor
} = require("@arangodb/test-helper");

const maxNumberOfShards    = getMaxNumberOfShards();
const maxReplicationFactor = getMaxReplicationFactor();
const minReplicationFactor = getMinReplicationFactor();
let IM = global.instanceManager;

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////

function ClusterCollectionSuite () {
  'use strict';

  return {

////////////////////////////////////////////////////////////////////////////////
/// @brief tear down
////////////////////////////////////////////////////////////////////////////////

    tearDown : function () {
      db._drop("UnitTestsClusterCrud");
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test create, single shard
////////////////////////////////////////////////////////////////////////////////

    testCreateSingleShard : function () {
      var c = db._create("UnitTestsClusterCrud");
      assertEqual("UnitTestsClusterCrud", c.name());
      assertEqual(2, c.type());
      assertEqual(1, c.shards().length);
      assertTrue(typeof c.shards()[0] === 'string');
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test create, multiple shards
////////////////////////////////////////////////////////////////////////////////

    testCreateMultipleShards : function () {
      var c = db._create("UnitTestsClusterCrud", { numberOfShards: 8 });
      assertEqual("UnitTestsClusterCrud", c.name());
      assertEqual(2, c.type());
      assertEqual(8, c.shards().length);
      for (var i = 0; i < 8; ++i) {
        assertTrue(typeof c.shards()[i] === 'string');
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test create collection
////////////////////////////////////////////////////////////////////////////////

    testCreate : function () {
      var c = db._create("UnitTestsClusterCrud");
      assertEqual("UnitTestsClusterCrud", c.name());
      assertEqual(2, c.type());
      assertEqual(3, c.status());
      assertTrue(c.hasOwnProperty("_id"));

      assertEqual(c.name(), db._collection("UnitTestsClusterCrud").name());
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test create collection fail
////////////////////////////////////////////////////////////////////////////////

    testCreateInvalid : function () {
      let invalidNames = ["123", "_x", "/", "/?/", "\t", "\r\n"];

      invalidNames.forEach(function (collectionName) {
        try {
          db._create(collectionName);
          fail();
        } catch (err) {
          assertEqual(ERRORS.ERROR_ARANGO_ILLEGAL_NAME.code, err.errorNum, collectionName);
        }
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test create collection with replicationFactor
////////////////////////////////////////////////////////////////////////////////

    testCreateValidReplicationFactor : function () {
      // would like to test replication to the allowd maximum, but testsuite is
      // starting 2 dbservers, so setting to > 2 is currently not possible
      for ( let i = 1; i < 3; i++) {
        try {
          const c = db._create("UnitTestsClusterCrud", {
            replicationFactor: i
          });
          assertEqual("UnitTestsClusterCrud", c.name());
          assertEqual(2, c.type());
          assertEqual(3, c.status());
          assertTrue(c.hasOwnProperty("_id"));
          assertEqual(i, c.properties().replicationFactor);
        } finally {
          db._drop("UnitTestsClusterCrud");
        }
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test create collection with replicationFactor && writeConcern
/// writeConcern is equally set to replicationFactor
////////////////////////////////////////////////////////////////////////////////

    testCreateValidMinReplicationFactor : function () {
      // would like to test replication to the allowd maximum, but testsuite is
      // starting 2 dbservers, so setting to > 2 is currently not possible
      for ( let i = 1; i < 3; i++) {
        try {
          const c = db._create("UnitTestsClusterCrud", {
            replicationFactor: i,
            minReplicationFactor: i
          });
          assertEqual("UnitTestsClusterCrud", c.name());
          assertEqual(2, c.type());
          assertEqual(3, c.status());
          assertTrue(c.hasOwnProperty("_id"));
          assertEqual(i, c.properties().replicationFactor);
          assertEqual(i, c.properties().minReplicationFactor);
          assertEqual(i, c.properties().writeConcern);
        } finally {
          db._drop("UnitTestsClusterCrud");
        }
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test create collection with replicationFactor && writeConcern
/// writeConcern is set to replicationFactor - 1
////////////////////////////////////////////////////////////////////////////////

    testCreateValidMinReplicationFactorSmaller : function () {
      // would like to test replication to the allowd maximum, but testsuite is
      // starting 2 dbservers, so setting to > 2 is currently not possible
      for ( let i = 2; i < 3; i++) {
        try {
          const c = db._create("UnitTestsClusterCrud", {
            replicationFactor: i,
            minReplicationFactor: i - 1
          });
          assertEqual("UnitTestsClusterCrud", c.name());
          assertEqual(2, c.type());
          assertEqual(3, c.status());
          assertTrue(c.hasOwnProperty("_id"));
          assertEqual(i, c.properties().replicationFactor);
          assertEqual(i - 1, c.properties().minReplicationFactor);
          assertEqual(i - 1, c.properties().writeConcern);
        } finally {
          db._drop("UnitTestsClusterCrud");
        }
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test create collection with replicationFactor && writeConcern
/// set to 2. Then decrease both to 1 (update).
////////////////////////////////////////////////////////////////////////////////

    testCreateValidMinReplicationFactorThenDecrease : function () {
      let c = db._create("UnitTestsClusterCrud", {
        replicationFactor: 2,
        writeConcern: 2
      });
      assertEqual("UnitTestsClusterCrud", c.name());
      assertEqual(2, c.type());
      assertEqual(3, c.status());
      assertTrue(c.hasOwnProperty("_id"));
      assertEqual(2, c.properties().replicationFactor);
      assertEqual(2, c.properties().minReplicationFactor); // deprecated
      assertEqual(2, c.properties().writeConcern);

      db._collection("UnitTestsClusterCrud").properties({
        replicationFactor: 1,
        writeConcern: 1
      });

      c = db._collection("UnitTestsClusterCrud");
      assertEqual(1, c.properties().replicationFactor);
      assertEqual(1, c.properties().minReplicationFactor); // deprecated
      assertEqual(1, c.properties().writeConcern);

      db._drop("UnitTestsClusterCrud");
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test create collection with replicationFactor && writeConcern
/// set to 1. Then increase both to 2 (update).
////////////////////////////////////////////////////////////////////////////////

    testCreateValidMinReplicationFactorThenIncrease : function () {
      let c = db._create("UnitTestsClusterCrud", {
        replicationFactor: 1,
        writeConcern: 1
      });
      assertEqual("UnitTestsClusterCrud", c.name());
      assertEqual(2, c.type());
      assertEqual(3, c.status());
      assertTrue(c.hasOwnProperty("_id"));
      assertEqual(1, c.properties().replicationFactor);
      assertEqual(1, c.properties().minReplicationFactor);
      assertEqual(1, c.properties().writeConcern);

      db._collection("UnitTestsClusterCrud").properties({
        replicationFactor: 2,
        writeConcern: 2
      });

      c = db._collection("UnitTestsClusterCrud");
      assertEqual(2, c.properties().replicationFactor);
      assertEqual(2, c.properties().minReplicationFactor);
      assertEqual(2, c.properties().writeConcern);

      db._drop("UnitTestsClusterCrud");
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test create collection with replicationFactor && writeConcern
/// set to 2. Then increase both to 3 (update).
////////////////////////////////////////////////////////////////////////////////

    testCreateInvalidMinReplicationFactorThenIncrease : function () {
      let c = db._create("UnitTestsClusterCrud", {
        replicationFactor: 2,
        writeConcern: 2
      });
      assertEqual("UnitTestsClusterCrud", c.name());
      assertEqual(2, c.type());
      assertEqual(3, c.status());
      assertTrue(c.hasOwnProperty("_id"));
      assertEqual(2, c.properties().replicationFactor);
      assertEqual(2, c.properties().minReplicationFactor);
      assertEqual(2, c.properties().writeConcern);

      try {
        db._collection("UnitTestsClusterCrud").properties({
          replicationFactor: 5,
          minReplicationFactor: 5
        });
        fail();
      } catch (err) {
      }

      c = db._collection("UnitTestsClusterCrud");
      assertEqual(2, c.properties().replicationFactor);
      assertEqual(2, c.properties().minReplicationFactor);
      assertEqual(2, c.properties().writeConcern);

      db._drop("UnitTestsClusterCrud");
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief with invalid input
////////////////////////////////////////////////////////////////////////////////

    testCreateInvalidInputsMinReplicationFactor : function () {
      const invalidMinReplFactors = [null, "a", true, false, [1,2,3], {some: "object"}];

      invalidMinReplFactors.forEach(function(minFactor) {
        try {
          db._create("UnitTestsClusterCrud", {
            replicationFactor: 2,
            minReplicationFactor: minFactor
          });
          fail();
        } catch (err) {
          assertEqual(ERRORS.ERROR_BAD_PARAMETER.code, err.errorNum);
        }
      });
      invalidMinReplFactors.forEach(function(minFactor) {
        try {
          db._create("UnitTestsClusterCrud", {
            replicationFactor: 2,
            writeConcern: minFactor
          });
          fail();
        } catch (err) {
          assertEqual(ERRORS.ERROR_BAD_PARAMETER.code, err.errorNum);
        }
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test create collection with minReplicationFactor 0 (invalid)
////////////////////////////////////////////////////////////////////////////////

    testCreateInvalidNumbersMinReplicationFactor : function () {
      const invalidMinReplFactors = [0];

      invalidMinReplFactors.forEach(function(minFactor) {
        try {
          db._create("UnitTestsClusterCrud", {
            replicationFactor: 2,
            minReplicationFactor: minFactor
          });
          fail();
        } catch (err) {
        }
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief create with id
////////////////////////////////////////////////////////////////////////////////

    testCreateWithId: function () {
      var cn = "example", id = "1234567890";

      db._drop(cn);
      db._drop(id);
      try {
        var c1 = db._create(cn, {id: id});

        assertTypeOf("string", c1._id);
        assertEqual(id, c1._id);
        assertEqual(cn, c1.name());
        assertTypeOf("number", c1.status());

        var c2 = db._collection(cn);

        assertEqual(c1._id, c2._id);
        assertEqual(c1.name(), c2.name());
        assertEqual(c1.status(), c2.status());
      } finally {
        db._drop(cn);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test create collection with replicationFactor && minReplicationFactor
/// minReplicationFactor is set to replicationFactor + 1
////////////////////////////////////////////////////////////////////////////////

    testCreateInvalidMinReplicationFactorBigger : function () {
      try {
        let c;
        for ( let i = 1; i < 3; i++) {
          c = db._create("UnitTestsClusterCrud", {
            replicationFactor: i,
            minReplicationFactor: i + 1
          });
        }
        fail();
      } catch (err) {
        assertEqual(ERRORS.ERROR_BAD_PARAMETER.code, err.errorNum);
      }
      try {
        let c;
        for ( let i = 1; i < 3; i++) {
          c = db._create("UnitTestsClusterCrud", {
            replicationFactor: i,
            writeConcern: i + 1
          });
        }
        fail();
      } catch (err) {
        assertEqual(ERRORS.ERROR_BAD_PARAMETER.code, err.errorNum);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test create
////////////////////////////////////////////////////////////////////////////////

    testCreateEdge : function () {
      var c = db._createEdgeCollection("UnitTestsClusterCrud");
      assertEqual("UnitTestsClusterCrud", c.name());
      assertEqual(3, c.type());
      assertEqual(3, c.status());
      assertTrue(c.hasOwnProperty("_id"));

      assertEqual(c.name(), db._collection("UnitTestsClusterCrud").name());
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test create
////////////////////////////////////////////////////////////////////////////////

    testCreateEdgeMultipleShards : function () {
      var c = db._createEdgeCollection("UnitTestsClusterCrud", { numberOfShards: 8 });
      assertEqual("UnitTestsClusterCrud", c.name());
      assertEqual(3, c.type());
      assertEqual(3, c.status());
      assertTrue(c.hasOwnProperty("_id"));
      assertEqual(8, c.shards().length);

      assertEqual(c.name(), db._collection("UnitTestsClusterCrud").name());
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test create / drop
////////////////////////////////////////////////////////////////////////////////

    testCreateDrop : function () {
      var c = db._create("UnitTestsClusterCrud");
      assertEqual("UnitTestsClusterCrud", c.name());
      assertEqual(2, c.type());
      assertEqual(3, c.status());
      assertTrue(c.hasOwnProperty("_id"));

      c.drop();
      assertNull(db._collection("UnitTestsClusterCrud"));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test create
////////////////////////////////////////////////////////////////////////////////
    
    testCreateEmptyShardKeysArray : function () {
      try {
        db._create("UnitTestsClusterCrud", {shardKeys: []});
        fail("Managed to create collection with illegal shardKey entry");
      } catch (err) {
        assertEqual(ERRORS.ERROR_BAD_PARAMETER.code, err.errorNum);
      }
    },
    
    testCreateShardKeysOnKey : function () {
      db._create("UnitTestsClusterCrud", { shardKeys: [ "_key" ] });
      let props = db["UnitTestsClusterCrud"].properties();
      assertEqual(["_key"], props.shardKeys);
    },
    
    testCreateShardKeysOnId : function () {
      try {
        db._create("UnitTestsClusterCrud", { shardKeys: [ "_id" ] });
        fail();
      } catch (err) {
        assertEqual(ERRORS.ERROR_BAD_PARAMETER.code, err.errorNum);
      }
    },

    testCreateShardKeysOnRev : function () {
      try {
        db._create("UnitTestsClusterCrud", { shardKeys: [ "_rev" ] });
        fail();
      } catch (err) {
        assertEqual(ERRORS.ERROR_BAD_PARAMETER.code, err.errorNum);
      }
    },

    testCreateIncompleteShardKeys1 : function () {
      db._create("UnitTestsClusterCrud", { shardKeys: [ "", "foo" ] });
      let props = db["UnitTestsClusterCrud"].properties();
      assertEqual(["foo"], props.shardKeys);
    },
    
    testCreateIncompleteShardKeys2 : function () {
      db._create("UnitTestsClusterCrud", { shardKeys: [ "bar", "" ] });
      let props = db["UnitTestsClusterCrud"].properties();
      assertEqual(["bar"], props.shardKeys);
    },

    testCreateShardKeysOnFrom : function () {
      db._create("UnitTestsClusterCrud", { shardKeys: [ "_from" ] });
      let props = db["UnitTestsClusterCrud"].properties();
      assertEqual(["_from"], props.shardKeys);
    },
    
    testCreateShardKeysOnTo : function () {
      db._create("UnitTestsClusterCrud", { shardKeys: [ "_to" ] });
      let props = db["UnitTestsClusterCrud"].properties();
      assertEqual(["_to"], props.shardKeys);
    },
    
    testCreateShardKeysOnFromTo : function () {
      db._create("UnitTestsClusterCrud", { shardKeys: [ "_from", "_to" ] });
      let props = db["UnitTestsClusterCrud"].properties();
      assertEqual(["_from", "_to"], props.shardKeys);
    },

    testCreateShardKeysMixed : function () {
      db._create("UnitTestsClusterCrud", { shardKeys: [ "a", "_from" ] });
      let props = db["UnitTestsClusterCrud"].properties();
      assertEqual(["a", "_from"], props.shardKeys);
    },
    
    testCreateShardKeysMany : function () {
      db._create("UnitTestsClusterCrud", { shardKeys: [ "a", "b", "c", "d", "e", "f", "g", "h" ] });
      let props = db["UnitTestsClusterCrud"].properties();
      assertEqual(["a", "b", "c", "d", "e", "f", "g", "h"], props.shardKeys);
    },
    
    testCreateShardKeysTooMany : function () {
      try {
        // only 8 shard keys are allowed
        db._create("UnitTestsClusterCrud", { shardKeys: [ "a", "b", "c", "d", "e", "f", "g", "h", "i" ] });
        fail();
      } catch (err) {
        assertEqual(ERRORS.ERROR_BAD_PARAMETER.code, err.errorNum);
      }
    },

    testCreateInvalidNumberOfShards1 : function () {
      try {
        db._create("UnitTestsClusterCrud", { numberOfShards : 0 });
        fail();
      } catch (err) {
        assertEqual(ERRORS.ERROR_BAD_PARAMETER.code, err.errorNum);
      }
    },

    testCreateInvalidNumberOfShards2 : function () {
      try {
        db._create("UnitTestsClusterCrud", { numberOfShards : 1024 * 1024 });
        fail();
      } catch (err) {
        assertEqual(ERRORS.ERROR_CLUSTER_TOO_MANY_SHARDS.code, err.errorNum);
      }
    },

    testCreateAsManyShardsAsAllowed : function () {
      let max = maxNumberOfShards;
      if (max > 0) {
        db._create("UnitTestsClusterCrud", { numberOfShards : max });
        let properties = db["UnitTestsClusterCrud"].properties();
        assertEqual(max, properties.numberOfShards);
      }
    },

    testCreateMoreShardsThanAllowed : function () {
      let max = maxNumberOfShards;
      if (max > 0) {
        try {
          db._create("UnitTestsClusterCrud", { numberOfShards : max + 1 });
          fail();
        } catch (err) {
          assertEqual(ERRORS.ERROR_CLUSTER_TOO_MANY_SHARDS.code, err.errorNum);
        }
      }
    },

    testCreateFailureDuringIsBuilding : function () {
      if (!IM.debugCanUseFailAt()) {
        console.info('Failure tests disabled, skipping...');
        return;
      }

      const failurePoint = 'ClusterInfo::createCollectionsCoordinator';
      try {
        IM.debugSetFailAt(failurePoint);
        const colName = "UnitTestClusterShouldNotBeCreated";
        let threw = false;
        try {
          db._create(colName);
        } catch (e) {
          threw = true;
          assertTrue(e instanceof ArangoError);
          assertEqual(22, e.errorNum);
          assertEqual('intentional debug error', e.errorMessage);
        }
        assertTrue(threw);
        const collections = IM.agencyMgr.get(`Plan/Collections/${db._name()}`)
          .arango.Plan.Collections[db._name()];
        assertEqual([], Object.values(collections).filter(col => col.name === colName),
          'Collection should have been deleted');
      } finally {
        IM.debugRemoveFailAt(failurePoint);
      }
    },

    testCreateFailureWhenRemovingIsBuilding : function () {
      const failurePoint = 'ClusterInfo::createCollectionsCoordinatorRemoveIsBuilding';
      try {
        IM.debugSetFailAt(failurePoint);
        const colName = "UnitTestClusterShouldNotBeCreated1";
        let threw = false;
        try {
          db._create(colName);
        } catch (e) {
          threw = true;
          assertTrue(e instanceof ArangoError);
          assertEqual(503, e.errorNum);
        } finally {
          // we need to wait for the collecion to show up before the drop can work.
          while (!db._collection(colName)) {
            require("internal").sleep(0.1);
          }
          db._drop(colName);
        }
        assertTrue(threw);
      } finally {
        IM.debugRemoveFailAt(failurePoint);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test replicationFactor
////////////////////////////////////////////////////////////////////////////////
    
    testMinReplicationFactor : function () {
      let min = minReplicationFactor;
      if (min > 0) {
        db._create("UnitTestsClusterCrud", { replicationFactor: min });
        let properties = db["UnitTestsClusterCrud"].properties();
        assertEqual(min, properties.replicationFactor);

        try {
          db["UnitTestsClusterCrud"].properties({ replicationFactor: min - 1 });
          fail();
        } catch (err) {
          assertEqual(ERRORS.ERROR_BAD_PARAMETER.code, err.errorNum);
        }
      }
    },
    
    testMaxReplicationFactor : function () {
      let max = maxReplicationFactor;
      if (max > 0) {
        try {
          db._create("UnitTestsClusterCrud", { replicationFactor: max });
          let properties = db["UnitTestsClusterCrud"].properties();
          assertEqual(max, properties.replicationFactor);
          
          try {
            db["UnitTestsClusterCrud"].properties({ replicationFactor: max + 1 });
            fail();
          } catch (err) {
            assertEqual(ERRORS.ERROR_BAD_PARAMETER.code, err.errorNum);
          }
        } catch (err) {
          // if creation fails, then it must have been exactly this error
          assertEqual(ERRORS.ERROR_CLUSTER_INSUFFICIENT_DBSERVERS.code, err.errorNum);
        }
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test count
////////////////////////////////////////////////////////////////////////////////

    testCountAll : function () {
      var c = db._create("UnitTestsClusterCrud", { numberOfShards : 5 });
      let docs = [];
      for (let i = 0; i < 1000; ++i) {
        docs.push({ value: i });
      }
      c.insert(docs);

      assertEqual(1000, c.count());
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test count
////////////////////////////////////////////////////////////////////////////////

    testCountDetailed : function () {
      var c = db._create("UnitTestsClusterCrud", { numberOfShards : 5 });
      let docs = [];
      for (let i = 0; i < 1000; ++i) {
        docs.push({ value: i });
      }
      c.insert(docs);

      var total = 0, res = c.count(true);
      assertEqual(5, Object.keys(res).length);
      Object.keys(res).forEach(function(key) { total += res[key]; });
      assertEqual(1000, total);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test create
////////////////////////////////////////////////////////////////////////////////

    testCreateProperties1 : function () {
      var c = db._create("UnitTestsClusterCrud", { numberOfShards : 5, shardKeys: [ "_key" ] });
      assertEqual("UnitTestsClusterCrud", c.name());
      assertEqual(2, c.type());
      assertEqual(3, c.status());
      assertTrue(c.hasOwnProperty("_id"));
      assertEqual([ "_key" ], c.properties().shardKeys);
      assertFalse(c.properties().waitForSync);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test create
////////////////////////////////////////////////////////////////////////////////

    testCreateProperties2 : function () {
      var c = db._create("UnitTestsClusterCrud", { shardKeys: [ "foo", "bar" ], waitForSync : true });
      assertEqual("UnitTestsClusterCrud", c.name());
      assertEqual(2, c.type());
      assertEqual(3, c.status());
      assertTrue(c.hasOwnProperty("_id"));
      assertEqual([ "foo", "bar" ], c.properties().shardKeys);
      assertTrue(c.properties().waitForSync);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test create
////////////////////////////////////////////////////////////////////////////////

    testCreateProperties3 : function () {
      var c = db._create("UnitTestsClusterCrud");
      assertEqual("UnitTestsClusterCrud", c.name());
      assertEqual(2, c.type());
      assertEqual(3, c.status());
      assertTrue(c.hasOwnProperty("_id"));
      assertEqual([ "_key" ], c.properties().shardKeys);
      assertFalse(c.properties().waitForSync);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test recreate
////////////////////////////////////////////////////////////////////////////////

    testRecreate : function () {
      var c = db._create("UnitTestsClusterCrud");
      assertEqual("UnitTestsClusterCrud", c.name());
      assertEqual(2, c.type());
      assertEqual(3, c.status());
      assertTrue(c.hasOwnProperty("_id"));

      try {
        db._create("UnitTestsClusterCrud");
        fail();
      } catch (err) {
        assertEqual(ERRORS.ERROR_ARANGO_DUPLICATE_NAME.code, err.errorNum);
      }

      assertEqual(c.name(), db._collection("UnitTestsClusterCrud").name());
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test create
////////////////////////////////////////////////////////////////////////////////

    testCreateIllegalName1 : function () {
      try {
        db._create("1234");
        fail();
      } catch (err) {
        assertEqual(ERRORS.ERROR_ARANGO_ILLEGAL_NAME.code, err.errorNum);
      }

      assertNull(db._collection("1234"));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test create
////////////////////////////////////////////////////////////////////////////////

    testCreateIllegalName2 : function () {
      try {
        db._create("");
        fail();
      } catch (err) {
        assertEqual(ERRORS.ERROR_ARANGO_ILLEGAL_NAME.code, err.errorNum);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test create
////////////////////////////////////////////////////////////////////////////////

    testCreateIllegalName3 : function () {
      try {
        db._drop("_foo");
      } catch (e) {
      }

      try {
        db._create("_foo");
        fail();
      } catch (err) {
        assertEqual(ERRORS.ERROR_ARANGO_ILLEGAL_NAME.code, err.errorNum);
      }

      assertNull(db._collection("_foo"));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test create
////////////////////////////////////////////////////////////////////////////////

    testCreateInsufficientDBServersDefault : function () {
      try {
        try {
          db._create("bigreplication", { replicationFactor: 8 });
          fail();
        } catch (err) {
          assertEqual(ERRORS.ERROR_CLUSTER_INSUFFICIENT_DBSERVERS.code, err.errorNum);
        }
      } finally {
        db._drop('bigreplication');
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test create
////////////////////////////////////////////////////////////////////////////////

    testCreateInsufficientDBServersIgnoreReplicationFactor : function () {
      // should not throw (just a warning)
      try {
      db._create("bigreplication", {replicationFactor: 8}, {enforceReplicationFactor: false});
      } finally{
        db._drop('bigreplication');
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test create
////////////////////////////////////////////////////////////////////////////////

    testCreateInsufficientDBServersEnforceReplicationFactor : function () {
      try {
        try {
          db._create("bigreplication", { replicationFactor: 8 }, { enforceReplicationFactor: true });
          fail();
        } catch (err) {
          assertEqual(ERRORS.ERROR_CLUSTER_INSUFFICIENT_DBSERVERS.code, err.errorNum);
        }
      } finally {
        db._drop('bigreplication');
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test creation / deleting of documents with replication set
////////////////////////////////////////////////////////////////////////////////

    testCreateReplicated : function () {
      var cn = "UnitTestsClusterCrudRepl";
      try {
        var c = db._create(cn, { numberOfShards: 2, replicationFactor: 2});

        // store and delete document
        c.save({foo: 'bar'});
        db._query(`FOR x IN @@cn REMOVE x IN @@cn`, {'@cn': cn});
        assertEqual(0, c.toArray().length);

        //insert
        var cursor = db._query(`
          let x = (FOR a IN [1]
                  INSERT {
                    "_key" : "ulf",
                    "super" : "dog"
                  } IN @@cn)
          return x`,
          {'@cn' : cn});
        assertEqual(1, c.toArray().length);

        //update
        cursor = db._query(`
          let x = (UPDATE 'ulf' WITH {
                    "super" : "cat"
                  } IN @@cn RETURN NEW)
          RETURN x`,
          {'@cn' : cn});
        assertEqual(1, c.toArray().length);
        var doc = c.any();
        assertEqual(doc.super, "cat");
        doc = cursor.next();                // should be: cursor >>= id
        assertEqual(doc[0].super, "cat");  // extra [] buy subquery return

        //remove
        cursor = db._query(`
          let x = (REMOVE 'ulf' IN @@cn)
          return x`,
          {'@cn' : cn});
        assertEqual(0, c.toArray().length);
      } finally {
          db._drop(cn);
      }
    },

    // regression test for https://github.com/arangodb/arangodb/pull/21071
    testCreateWithSlowCurrentUpdate : function () {
      if (!IM.debugCanUseFailAt()) {
        console.info('Failure tests disabled, skipping...');
        return;
      }

      const failurePoint = 'ClusterInfo::slowCurrentSyncer';
      const cn = "UnitTestsClusterCrud";
      try {
        // delay updates for current in the syncer thread
        IM.debugSetFailAt(failurePoint);
        // create a collection
        const c = db._create(cn, { numberOfShards: 1, replicationFactor: 1 });
        const docs = [];
        for (let i = 0; i < 100; ++i) {
          docs.push({ _key: "test" + i });
        }
        // inserting a document fails if it needs to look up a shard and
        // ClusterInfo/Current hasn't caught up yet, in which case insert
        // throws an exception.
        c.insert(docs);
      } finally {
        IM.debugRemoveFailAt(failurePoint);
        db._drop(cn);
      }
    },
  };
}

jsunity.run(ClusterCollectionSuite);

return jsunity.done();
