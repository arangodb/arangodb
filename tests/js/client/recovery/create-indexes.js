/* jshint globalstrict:false, strict:false, unused : false */
/* global runSetup assertEqual, assertNotNull */
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
  let c;
  for (let i = 0; i < 5; ++i) {
    db._drop('UnitTestsRecovery' + i);
    c = db._create('UnitTestsRecovery' + i);

    let docs = [];
    for (let j = 0; j < 100; ++j) {
      docs.push({ _key: 'test' + j, value1: 'foo' + j, value2: 'bar' + j });
    }
    c.insert(docs);

    c.ensureIndex({ type: "hash", fields: ["value1"], unique: true });
    c.ensureIndex({ type: "skiplist", fields: ["value2"], unique: true });
  }

  db._drop('test');
  c = db._create('test');
  c.save({ _key: 'crashme' }, true);

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

    testCreateIndexes: function () {
      var i, j, c, idx, doc;
      var hash = null, skip = null;
      for (i = 0; i < 5; ++i) {
        c = db._collection('UnitTestsRecovery' + i);
        assertEqual(100, c.count());
        idx = c.getIndexes();
        assertEqual(3, idx.length);

        for (j = 1; j < 3; ++j) {
          if (idx[j].type === 'hash') {
            hash = idx[j];
          } else if (idx[j].type === 'skiplist') {
            skip = idx[j];
          }
        }

        assertNotNull(hash);
        assertNotNull(skip);

        for (j = 0; j < 100; ++j) {
          doc = c.document('test' + j);
          assertEqual('foo' + j, doc.value1);
          assertEqual('bar' + j, doc.value2);

          assertEqual(1, c.byExample({ value1: 'foo' + j }).toArray().length);
          assertEqual(1, c.byExample({ value2: 'bar' + j }).toArray().length);
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
