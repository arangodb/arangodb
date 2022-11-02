/* jshint globalstrict:false, strict:false, unused : false */
/* global assertEqual, assertFalse, assertTrue */

// //////////////////////////////////////////////////////////////////////////////
// / DISCLAIMER
// /
// / Copyright 2022 ArangoDB GmbH, Cologne, Germany
// /
// / Licensed under the Apache License, Version 2.0 (the "License");
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
// / Copyright holder is ArangoDB GmbH, Cologne, Germany
// /
// / @author Julia Puget
// / @author Copyright 2022, ArangoDB GmbH, Cologne, Germany
// //////////////////////////////////////////////////////////////////////////////

const db = require('@arangodb').db;
const internal = require('internal');
const jsunity = require('jsunity');
const fs = require('fs');
const cn = "UnitTestCollection";

function runSetup() {
  'use strict';
  internal.debugClearFailAt();

  db._drop(cn);
  let c = db._create(cn);

  let docs = [];
  for (let i = 0; i < 500000; ++i) {
    docs.push({value1: i});
    if (docs.length === 10000) {
      c.insert(docs);
      docs = [];
    }
  }

  db._query(`FOR doc IN ${cn} SORT doc.value1 ASC RETURN doc`, null, {spillOverThresholdNumRows: 5000, stream: true});

  internal.debugTerminate('crashing server');
}

function recoverySuite() {
  'use strict';
  jsunity.jsUnity.attachAssertions();

  return {
    // //////////////////////////////////////////////////////////////////////////////
    // / @brief test whether the temp directory was cleaned up after query execution
    // //////////////////////////////////////////////////////////////////////////////

    testTempDirCleanupAfterQuery: function() {
      const tempDir = fs.join(internal.options()["temp.intermediate-results-path"], "temp");
      const tree = fs.listTree(tempDir);
      assertEqual(tree.length, 1);
    }
  };
}

function main(argv) {
  'use strict';
  if (argv[1] === 'setup') {
    runSetup();
    return 0;
  } else {
    jsunity.run(recoverySuite);
    return jsunity.writeDone().status ? 0 : 1;
  }
}
