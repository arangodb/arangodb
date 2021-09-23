/* jshint globalstrict:false, strict:false, unused : false */
/* global assertEqual, assertFalse, assertTrue, assertNotNull */

// //////////////////////////////////////////////////////////////////////////////
// / @brief tests for revision trees
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

const fs = require('fs');
const db = require('@arangodb').db;
const internal = require('internal');
const jsunity = require('jsunity');
const time = internal.time;

const colName = 'UnitTestsRecovery';
// number of collections
const n = 5;
// max number of restarts/iterations
const maxIterations = 5;
// runtime for each iteration
const runTime = 20;
  
const stateFile = require("internal").env['state-file'];
let iteration = 1;
try {
  iteration = parseInt(String(fs.readFileSync(stateFile)));
} catch (err) {
  // it does not matter if the file does not exist
}

function runSetup () {
  'use strict';

  // create missing collections
  for (let i = 0; i < n; ++i) {
    if (db._collection(colName + i) === null) {
      db._create(colName + i);
    }
  }
  if (db._collection("control") === null) {
    db._create("control");
  }

  require("console").warn("summoning chaos...");

  let collectionCounts = {};
  for (let i = 0; i < n; ++i) {
    let c = db._collection(colName + i);
    if (c.count() !== c.toArray().length) {
      throw "invalid collection counts for collection " + c.name();
    }
    collectionCounts[c.name()] = c.count();
  }

  const end = time() + runTime;
  do {
    // pick a random collection
    const cn = Math.floor(Math.random() * n);
    const c = db._collection(colName + cn);

    const opType = Math.random();
    let numOps = Math.floor(1000 * Math.random());
    if (opType >= 0.75) {
      // multi-document insert
      let docs = [];
      for (let i = 0; i < numOps; ++i) {
        docs.push({ value: i, valueString: String(i), type: "multi" });
      }
      c.insert(docs);
      collectionCounts[c.name()] += numOps;
    } else if (opType >= 0.5) {
      // single doc inserts
      for (let i = 0; i < numOps; ++i) {
        c.insert({ value: i, valueString: String(i), type: "individual" });
      }
      collectionCounts[c.name()] += numOps;
    } else if (opType >= 0.3) {
      // single operation insert
      c.insert({ value: 666, valueString: "666", type: "single" });
      collectionCounts[c.name()] += 1;
    } else if (opType >= 0.05) {
      // remove
      numOps /= 2;
      let keys = db._query("FOR doc IN " + c.name() + " LIMIT @numOps RETURN doc._key", { numOps }).toArray();
      if (opType >= 0.15) {
        // multi-document remove
        c.remove(keys);
      } else {
        keys.forEach((key) => {
          c.remove(key);
        });
      }
      collectionCounts[c.name()] -= keys.length;
    } else {
      // truncate
      c.truncate();
      collectionCounts[c.name()] = 0;
    }
  } while (time() < end);
 
  db.control.insert({ _key: "counts", collectionCounts }, { waitForSync: true, overwriteMode: "replace" });

  internal.debugTerminate('crashing server');
}

function recoverySuite () {
  'use strict';
  jsunity.jsUnity.attachAssertions();
  
  if (iteration < maxIterations) {
    ++iteration;
    // update iteration counter
    fs.writeFileSync(stateFile, String(iteration));
  } else {
    // 10 iterations done, now remove state file
    fs.remove(stateFile);
  }

  return {
    setUp: function () {
      internal.waitForEstimatorSync(); // make sure estimates are consistent
    },

    testConsistency: function() {
      const counts = db.control.document("counts");
      assertNotNull(counts);

      for (let i = 0; i < n; ++i) {
        let c = db._collection(colName + i);
        assertEqual(c.count(), c.toArray().length);
        assertTrue(counts.collectionCounts.hasOwnProperty(c.name()));
        assertEqual(c.count(), counts.collectionCounts[c.name()]);
        assertEqual(c._revisionTreeSummary().count, c.count());
        assertTrue(c._revisionTreeVerification().equal);
      }
    },

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
