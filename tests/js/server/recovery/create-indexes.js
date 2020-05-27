/* jshint globalstrict:false, strict:false, unused : false */
/* global assertEqual, assertNotNull */
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
  var i, j, c;
  for (i = 0; i < 5; ++i) {
    db._drop('UnitTestsRecovery' + i);
    c = db._create('UnitTestsRecovery' + i);

    for (j = 0; j < 100; ++j) {
      c.save({ _key: 'test' + j, value1: 'foo' + j, value2: 'bar' + j });
    }

    c.ensureUniqueConstraint('value1');
    c.ensureUniqueSkiplist('value2');
  }

  db._drop('test');
  c = db._create('test');
  c.save({ _key: 'crashme' }, true);

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

    testCreateIndexes: function () {
      var i, j, c, idx, doc;
      var hash = null, skip = null;
      for (i = 0; i < 5; ++i) {
        c = db._collection('UnitTestsRecovery' + i);
        assertEqual(100, c.count());
        idx = c.getIndexes();
        assertEqual(3, idx.length);

        for (j = 1; j < 3; ++j) {
          if (idx[j].type === 'hash') {
            hash = idx[j];
          } else if (idx[j].type === 'skiplist') {
            skip = idx[j];
          }
        }

        assertNotNull(hash);
        assertNotNull(skip);

        for (j = 0; j < 100; ++j) {
          doc = c.document('test' + j);
          assertEqual('foo' + j, doc.value1);
          assertEqual('bar' + j, doc.value2);

          assertEqual(1, c.byExample({ value1: 'foo' + j }).toArray().length);
          assertEqual(1, c.byExample({ value2: 'bar' + j }).toArray().length);
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
