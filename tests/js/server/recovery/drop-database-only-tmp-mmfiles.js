/* jshint globalstrict:false, strict:false, unused: false */
/* global fail, assertEqual */
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
var fs = require('fs');
var jsunity = require('jsunity');

function runSetup () {
  'use strict';
  internal.debugClearFailAt();

  var path = fs.join(fs.join(db._path(), ".."), "database-999999999");
  fs.makeDirectory(path);

  var data = { deleted: true, name: "UnitTestsRecovery", id: "999999999" };
  fs.writeFileSync(fs.join(path, "parameter.json"), JSON.stringify(data));
  fs.writeFileSync(fs.join(path, ".tmp"), "");

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
    // / @brief test whether we the data are correct after restart
    // //////////////////////////////////////////////////////////////////////////////

    testDropDatabaseOnlyTmp: function () {
      try {
        db._useDatabase('UnitTestsRecovery');
        fail();
      } catch (err) {
        assertEqual(internal.errors.ERROR_ARANGO_DATABASE_NOT_FOUND.code, err.errorNum);
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
    return jsunity.done().status ? 0 : 1;
  }
}
