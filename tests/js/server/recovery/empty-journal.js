/* jshint globalstrict:false, strict:false, unused: false */
/* global runSetup assertNotNull */
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
// / @author Jan Steemann
// / @author Copyright 2013, triAGENS GmbH, Cologne, Germany
// //////////////////////////////////////////////////////////////////////////////

const db = require('@arangodb').db;
;
const fs = require('fs');
const jsunity = require('jsunity');

if (runSetup === true) {
  'use strict';
  global.instanceManager.debugClearFailAt();

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
        return global.instanceManager.debugTerminate('crashing server');
      }
    }
  }
  return 1;
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

jsunity.run(recoverySuite);
return jsunity.done();
