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
  var v;

  db._drop('UnitTestsDummy');
  db._create('UnitTestsDummy');

  db._dropView('UnitTestsRecovery1');
  db._dropView('UnitTestsRecovery2');
  v = db._createView('UnitTestsRecovery1', 'arangosearch', {});
  v.properties({ "consolidationIntervalMsec": 17 });
  v.rename('UnitTestsRecovery2');

  db._dropView('UnitTestsRecovery3');
  db._dropView('UnitTestsRecovery4');
  v = db._createView('UnitTestsRecovery3', 'arangosearch', {});
  v.properties({ "consolidationIntervalMsec": 16 });
  v.rename('UnitTestsRecovery4');

  db._dropView('UnitTestsRecovery5');
  db._dropView('UnitTestsRecovery6');
  v = db._createView('UnitTestsRecovery5', 'arangosearch', {});
  v.rename('UnitTestsRecovery6');
  v.rename('UnitTestsRecovery5');

  db._dropView('UnitTestsRecovery7');
  db._dropView('UnitTestsRecovery8');
  v = db._createView('UnitTestsRecovery7', 'arangosearch', {});
  v.rename('UnitTestsRecovery8');
  db._dropView('UnitTestsRecovery8');
  v = db._createView('UnitTestsRecovery8', 'arangosearch', {});

  db._collection('UnitTestsDummy').save({ _key: 'foo' }, { waitForSync: true });

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
    // / @brief test whether rename works
    // //////////////////////////////////////////////////////////////////////////////

    testViewRename: function () {
      var v, prop;

      assertNull(db._view('UnitTestsRecovery1'));
      v = db._view('UnitTestsRecovery2');
      prop = v.properties();
      assertEqual(prop.consolidationIntervalMsec, 17);

      assertNull(db._view('UnitTestsRecovery3'));
      v = db._view('UnitTestsRecovery4');
      prop = v.properties();
      assertEqual(prop.consolidationIntervalMsec, 16);

      assertNull(db._view('UnitTestsRecovery6'));
      assertNotNull(db._view('UnitTestsRecovery5'));

      assertNull(db._view('UnitTestsRecovery7'));
      v = db._view('UnitTestsRecovery8');
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
