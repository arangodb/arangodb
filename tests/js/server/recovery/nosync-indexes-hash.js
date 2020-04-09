/* jshint globalstrict:false, strict:false, unused : false */
/* global assertEqual, assertFalse, assertTrue */

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

const db = require('@arangodb').db;
const internal = require('internal');
const jsunity = require('jsunity');

const colName1 = 'UnitTestsRecovery1';
const colName2 = 'UnitTestsRecovery2';
const colName3 = 'UnitTestsRecovery3';
const est1 = 1; // The index is de-facto unique so estimate 1
const est2 = 1; // This index is unique. Estimate 1
const est3 = 4 / 1000; // This index has 4 different values and stores 1000 documents

function runSetup () {
  'use strict';
  internal.debugClearFailAt();

  // turn off syncing of counters etc.  
  internal.debugSetFailAt("RocksDBSettingsManagerSync"); 

  db._drop(colName1);
  var c = db._create(colName1), i;
  c.ensureHashIndex('value');

  for (i = 0; i < 1000; ++i) {
    c.save({ value: i });
  }

  db._drop(colName2);
  c = db._create(colName2);
  c.ensureUniqueConstraint('a.value');

  for (i = 0; i < 1000; ++i) {
    c.save({ a: { value: i } });
  }

  db._drop(colName3);
  c = db._create(colName3);
  c.ensureHashIndex('a', 'b');

  for (i = 0; i < 500; ++i) {
    c.save({ a: (i % 2) + 1, b: 1 });
    c.save({ a: (i % 2) + 1, b: 2 });
  }

  db._drop('test');
  c = db._create('test');
  c.save({ _key: 'crashme' }, true);

  internal.debugTerminate('crashing server');
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief test suite
// //////////////////////////////////////////////////////////////////////////////

function recoverySuite () {
  'use strict';
  jsunity.jsUnity.attachAssertions();

  return {
    setUp: function () {
      internal.waitForEstimatorSync(); // make sure estimates are consistent
    },
    tearDown: function () {},

    // //////////////////////////////////////////////////////////////////////////////
    // / @brief test whether we can restore the trx data
    // //////////////////////////////////////////////////////////////////////////////

    testNoSyncSingleAttributeHashIndexInfo: function() {
      let c = db._collection(colName1);
      assertEqual(c.count(), 1000);
      let idx = c.getIndexes()[1];
      assertFalse(idx.unique);
      assertFalse(idx.sparse);
      assertEqual([ 'value' ], idx.fields);
    },

    testNoSyncSingleAttributeHashIndexByExample: function() {
      let c = db._collection(colName1);
      for (let i = 0; i < 1000; ++i) {
        assertEqual(1, c.byExample({ value: i }).toArray().length);
      }
    },

    testNoSyncSingleAttributeHashIndexAql: function() {
      assertEqual(1, db._query(`FOR doc IN ${colName1} FILTER doc.value == 0 RETURN doc`).toArray().length);
    },

    testNoSyncSingleAttributeHashIndexEstimate: function () {
      let c = db._collection(colName1);
      let idx = c.getIndexes()[1];
      assertEqual(est1, idx.selectivityEstimate);
    },

    testNoSyncNestedAttributeHashIndexInfo: function() {
      let c = db._collection(colName2);
      assertEqual(c.count(), 1000);
      let idx = c.getIndexes()[1];
      assertTrue(idx.unique);
      assertFalse(idx.sparse);
      assertEqual([ 'a.value' ], idx.fields);
    },

    testNoSyncNestedAttributeHashIndexByExample: function() {
      let c = db._collection(colName2);
      for (let i = 0; i < 1000; ++i) {
        assertEqual(1, c.byExample({ 'a.value': i }).toArray().length);
      }
    },

    testNoSyncNestedAttributeHashIndexAql: function() {
      assertEqual(1, db._query(`FOR doc IN ${colName2} FILTER doc.a.value == 0 RETURN doc`).toArray().length);
    },

    testNoSyncNestedAttributeHashIndexEstimate: function () {
      let c = db._collection(colName2);
      let idx = c.getIndexes()[1];
      assertEqual(est2, idx.selectivityEstimate);
    },

    testNoSyncManyAttributesHashIndexInfo: function() {
      let c = db._collection(colName3);
      let idx = c.getIndexes()[1];
      assertFalse(idx.unique);
      assertFalse(idx.sparse);
      assertEqual([ 'a', 'b' ], idx.fields);
    },

    testNoSyncManyAttributesHashIndexByExample: function() {
      let c = db._collection(colName3);
      assertEqual(250, c.byExample({ a: 1, b: 1 }).toArray().length);
      assertEqual(250, c.byExample({ a: 1, b: 2 }).toArray().length);
      assertEqual(250, c.byExample({ a: 2, b: 1 }).toArray().length);
      assertEqual(250, c.byExample({ a: 2, b: 2 }).toArray().length);
    },

    testNoSyncManyAttributesHashIndexAql: function() {
      assertEqual(250, db._query(`FOR doc IN ${colName3} FILTER doc.a == 1 && doc.b == 1 RETURN doc`).toArray().length);
    },

    testNoSyncManyAttributesHashIndexEstimate: function () {
      let c = db._collection(colName3);
      assertEqual(c.count(), 1000);
      let idx = c.getIndexes()[1];
      assertEqual(est3, idx.selectivityEstimate);
    },

  };
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief executes the test suite
// //////////////////////////////////////////////////////////////////////////////

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
