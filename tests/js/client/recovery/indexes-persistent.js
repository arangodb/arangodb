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

var db = require('@arangodb').db;
var internal = require('internal');
var jsunity = require('jsunity');

if (runSetup === true) {
  'use strict';
  global.instanceManager.debugClearFailAt();

  db._drop('UnitTestsRecovery1');
  var c = db._create('UnitTestsRecovery1'), i;
  c.ensureIndex({ type: 'persistent', fields: ['value'] });

  for (i = 0; i < 1000; ++i) {
    c.save({ value: i });
  }

  db._drop('UnitTestsRecovery2');
  c = db._create('UnitTestsRecovery2');
  c.ensureIndex({ type: 'persistent', fields: ['a.value'], unique: true });

  for (i = 0; i < 1000; ++i) {
    c.save({ a: { value: i } });
  }

  db._drop('UnitTestsRecovery3');
  c = db._create('UnitTestsRecovery3');
  c.ensureIndex({ type: 'persistent', fields: ['a', 'b'] });

  for (i = 0; i < 500; ++i) {
    c.save({ a: (i % 2) + 1, b: 1 });
    c.save({ a: (i % 2) + 1, b: 2 });
  }

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


    // //////////////////////////////////////////////////////////////////////////////
    // / @brief test whether we can restore the trx data
    // //////////////////////////////////////////////////////////////////////////////

    testIndexesRocksDB: function () {
      var c = db._collection('UnitTestsRecovery1'), idx, i;
      idx = c.getIndexes()[1];
      assertFalse(idx.unique);
      assertFalse(idx.sparse);
      assertEqual([ 'value' ], idx.fields);
      for (i = 0; i < 1000; ++i) {
        assertEqual(1, c.byExample({ value: i }).toArray().length, i);
      }

      c = db._collection('UnitTestsRecovery2');
      idx = c.getIndexes()[1];
      assertTrue(idx.unique);
      assertFalse(idx.sparse);
      assertEqual([ 'a.value' ], idx.fields);
      for (i = 0; i < 1000; ++i) {
        assertEqual(1, c.byExample({ 'a.value': i }).toArray().length);
      }

      c = db._collection('UnitTestsRecovery3');
      idx = c.getIndexes()[1];
      assertFalse(idx.unique);
      assertFalse(idx.sparse);
      assertEqual([ 'a', 'b' ], idx.fields);

      assertEqual(250, c.byExample({ a: 1, b: 1 }).toArray().length);
      assertEqual(250, c.byExample({ a: 1, b: 2 }).toArray().length);
      assertEqual(250, c.byExample({ a: 2, b: 1 }).toArray().length);
      assertEqual(250, c.byExample({ a: 2, b: 2 }).toArray().length);
    }

  };
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief executes the test suite
// //////////////////////////////////////////////////////////////////////////////

jsunity.run(recoverySuite);
return jsunity.done();
