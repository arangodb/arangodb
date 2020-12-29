/* jshint globalstrict:false, strict:false, unused : false */
/* global assertEqual, assertNull, assertTrue, assertFalse, assertNotNull, AQL_EXECUTE */

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
  v.properties({ links: { 'UnitTestsDummy': { includeAllFields: true } } });
  db._collection('UnitTestsDummy').save({ _key: 'foo', num: 1 }, { waitForSync: true });

  internal.wal.flush(true, true);
  internal.debugSetFailAt("FlushThreadDisableAll");
  internal.wait(2); // make sure failure point takes effect

  v.rename('UnitTestsRecovery2');

  db._collection('UnitTestsDummy').save({ _key: 'bar', num: 2 }, { waitForSync: true });

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

    testIResearchViewRenameNoFlushThread: function () {
      var v, res;

      assertNull(db._view('UnitTestsRecovery1'));
      assertNotNull(db._view('UnitTestsRecovery2'));
      res = db._query('FOR doc IN `UnitTestsRecovery2` SEARCH doc.num > 0 OPTIONS {waitForSync: true}  RETURN doc').toArray();
      assertEqual(res.length, 2);
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
