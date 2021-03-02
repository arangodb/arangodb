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

const jsunity = require('jsunity');
const internal = require('internal');
const arangodb = require('@arangodb');
const db = arangodb.db;
const fs = require('fs');
const time = internal.time;
const wait = internal.wait;
const request = require('@arangodb/request');

function trxWriteHotbackupDeadlock () {
  'use strict';
  const cn = 'UnitTestsDeadlockHotbackup';
  let c = null;

  return {

    setUp: function () {
      db._drop(cn);
      c = db._create(cn, {numberOfShards: 1, replicationFactor: 2},
                     {waitForSyncReplication: true});
    },

    tearDown: function () {
      db._drop(cn);
    },

    tearDownAll: function () {
      let dirs = JSON.parse(internal.env.INSTANCEINFO).arangods.filter((s) => s.role === 'dbserver').map((s) => s.rootDir);
      dirs.forEach((d) => {
        // remove all the hotbackups that we created, because they will consume a
        // lot of disk space
        try {
          let path = fs.join(d, 'data', 'backups');
          if (fs.exists(path)) {
            fs.removeDirectoryRecursive(path);
          }
        } catch (err) {
          require("console").warn("unable to remove hotbackups from base directory " + d, err);
        }
      });
    },

    testRunAQLWritingTransactionsDuringHotbackup: function () {
      let start = time();
      // This will create lots of hotbackups for approximately 60 seconds
      let res = arango.POST_RAW("/_api/transaction",
        { action: `function() {
                    let console = require("console");
                    let internal = require("internal");
                    let wait = internal.wait;
                    let time = internal.time;
                    let start = time();
                    let good = 0;
                    let bad = 0;
                    while (time() - start < 60) {
                      try {
                        internal.createHotbackup({});
                        console.log("Created a hotbackup!");
                        good++;
                      } catch (e) {
                        console.error("Caught exception when creating a hotbackup!", e);
                        bad++;
                        wait(0.5, false);
                      }

                      if (internal.db._collection("${cn}") === null) {
                        /* the other task is done with its work */
                        break;
                      }
                    }
                    return {good, bad};
                  }`,
          collections: {} }, {"x-arango-async":"store"});
      assertFalse(res.error, "Could not POST transaction.");
      assertEqual(202, res.code, "Bad response code.");
      let jobid = res.headers["x-arango-async-id"];

      let timeout = 60;
      if (global.ARANGODB_CLIENT_VERSION(true).asan === 'true' ||
          global.ARANGODB_CLIENT_VERSION(true).tsan === 'true' ||
          process.env.hasOwnProperty('GCOV_PREFIX')) {
        timeout *= 10;
      }

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
      assertTrue(diff < timeout, "750 transactions took too long, probably some deadlock with hotbackups");
      // by dropping the collection we signal to the other task that it can finish
      db._drop(cn);
      while (time() - start < timeout + 20) {
        res = arango.PUT(`/_api/job/${jobid}`, {});
        if (res.code !== 204) {
          assertEqual(200, res.code, "Response code bad.");
          print("Managed to perform", res.result.good, "hotbackups.");
          assertTrue(res.result.bad < 3, "Too many failed hotbackups!");
          assertTrue(res.result.good > 2, "Too few good hotbackups!");
          return;
        }
        wait(1.0, false);
        print("Waiting for hotbackups to finish...");
      }
      arango.DELETE(`/_api/job/${jobid}`);
      assertFalse(true, "Did not finish in expected time.");
    }
  };

}

jsunity.run(trxWriteHotbackupDeadlock);
return jsunity.done();
