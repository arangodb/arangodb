/* jshint globalstrict:false, strict:false, unused : false */
/* global runSetup, assertEqual, assertFalse, assertTrue */

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
// //////////////////////////////////////////////////////////////////////////////

var db = require('@arangodb').db;
var internal = require('internal');
var jsunity = require('jsunity');
let IM = global.instanceManager;

function runSetupRoutine () {
  'use strict';
  db._drop('UnitTestsRecovery1');
  let c = db._create('UnitTestsRecovery1');

  let docs = [];
  for (let i = 0; i < 1000; ++i) {
    docs.push({ value: i });
  }
  c.insert(docs);

  IM.debugSetFailAt("RocksDBBuilderIndex::fillIndex");
  try {
    c.ensureIndex({ type: "skiplist", fields: ["value"] });
    fail();
  } catch (ex) {
    if (ex.errorNum !== internal.errors.ERROR_SIMPLE_CLIENT_COULD_NOT_CONNECT.code) {
      print(ex);
      throw ex;
    }
  }
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

    testBrokenIndex: function () {
      const c = db._collection('UnitTestsRecovery1');
      const indexes = c.indexes();
      assertEqual(indexes.length, 1);
      assertEqual(indexes[0].type, 'primary');
      assertEqual(indexes[0].id, 'UnitTestsRecovery1/0');
    }

  };
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief executes the test suite
// //////////////////////////////////////////////////////////////////////////////

'use strict';
if (runSetup === true ) {
  runSetupRoutine();
  return 0;
} else {
  jsunity.run(recoverySuite);
  return jsunity.done();
}
