/* jshint globalstrict:false, strict:false, unused : false */
/* global assertEqual, assertFalse, assertTrue */

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

const db = require('@arangodb').db;
const internal = require('internal');
const jsunity = require('jsunity');

const colName1 = 'UnitTestsRecovery1';
const colName2 = 'UnitTestsRecovery2';

function runSetup () {
  'use strict';
  jsunity.jsUnity.attachAssertions();
  
  internal.debugSetFailAt("MerkleTree::serializeUncompressed");
  
  let c = db._create(colName1);

  for (let i = 0; i < 1000; ++i) {
    c.insert({ _key: "test_" + i });
  }

  c = db._create(colName2);
  c.insert({ _key: "piff" });

  let haveUpdates;
  while (true) {
    haveUpdates = false;
    [colName1, colName2].forEach((cn) => {
      let updates = db[cn]._revisionTreePendingUpdates();
      if (!updates.hasOwnProperty('inserts')) {
        // no revision tree yet
        haveUpdates = true;
        return;
      }
      haveUpdates |= updates.inserts > 0;
      haveUpdates |= updates.removes > 0;
      haveUpdates |= updates.truncates > 0;
    });
    if (!haveUpdates) {
      break;
    }
    internal.wait(0.25);
  }

  // intentionally corrupt the trees
  require("console").warn("intentionally corrupting revision trees");
  db[colName1]._revisionTreeCorrupt(23, 42);
  db[colName2]._revisionTreeCorrupt(42, 69);

  assertEqual(23, db[colName1]._revisionTreeSummary().count);
  assertEqual(42, db[colName2]._revisionTreeSummary().count);
  
  internal.debugSetFailAt("RocksDBMetaCollection::forceSerialization");
  internal.debugSetFailAt("applyUpdates::forceHibernation1");
  internal.debugSetFailAt("applyUpdates::forceHibernation2");

  // and force a write
  db[colName1].insert({});
  db[colName2].insert({});
  
  while (true) {
    haveUpdates = false;
    [colName1, colName2].forEach((cn) => {
      let updates = db[cn]._revisionTreePendingUpdates();
      if (!updates.hasOwnProperty('inserts')) {
        // no revision tree yet
        haveUpdates = true;
        return;
      }
      haveUpdates |= updates.inserts > 0;
      haveUpdates |= updates.removes > 0;
      haveUpdates |= updates.truncates > 0;
    });
    if (!haveUpdates) {
      break;
    }
    internal.wait(0.25);
  }
  
  assertEqual(24, db[colName1]._revisionTreeSummary().count);
  assertEqual(43, db[colName2]._revisionTreeSummary().count);

  c.insert({ _key: 'crashme' }, true);

  internal.debugTerminate('crashing server');
}

function recoverySuite () {
  'use strict';
  jsunity.jsUnity.attachAssertions();

  return {
    setUp: function () {
      internal.waitForEstimatorSync(); // make sure estimates are consistent
    },

    testRevisionTreeCorruption: function() {
      const c1 = db._collection(colName1);
      assertEqual(c1._revisionTreeSummary().count, c1.count());
      assertEqual(c1._revisionTreeSummary().count, 1001);

      const c2 = db._collection(colName2);
      assertEqual(c2._revisionTreeSummary().count, c2.count());
      assertEqual(c2._revisionTreeSummary().count, 3);
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
