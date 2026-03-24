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
// / @author Copyright 2013, triAGENS GmbH, Cologne, Germany
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

    const docs = [];
    for (let i = 0; i < 100000; ++i) {
      docs.push({ _key: 'test' + i, value1: 'test' + i, value2: i });
    }
    c.insert(docs);

    const toRemove = [];
    for (let i = 0; i < 100000; i += 2) {
      toRemove.push('test' + i);
    }
    c.remove(toRemove);

    trx.commit();
  } catch (e) {
    trx.abort();
    throw e;
  }

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

    testBigTransactionDurability: function () {
      var i, c = db._collection('UnitTestsRecovery');

      assertEqual(50000, c.count());
      for (i = 0; i < 100000; ++i) {
        if (i % 2 === 0) {
          assertFalse(c.exists('test' + i));
        } else {
          assertEqual('test' + i, c.document('test' + i)._key);
          assertEqual('test' + i, c.document('test' + i).value1);
          assertEqual(i, c.document('test' + i).value2);
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
