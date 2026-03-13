/* jshint globalstrict:false, strict:false, unused : false */
/* global runSetup assertEqual, assertFalse */
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
// / @author Copyright 2012, triAGENS GmbH, Cologne, Germany
// //////////////////////////////////////////////////////////////////////////////

var db = require('@arangodb').db;
var internal = require('internal');
var jsunity = require('jsunity');

if (runSetup === true) {
  'use strict';
  global.instanceManager.debugClearFailAt();

  db._drop('UnitTestsRecovery');
  db._create('UnitTestsRecovery');

  const trx = db._createTransaction({ collections: { write: 'UnitTestsRecovery' } });
  try {
    const c = trx.collection('UnitTestsRecovery');

    // Build final state directly: insert + update + conditional remove/update collapsed
    const docs = [];
    for (let i = 0; i < 10000; ++i) {
      if (i % 10 === 0) {
        continue; // these get removed
      }
      const doc = { _key: 'test' + i, value1: 'test' + i, value2: i,
                    value3: 'additional value', value4: i };
      if (i % 5 === 0) {
        doc.value6 = 'something else';
      }
      docs.push(doc);
    }
    c.insert(docs);

    trx.commit();
  } catch (e) {
    trx.abort();
    throw e;
  }

  internal.wal.flush(true, true);

  return 0;
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

    testInsertUpdateRemove: function () {
      var i, c = db._collection('UnitTestsRecovery');

      assertEqual(9000, c.count());
      for (i = 0; i < 10000; ++i) {
        if (i % 10 === 0) {
          assertFalse(c.exists('test' + i));
        } else {
          var doc = c.document('test' + i);

          assertEqual('test' + i, doc.value1);
          assertEqual(i, doc.value2);
          assertEqual('additional value', doc.value3);
          assertEqual(i, doc.value4);

          if (i % 5 === 0) {
            assertEqual('something else', doc.value6);
          }
        }
      }
    }

  };
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief executes the test suite
// //////////////////////////////////////////////////////////////////////////////

jsunity.run(recoverySuite);
return jsunity.done();
