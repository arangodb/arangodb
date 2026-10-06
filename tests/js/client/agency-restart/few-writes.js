/* jshint globalstrict:false, strict:false, unused : false */
/* global assertEqual, assertTrue, runSetup */

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
// //////////////////////////////////////////////////////////////////////////////

let db = require('@arangodb').db;
let internal = require('internal');
let jsunity = require('jsunity');
let IM = global.instanceManager;
let AM = IM.agencyMgr;

function runSetupRoutine () {
  'use strict';
  
  db._drop('UnitTestsRecovery');
  let c = db._create('UnitTestsRecovery');

  // write 10 log entries, and then crash
  for (let i = 0; i < 10; ++i) {
    let request = {
      ["arango/test" + i] : {
        "op": "set",
        "new": "testmann" + i
      }
    };
        
    AM.write([[ request, {}, "testi"]]);
  }
  
  // make sure everything is synced to disk before we crash
  c.insert({ _key: "sync" }, true); // wait for sync 
  
  IM.debugTerminate('crashing server');
}

function recoverySuite () {
  'use strict';
  jsunity.jsUnity.attachAssertions();

  return {
    testRestart: function () {
      let state = AM.state();
      assertEqual(0, state.current);
      assertTrue(state.log.length > 10);
      let start = 0;
      let index = 0;
      while (start < state.log.length) {
        if (state.log[start].clientId === 'testi') {
          index = state.log[start].index;
          break;
        }
        ++start;
      }

      for (let i = 0; i < 10; ++i) {
        let entry = state.log[start + i];
        assertEqual("testi", entry.clientId);
        let request = {
          ["arango/test" + i] : {
            "op": "set",
            "new": "testmann" + i
          }
        };
        assertEqual(request, entry.query);
        assertEqual("number", typeof entry.timestamp);
        assertEqual("number", typeof entry.term);
        assertEqual("number", typeof entry.index);
        assertEqual(index, entry.index);
        ++index;
      }
      
      for (let i = 0; i < 10; ++i) {
        let r = AM.get([["/arango/test" + i]]);
        assertEqual([ { "arango": { ["test" + i] : "testmann" +i } } ], r); 
      }
    }

  };
}

for (let i = 0; i < 100; i++) {
  if (AM.leading().leading) {
    break;
  }
  require('internal').sleep(0.5);
}

if (runSetup) {
  runSetupRoutine();
  return 0;
} else {
  jsunity.run(recoverySuite);
  return jsunity.writeDone();
}

