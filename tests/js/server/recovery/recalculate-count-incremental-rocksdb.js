/* jshint globalstrict:false, strict:false, unused: false */
/* global assertEqual, assertTrue */
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
var jsunity = require('jsunity');

function runSetup () {
  'use strict';
  
  internal.debugSetFailAt('DisableCommitCounts');
  
  db._drop('UnitTestsRecovery1');
  db._drop('UnitTestsRecovery2');
  let c = db._create('UnitTestsRecovery1');
  let control = db._create('UnitTestsRecovery2');

  // count = 0
  c.recalculateCount();
  control.insert({ _key: "step1", expected: 0, actual: c.count() });

  c.insert({ _key: "test1" });
  // count = 1
  c.recalculateCount();
  control.insert({ _key: "step2", expected: 1, actual: c.count() });
  
  c.insert({ _key: "test2" });
  // count = 2
  c.recalculateCount();
  control.insert({ _key: "step3", expected: 2, actual: c.count() });

  c.insert({ _key: "test3" });
  // count = 3
  c.recalculateCount();
  control.insert({ _key: "step4", expected: 3, actual: c.count() });
  
  c.remove("test2");
  // count = 2
  c.recalculateCount();
  control.insert({ _key: "step5", expected: 2, actual: c.count() });

  c.update("test1", { foo: "bar" });
  // count = 2
  c.recalculateCount();
  control.insert({ _key: "step6", expected: 2, actual: c.count() });

  c.truncate();
  // count = 0;
  c.recalculateCount();
  control.insert({ _key: "step7", expected: 0, actual: c.count() }, { waitForSync: true });

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

    testRecalculateCount: function () {
      let c = db._collection('UnitTestsRecovery1');
      assertEqual(0, c.count());
      assertTrue([], c.toArray());
      
      let control = db._collection('UnitTestsRecovery2');
      assertEqual(7, control.count());

      for (let i = 1; i <= 7; ++i) {
        let doc = control.document("step" + i);
        assertEqual("step" + i, doc._key);
        assertEqual(doc.expected, doc.actual);
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
    return jsunity.writeDone().status ? 0 : 1;
  }
}
