/* jshint globalstrict:false, strict:false, unused : false */
/* global runSetup assertEqual, assertFalse, assertTrue */

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
const colName3 = 'UnitTestsRecovery3';
const est1 = 1; // The index is de-facto unique so estimate 1
const est2 = 1; // This index is unique. Estimate 1
const est3 = 4 / 1000; // This index has 4 different values and stores 1000 documents

if (runSetup === true) {
  'use strict';
  global.instanceManager.debugClearFailAt();

  db._drop(colName1);
  let c = db._create(colName1);
  c.ensureIndex({ type: "hash", fields: ["value"] });

  let docs = [];
  for (let i = 0; i < 1000; ++i) {
    docs.push({ value: i });
  }
  c.insert(docs);

  db._drop(colName2);
  c = db._create(colName2);
  c.ensureIndex({ type: "hash", fields: ["a.value"], unique: true });

  docs = [];
  for (let i = 0; i < 1000; ++i) {
    docs.push({ a: { value: i } });
  }
  c.insert(docs);

  db._drop(colName3);
  c = db._create(colName3);
  c.ensureIndex({ type: "hash", fields: ["a", "b"] });

  docs = [];
  for (let i = 0; i < 500; ++i) {
    docs.push({ a: (i % 2) + 1, b: 1 });
    docs.push({ a: (i % 2) + 1, b: 2 });
  }
  c.insert(docs);

  db._drop('test');
  c = db._create('test');
  c.save({ _key: 'crashme' }, true);
  return 0;
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief test suite
// //////////////////////////////////////////////////////////////////////////////

function recoverySuite () {
  'use strict';
  jsunity.jsUnity.attachAssertions();

  return {
    setUp: function () {
    },
    tearDown: function () {},

    // //////////////////////////////////////////////////////////////////////////////
    // / @brief test whether we can restore the trx data
    // //////////////////////////////////////////////////////////////////////////////

    testSingleAttributeHashIndexInfo: function() {
      let c = db._collection(colName1);
      let idx = c.getIndexes()[1];
      assertFalse(idx.unique);
      assertFalse(idx.sparse);
      assertEqual([ 'value' ], idx.fields);
    },

    testSingleAttributeHashIndexByExample: function() {
      let c = db._collection(colName1);
      for (let i = 0; i < 1000; ++i) {
        assertEqual(1, c.byExample({ value: i }).toArray().length);
      }
    },

    testSingleAttributeHashIndexAql: function() {
      assertEqual(1, db._query(`FOR doc IN ${colName1} FILTER doc.value == 0 RETURN doc`).toArray().length);
    },

    testSingleAttributeHashIndexEstimate: function () {
      let c = db._collection(colName1);
      let idx = c.getIndexes()[1];
      assertEqual(est1, idx.selectivityEstimate);
    },

    testNestedAttributeHashIndexInfo: function() {
      let c = db._collection(colName2);
      let idx = c.getIndexes()[1];
      assertTrue(idx.unique);
      assertFalse(idx.sparse);
      assertEqual([ 'a.value' ], idx.fields);
    },

    testNestedAttributeHashIndexByExample: function() {
      let c = db._collection(colName2);
      for (let i = 0; i < 1000; ++i) {
        assertEqual(1, c.byExample({ 'a.value': i }).toArray().length);
      }
    },

    testNestedAttributeHashIndexAql: function() {
      assertEqual(1, db._query(`FOR doc IN ${colName2} FILTER doc.a.value == 0 RETURN doc`).toArray().length);
    },

    testNestedAttributeHashIndexEstimate: function () {
      let c = db._collection(colName2);
      let idx = c.getIndexes()[1];
      assertEqual(est2, idx.selectivityEstimate);
    },

    testManyAttributesHashIndexInfo: function() {
      let c = db._collection(colName3);
      let idx = c.getIndexes()[1];
      assertFalse(idx.unique);
      assertFalse(idx.sparse);
      assertEqual([ 'a', 'b' ], idx.fields);
    },

    testManyAttributesHashIndexByExample: function() {
      let c = db._collection(colName3);
      assertEqual(250, c.byExample({ a: 1, b: 1 }).toArray().length);
      assertEqual(250, c.byExample({ a: 1, b: 2 }).toArray().length);
      assertEqual(250, c.byExample({ a: 2, b: 1 }).toArray().length);
      assertEqual(250, c.byExample({ a: 2, b: 2 }).toArray().length);
    },

    testManyAttributesHashIndexAql: function() {
      assertEqual(250, db._query(`FOR doc IN ${colName3} FILTER doc.a == 1 && doc.b == 1 RETURN doc`).toArray().length);
    },

    testManyAttributesHashIndexEstimate: function () {
      let c = db._collection(colName3);
      let idx = c.getIndexes()[1];
      assertEqual(est3, idx.selectivityEstimate);
    },

  };
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief executes the test suite
// //////////////////////////////////////////////////////////////////////////////

jsunity.run(recoverySuite);
return jsunity.done();
