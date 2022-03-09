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
      let state = ArangoAgent.state();
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
