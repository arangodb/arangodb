/*jshint globalstrict:false, strict:false */
/*global fail, assertEqual, assertTrue, assertFalse, assertNull */

////////////////////////////////////////////////////////////////////////////////
/// @brief test collection functionality in a cluster
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
/// @author Copyright 2014, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

var jsunity = require("jsunity");
var arangodb = require("@arangodb");
var ERRORS = arangodb.errors;
var db = arangodb.db;

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////

function ClusterCollectionSuite () {
  'use strict';
  return {

////////////////////////////////////////////////////////////////////////////////
/// @brief set up
////////////////////////////////////////////////////////////////////////////////

    setUp : function () {
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief tear down
////////////////////////////////////////////////////////////////////////////////

    tearDown : function () {
      try {
        db._drop("UnitTestsClusterCrud");
      }
      catch (err) {
      }
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
/// @brief test create
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

    testCreateInvalidShardKeys1 : function () {
      try {
        db._create("UnitTestsClusterCrud", { shardKeys: [ ] });
      }
      catch (err) {
        assertEqual(ERRORS.ERROR_BAD_PARAMETER.code, err.errorNum);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test create
////////////////////////////////////////////////////////////////////////////////

    testCreateInvalidShardKeys2 : function () {
      try {
        db._create("UnitTestsClusterCrud", { shardKeys: [ "_rev" ] });
      }
      catch (err) {
        assertEqual(ERRORS.ERROR_BAD_PARAMETER.code, err.errorNum);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test create
////////////////////////////////////////////////////////////////////////////////

    testCreateInvalidShardKeys3 : function () {
      try {
        db._create("UnitTestsClusterCrud", { shardKeys: [ "", "foo" ] });
      }
      catch (err) {
        assertEqual(ERRORS.ERROR_BAD_PARAMETER.code, err.errorNum);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test create
////////////////////////////////////////////////////////////////////////////////

    testCreateInvalidShardKeys4 : function () {
      try {
        db._create("UnitTestsClusterCrud", { shardKeys: [ "a", "_from" ] });
      }
      catch (err) {
        assertEqual(ERRORS.ERROR_BAD_PARAMETER.code, err.errorNum);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test create
////////////////////////////////////////////////////////////////////////////////

    testCreateInvalidNumberOfShards1 : function () {
      try {
        db._create("UnitTestsClusterCrud", { numberOfShards : 0 });
      }
      catch (err) {
        assertEqual(ERRORS.ERROR_BAD_PARAMETER.code, err.errorNum);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test create
////////////////////////////////////////////////////////////////////////////////

    testCreateInvalidNumberOfShards2 : function () {
      try {
        db._create("UnitTestsClusterCrud", { numberOfShards : 1024 * 1024 });
      }
      catch (err) {
        assertEqual(ERRORS.ERROR_BAD_PARAMETER.code, err.errorNum);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test count
////////////////////////////////////////////////////////////////////////////////

    testCountAll : function () {
      var c = db._create("UnitTestsClusterCrud", { numberOfShards : 5 });
      for (var i = 0; i < 1000; ++i) {
        c.insert({ value: i });
      }

      assertEqual(1000, c.count());
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test count
////////////////////////////////////////////////////////////////////////////////

    testCountDetailed : function () {
      var c = db._create("UnitTestsClusterCrud", { numberOfShards : 5 });
      for (var i = 0; i < 1000; ++i) {
        c.insert({ value: i });
      }

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
      var c = db._create("UnitTestsClusterCrud", { journalSize: 1048576 });
      assertEqual("UnitTestsClusterCrud", c.name());
      assertEqual(2, c.type());
      assertEqual(3, c.status());
      assertTrue(c.hasOwnProperty("_id"));
      assertEqual([ "_key" ], c.properties().shardKeys);
      assertFalse(c.properties().waitForSync);
      if (db._engine().name === "mmfiles") {
        assertEqual(1048576, c.properties().journalSize);
      }
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
      }
      catch (err) {
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
      }
      catch (err) {
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
      }
      catch (err) {
        assertEqual(ERRORS.ERROR_ARANGO_ILLEGAL_NAME.code, err.errorNum);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test create
////////////////////////////////////////////////////////////////////////////////

    testCreateIllegalName3 : function () {
      try {
        db._drop("_foo");
      }
      catch (e) {
      }

      try {
        db._create("_foo");
        fail();
      }
      catch (err) {
        assertEqual(ERRORS.ERROR_ARANGO_ILLEGAL_NAME.code, err.errorNum);
      }

      assertNull(db._collection("_foo"));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test create
////////////////////////////////////////////////////////////////////////////////

    testCreateInsufficientDBServers : function () {

      try {
        db._create("bigreplication", {replicationFactor: 8});
        fail();
      }
      catch (err) {
        assertEqual(ERRORS.ERROR_CLUSTER_INSUFFICIENT_DBSERVERS.code, err.errorNum);
      }
      db._drop('bigreplication');
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test creation / deleting of documents with replication set
////////////////////////////////////////////////////////////////////////////////

    testCreateReplicated : function () {
      var cn = "UnitTestsClusterCrudRepl";
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
      assertTrue(doc.super === "cat");
      doc = cursor.next();                // should be: cursor >>= id
      assertTrue(doc[0].super === "cat");  // extra [] buy subquery return

      //remove
      cursor = db._query(`
        let x = (REMOVE 'ulf' IN @@cn)
        return x`,
        {'@cn' : cn});
      assertEqual(0, c.toArray().length);

      db._drop(cn);
    },

////////////////////////////////////////////////////////////////////////////////
    testIndexEstimates : function () {
      // index estimate only availalbe with rocksdb for skiplist
      if (db._engine().name === 'rocksdb') {
        var cn = "UnitTestsClusterCrudRepl";
        // numer of shards is one so the estimages behave like in the single server
        // if the shard number is higher we could just ensure theat the estimate
        // should be between 0 and 1
        var c = db._create(cn, { numberOfShards: 1, replicationFactor: 1});

        c.ensureIndex({type:"skiplist", fields:["foo"]});

        var i;
        var indexes;

        for(i=0; i < 10; ++i){
          c.save({foo: i});
        }
        indexes = c.getIndexes(true);
        assertEqual(indexes[1].selectivityEstimate, 1);

        for(i=0; i < 10; ++i){
          c.save({foo: i});
        }
        indexes = c.getIndexes(true);
        assertEqual(indexes[1].selectivityEstimate, 0.5);

        db._drop(cn);
      }
    }

////////////////////////////////////////////////////////////////////////////////
  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suite
////////////////////////////////////////////////////////////////////////////////

jsunity.run(ClusterCollectionSuite);

return jsunity.done();

