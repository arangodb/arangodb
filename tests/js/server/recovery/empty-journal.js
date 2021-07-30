/* jshint globalstrict:false, strict:false, unused: false */
/* global assertNotNull */
// //////////////////////////////////////////////////////////////////////////////
// / @brief tests for empty journal files
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
// / @author Copyright 2013, triAGENS GmbH, Cologne, Germany
// //////////////////////////////////////////////////////////////////////////////

const db = require('@arangodb').db;
const internal = require('internal');
const fs = require('fs');
const jsunity = require('jsunity');

function runSetup () {
  'use strict';
  internal.debugClearFailAt();

  let c = db._create('UnitTestsRecovery');
  let walfiles = () => {
    return db._currentWalFiles().map(function(f) {
      // strip off leading `/` or `/archive/` if it exists
      let p = f.split('/');
      return p[p.length - 1];
    });
  };

  let docs = [];
  let payload = Array(1024).join("XuxU");
  for (let i = 0; i < 1000; ++i) {
    docs.push({ value: i, payload });
  }

  let initial = walfiles();
  
  while (true) {
    c.insert(docs);
    let now = walfiles();
    if (now.length > initial.length) {
      // filter out the original files
      let remain = now.filter((f) => {
        return initial.indexOf(f) === -1;
      });

      if (remain.length > 0) {
        // ok, we found a WAL file to destroy!
        let fn = fs.join(db._path(), 'engine-rocksdb', 'journals', remain[0]);
        // remove file and replace it with an empty one!
        require("console").warn("intentionally truncating log file " + fn);
        fs.remove(fn);
        fs.writeFileSync(fn, "");
        
        // crash
        internal.debugTerminate('crashing server');
      }
    }
  }
}

function recoverySuite () {
  'use strict';
  jsunity.jsUnity.attachAssertions();

  return {
    // //////////////////////////////////////////////////////////////////////////////
    // / @brief test that we can still start
    // //////////////////////////////////////////////////////////////////////////////

    testRestart: function () {
      // assert that initial collection exists.
      // as we manually wiped one of the WAL files, 
      // we cannot know how much data is still in it.
      // all we require is a successful restart here
      let c = db._collection('UnitTestsRecovery');
      assertNotNull(c);
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
