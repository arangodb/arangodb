/* jshint globalstrict:false, strict:false, maxlen: 200 */
/* global fail, assertEqual, assertNotEqual */

// //////////////////////////////////////////////////////////////////////////////
// / @brief ArangoTransaction sTests
// /
// /
// / DISCLAIMER
// /
// / Copyright 2018 ArangoDB GmbH, Cologne, Germany
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
// //////////////////////////////////////////////////////////////////////////////

let jsunity = require('jsunity');
let internal = require('internal');
let arangodb = require('@arangodb');
let db = arangodb.db;

function transactionExclusiveSuite () {
  'use strict';
  var cn = 'UnitTestsTransaction';
  var c = null;

  return {

    setUp: function () {
      db._drop(cn);
      c = db._create(cn, {numberOfShards: 4});
    },

    tearDown: function () {

      if (c !== null) {
        c.drop();
      }

      c = null;
      internal.wait(0);
    },

    testInsertConcurrently: function () {
      c.insert({ _key: 'test', value: 1 });

      let trx = db._createTransaction({ 
        collections: { exclusive: c.name()  },
        lockTimeout: 10000.0 // arbitrary high timeout
      });
      try {
        let tc = trx.collection(c.name());
        for (let i = 0; i < 1000) {
          tc.insert({ _key: 'test', value: 2 }); // should fail
        }
        trx.commit(); // should not get here
      } finally {
        trx.abort(); 
      }
      
      assertEqual(1, c.count());
      assertEqual(1, c.toArray().length);
      assertEqual(1, c.document('test').value);
    }

  };
}

jsunity.run(transactionExclusiveSuite);

return jsunity.done();
