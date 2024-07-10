/* jshint globalstrict:false, strict:false, unused : false */
/* global runSetup assertEqual, assertNull, assertTrue, assertFalse */

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

  db._drop('UnitTestsRecovery');
  db._create('UnitTestsRecovery');
  var i;
  for (i = 0; i < 10; ++i) {
    db._collection('UnitTestsRecovery').save({ _key: 'test' + i, value: i });
  }
  var id = db.UnitTestsRecovery._id;

  db._drop('UnitTestsRecovery');
  // wait until directory gets deleted
  require('internal').wait(5, false);

  // re-create using the same id
  var c = db._create('UnitTestsRecovery', { id: id });
  for (i = 100; i < 110; ++i) {
    c.save({ _key: 'test' + i, value: i });
  }

  require('internal').wal.flush(true, true);
  require('internal').wait(5, false);

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

    testCollectionDropAndRecreate: function () {
      var c = db._collection('UnitTestsRecovery'), i;
      assertEqual(11, c.count());

      for (i = 0; i < 10; ++i) {
        assertFalse(c.exists('test' + i));
      }

      for (i = 100; i < 110; ++i) {
        assertTrue(c.exists('test' + i));
        assertEqual(i, c.document('test' + i).value);
      }
    }

  };
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief executes the test suite
// //////////////////////////////////////////////////////////////////////////////

jsunity.run(recoverySuite);
return jsunity.done();
