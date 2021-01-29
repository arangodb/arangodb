/* jshint globalstrict:false, strict:false, unused : false */
/* global assertEqual, ArangoAgent */

// //////////////////////////////////////////////////////////////////////////////
// / @brief tests for dump/reload
// /
// / @file
// /
// / DISCLAIMER
// /
// / Copyright 2010-2012 triagens GmbH, Cologne, Germany
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
// / @author Jan Steemann
// / @author Copyright 2012, triAGENS GmbH, Cologne, Germany
// //////////////////////////////////////////////////////////////////////////////

let db = require('@arangodb').db;
let internal = require('internal');
let jsunity = require('jsunity');

function runSetup () {
  'use strict';
  internal.debugClearFailAt();
 
  db._drop('UnitTestsRecovery');
  let c = db._create('UnitTestsRecovery');

  // write 50k log entries, and then crash
  for (let i = 0; i < 50000; ++i) {
    let request = {
      ["arango/test" + i] : {
        "op": "set",
        "new": "testmann" + i
      }
    };
        
    ArangoAgent.write([[ request, {}, "testi"]]);
  }

  // intentionally drop "compact" collection so it is
  // not there at restart
  db._drop("compact");
  
  // make sure everything is synced to disk before we crash
  c.insert({ _key: "sync" }, true); // wait for sync 
  
  internal.debugTerminate('crashing server');
}

function recoverySuite () {
  'use strict';
  jsunity.jsUnity.attachAssertions();

  return {
    testRestart: function () {
      let state = ArangoAgent.state();
      assertEqual(0, state.current);
      assertEqual(state.current, state.log[0].index);

      // if the compaction collection does not exist, then
      // it means all data will be wiped at startup
      let index = state.log[0].index;
      let found = 0;
      for (let i = 0; i < state.log.length; ++i) {
        let entry = state.log[i];
        if (entry.clientId === 'testi') {
          ++found;
        }
        assertEqual("number", typeof entry.timestamp);
        assertEqual("number", typeof entry.term);
        assertEqual("number", typeof entry.index);
        assertEqual(index, entry.index);
        ++index;
      }
      assertEqual(0, found);
      
      for (let i = 0; i < 50000; ++i) {
        let r = ArangoAgent.read([["/arango/test" + i]]);
        assertEqual([{}], r);
      }
    }

  };
}

function main (argv) {
  'use strict';
  if (argv[1] === 'setup') {
    runSetup();
    return 0;
  } else {
    jsunity.run(recoverySuite);
    return jsunity.writeDone().status ? 0 : 1;
  }
}
