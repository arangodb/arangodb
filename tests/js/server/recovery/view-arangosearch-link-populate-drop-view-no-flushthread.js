/* jshint globalstrict:false, strict:false, unused : false */
/* global assertEqual, assertTrue, assertFalse, assertNull, fail, AQL_EXECUTE */
// //////////////////////////////////////////////////////////////////////////////
// / @brief recovery tests for views
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
var fs = require('fs');
var internal = require('internal');
var jsunity = require('jsunity');

function runSetup () {
  'use strict';
  internal.debugClearFailAt();

  db._drop('UnitTestsRecoveryDummy');
  var c = db._create('UnitTestsRecoveryDummy');

  db._dropView('UnitTestsRecoveryView');
  var v = db._createView('UnitTestsRecoveryView', 'arangosearch', {});

  var meta = { links: { 'UnitTestsRecoveryDummy': { includeAllFields: true } } };
  v.properties(meta);


  for (let i = 0; i < 10000; i++) {
    c.save({ a: "foo_" + i,
b: "bar_" + i,
c: i });
  }

  internal.wal.flush(true, true);
  internal.debugSetFailAt("RocksDBBackgroundThread::run");
  internal.wait(2); // make sure failure point takes effect

  v.drop();

  c.save({ name: 'crashme' }, { waitForSync: true });

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
    // / @brief test whether we can restore the trx data
    // //////////////////////////////////////////////////////////////////////////////

    testIResearchLinkPopulateDropViewNoFlushThread: function () {
      var v = db._view('UnitTestsRecoveryView');
      assertNull(v);
      let path = fs.join(fs.join(db._path(), 'databases'), 'database-' + db._id());
      assertEqual(0, fs.list(path).length);
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
