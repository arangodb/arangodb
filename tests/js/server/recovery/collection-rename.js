/* jshint globalstrict:false, strict:false, unused : false */
/* global assertEqual, assertNull, assertTrue, assertFalse, assertNotNull */

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
  var c;

  db._drop('UnitTestsRecovery1');
  db._drop('UnitTestsRecovery2');
  c = db._create('UnitTestsRecovery1');
  c.properties({ waitForSync: true });
  c.rename('UnitTestsRecovery2');

  db._drop('UnitTestsRecovery3');
  db._drop('UnitTestsRecovery4');
  c = db._create('UnitTestsRecovery3');
  c.properties({ waitForSync: false });
  c.rename('UnitTestsRecovery4');

  db._drop('UnitTestsRecovery5');
  db._drop('UnitTestsRecovery6');
  c = db._create('UnitTestsRecovery5');
  c.rename('UnitTestsRecovery6');
  c.rename('UnitTestsRecovery5');

  db._drop('UnitTestsRecovery7');
  db._drop('UnitTestsRecovery8');
  c = db._create('UnitTestsRecovery7');
  c.rename('UnitTestsRecovery8');
  db._drop('UnitTestsRecovery8');
  c = db._create('UnitTestsRecovery8');

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
    // / @brief test whether rename works
    // //////////////////////////////////////////////////////////////////////////////

    testCollectionRename: function () {
      var c, prop;

      assertNull(db._collection('UnitTestsRecovery1'));
      c = db._collection('UnitTestsRecovery2');
      prop = c.properties();
      assertTrue(prop.waitForSync);

      assertNull(db._collection('UnitTestsRecovery3'));
      c = db._collection('UnitTestsRecovery4');
      prop = c.properties();
      assertFalse(prop.waitForSync);

      assertNull(db._collection('UnitTestsRecovery6'));
      assertNotNull(db._collection('UnitTestsRecovery5'));

      assertNull(db._collection('UnitTestsRecovery7'));
      c = db._collection('UnitTestsRecovery8');
      assertEqual(1, c.count());
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
