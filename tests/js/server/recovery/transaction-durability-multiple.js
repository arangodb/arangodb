/* jshint globalstrict:false, strict:false, unused : false */
/* global assertEqual */

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

  db._drop('UnitTestsRecovery1');
  db._drop('UnitTestsRecovery2');
  db._create('UnitTestsRecovery1');
  db._create('UnitTestsRecovery2');

  db._executeTransaction({
    collections: {
      write: [ 'UnitTestsRecovery1', 'UnitTestsRecovery2' ]
    },
    action: function () {
      var db = require('@arangodb').db;

      var c1 = db._collection('UnitTestsRecovery1');
      var c2 = db._collection('UnitTestsRecovery2');
      var i;

      for (i = 0; i < 100; ++i) {
        c1.save({ _key: 'foo' + i, value: 'testfoo' + i });
        c2.save({ _key: 'bar' + i, value: 'testbar' + i }, {
          waitForSync: (i === 99)
        });
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
    // / @brief test whether we can restore the trx data
    // //////////////////////////////////////////////////////////////////////////////

    testTransactionDurabilityMultiple: function () {
      var c1 = db._collection('UnitTestsRecovery1');
      var c2 = db._collection('UnitTestsRecovery2');

      assertEqual(100, c1.count());
      assertEqual(100, c2.count());

      var i;
      for (i = 0; i < 100; ++i) {
        assertEqual('testfoo' + i, c1.document('foo' + i).value);
        assertEqual('foo' + i, c1.document('foo' + i)._key);
        assertEqual('testbar' + i, c2.document('bar' + i).value);
        assertEqual('bar' + i, c2.document('bar' + i)._key);
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
