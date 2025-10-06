/* jshint globalstrict:false, strict:false, unused : false */
/* global runSetup fail, assertTrue, assertFalse, assertEqual, TRANSACTION, arango */
// //////////////////////////////////////////////////////////////////////////////
// / DISCLAIMER
// /
// / Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
// / Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
// /
// / Licensed under the Business Source License 1.1 (the "License");
// / you may not use this file except in compliance with the License.
// / You may obtain a copy of the License at
// /
// /     https://github.com/arangodb/arangodb/blob/devel/LICENSE
// /
// / Unless required by applicable law or agreed to in writing, software
// / distributed under the License is distributed on an "AS IS" BASIS,
// / WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// / See the License for the specific language governing permissions and
// / limitations under the License.
// /
// / Copyright holder is ArangoDB GmbH, Cologne, Germany
// /
// / @author Jan Steemann
// / @author Copyright 2013, triAGENS GmbH, Cologne, Germany
// //////////////////////////////////////////////////////////////////////////////

var db = require('@arangodb').db;
var internal = require('internal');
var FoxxServiceAppPath = arango.POST("/_admin/execute", `return require('@arangodb/foxx/service')._appPath`);
var jsunity = require('jsunity');
var fs = require('fs');

function fileExists(path) {
  try {
    fs.readFileSync(path);
    return true;
  } catch (err) {
    return false;
  }
}

if (runSetup === true) {
  'use strict';
  global.instanceManager.debugClearFailAt();

  let appPath = fs.join(FoxxServiceAppPath, '..');

  try {
    db._dropDatabase('UnitTestsRecovery1');
  } catch (err) {}

  db._createDatabase('UnitTestsRecovery1');

  fs.write(fs.join(appPath, 'UnitTestsRecovery1', 'foo.json'), 'test');

  try {
    db._dropDatabase('UnitTestsRecovery2');
  } catch (err) {}

  db._createDatabase('UnitTestsRecovery2');

  const path = fs.join(appPath, 'UnitTestsRecovery2', 'bar.json');
  fs.write(path, 'test');

  // tries to force the operating system to refresh its inode cache
  db._dropDatabase('UnitTestsRecovery2');
  // garbage-collect once
  require("internal").wait(0.5, true);

  // we need to wait long enough for the DatabaseManagerThread to 
  // physically carry out the deletion
  let gone = false;
  let tries = 0;
  while (++tries < 120) {
    if (!fileExists(path)) {
      gone = true;
      require("console").log("database directory for UnitTestsRecovery2 is gone");
      break;
    }
    internal.wait(1, false);
  }
  if (!gone) {
    throw new Error(`"${path}" did not disappear in 120s!`);
  }
  return 0;
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief test suite
// //////////////////////////////////////////////////////////////////////////////

function recoverySuite () {
  'use strict';
  jsunity.jsUnity.attachAssertions();

  return {


    // //////////////////////////////////////////////////////////////////////////////
    // / @brief test whether the data are correct after restart
    // //////////////////////////////////////////////////////////////////////////////

    testFoxxDirectories: function () {
      var appPath = fs.join(FoxxServiceAppPath, '..');

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

jsunity.run(recoverySuite);
return jsunity.done();
