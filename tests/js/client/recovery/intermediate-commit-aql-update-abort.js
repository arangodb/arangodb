/* jshint globalstrict:false, strict:false, unused : false */
/* global assertEqual, assertNull, fail */

// //////////////////////////////////////////////////////////////////////////////
// / @brief tests for intermediate commits
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

  db._drop('UnitTestsRecovery');
  var c = db._create('UnitTestsRecovery');

  db._query("FOR i IN 1..10000 INSERT { value: i, modified: false } INTO UnitTestsRecovery");

  try {
    db._query("FOR doc IN UnitTestsRecovery FILTER doc.value < 9999 OR FAIL('peng') UPDATE doc WITH { modified: true } INTO UnitTestsRecovery", 
              {}, {intermediateCommitCount: 100000});
    // none of the above should commit
  } catch (err) {
    // intentionally fail
  }
          
  c.insert({ _key: 'crash' }, { waitForSync: true });
  //internal.debugTerminate('crashing server');
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

    testIntermediateCommitsAqlAbort: function () {
      var c = db._collection('UnitTestsRecovery');

      assertEqual(10001, c.count());
      assertEqual(10000, c.byExample({ modified : false }).toArray().length);
      assertNull(c.firstExample({ modified : true }));
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
