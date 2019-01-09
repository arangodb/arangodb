/* jshint globalstrict:false, strict:false, unused: false */
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
  var c, i;

  // write some documents with autoincrement keys
  db._drop('UnitTestsRecovery1');
  c = db._create('UnitTestsRecovery1', { keyOptions: { type: 'autoincrement',
    offset: 0, increment: 10 } } );
  for (i = 0; i < 1000; i++) {
    c.save({ value: i });
  }
  var wals = db._currentWalFiles().map(function(f) {
    // strip off leading `/` or `/archive/` if it exists
    var p = f.split('/');
    return p[p.length - 1];
  });

  // write to other collection until all documents from first collection are
  // out of the wal
  db._drop('UnitTestsRecovery2');
  c = db._create('UnitTestsRecovery2');
  var keepWriting = true;
  while (keepWriting) {
    var padding = 'aaa';
    for (i = 0; i < 10000; i++) {
      padding = padding.concat('aaa');
      c.save({ value: i , text: padding });
    }

    keepWriting = false;
    var walsLeft = db._currentWalFiles().map(function(f) {
      // strip off leading `/` or `/archive/` if it exists
      var p = f.split('/');
      return p[p.length - 1];
    });
    for (var j = 0; j < wals.length; j++) {
      if (walsLeft.indexOf(wals[j]) !== -1) { // still have old wal file
        keepWriting = true;
      }
    }
  }
  c.save({ value: 0 }, { waitForSync: true });

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
    // / @brief test whether we still pick up the right autoincrement value
    // //////////////////////////////////////////////////////////////////////////////

    testCollectionKeyGenRocksDB: function () {
      var c, d;

      c = db._collection('UnitTestsRecovery1');
      assertEqual(c.count(), 1000);
      d = c.save({ value: 1001});
      assertEqual("10010", d._key);
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
