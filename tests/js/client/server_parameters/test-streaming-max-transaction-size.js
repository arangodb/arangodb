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
