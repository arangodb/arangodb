/* jshint globalstrict:false, strict:false, unused: false */
/* global runSetup assertEqual, assertTrue */
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
  
  global.instanceManager.debugSetFailAt('DisableCommitCounts');
  
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

  return 0;
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief test suite
// //////////////////////////////////////////////////////////////////////////////

function recoverySuite () {
  'use strict';
  jsunity.jsUnity.attachAssertions();

  return {


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

jsunity.run(recoverySuite);
return jsunity.done();
