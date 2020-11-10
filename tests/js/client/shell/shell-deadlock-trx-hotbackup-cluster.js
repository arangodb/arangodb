/* jshint globalstrict:false, strict:false, maxlen: 200 */
/* global fail, assertTrue, assertFalse, assertEqual, assertNotUndefined, arango, print */

// //////////////////////////////////////////////////////////////////////////////
// / @brief ArangoDeadLockTests
// /
// /
// / DISCLAIMER
// /
// / Copyright 2020 ArangoDB GmbH, Cologne, Germany
// /
// / Licensed under the Apache License, Version 2.0 (the "License")
// / you may not use this file except in compliance with the License.
// / You may obtain a copy of the License at
// /
// /     http://www.apache.org/licenses/LICENSE-2.0
// /
// / Unless required by applicable law or agreed to in writing, software
// / distributed under the License is distributed on an "AS IS" BASIS,
// / WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// / See the License for the specific language governing permissions and
// / limitations under the License.
// /
// / @author Max Neunhoeffer
// //////////////////////////////////////////////////////////////////////////////

// tests for deadlocks in the cluster with writing trxs and hotbackup

var jsunity = require('jsunity');
var internal = require('internal');
var arangodb = require('@arangodb');
var db = arangodb.db;
const isCluster = internal.isCluster();
const time = internal.time;
const wait = internal.wait;

function trxWriteHotbackupDeadlock () {
  'use strict';
  var cn = 'UnitTestsDeadlockHotbackup';
  var c = null;

  return {

    setUp: function () {
      db._drop(cn);
      c = db._create(cn, {numberOfShards: 1, replicationFactor: 2});
    },

    tearDown: function () {
      if (c !== null) {
        c.drop();
      }
      c = null;
      internal.wait(0);
    },

    testRunAQLWritingTransactionsDuringHotbackup: function () {
      if (!isCluster) {
        return true;
      }

      wait(3);   // give the cluster some time to settle before we start
                 // the hotbackups

      let start = time();
      // This will create lots of hotbackups for approximately 60 seconds
      db._connection.POST("_admin/execute",`require("@arangodb/cluster.js").createManyBackups(60); return true;`,{"x-arango-async":"store"});
      // Now we try to write something:
      for (let i = 0; i < 1000; ++i) {
        c.insert({Hallo:i});
      }
      // And now to execute exclusive write locking transactions:
      for (let i = 1; i <= 750; ++i) {
        db._query(`LET m = (FOR d IN ${cn} 
                              SORT d.Hallo DESC
                              LIMIT 1
                              RETURN d)
                   INSERT {Hallo: m[0].Hallo+1} INTO ${cn}
                   OPTIONS {exclusive: true}
                   RETURN NEW`).toArray();
        if (i % 50 === 0) {
          print("Done", i, "write transactions.");
        }
        wait(0.01);
      }
      let diff = time() - start;
      print("Done 750 exclusive transactions in", diff, "seconds!");
      assertTrue(diff < 60, "750 transactions took too long, probably some deadlock with hotbackups");
      while (time() - start < 65) {
        wait(1.0);
        print("Waiting for hotbackups to finish...");
      }
    }
  };

}

jsunity.run(trxWriteHotbackupDeadlock);
return jsunity.done();
