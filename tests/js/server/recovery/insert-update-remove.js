/* jshint globalstrict:false, strict:false, unused : false */
/* global assertEqual, assertFalse */

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

  db._executeTransaction({
    collections: {
      write: 'UnitTestsRecovery'
    },
    action: function () {
      var db = require('@arangodb').db;

      var i, c = db._collection('UnitTestsRecovery');
      for (i = 0; i < 10000; ++i) {
        c.save({ _key: 'test' + i, value1: 'test' + i, value2: i });
        c.update('test' + i, { value3: 'additional value', value4: i });

        if (i % 10 === 0) {
          c.remove('test' + i, true);
        } else if (i % 5 === 0) {
          c.update('test' + i, { value6: 'something else' });
        }
      }
    }
  });

  internal.wal.flush(true, true);

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

    testInsertUpdateRemove: function () {
      var i, c = db._collection('UnitTestsRecovery');

      assertEqual(9000, c.count());
      for (i = 0; i < 10000; ++i) {
        if (i % 10 === 0) {
          assertFalse(c.exists('test' + i));
        } else {
          var doc = c.document('test' + i);

          assertEqual('test' + i, doc.value1);
          assertEqual(i, doc.value2);
          assertEqual('additional value', doc.value3);
          assertEqual(i, doc.value4);

          if (i % 5 === 0) {
            assertEqual('something else', doc.value6);
          }
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
