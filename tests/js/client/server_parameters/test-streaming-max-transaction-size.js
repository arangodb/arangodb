/*jshint globalstrict:false, strict:false */
/* global getOptions, assertEqual, assertTrue, fail, arango */

////////////////////////////////////////////////////////////////////////////////
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
    'transaction.streaming-max-transaction-size': "16MB",
  };
}
const jsunity = require('jsunity');
const errors = require('@arangodb').errors;
const cn = "UnitTestsCollection";
const db = require('internal').db;
const ArangoError = require('@arangodb').ArangoError;

function testSuite() {
  return {
    setUp: function () {
      db._create(cn);
    },

    tearDown: function () {
      db._drop(cn);
    },

    testTransactionBelowLimit: function() {
      let trx = db._createTransaction({ collections: { write: [cn] } });
      try {
        let c = trx.collection(cn);
        for (let i = 0; i < 1000; ++i) {
          c.insert({ value1: i, value2: "testmann " + i });
        }
        assertEqual(1000, c.count());
        trx.commit();
        trx = null;
        assertEqual(1000, db[cn].count());
      } finally {
        if (trx) {
          trx.abort();
        }
      }
    },
    
    testAQLTransactionBelowLimit: function() {
      let trx = db._createTransaction({ collections: { write: [cn] } });
      try {
        let res = trx.query("FOR i IN 1..1000 INSERT { value1: i, value2: CONCAT('testmann', i) } INTO " + cn).getExtra();
        assertTrue(res.stats.peakMemoryUsage <= 16 * 1024 * 1024, res);
        let c = trx.collection(cn);
        assertEqual(1000, c.count());
        assertEqual(1000, c.count());
        trx.commit();
        trx = null;
        assertEqual(1000, db[cn].count());
      } finally {
        if (trx) {
          trx.abort();
        }
      }
    },
    
    testTransactionAboveLimit: function() {
      const payload = { value1: Array(256).join("rip"), value2: Array(256).join("crap") };
      let trx = db._createTransaction({ collections: { write: [cn] } });
      try {
        let c = trx.collection(cn);
        try {
          let docs = [];
          for (let i = 0; i < 100; ++i) {
            docs.push({ payload });
          }
          while (true) {
            let res = c.insert(docs);
            res.forEach((r) => {
              if (r.error) {
                throw new ArangoError(r);
              }
            });
          }
          fail();
        } catch (err) {
          assertEqual(errors.ERROR_RESOURCE_LIMIT.code, err.errorNum);
        }
      } finally {
        trx.abort();
      }
    },
    
    testAQLTransactionAboveLimit: function() {
      let trx = db._createTransaction({ collections: { write: [cn] } });
      try {
        try {
          trx.query("FOR i IN 1..1000000 INSERT { value1: i, value2: CONCAT('testmann', i) } INTO " + cn);
          fail();
        } catch (err) {
          assertEqual(errors.ERROR_RESOURCE_LIMIT.code, err.errorNum);
        }
      } finally {
        trx.abort();
      }
    },
    
  };
}

jsunity.run(testSuite);
return jsunity.done();
