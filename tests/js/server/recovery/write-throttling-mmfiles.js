/* jshint globalstrict:false, strict:false, unused : false */
/* global assertTrue, assertFalse, assertEqual */
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
  var c = db._create('UnitTestsRecovery'), i;
  internal.wal.flush(true, true);
  internal.wal.properties({ throttleWait: 1000, throttleWhenPending: 1000 });

  internal.debugSetFailAt('CollectorThreadProcessQueuedOperations');
  for (i = 0; i < 10000; ++i) {
    c.save({ _key: 'test' + i, value1: 'test' + i, value2: i });
  }
  
  internal.wal.flush(true, false);

  // now let the write throttling become active
  internal.wait(7);
  try {
    c.save({ _key: 'foo' });
  } catch (err) {}

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
    // / @brief test whether we can restore the data
    // //////////////////////////////////////////////////////////////////////////////

    testWriteThrottling: function () {
      var i, c = db._collection('UnitTestsRecovery');

      let count = c.count();
      // we either have this many or that many documents... it doesn't matter, as
      // long as _at least_ 10000 documents are there
      assertTrue(count === 10000 || count === 10001);
      for (i = 0; i < 10000; ++i) {
        var doc = c.document('test' + i);

        assertEqual('test' + i, doc.value1);
        assertEqual(i, doc.value2);
      }

      assertFalse(c.exists('foo'));
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
