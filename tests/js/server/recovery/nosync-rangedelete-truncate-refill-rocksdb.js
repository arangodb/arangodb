/* jshint globalstrict:false, strict:false, unused: false */
/* global assertEqual, assertFalse, assertNull, assertNotNull */
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

  // turn off syncing of counters etc.  
  internal.debugSetFailAt("RocksDBSettingsManagerSync"); 

  db._drop('UnitTestsRecovery1');
  let c = db._create('UnitTestsRecovery1');
  let docs = [];
  for (let i = 0; i < 100000; i++) {
    docs.push({ value: i });
    if (docs.length === 10000) {
      c.insert(docs);
      docs = [];
    }
  }
 
  // should trigger range deletion
  c.truncate();

  for (let i = 0; i < 5000; i++) {
    docs.push({ _key: "test" + i, value: i });
  }
  c.insert(docs, { waitForSync: true });

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

    testNosyncRangeDeleteTruncateRefill: function () {
      let c = db._collection('UnitTestsRecovery1');
      assertEqual(5000, c.count());

      for (let i = 0; i < 5000; ++i) {
        assertEqual(i, c.document("test" + i).value);
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
