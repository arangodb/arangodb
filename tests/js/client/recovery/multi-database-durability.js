/* jshint globalstrict:false, strict:false, unused : false */
/* global runSetup assertFalse, assertEqual */
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
  global.instanceManager.debugClearFailAt();

  var databases = [ 'UnitTestsRecovery1', 'UnitTestsRecovery2', 'UnitTestsRecovery3' ];

  databases.forEach(function (d) {
    db._useDatabase('_system');
    try {
      db._dropDatabase(d);
    } catch (err) {}
    db._createDatabase(d);
    db._useDatabase(d);

    db._create('UnitTestsRecovery');

    db._executeTransaction({
      collections: {
        write: 'UnitTestsRecovery'
      },
      action: function () {
        var db = require('@arangodb').db;

        var i, c = db._collection('UnitTestsRecovery');
        for (i = 0; i < 10000; ++i) {
          c.save({ _key: 'test' + i, value1: 'test' + i, value2: i });
        }
        for (i = 0; i < 10000; i += 2) {
          c.remove('test' + i);
        }
        c.update('test1', { value2: 1 }, { waitForSync: true }); // enfore sync
      }
    });
  });

  db._useDatabase('_system');

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
    // / @brief test whether we can restore the trx data
    // //////////////////////////////////////////////////////////////////////////////

    testMultiDatabaseDurability: function () {
      var databases = [ 'UnitTestsRecovery1', 'UnitTestsRecovery2', 'UnitTestsRecovery3' ];

      databases.forEach(function (d) {
        db._useDatabase(d);

        var i, c = db._collection('UnitTestsRecovery');

        assertEqual(5000, c.count());
        for (i = 0; i < 10000; ++i) {
          if (i % 2 === 0) {
            assertFalse(c.exists('test' + i));
          } else {
            assertEqual('test' + i, c.document('test' + i)._key);
            assertEqual('test' + i, c.document('test' + i).value1);
            assertEqual(i, c.document('test' + i).value2);
          }
        }
      });
    }

  };
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief executes the test suite
// //////////////////////////////////////////////////////////////////////////////

jsunity.run(recoverySuite);
return jsunity.done();
