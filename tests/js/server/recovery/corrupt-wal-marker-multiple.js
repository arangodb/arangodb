/* jshint globalstrict:false, strict:false, unused : false */
/* global assertEqual, */
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

  var c = [], i, j;
  for (i = 0; i < 10; ++i) {
    db._drop('UnitTestsRecovery' + i);
    c[i] = db._create('UnitTestsRecovery' + i);

    for (j = 0; j < 49; ++j) {
      c[i].save({ _key: 'test' + j, a: j, b: 'test' + j });
    }

    c[i].save({ _key: 'test49', a: 49, b: 'test49' }, true); // sync 
  }

  internal.debugSetFailAt('WalSlotCrc');

  // now corrupt all the collections
  for (i = 0; i < 10; ++i) {
    c[i].save({ _key: 'test50', a: 50, b: 'test50' });
  }

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

    testCorruptwalMarkerMultiple: function () {
      var i, j, c;
      for (i = 0; i < 10; ++i) {
        c = db._collection('UnitTestsRecovery' + i);

        // rocksdb does not have this failure point
        assertEqual(51, c.count());
        for (j = 0; j < 50; ++j) {
          assertEqual(j, c.document('test' + j).a);
          assertEqual('test' + j, c.document('test' + j).b);
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
