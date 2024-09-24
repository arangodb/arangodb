/* jshint globalstrict:false, strict:false, unused : false */
/* global runSetup fail, assertEqual */
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
var jsunity = require('jsunity');

if (runSetup === true) {
  'use strict';
  var i;
  global.instanceManager.debugClearFailAt();

  internal.wal.properties({ historicLogfiles: 0 });

  db._drop('test');
  db._create('test');

  db._createDatabase('UnitTestsRecovery');
  internal.wal.flush(true, true, true);
  internal.wait(3, true);

  db._useDatabase('UnitTestsRecovery');

  db._create('test');
  for (i = 0; i < 100; ++i) {
    db.test.save({ _key: 'test' + i, value: i });
  }

  db._useDatabase('_system');
  db._dropDatabase('UnitTestsRecovery');

  internal.wait(5, true);

  db.test.save({ _key: 'crashme' }, true);

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
    // / @brief test whether we the data are correct after restart
    // //////////////////////////////////////////////////////////////////////////////

    testFlushDropDatabaseAndFail: function () {
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

jsunity.run(recoverySuite);
return jsunity.done();
