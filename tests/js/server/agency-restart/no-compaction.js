/* jshint globalstrict:false, strict:false, unused : false */
/* global assertEqual, assertTrue, ArangoAgent */

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
 
  // turn off compaction
  internal.debugSetFailAt("State::compact");
  
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
  
  // make sure everything is synced to disk before we crash
  c.insert({ _key: "sync" }, true); // wait for sync 
  
  internal.debugTerminate('crashing server');
}

function recoverySuite () {
  'use strict';
  jsunity.jsUnity.attachAssertions();

  return {
    testRestart: function () {
      // turn off compaction
      internal.debugSetFailAt("State::compact");

      let state = ArangoAgent.state();
      assertEqual(state.current, state.log[0].index);

      let found = 0;
      let index = state.log[0].index;
      for (let i = 0; i < state.log.length; ++i) {
        let entry = state.log[i];
        if (entry.clientId === 'testi') {
          ++found;
          assertEqual("testi", entry.clientId);
          let keys = Object.keys(entry.query);
          assertEqual(1, keys.length);
          assertTrue(keys[0].startsWith("arango/test"));
          let request = {
            [keys[0]] : {
              "op": "set",
              "new": "testmann" + keys[0].substr(- (keys[0].length - "arango/test".length))
            }
          };
          assertEqual(request, entry.query);
        }
        assertEqual("number", typeof entry.timestamp);
        assertEqual("number", typeof entry.term);
        assertEqual("number", typeof entry.index);
        assertEqual(index, entry.index);
        ++index;
      }
      assertTrue(found > 0);
      
      for (let i = 0; i < 50000; ++i) {
        let r = ArangoAgent.read([["/arango/test" + i]]);
        assertEqual([ { "arango": { ["test" + i] : "testmann" +i } } ], r); 
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
