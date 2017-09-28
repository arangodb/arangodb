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
  var c;

  db._drop('UnitTestsRecovery1');
  c = db._create('UnitTestsRecovery1', { keyOptions: { type: 'autoincrement',
    offset: 0, increment: 1 } } );
  c.save({ name: 'a' });
  c.save({ name: 'b' });
  c.save({ name: 'c' }, { waitForSync: true });

  db._drop('UnitTestsRecovery2');
  c = db._create('UnitTestsRecovery2', { keyOptions: { type: 'autoincrement',
    offset: 10, increment: 5 } } );
  c.save({ name: 'a' });
  c.save({ name: 'b' });
  c.save({ name: 'c' }, { waitForSync: true });

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

    testCollectionKeyGen: function () {
      var c, d;

      c = db._collection('UnitTestsRecovery1');
      assertEqual(["1", "2", "3"], c.toArray().map(function(doc) { return doc._key; }).sort());
      d = c.save({ name: "d"});
      assertEqual("4", d._key);

      c = db._collection('UnitTestsRecovery2');
      assertEqual(["10", "15", "20"], c.toArray().map(function(doc) { return doc._key; }).sort());
      d = c.save({ name: "d"});
      assertEqual("25", d._key);
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
