/* jshint globalstrict:false, strict:false, unused : false */
/* global fail, assertTrue, assertFalse, assertEqual, TRANSACTION */
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
var FoxxService = require('@arangodb/foxx/service');
var jsunity = require('jsunity');
var fs = require('fs');

function runSetup () {
  'use strict';
  internal.debugClearFailAt();

  let appPath = fs.join(FoxxService._appPath, '..');

  try {
    db._dropDatabase('UnitTestsRecovery1');
  } catch (err) {}

  db._createDatabase('UnitTestsRecovery1');

  fs.write(fs.join(appPath, 'UnitTestsRecovery1', 'foo.json'), 'test');

  try {
    db._dropDatabase('UnitTestsRecovery2');
  } catch (err) {}

  db._createDatabase('UnitTestsRecovery2');

  fs.write(fs.join(appPath, 'UnitTestsRecovery2', 'bar.json'), 'test');

  db._dropDatabase('UnitTestsRecovery2');
  // garbage-collect once
  require("internal").wait(0.5, true);

  // we need to wait long enough for the DatabaseManagerThread to 
  // physically carry out the deletion
  let path = fs.join(appPath, 'UnitTestsRecovery2');
  let tries = 0;
  while (++tries < 30) {
    if (!fs.isDirectory(path)) {
      require("console").log("database directory for UnitTestsRecovery2 is gone");
      break;
    }
    internal.wait(1, false);
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
    // / @brief test whether the data are correct after restart
    // //////////////////////////////////////////////////////////////////////////////

    testFoxxDirectories: function () {
      var appPath = fs.join(FoxxService._appPath, '..');

      assertTrue(fs.isDirectory(fs.join(appPath, 'UnitTestsRecovery1')));
      assertTrue(fs.isFile(fs.join(appPath, 'UnitTestsRecovery1', 'foo.json')));

      assertFalse(fs.isDirectory(fs.join(appPath, 'UnitTestsRecovery2')));
      assertFalse(fs.isFile(fs.join(appPath, 'UnitTestsRecovery2', 'bar.json')));
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
