/* jshint globalstrict:false, strict:false, unused : false */
/* global assertEqual, assertFalse */

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
  var i;

  db._drop('UnitTestsDummy');
  db._create('UnitTestsDummy');

  db._dropView('UnitTestsRecovery1');
  db._dropView('UnitTestsRecovery2');
  var v = db._createView('UnitTestsRecovery1', 'arangosearch', {});
  v.properties({ "consolidationIntervalMsec": 17 });

  v.rename('UnitTestsRecovery2');

  v = db._createView('UnitTestsRecovery1', 'arangosearch', {});
  v.properties({ "consolidationIntervalMsec": 7 });

  db.UnitTestsDummy.save({ _key: 'foo' }, { waitForSync: true });

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
    // / @brief test whether rename and recreate works
    // //////////////////////////////////////////////////////////////////////////////

    testViewRenameRecreate: function () {
      var v, prop;

      v = db._view('UnitTestsRecovery1');
      assertEqual(v.properties().consolidationIntervalMsec, 7);

      v = db._view('UnitTestsRecovery2');
      assertEqual(v.properties().consolidationIntervalMsec, 17);
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
