/* jshint globalstrict:false, strict:false, unused : false */
/* global runSetup assertEqual, assertTrue, assertFalse, assertNull, fail, print, idx */
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
/// @author Andrei Lobov
// //////////////////////////////////////////////////////////////////////////////

const db = require('@arangodb').db;
const internal = require('internal');
const jsunity = require('jsunity');
const ct = require('@arangodb/testutils/client-tools');
let IM = global.instanceManager;
const RED = internal.COLORS.COLOR_RED;
const RESET = internal.COLORS.COLOR_RESET;

if (runSetup === true) {
  'use strict';
  global.instanceManager.debugClearFailAt();

  db._drop('UnitTestsRecoveryDummy');
  let c = db._create('UnitTestsRecoveryDummy');

  db._dropView('UnitTestsRecoveryView');
  db._createView('UnitTestsRecoveryView', 'arangosearch', {});

  let meta = {links: {'UnitTestsRecoveryDummy': {includeAllFields: true}}};
  db._view('UnitTestsRecoveryView').properties(meta);
  let data = [];
  for (let i = 0; i < 1000; i++) {
    data.push({a: "foo_" + i, b: "bar_" + i, c: i});
  }
  db.UnitTestsRecoveryDummy.save(data);
  db._query("FOR d IN UnitTestsRecoveryView OPTIONS {waitForSync:true} LIMIT 1 RETURN d");
  let clientInstances = [];
  ct.run.spawnStressArangoshInBG(
    clientInstances,
    function () {
      const minMax = [
        [1250, 1500],
        [1000, 1100],
        [1500, 2000]
      ];
      const db = require('@arangodb').db;
      let col = db._collection('UnitTestsRecoveryDummy');
      let data = [];
      for (let i = minMax[idx][0]; i < minMax[idx][1]; i++) {
        data.push({a: "foo_" + i, b: "bar_" + i, c: i});
      }
      db.UnitTestsRecoveryDummy.save(data);
      console.log('done');
      return 0;
    }, 'doc', 3);
  let wait = 100;
  while (wait > 0) {
    wait--;
    internal.wait(1);
    // checking if server is alive.
    db._query("FOR d IN UnitTestsRecoveryDummy LIMIT 1 RETURN d");
  }

  try {
    if (ct.run.joinForceBGShells(IM.options, clientInstances)) {
      print(`collected ${clientInstances.length} clients`);
      return 0;
    } else {
      print(`${RED}not all clients could be joined successfully: ${JSON.stringify(clientInstances)}${RESET}`);
      return 1;
    }
  } catch (ex) {
    print(`${RED}Failed to stop stress arangoshs: ${ex.message}\n${ex.stack}${RESET}`);
    return -1;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////

function recoverySuite() {
  'use strict';
  jsunity.jsUnity.attachAssertions();

  return {
    setUp: function () {
    },
    tearDown: function () {
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief test whether we can restore the trx data
    ////////////////////////////////////////////////////////////////////////////////

    testIResearchLinkThreeTransactionsStage2: function () {
      let v = db._view('UnitTestsRecoveryView');
      assertEqual(v.name(), 'UnitTestsRecoveryView');
      assertEqual(v.type(), 'arangosearch');
      let p = v.properties().links;
      assertTrue(p.hasOwnProperty('UnitTestsRecoveryDummy'));
      assertTrue(p.UnitTestsRecoveryDummy.includeAllFields);

      let result = db._query("FOR doc IN UnitTestsRecoveryView SEARCH doc.c >= 0 OPTIONS {waitForSync: true} COLLECT WITH COUNT INTO length RETURN length").toArray();
      let expectedResult = db._query("FOR doc IN UnitTestsRecoveryDummy FILTER doc.c >= 0 COLLECT WITH COUNT INTO length RETURN length").toArray();
      assertEqual(result[0], expectedResult[0]);
      assertEqual(result[0], 1850);
    }

  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suite
////////////////////////////////////////////////////////////////////////////////

jsunity.run(recoverySuite);
return jsunity.done();
