/* jshint globalstrict:false, strict:false, unused: false */
/* global assertTrue, assertFalse, assertEqual */
// //////////////////////////////////////////////////////////////////////////////
// / @brief tests for transactions
// /
// / @file
// /
// / DISCLAIMER
// /
// / Copyright 2019 ArangoDB GmbH, Cologne, Germany
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
// / Copyright holder is ArangoDB GmbH, Cologne, Germany
// / 
// / @author Simon Grätzer
// //////////////////////////////////////////////////////////////////////////////

var db = require('@arangodb').db;
var internal = require('internal');
var jsunity = require('jsunity');

const bigNumber = 99999999999999;

function runSetup () {
  'use strict';
  internal.debugClearFailAt();
  var c, i;

  // write a documents with a large numeric key
  db._drop('UnitTestsRecovery1');
  c = db._create('UnitTestsRecovery1', { keyOptions: { type: 'traditional'} } );
  c.save({ _key: String(bigNumber) });
  for (let i = 0; i < 10000; i++) { // fill up the WAL
    c.save({some: 'valuexxxxxxxxxxxxx'});
  }

  // write to other collection 
  db._drop('UnitTestsRecovery2');
  c = db._create('UnitTestsRecovery2');
  c.save({ value: 0 });
  c.save({ value: 1 }, { waitForSync: true });

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

    // //////////////////////////////////////////////////////////////////////////////
    // / @brief test whether the server properly handles large numeric keys
    // //////////////////////////////////////////////////////////////////////////////

    testCollectionKeyGenRocksDB: function () {

      let c = db._collection('UnitTestsRecovery1');
      assertEqual(c.count(), 10001);

      let d = c.save({ value: 'a'});
      assertTrue(parseInt(d._key) > bigNumber + 1);

      // check that the second collection is unaffected

      c = db._collection('UnitTestsRecovery2');
      assertEqual(c.count(), 2);

      d = c.save({ value: 'a'});
      print(d);
      assertTrue(parseInt(d._key) < bigNumber);
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
