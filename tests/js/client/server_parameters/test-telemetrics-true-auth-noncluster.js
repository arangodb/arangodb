/* jshint globalstrict:false, strict:false, maxlen: 200 */
/* global getOptions, fail, assertEqual, assertTrue, assertFalse, assertNotNull, assertNotUndefined, arango */

// //////////////////////////////////////////////////////////////////////////////
// / @brief dropping followers while replicating
// /
// / DISCLAIMER
// /
// / Copyright 2023 ArangoDB GmbH, Cologne, Germany
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
// / Copyright holder is triAGENS GmbH, Cologne, Germany
// /
// / @author Julia Puget
// //////////////////////////////////////////////////////////////////////////////

if (getOptions === true) {
  return {
    'server.send-telemetrics': 'true',
    'server.authentication': 'true',
  };
}

let jsunity = require('jsunity');
let internal = require('internal');
let arangodb = require('@arangodb');
const users = require('@arangodb/users');
const {HTTP_FORBIDDEN} = require("../../../../js/server/modules/@arangodb/actions");
const isCluster = require("internal").isCluster();
const userName = "abc";
const databaseName = "databaseTest";
let db = arangodb.db;

const createUser = () => {
  users.save(userName, '123', true);
  users.grantDatabase(userName, "_system", 'ro');
  users.reload();
};

const removeUser = () => {
  users.remove(userName);
};

function getTelemetricsResult() {
  try {
    let res;
    let numSecs = 0.5;
    while (true) {
      res = arango.getTelemetricsInfo();
      if (res !== undefined || numSecs >= 16) {
        break;
      }
      internal.sleep(numSecs);
      numSecs *= 2;
    }
    assertNotUndefined(res);
    return res;
  } catch (err) {
  }
}

function assertForTelemetricsResponse(telemetrics) {
  assertTrue(telemetrics.hasOwnProperty("deployment"));
  assertTrue(telemetrics["deployment"].hasOwnProperty("type"));
  assertTrue(telemetrics["deployment"].hasOwnProperty("license"));
  if (isCluster) {
    assertEqual(telemetrics["deployment"]["type"], "cluster");
  } else {
    assertEqual(telemetrics["deployment"]["type"], "single");
  }
  assertTrue(telemetrics.hasOwnProperty("host"));
  const host = telemetrics["host"];
  assertTrue(host.hasOwnProperty("role"));
  assertTrue(host.hasOwnProperty("maintenance"));
  assertTrue(host.hasOwnProperty("readOnly"));
  assertTrue(host.hasOwnProperty("version"));
  assertTrue(host.hasOwnProperty("build"));
  assertTrue(host.hasOwnProperty("os"));
  assertTrue(host.hasOwnProperty("platform"));
  assertTrue(host.hasOwnProperty("physMem"));
  assertTrue(host["physMem"].hasOwnProperty("value"));
  assertTrue(host["physMem"].hasOwnProperty("overridden"));
  assertTrue(host.hasOwnProperty("nCores"));
  assertTrue(host["nCores"].hasOwnProperty("value"));
  assertTrue(host["nCores"].hasOwnProperty("overridden"));
  assertTrue(host.hasOwnProperty("processStats"));
  assertTrue(host["processStats"].hasOwnProperty("processUptime"));
  assertTrue(host["processStats"].hasOwnProperty("nThreads"));
  assertTrue(host["processStats"].hasOwnProperty("virtualSize"));
  assertTrue(host["processStats"].hasOwnProperty("residentSetSize"));
  assertTrue(host.hasOwnProperty("cpuStats"));
  assertTrue(host["cpuStats"].hasOwnProperty("userPercent"));
  assertTrue(host["cpuStats"].hasOwnProperty("systemPercent"));
  assertTrue(host["cpuStats"].hasOwnProperty("idlePercent"));
  assertTrue(host["cpuStats"].hasOwnProperty("iowaitPercent"));
  assertTrue(host.hasOwnProperty("engineStats"));
  assertTrue(host["engineStats"].hasOwnProperty("cache.limit"));
  assertTrue(host["engineStats"].hasOwnProperty("cache.allocated"));
  assertTrue(host["engineStats"].hasOwnProperty("rocksdb.estimate-num-keys"));
  assertTrue(host["engineStats"].hasOwnProperty("rocksdb.estimate-live-data-size"));
  assertTrue(host["engineStats"].hasOwnProperty("rocksdb.live-sst-files-size"));
  assertTrue(host["engineStats"].hasOwnProperty("rocksdb.block-cache-capacity"));
  assertTrue(host["engineStats"].hasOwnProperty("rocksdb.block-cache-usage"));
  assertTrue(host["engineStats"].hasOwnProperty("rocksdb.free-disk-space"));
  assertTrue(host["engineStats"].hasOwnProperty("rocksdb.total-disk-space"));
  assertTrue(telemetrics.hasOwnProperty("date"));
  assertTrue(telemetrics.hasOwnProperty("databases"));
  const databases = telemetrics["databases"];
  assertTrue(databases[0].hasOwnProperty("colls"));
  const colls = databases[0]["colls"];
  assertTrue(colls[0].hasOwnProperty("nShards"));
  assertTrue(colls[0].hasOwnProperty("nDocs"));
  assertTrue(colls[0].hasOwnProperty("type"));
  assertTrue(colls[0].hasOwnProperty("idxs"));
  const idxs = colls[0]["idxs"];
  assertTrue(idxs[0].hasOwnProperty("mem"));
  assertTrue(idxs[0].hasOwnProperty("type"));
  assertTrue(idxs[0].hasOwnProperty("sparse"));
  assertTrue(idxs[0].hasOwnProperty("unique"));
  assertTrue(databases[0].hasOwnProperty("isSingleShard"));
  assertTrue(databases[0].hasOwnProperty("nViews"));
  assertTrue(databases[0].hasOwnProperty("nDisjointGraphs"));
  assertTrue(databases[0].hasOwnProperty("nColls"));
  assertTrue(databases[0].hasOwnProperty("nGraphColls"));
  assertTrue(databases[0].hasOwnProperty("nSmartColls"));
}


function telemetricsOnShellReconnectTestsuite() {
  'use strict';
  const cn = 'testCollection';

  return {

    setUpAll: function () {
      db._create(cn);
    },

    tearDownAll: function () {
      db._drop(cn);
    },

    testTelemetricsRequestByUserAuthorized: function () {
      try {
        let res = getTelemetricsResult();
        assertTrue(res.hasOwnProperty("telemetrics"));
        let telemetrics = res["telemetrics"];
        assertForTelemetricsResponse(telemetrics);
        assertEqual(telemetrics["databases"].length, 1);


        db._createDatabase(databaseName);
        db._useDatabase(databaseName);
        db._create(cn);
        for (let i = 0; i < 2000; ++i) {
          db[cn].insert({value: i + 1, name: "abc"});
        }
        db[cn].ensureIndex({type: "persistent", fields: ["value"], name: "persistentIdx1"});
        db._createView('testView1', 'arangosearch', {});
        db._useDatabase("_system");
        arango.restartTelemetrics();

        res = getTelemetricsResult();
        assertTrue(res.hasOwnProperty("telemetrics"));
        telemetrics = res["telemetrics"];

        assertForTelemetricsResponse(telemetrics);
        assertEqual(telemetrics["databases"].length, 2);
        const databases = telemetrics["databases"];
        const idx = databases[0].nViews === 1 ? 0 : 1;
        assertEqual(databases[idx].nViews, 1);
        let numColls = 0;
        databases[idx]["colls"].forEach(coll => {
          if (coll.nDocs === 2000) {
            numColls++;
            assertEqual(coll.idxs.length, 2);
          } else {
            assertEqual(coll.nDocs, 0);
          }
        });
        // only one of the collections has 2000 documents
        assertEqual(numColls, 1);
      } finally {
        db._dropDatabase(databaseName);
      }
    },

    testTelemetricsRequestByUserNotAuthorized: function () {
      try {
        createUser();
        arango.reconnect(arango.getEndpoint(), '_system', userName, "123");
        arango.restartTelemetrics();
        const res = getTelemetricsResult();
        assertTrue(res.hasOwnProperty("errorNum"));
        assertTrue(res.hasOwnProperty("errorMessage"));
        assertEqual(res.errorNum, HTTP_FORBIDDEN);
        assertTrue(res.errorMessage.includes("insufficient permissions"));
      } finally {
        arango.reconnect(arango.getEndpoint(), '_system', 'root', '');
        if (users.exists(userName)) {
          removeUser();
        }
      }
    },

  };
}

jsunity.run(telemetricsOnShellReconnectTestsuite);
return jsunity.done();
