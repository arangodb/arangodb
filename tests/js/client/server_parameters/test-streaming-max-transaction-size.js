/*jshint globalstrict:false, strict:false */
/* global getOptions, assertEqual, assertTrue, fail, arango, aql */

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
/// @author Jan Steemann
/// @author Copyright 2019, ArangoDB Inc, Cologne, Germany
// //////////////////////////////////////////////////////////////////////////////

if (getOptions === true) {
  return {
    'transaction.streaming-max-transaction-size': "16MB",
    'opts': {
      coordinators: 2
    }
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

    testTransactionAboveLimitCommit: function(testName) {
      // 1KB limit is quite low
      // We're testing that the transaction is not aborted when the limit is reached.
      // Rather, the commit should succeed.
      let trx = db._createTransaction({ collections: { write: [cn] }, maxTransactionSize: 1024 });
      let c = trx.collection(cn);
      let docCount = 0;
      const maxDocs = 10;
      while (docCount < maxDocs) {
        try {
          // The insertion takes place inside a loop, because we are particularly interested in
          // the iteration where the limit is reached. We are expecting a failure after a certain document count.
          c.insert({_key: `${testName}-${docCount}`, value: docCount});
        } catch (err) {
          assertEqual(errors.ERROR_RESOURCE_LIMIT.code, err.errorNum);
          break;
        }
        ++docCount;
      }
      assertTrue(docCount > 0, "We didn't insert anything, better check maxTransactionSize!");
      assertTrue(docCount < maxDocs, "We actually inserted everything, better check maxTransactionSize!");
      trx.commit();
      c = db._collection(cn);
      assertEqual(docCount, c.count(), `expected ${docCount} documents in collection ${cn} but got ${c.count()}`);
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
    testUpdateWithDocumentStatement: function () {
      // ConcurrentStreamTransactionsTest.java
      let docs = [];
      let c = db[cn];
      for (let i = 0; i < 1000; ++i) {
        docs.push({ _key: `${i}`, value2: "testmann " + i });
      }
      c.insert(docs);
      let docid = `${cn}/500`;
      let trx = db._createTransaction({ collections: { write: [c] } });
      trx.query(aql`LET d = DOCUMENT(${docid})
                        UPDATE d WITH { "aaa": "aaa" } IN ${c}
                        RETURN true`);
      trx.commit();
      let doc = c.byExample({_key: '500'}).toArray()[0];
      assertTrue(doc.hasOwnProperty('aaa'), JSON.stringify(doc));
    },
    testStreamTransactionCluster: function () {
      let IM = global.instanceManager;
      if (IM.options.cluster) {
        let coordinators = IM.arangods.filter(arangod => { return arangod.isFrontend();});
        let count = 0;
        let trx = db._createTransaction({ collections: { write: [cn], exclusive: [cn] } });
        let c = trx.collection(cn);
        while (count < 50){
          coordinators[count %2].connect();
          c.insert({count: count});
          count ++;
        }
        trx.abort();
        coordinators[0].connect();
        assertEqual(db[cn].count(), 0);
      }
    }
  };
}

jsunity.run(testSuite);
return jsunity.done();
