/* jshint globalstrict:false, strict:false, unused : false */
/* global assertEqual, assertNull, assertTrue, assertFalse */

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

var db = require('@arangodb').db;
var internal = require('internal');
var jsunity = require('jsunity');

function runSetup () {
  'use strict';
  internal.debugClearFailAt();

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
  internal.debugSegfault('crashing server');
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief test suite
// //////////////////////////////////////////////////////////////////////////////

function recoverySuite () {
  'use strict';
  jsunity.jsUnity.attachAssertions();

  return {
    setUp: function () {},
    tearDown: function () {},

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
