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
  let c = db._create('UnitTestsRecovery');
  c.ensureIndex({ type: "hash", fields: ["value1"] });
  c.ensureIndex({ type: "skiplist", fields: ["value2"] });

  let docs = [];
  for (let i = 0; i < 1000; ++i) {
    docs.push({ _key: 'test' + i, value1: i, value2: 'test' + (i % 10) });
  }
  c.insert(docs);

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

    testIndexes: function () {
      var docs;
      var c = db._collection('UnitTestsRecovery'), i;
      var idx = c.getIndexes().sort(function (l, r) {
        if (l.id.length !== r.id.length) {
          return l.id.length - r.id.length < 0 ? -1 : 1;
        }

        // length is equal
        for (var i = 0; i < l.id.length; ++i) {
          if (l.id[i] !== r.id[i]) {
            return l.id[i] < r.id[i] ? -1 : 1;
          }
        }

        return 0;
      });

      assertEqual(3, idx.length);

      assertEqual('primary', idx[0].type);
      assertEqual('hash', idx[1].type);
      assertFalse(idx[1].unique);
      assertFalse(idx[1].sparse);
      assertEqual([ 'value1' ], idx[1].fields);

      assertEqual('skiplist', idx[2].type);
      assertFalse(idx[2].unique);
      assertFalse(idx[2].sparse);
      assertEqual([ 'value2' ], idx[2].fields);

      for (i = 0; i < 1000; ++i) {
        var doc = c.document('test' + i);
        assertEqual(i, doc.value1);
        assertEqual('test' + (i % 10), doc.value2);
      }

      for (i = 0; i < 1000; ++i) {
        docs = c.byExample({ value1: i }).toArray();
        assertEqual(1, docs.length);
        assertEqual('test' + i, docs[0]._key);
      }

      for (i = 0; i < 1000; ++i) {
        docs = c.byExample({ value2: 'test' + (i % 10) }).toArray();
        assertEqual(100, docs.length);
      }
    }

  };
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief executes the test suite
// //////////////////////////////////////////////////////////////////////////////

jsunity.run(recoverySuite);
return jsunity.done();
