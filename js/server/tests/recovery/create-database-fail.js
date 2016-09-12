/* jshint globalstrict:false, strict:false, unused : false */
/* global assertEqual, assertNotEqual, fail */

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
  internal.debugSetFailAt('CreateDatabase::tempDirectory');
  try {
    db._createDatabase('UnitTestsRecovery1');
    fail();
  } catch (err) {}

  internal.debugClearFailAt();
  internal.debugSetFailAt('CreateDatabase::tempFile');
  try {
    db._createDatabase('UnitTestsRecovery2');
    fail();
  } catch (err) {}

  internal.debugClearFailAt();
  internal.debugSetFailAt('CreateDatabase::renameDirectory');
  try {
    db._createDatabase('UnitTestsRecovery3');
    fail();
  } catch (err) {}

  internal.debugClearFailAt();
  db._createDatabase('UnitTestsRecovery3'); // must work now
  db._createDatabase('UnitTestsRecovery4');

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
    // / @brief test whether failures when creating databases causes issues
    // //////////////////////////////////////////////////////////////////////////////

    testCreateDatabaseFail: function () {
      assertEqual(-1, db._databases().indexOf('UnitTestsRecovery1'));
      assertEqual(-1, db._databases().indexOf('UnitTestsRecovery2'));
      assertNotEqual(-1, db._databases().indexOf('UnitTestsRecovery3'));
      assertNotEqual(-1, db._databases().indexOf('UnitTestsRecovery4'));
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
