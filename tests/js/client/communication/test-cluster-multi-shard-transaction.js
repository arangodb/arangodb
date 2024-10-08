/* jshint globalstrict:false, strict:false, maxlen: 200 */
/* global fail, assertTrue, assertFalse, assertEqual, assertNotEqual, arango */

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
// / @author Wilfried Goesgens
// //////////////////////////////////////////////////////////////////////////////
const _ = require('lodash');
let jsunity = require('jsunity');
let internal = require('internal');
let arangodb = require('@arangodb');
const isEnterprise = require("internal").isEnterprise();
let fs = require('fs');
let pu = require('@arangodb/testutils/process-utils');
let db = arangodb.db;

let { debugCanUseFailAt,
      debugSetFailAt,
      debugResetRaceControl,
      debugRemoveFailAt,
      debugClearFailAt,
      versionHas
    } = require('@arangodb/test-helper');

const isCov = versionHas('coverage');
const {
  launchSnippetInBG,
  joinBGShells,
  cleanupBGShells
} = require('@arangodb/testutils/client-tools').run;

let IM = global.instanceManager;

let waitFor = 60; //isCov ? 60 * 4 : 60;
if (versionHas('asan') || versionHas('tsan') || versionHas('coverage')) {
  waitFor *= 10;
}

function getRandomString() {
  return require("@arangodb/crypto").md5(internal.genRandomAlphaNumbers(32));
}
function MultiShardTransactionSuite () {
  'use strict';
  // generate a random collection name
  const cn = "UnitTests" + getRandomString();
  const testCol = "UnitTestTemp";

  let debug = function(text) {
    console.warn(text);
  };
  let runTests = function (tests, duration) {
    assertFalse(db[cn].exists("stop"));
    let clients = [];
    debug("starting " + tests.length + " test clients");
    try {
      tests.forEach(function (test) {
        let key = test[0];
        let code = test[1];
        let client = launchSnippetInBG(IM.options, code, key, cn);
        client.done = false;
        client.failed = true; // assume the worst
        clients.push(client);
      });

      debug("running test for " + duration + " s...");

      internal.sleep(duration);

      debug("stopping all test clients");

      // broad cast stop signal
      assertFalse(db[cn].exists("stop"));
      db[cn].insert({ _key: "stop" }, { overwriteMode: "ignore" });
      joinBGShells(IM.options, clients, waitFor, cn);

      assertEqual(1 + clients.length, db[cn].count());
      let stats = {};
      clients.forEach(function (client) {
        let doc = db[cn].document(client.key);
        assertEqual(client.key, doc._key);
        assertTrue(doc.done);

        stats[client.key] = doc.iterations;
      });

      debug("test run iterations: " + JSON.stringify(stats));
    } finally {
      cleanupBGShells(clients, cn);
    }
  };

  return {
    setUp: function () {
      db._drop(cn);
      db._create(cn);

      db._drop(testCol);
      let c = db._create(testCol, { numberOfShards: 2 });
    },

    tearDown: function () {
      db._drop(cn);
      db._drop(testCol);
    },

    testQueryInParallel: function () {
      let assocShard = {};
      function getAssocShard(shardId) {
        // print(assocShard)
        if (!assocShard.hasOwnProperty(shardId)) {
          assocShard[shardId] = Object.keys(assocShard).length;
        }
        return assocShard[shardId];
      }
      let c = db[testCol];
      let shardIds = [[],[]];
      const maxLength = 1000;
      while (shardIds[0].length + shardIds[1].length < maxLength * 2) {
        let id = getRandomString();
        let shardNo = getAssocShard(c.getResponsibleShard(id));
        if (shardIds[shardNo].length < maxLength) {
          shardIds[shardNo].push(id);
        }
      }
      let tests = [];
      for(let count = 0; count < 2; count++) {
        tests.push([
          `TransactionInsertTest_${count}`,
          `
let myKeys = ${JSON.stringify(shardIds[count])};
let cn = "${testCol}";
let lastCount = 0;
myKeys.forEach(oneKey => {
  let trx = db._createTransaction({ collections: { write: [cn] } });
  let c = trx.collection(cn);
  c.insert({_key: oneKey});
  trx.commit();
  let count = db[cn].count();
  if (count <= lastCount) {
    print(oneKey + " - Was expecting to have " + count + " > " + lastCoun)
    throw new Error("Was expecting to have " + count + " > " + lastCoun);
  }
  lastCount = count;
})

return false;`
        ]);
      }
      // print(JSON.stringify(tests))
      // run the suite for a while...
      runTests(tests, 30);
      assertEqual(c.count(), maxLength*2);
    },
    testQueryCountInParallel: function () {
      let assocShard = {};
      function getAssocShard(shardId) {
        // print(assocShard)
        if (!assocShard.hasOwnProperty(shardId)) {
          assocShard[shardId] = Object.keys(assocShard).length;
        }
        return assocShard[shardId];
      }
      let c = db[testCol];
      let shardIds = [[],[]];
      const maxLength = 1000;
      while (shardIds[0].length + shardIds[1].length < maxLength * 2) {
        let id = getRandomString();
        let shardNo = getAssocShard(c.getResponsibleShard(id));
        if (shardIds[shardNo].length < maxLength) {
          shardIds[shardNo].push(id);
        }
      }
      let tests = [];
      for(let count = 0; count < 2; count++) {
        tests.push([
          `TransactionInsertTest_${count}`,
          `
let myKeys = ${JSON.stringify(shardIds[count])};
let cn = "${testCol}";
let lastCount = 0;
let loopCount = 0;
const stepWidth = 10;
myKeys.forEach(oneKey => {
  let trx = db._createTransaction({ collections: { write: [cn], exclusive: [cn] } });
  let c = trx.collection(cn);
  c.insert({_key: oneKey});
  let count = c.count();
  if (count <= lastCount) {
    print(oneKey + " - Was expecting to have " + count + " > " + lastCoun)
    trx.abort();
    throw new Error("Was expecting to have " + count + " > " + lastCoun);
  }
  trx.commit();
  loopCount += 1;
  count = db[cn].count();
  if (count <= lastCount) {
    print(oneKey + " - Was expecting to have " + count + " > " + lastCoun)
    throw new Error("Was expecting to have " + count + " > " + lastCoun);
  }
  let tries = 0;
  while ((loopCount % stepWidth === 0) && (count % stepWidth !== 0)) {
    tries ++;
    if (tries > 10) {
      throw new Error("failed to get to the next step in 5s");
    }
    require('internal').sleep(0.5);
    count = db[cn].count();
  }
  lastCount = count;
})

return false;`
        ]);
      }
      // print(JSON.stringify(tests))
      // run the suite for a while...
      runTests(tests, 30);
      assertEqual(c.count(), maxLength*2);
    },
  };
}

jsunity.run(MultiShardTransactionSuite);

return jsunity.done();
