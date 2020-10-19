/* jshint globalstrict:false, strict:false, unused: false */
/* global assertTrue, assertFalse, assertEqual, assertMatch */
// //////////////////////////////////////////////////////////////////////////////
// / @brief tests for count recovery
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

const db = require('@arangodb').db;
const internal = require('internal');
const jsunity = require('jsunity');
const fs = require("fs");
const continueFile = fs.join(fs.getTempPath(), "CONTINUE");

function runSetup () {
  'use strict';
  internal.debugClearFailAt();

  if (!fs.exists(continueFile)) {
    // phase one of setUp
    require("console").info("counts test, phase 1");

    db._drop('UnitTestsRecovery');
    let c = db._create('UnitTestsRecovery');

    internal.debugSetFailAt('RocksDBSettingsManagerSync');

    c.insert({ _key: "test" }, { waitForSync: true });
    
    // we want to run setUp just once more
    fs.write(continueFile, "1");
  } else {
    // phase two of setUp
    require("console").info("counts test, phase 2");

    fs.remove(continueFile);
      
    internal.waitForEstimatorSync();
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
    testCountsAfterRestart: function () {
      let c = db._collection('UnitTestsRecovery');
      assertEqual(1, c.count());
      assertEqual(1, c.toArray().length);
    }

  };
}

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
