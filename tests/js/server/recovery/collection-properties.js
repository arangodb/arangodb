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
  c = db._create('UnitTestsRecovery1');
  c.properties({ waitForSync: true, journalSize: 8 * 1024 * 1024, doCompact: false });

  db._drop('UnitTestsRecovery2');
  c = db._create('UnitTestsRecovery2');
  c.properties({ waitForSync: false, journalSize: 16 * 1024 * 1024, doCompact: true });

  db._drop('UnitTestsRecovery3');
  c = db._create('UnitTestsRecovery3');
  c.properties({ waitForSync: false, journalSize: 16 * 1024 * 1024, doCompact: true });
  c.properties({ waitForSync: true, journalSize: 4 * 1024 * 1024, doCompact: false });

  db._drop('UnitTestsRecovery4');
  c = db._create('UnitTestsRecovery4');
  c.properties({ waitForSync: false, indexBuckets: 16 });
  c.properties({ waitForSync: true, indexBuckets: 32 });

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

    testCollectionProperties: function () {
      var c, prop;

      c = db._collection('UnitTestsRecovery1');
      prop = c.properties();
      assertTrue(prop.waitForSync);

      c = db._collection('UnitTestsRecovery2');
      prop = c.properties();
      assertFalse(prop.waitForSync);

      c = db._collection('UnitTestsRecovery3');
      prop = c.properties();
      assertTrue(prop.waitForSync);
      
      c = db._collection('UnitTestsRecovery4');
      prop = c.properties();
      assertTrue(prop.waitForSync);
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
