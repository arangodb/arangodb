/* jshint globalstrict:false, strict:false, unused : false */
/* global assertEqual, assertFalse, assertTrue */

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
// / @author Copyright 2012, triAGENS GmbH, Cologne, Germany
// //////////////////////////////////////////////////////////////////////////////

const db = require('@arangodb').db;
const internal = require('internal');
const jsunity = require('jsunity');

const colName1 = 'UnitTestsRecovery1';
const colName2 = 'UnitTestsRecovery2';

function runSetup () {
  'use strict';
  let c = db._create(colName1);

  for (let i = 0; i < 1000; ++i) {
    c.insert({ _key: "test_" + i });
  }

  c = db._create(colName2);

  let docs = [];
  for (let i = 0; i < 100000; ++i) {
    docs.push({ _key: "test" + i });
    if (docs.length === 5000) {
      c.insert(docs);
      docs = [];
    }
  }
  
  // wait until all changes have been applied
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
  
  // now break inserts
  internal.debugSetFailAt("RevisionTree::applyInserts");
  
  c = db._collection(colName1);

  for (let i = 0; i < 1000; ++i) {
    c.insert({ _key: "new_" + i });
  }

  c = db._collection(colName2);

  docs = [];
  for (let i = 0; i < 100000; ++i) {
    docs.push({ _key: "new_" + i });
    if (docs.length === 5000) {
      c.insert(docs);
      docs = [];
    }
  }

  // wait for at least one iteration of RocksDBBackgroundThread.
  // unfortunately there is no proper way to control that it was
  // was already run with our data above
  internal.wait(6.0);
  
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

    testRevisionTreeInsertFails: function() {
      const c1 = db._collection(colName1);
      assertEqual(2000, c1.count());
      assertEqual(c1._revisionTreeSummary().count, c1.count());
      assertEqual(c1._revisionTreeSummary().count, 2000);

      const c2 = db._collection(colName2);
      assertEqual(200001, c2.count());
      assertEqual(c2._revisionTreeSummary().count, c2.count());
      assertEqual(c2._revisionTreeSummary().count, 200001);
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
