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

  let waitForUpdatesToFinish = (c) => {
    while (true) {
      let updates = c._revisionTreePendingUpdates();
      if (!updates.hasOwnProperty('inserts')) {
        return;
      }
      if (updates.inserts === 0 && updates.removes === 0) {
        return;
      }
      internal.sleep(0.25);
    }
  };

  
  let docs = [];
  for (let i = 0; i < 1000; ++i) {
    docs.push({ value: i });
  }

  let c1 = db._create(colName1);
  c1.insert(docs);

  waitForUpdatesToFinish(c1);

  // wait long enough so that the tree of c1 is persisted
  internal.sleep(5);

  assertTrue(c1._revisionTreeVerification().equal);
  
  // disable later tree syncs
  internal.debugSetFailAt("needToPersistRevisionTree::checkBuffers");
  internal.debugSetFailAt("serializeMeta::delayCallToLastSerializedRevisionTree");
  internal.debugSetFailAt("RocksDBMetaCollection::forceSerialization");
  
  // let background thread finish
  internal.sleep(2);

  // create another collection, for which the initial tree will be persisted
  let c2 = db._create(colName2);
  c2.insert(docs);
  
  // insert into the original collection
  c1.insert(docs);

  waitForUpdatesToFinish(c2);
  
  // wait long enough so that the tree of c2 is persisted
  internal.sleep(15);

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
      assertEqual(c1._revisionTreeSummary().count, 2000);
      assertTrue(c1._revisionTreeVerification().equal);

      const c2 = db._collection(colName2);
      assertEqual(c2._revisionTreeSummary().count, c2.count());
      assertEqual(c2._revisionTreeSummary().count, 1000);
      assertTrue(c2._revisionTreeVerification().equal);
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
