/* jshint globalstrict:false, strict:false, unused : false */
/* global assertEqual */
// //////////////////////////////////////////////////////////////////////////////
// / @brief tests for transactions
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
// / @author Copyright 2013, triAGENS GmbH, Cologne, Germany
// //////////////////////////////////////////////////////////////////////////////

var db = require('@arangodb').db;
var internal = require('internal');
var jsunity = require('jsunity');

function runSetup () {
  'use strict';
  internal.debugClearFailAt();

  db._drop('UnitTestsRecovery');
  db._create('UnitTestsRecovery');

  internal.debugSetFailAt('LogfileManagerWriteShutdown');

  db._executeTransaction({
    collections: {
      write: [ 'UnitTestsRecovery' ]
    },
    waitForSync: true,
    action: function () {
      var db = require('@arangodb').db, i;
      var c = db._collection('UnitTestsRecovery');

      for (i = 0; i < 200000; ++i) {
        c.save({ _key: 'test' + i, value1: i, value2: 'foobarbaz' + i });
      }
    }
  });

  internal.debugTerminate('crashing server');
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
    // / @brief test whether we can restore the 10 collections
    // //////////////////////////////////////////////////////////////////////////////

    testNoShutdownInfoMultipleLogs: function () {
      var c = db._collection('UnitTestsRecovery');

      assertEqual(200000, c.count());

      var i;
      for (i = 0; i < 200000; ++i) {
        assertEqual(i, c.document('test' + i).value1);
        assertEqual('foobarbaz' + i, c.document('test' + i).value2);
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
