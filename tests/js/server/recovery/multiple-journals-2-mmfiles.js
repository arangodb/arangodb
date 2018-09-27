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
  var c = db._create('UnitTestsRecovery'), i, fig;

  for (i = 0; i < 1000; ++i) {
    c.save({ _key: 'test' + i, value1: 'test' + i, value2: i });
  }

  internal.wal.flush(true, true);
 
  // wait until journal appears 
  while (true) {
    fig = c.figures();
    if (fig.journals.count > 0) {
      break;
    }
    internal.wait(1, false);
  }

  internal.debugSetFailAt('CreateMultipleJournals');

  c.rotate();

  for (i = 1000; i < 2000; ++i) {
    c.save({ _key: 'test' + i, value1: 'test' + i, value2: i });
  }

  internal.wal.flush(true, true);
  
  // wait until next journal appears 
  while (true) {
    fig = c.figures();
    if (fig.journals.count > 0) {
      break;
    }
    internal.wait(1, false);
  }

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

    testMultipleJournals2: function () {
      var i, c = db._collection('UnitTestsRecovery');

      assertEqual(2000, c.count());
      for (i = 0; i < 2000; ++i) {
        assertEqual('test' + i, c.document('test' + i)._key);
        assertEqual('test' + i, c.document('test' + i).value1);
        assertEqual(i, c.document('test' + i).value2);
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
    return jsunity.done().status ? 0 : 1;
  }
}
