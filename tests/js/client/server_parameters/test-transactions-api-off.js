/*jshint globalstrict:false, strict:false */
/* global getOptions, assertEqual, assertTrue, arango, fail */

////////////////////////////////////////////////////////////////////////////////
/// @brief test for security-related server options
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2010-2012 triagens GmbH, Cologne, Germany
///
/// Licensed under the Apache License, Version 2.0 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     http://www.apache.org/licenses/LICENSE-2.0
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB Inc, Cologne, Germany
///
/// @author Jan Steemann
/// @author Copyright 2019, ArangoDB Inc, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

if (getOptions === true) {
  return {
    'javascript.transactions-api': "false"
  };
}
let jsunity = require('jsunity');
let internal = require('internal');
let db = require('internal').db;
const errors = require('@arangodb').errors;

const cn = "UnitTestsCollection";

function testSuite() {
  return {
    testRunJavaScriptTransaction : function() {
      try {
        db._executeTransaction({
          collections: {},
          action: function() { return 1; }
        });
        fail();
      } catch (err) {
        assertEqual(errors.ERROR_NOT_IMPLEMENTED.code, err.errorNum);
      }
    },
    
    testRunSimpleStreamingTransaction : function() {
      // streaming transactions must still work
      const opts = { collections: {} };
      const trx = internal.db._createTransaction(opts);
      let id = trx.id();
      let result = trx.commit();
      assertEqual(id, result.id);
      assertEqual("committed", result.status);
    },
    
    testRunCollectionStreamingTransaction : function() {
      // streaming transactions must still work
      db._drop(cn);
      db._create(cn);
      try {
        const opts = { collections: { write: cn } };
        const trx = internal.db._createTransaction(opts);
        try {
          let id = trx.id();
          const c = trx.collection(cn);
          assertEqual(0, c.count());
            
          let result = c.insert({ _key: "test" });
          assertTrue(result.hasOwnProperty("_id"));
          assertTrue(result.hasOwnProperty("_key"));
          assertTrue(result.hasOwnProperty("_rev"));
          assertEqual("test", result._key);

          assertEqual(1, c.count());

          result = trx.commit();
          assertEqual(id, result.id);
          assertEqual("committed", result.status);

          assertEqual(1, db[cn].count());
        } catch (err) {
          trx.abort();
          throw err;
        }
      } finally {
        db._drop(cn);
      }
    },
    
  };
}

jsunity.run(testSuite);
return jsunity.done();
