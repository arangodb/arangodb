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

  // disable collector
  internal.debugSetFailAt('CollectorThreadProcessQueuedOperations');

  var c = db._create('UnitTestsRecovery'), i, j;

  for (i = 0; i < 10; ++i) {
    for (j = 0; j < 1000; ++j) {
      c.save({ _key: 'test-' + i + '-' + j, value1: 'test' + i, value2: 'test' + j });
    }

    // make sure the next operations go into a separate log
    internal.wal.flush(true, false);
  }

  c.save({ _key: 'foo' }, true);

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

    testManyLogs: function () {
      var c = db._collection('UnitTestsRecovery'), i, j;
      assertEqual(10001, c.count());

      for (i = 0; i < 10; ++i) {
        for (j = 0; j < 1000; ++j) {
          var doc = c.document('test-' + i + '-' + j);
          assertEqual('test' + i, doc.value1);
          assertEqual('test' + j, doc.value2);
        }
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
