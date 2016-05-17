/*jshint globalstrict:false, strict:false */
/*global assertTrue, assertEqual */

////////////////////////////////////////////////////////////////////////////////
/// @brief test synchronous replication in the cluster
///
/// @file js/server/tests/shell/shell-synchronous-replication-cluster.js
///
/// DISCLAIMER
///
/// Copyright 2016-2016 ArangoDB GmbH, Cologne, Germany
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
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Max Neunhoeffer
/// @author Copyright 2016, ArangoDB GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

var jsunity = require("jsunity");

var arangodb = require("@arangodb");
var db = arangodb.db;
var _ = require("lodash");
var print = require("internal").print;
var wait = require("internal").wait;


////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////

function SynchronousReplicationSuite () {
  'use strict';
  var cn = "UnitTestSyncRep";
  var c;
  var cinfo;
  var ccinfo;
  var shards;

////////////////////////////////////////////////////////////////////////////////
/// @brief wait for synchronous replication
////////////////////////////////////////////////////////////////////////////////

  function waitForSynchronousReplication() {
    cinfo = global.ArangoClusterInfo.getCollectionInfo("_system", cn);
    shards = Object.keys(cinfo.shards);
    var count = 0;
    while (++count <= 120) {
      ccinfo = shards.map(
        s => global.ArangoClusterInfo.getCollectionInfoCurrent("_system", cn, s)
      );
      let replicas = ccinfo.map(s => s.servers.length);
      print("Replicas:", replicas);
      if (_.all(replicas, x => x === 2)) {
        print("Replication up and running!");
        return true;
      }
      wait(0.5);
    }
    return false;
  }

  return {

////////////////////////////////////////////////////////////////////////////////
/// @brief set up
////////////////////////////////////////////////////////////////////////////////

    setUp : function () {
      db._drop(cn);
      c = db._create(cn, {numberOfShards: 2, replicationFactor: 2});
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief tear down
////////////////////////////////////////////////////////////////////////////////

    tearDown : function () {
      db._drop(cn);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief check whether we have access to global.instanceInfo
////////////////////////////////////////////////////////////////////////////////

    testCheckInstanceInfo : function () {
      require("internal").print("InstanceInfo is:", global.instanceInfo);
      assertTrue(global.instanceInfo !== undefined);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief check if a synchronously replicated collection gets online
////////////////////////////////////////////////////////////////////////////////

    testSetup : function () {
      assertTrue(waitForSynchronousReplication());
      assertEqual(12, 12);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief just to allow a trailing comma at the end of the last test
////////////////////////////////////////////////////////////////////////////////

    testDummy : function () {
    }

  };
}


////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suite
////////////////////////////////////////////////////////////////////////////////

jsunity.run(SynchronousReplicationSuite);

return jsunity.done();

