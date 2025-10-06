/*jshint globalstrict:false, strict:false, maxlen : 4000 */
/* global arango, assertTrue, assertFalse, assertEqual, assertNotEqual, fail */

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
/// @author Copyright 2012, triAGENS GmbH, Cologne, Germany
// //////////////////////////////////////////////////////////////////////////////

'use strict';
const jsunity = require('jsunity');
const internal = require('internal');
const db = require("@arangodb").db;
  
const clearTransactions = () => {
  arango.DELETE("/_api/transaction/history");
};

const getTransactions = (database = '_system', collections, type = '', origin = '') => {
  let trx = arango.GET("/_api/transaction/history");

  trx = trx.filter((trx) => {
    // we currently ignore all internal transactions for testing
    if (trx.type === 'internal') {
      return false;
    }
    // filter on origin
    if (origin !== '' && trx.origin !== origin) {
      return false;
    }
    // filter on type
    if (type !== '' && trx.type !== type) {
      return false;
    }
    // filter on database name
    if (trx.database !== database) {
      return false;
    }
    // filter on collection name(s)
    if (collections.length !== 0 && !collections.every((c) => trx.collections.includes(c))) {
      return false;
    }

    return true;
  });
  return trx;
};

function MemoryUsageSuite () {
  'use strict';

  const cn = "UnitTestsCollection";
  
  return {
    setUp: function () {
      db._create(cn);
    },

    tearDown: function () {
      db._drop(cn);
    },
    
    testInsertSingleDocument: function () {
      clearTransactions();

      db[cn].insert({});

      let trx = getTransactions('_system', [cn], 'REST', 'inserting document(s)');
      assertEqual(1, trx.length);
      assertEqual([cn], trx[0].collections, trx);
      assertEqual('REST', trx[0].type, trx);
      assertEqual('inserting document(s)', trx[0].origin, trx);
      assertTrue(trx[0].peakMemoryUsage > 100, trx);
      assertTrue(trx[0].peakMemoryUsage >= trx[0].memoryUsage, trx);
    },
    
    testInsertBatchOfDocuments: function () {
      clearTransactions();

      const n = 10000;
      let docs = [];
      for (let i = 0; i < n; ++i) {
        docs.push({ value1: "testmann" + i, value2: i });
      }
      db[cn].insert(docs);

      let trx = getTransactions('_system', [cn], 'REST', 'inserting document(s)');
      assertEqual(1, trx.length);
      assertEqual([cn], trx[0].collections, trx);
      assertEqual('REST', trx[0].type, trx);
      assertEqual('inserting document(s)', trx[0].origin, trx);
      assertTrue(trx[0].peakMemoryUsage > n * 500, trx);
      assertTrue(trx[0].peakMemoryUsage >= trx[0].memoryUsage, trx);
    },
    
    testInsertSingleAQL: function () {
      clearTransactions();

      db._query(`INSERT {} INTO ${cn}`);

      let trx = getTransactions('_system', [cn], 'AQL', 'running AQL query');
      assertEqual(1, trx.length);
      assertEqual([cn], trx[0].collections, trx);
      assertEqual('AQL', trx[0].type, trx);
      assertEqual('running AQL query', trx[0].origin, trx);
      assertTrue(trx[0].peakMemoryUsage > 100, trx);
      assertTrue(trx[0].peakMemoryUsage >= trx[0].memoryUsage, trx);
    },
    
    testInsertBatchOfDocumentsAQL: function () {
      clearTransactions();

      const n = 10000;
      db._query(`FOR i IN 0..${n - 1} INSERT {} INTO ${cn}`);

      let trx = getTransactions('_system', [cn], 'AQL', 'running AQL query');
      assertEqual(1, trx.length);
      assertEqual([cn], trx[0].collections, trx);
      assertEqual('AQL', trx[0].type, trx);
      assertEqual('running AQL query', trx[0].origin, trx);
      assertTrue(trx[0].peakMemoryUsage > n * 500, trx);
      assertTrue(trx[0].peakMemoryUsage >= trx[0].memoryUsage, trx);
    },

    testUpdateSingleDocument: function () {
      db[cn].insert({ _key: 'testmann' });

      clearTransactions();

      db[cn].update('testmann', {value: 42});

      let trx = getTransactions('_system', [cn], 'REST'); //, 'modifying document(s)');
      assertEqual(1, trx.length);
      assertEqual([cn], trx[0].collections, trx);
      assertEqual('REST', trx[0].type, trx);
      assertEqual('modifying document(s)', trx[0].origin, trx);
      assertTrue(trx[0].peakMemoryUsage > 100, trx);
      assertTrue(trx[0].peakMemoryUsage >= trx[0].memoryUsage, trx);
    },
    
    testUpdateBatchOfDocuments: function () {
      const n = 10000;
      let docs = [];
      for (let i = 0; i < n; ++i) {
        docs.push({ _key: "testmann" + i, value: i });
      }
      db[cn].insert(docs);

      docs.forEach((doc) => {
        doc.value = 42;
      });
      let keys = docs.map((doc) => doc._key);

      clearTransactions();

      db[cn].update(keys, docs);

      let trx = getTransactions('_system', [cn], 'REST', 'modifying document(s)');
      assertEqual(1, trx.length);
      assertEqual([cn], trx[0].collections, trx);
      assertEqual('REST', trx[0].type, trx);
      assertEqual('modifying document(s)', trx[0].origin, trx);
      assertTrue(trx[0].peakMemoryUsage > n * 500, trx);
      assertTrue(trx[0].peakMemoryUsage >= trx[0].memoryUsage, trx);
    },
    
    testUpdateSingleAQL: function () {
      db[cn].insert({ _key: 'testmann' });

      clearTransactions();

      db._query(`UPDATE 'testmann' WITH {value: 42} INTO ${cn}`);

      let trx = getTransactions('_system', [cn], 'AQL', 'running AQL query');
      assertEqual(1, trx.length);
      assertEqual([cn], trx[0].collections, trx);
      assertEqual('AQL', trx[0].type, trx);
      assertEqual('running AQL query', trx[0].origin, trx);
      assertTrue(trx[0].peakMemoryUsage > 100, trx);
      assertTrue(trx[0].peakMemoryUsage >= trx[0].memoryUsage, trx);
    },
    
    testUpdateBatchOfDocumentsAQL: function () {
      const n = 10000;
      let docs = [];
      for (let i = 0; i < n; ++i) {
        docs.push({ _key: "testmann" + i, value: i });
      }
      db[cn].insert(docs);

      clearTransactions();

      db._query(`FOR i IN 0..${n - 1} UPDATE CONCAT('testmann', i) WITH {value: 42} INTO ${cn}`);

      let trx = getTransactions('_system', [cn], 'AQL', 'running AQL query');
      assertEqual(1, trx.length);
      assertEqual([cn], trx[0].collections, trx);
      assertEqual('AQL', trx[0].type, trx);
      assertEqual('running AQL query', trx[0].origin, trx);
      assertTrue(trx[0].peakMemoryUsage > n * 500, trx);
      assertTrue(trx[0].peakMemoryUsage >= trx[0].memoryUsage, trx);
    },
    
    testRemoveSingleDocument: function () {
      db[cn].insert({_key: 'test'});
      clearTransactions();
      
      db[cn].remove('test');

      let trx = getTransactions('_system', [cn], 'REST', 'removing document(s)');
      assertEqual(1, trx.length);
      assertEqual([cn], trx[0].collections, trx);
      assertEqual('REST', trx[0].type, trx);
      assertEqual('removing document(s)', trx[0].origin, trx);
      assertTrue(trx[0].peakMemoryUsage > 100, trx);
      assertTrue(trx[0].peakMemoryUsage >= trx[0].memoryUsage, trx);
    },
    
    testRemoveBatchOfDocuments: function () {
      const n = 10000;
      let docs = [];
      for (let i = 0; i < n; ++i) {
        docs.push({ value1: "testmann" + i, value2: i });
      }
      let keys = db[cn].insert(docs).map((doc) => doc._key);
      clearTransactions();
      
      db[cn].remove(keys);

      let trx = getTransactions('_system', [cn], 'REST', 'removing document(s)');
      assertEqual(1, trx.length);
      assertEqual([cn], trx[0].collections, trx);
      assertEqual('REST', trx[0].type, trx);
      assertEqual('removing document(s)', trx[0].origin, trx);
      assertTrue(trx[0].peakMemoryUsage > n * 500, trx);
      assertTrue(trx[0].peakMemoryUsage >= trx[0].memoryUsage, trx);
    },
    
    testRemoveSingleAQL: function () {
      db[cn].insert({ _key: "test" });
      clearTransactions();

      db._query(`REMOVE 'test' INTO ${cn}`);
      
      let trx = getTransactions('_system', [cn], 'AQL', 'running AQL query');
      assertEqual(1, trx.length);
      assertEqual([cn], trx[0].collections, trx);
      assertEqual('AQL', trx[0].type, trx);
      assertEqual('running AQL query', trx[0].origin, trx);
      assertTrue(trx[0].peakMemoryUsage > 100, trx);
      assertTrue(trx[0].peakMemoryUsage >= trx[0].memoryUsage, trx);
    },
    
    testRemoveBatchOfDocumentsAQL: function () {
      const n = 10000;
      db._query(`FOR i IN 0..${n - 1} INSERT {_key: CONCAT('test', i)} INTO ${cn}`);
      clearTransactions();

      db._query(`FOR i IN 0..${n - 1} REMOVE {_key: CONCAT('test', i)} INTO ${cn}`);
     
      let trx = getTransactions('_system', [cn], 'AQL', 'running AQL query');
      assertEqual(1, trx.length);
      assertEqual([cn], trx[0].collections, trx);
      assertEqual('AQL', trx[0].type, trx);
      assertEqual('running AQL query', trx[0].origin, trx);
      assertTrue(trx[0].peakMemoryUsage > n * 500, trx);
      assertTrue(trx[0].peakMemoryUsage >= trx[0].memoryUsage, trx);
    },
    
    testInsertSingleDocumentInJavaScriptTransaction: function () {
      clearTransactions();

      db._executeTransaction({
        collections: { write: cn },
        action: (params) => {
          let db = require("internal").db;
          db[params.cn].insert({});
        },
        params: {cn},
      });
      
      let trx = getTransactions('_system', [cn], 'REST', 'JavaScript transaction');
      assertEqual(1, trx.length);
      assertEqual([cn], trx[0].collections, trx);
      assertEqual('REST', trx[0].type, trx);
      assertEqual('JavaScript transaction', trx[0].origin, trx);
      assertTrue(trx[0].peakMemoryUsage > 100, trx);
      assertTrue(trx[0].peakMemoryUsage >= trx[0].memoryUsage, trx);
    },
    
    testInsertMultipleDocumentsInJavaScriptTransaction: function () {
      clearTransactions();

      const n = 10000;
      db._executeTransaction({
        collections: { write: cn },
        action: (params) => {
          let db = require("internal").db;
          let docs = [];
          for (let i = 0; i < params.n; ++i) {
            docs.push({ value1: "testmann" + i, value2: i });
          }
          db[params.cn].insert(docs);
        },
        params: {cn, n},
      });
      
      let trx = getTransactions('_system', [cn], 'REST', 'JavaScript transaction');
      assertEqual(1, trx.length);
      assertEqual([cn], trx[0].collections, trx);
      assertEqual('REST', trx[0].type, trx);
      assertEqual('JavaScript transaction', trx[0].origin, trx);
      assertTrue(trx[0].peakMemoryUsage > n * 500, trx);
      assertTrue(trx[0].peakMemoryUsage >= trx[0].memoryUsage, trx);
    },
    
    testInsertMultipleDocumentsRollbackInJavaScriptTransaction: function () {
      clearTransactions();

      const n = 10000;
      try {
        db._executeTransaction({
          collections: { write: cn },
          action: (params) => {
            let db = require("internal").db;
            let docs = [];
            for (let i = 0; i < params.n; ++i) {
              docs.push({ value1: "testmann" + i, value2: i });
            }
            db[params.cn].insert(docs);
            throw "peng!";
          },
          params: {cn, n},
        });
        fail();
      } catch (err) {
        assertEqual(internal.errors.ERROR_TRANSACTION_INTERNAL.code, err.errorNum);
      }
      
      let trx = getTransactions('_system', [cn], 'REST', 'JavaScript transaction');
      assertEqual(1, trx.length);
      assertEqual([cn], trx[0].collections, trx);
      assertEqual('REST', trx[0].type, trx);
      assertEqual('JavaScript transaction', trx[0].origin, trx);
      assertTrue(trx[0].peakMemoryUsage > n * 500, trx);
      assertTrue(trx[0].peakMemoryUsage >= trx[0].memoryUsage, trx);
    },
    
    testInsertMultipleDocumentsAQLInJavaScriptTransaction: function () {
      clearTransactions();

      const n = 10000;
      db._executeTransaction({
        collections: { write: cn },
        action: (params) => {
          let db = require("internal").db;
          let docs = [];
          for (let i = 0; i < params.n; ++i) {
            docs.push({ value1: "testmann" + i, value2: i });
          }
          db[params.cn].insert(docs);
        },
        params: {cn, n},
      });
      
      let trx = getTransactions('_system', [cn], 'REST', 'JavaScript transaction');
      assertEqual(1, trx.length);
      assertEqual([cn], trx[0].collections, trx);
      assertEqual('REST', trx[0].type, trx);
      assertEqual('JavaScript transaction', trx[0].origin, trx);
      assertTrue(trx[0].peakMemoryUsage > n * 500, trx);
      assertTrue(trx[0].peakMemoryUsage >= trx[0].memoryUsage, trx);
    },
    
    testAQLInJavaScriptTransaction: function () {
      clearTransactions();

      const n = 10000;
      db._executeTransaction({
        collections: { write: cn },
        action: (params) => {
          let db = require("internal").db;
          db._query(`FOR i IN 0..${params.n - 1} INSERT {} INTO ${params.cn}`);
        },
        params: {cn, n},
      });
      
      let trx = getTransactions('_system', [cn], 'REST', 'JavaScript transaction');
      assertEqual(1, trx.length);
      assertEqual([cn], trx[0].collections, trx);
      assertEqual('REST', trx[0].type, trx);
      assertEqual('JavaScript transaction', trx[0].origin, trx);
      assertTrue(trx[0].peakMemoryUsage > n * 500, trx);
      assertTrue(trx[0].peakMemoryUsage >= trx[0].memoryUsage, trx);
    },
    
    testAQLRollbackInJavaScriptTransaction: function () {
      clearTransactions();

      const n = 10000;
      try {
        db._executeTransaction({
          collections: { write: cn },
          action: (params) => {
            let db = require("internal").db;
            let docs = [];
            for (let i = 0; i < params.n; ++i) {
              docs.push({ value1: "testmann" + i, value2: i });
            }
            db[params.cn].insert(docs);
            db._query(`FOR i IN 0..${params.n - 1} INSERT {_key: (i == ${params.n - 1} ? '' : CONCAT('test', i))} INTO ${params.cn}`);
          },
          params: {cn, n},
        });
        fail();
      } catch (err) {
        assertEqual(internal.errors.ERROR_ARANGO_DOCUMENT_KEY_BAD.code, err.errorNum);
      }

      let trx = getTransactions('_system', [cn], 'REST', 'JavaScript transaction');
      assertEqual(1, trx.length);
      assertEqual([cn], trx[0].collections, trx);
      assertEqual('REST', trx[0].type, trx);
      assertEqual('JavaScript transaction', trx[0].origin, trx);
      assertTrue(trx[0].peakMemoryUsage > (n * 2) * 500, trx);
      assertTrue(trx[0].peakMemoryUsage >= trx[0].memoryUsage, trx);
    },
    
    testEmptyStreamingTransaction: function () {
      clearTransactions();

      let t = db._createTransaction({
        collections: { write: cn },
      });

      try {
        let trx = getTransactions('_system', [cn], 'REST', 'streaming transaction');
        assertEqual(1, trx.length);
        assertEqual([cn], trx[0].collections, trx);
        assertEqual('REST', trx[0].type, trx);
        assertEqual('streaming transaction', trx[0].origin, trx);
        assertEqual(0, trx[0].memoryUsage);
        assertEqual(0, trx[0].peakMemoryUsage);
      } finally {
        t.abort();
      }
    },
    
    testReadOnlyStreamingTransaction: function () {
      clearTransactions();

      db[cn].insert({});

      let t = db._createTransaction({
        collections: { read: cn },
      });

      try {
        t.query(`FOR doc IN ${cn} RETURN doc`);

        let trx = getTransactions('_system', [cn], 'REST', 'streaming transaction');
        assertEqual(1, trx.length);
        assertEqual([cn], trx[0].collections, trx);
        assertEqual('REST', trx[0].type, trx);
        assertEqual('streaming transaction', trx[0].origin, trx);
        assertEqual(0, trx[0].memoryUsage);
        assertEqual(0, trx[0].peakMemoryUsage);
      } finally {
        t.abort();
      }
    },
    
    testInsertSingleDocumentInStreamingTransaction: function () {
      clearTransactions();

      let t = db._createTransaction({
        collections: { write: cn },
      });

      try {
        t.collection(cn).insert({});

        let trx = getTransactions('_system', [cn], 'REST', 'streaming transaction');
        assertEqual(1, trx.length);
        assertEqual([cn], trx[0].collections, trx);
        assertEqual('REST', trx[0].type, trx);
        assertEqual('streaming transaction', trx[0].origin, trx);
        assertTrue(trx[0].peakMemoryUsage > 100, trx);
        assertTrue(trx[0].peakMemoryUsage >= trx[0].memoryUsage, trx);
      } finally {
        t.abort();
      }
    },
    
    testInsertMultipleDocumentsInStreamingTransaction: function () {
      clearTransactions();

      const n = 10000;
      
      let t = db._createTransaction({
        collections: { write: cn },
      });
      
      try {
        let docs = [];
        for (let i = 0; i < n; ++i) {
          docs.push({ value1: "testmann" + i, value2: i });
        }
        t.collection(cn).insert(docs);

        let trx = getTransactions('_system', [cn], 'REST', 'streaming transaction');
        assertEqual(1, trx.length);
        assertEqual([cn], trx[0].collections, trx);
        assertEqual('REST', trx[0].type, trx);
        assertEqual('streaming transaction', trx[0].origin, trx);
        assertTrue(trx[0].peakMemoryUsage > n * 500, trx);
        assertTrue(trx[0].peakMemoryUsage >= trx[0].memoryUsage, trx);
      } finally {
        t.abort();
      }
    },
    
    testAqlQueryInStreamingTransaction: function () {
      clearTransactions();

      const n = 10000;
      
      let t = db._createTransaction({
        collections: { write: cn },
      });
      
      try {
        t.query(`FOR i IN 0..${n - 1} INSERT {} INTO ${cn}`);

        let trx = getTransactions('_system', [cn], 'REST', 'streaming transaction');
        assertEqual(1, trx.length);
        assertEqual([cn], trx[0].collections, trx);
        assertEqual('REST', trx[0].type, trx);
        assertEqual('streaming transaction', trx[0].origin, trx);
        assertTrue(trx[0].peakMemoryUsage > n * 500, trx);
        assertTrue(trx[0].peakMemoryUsage >= trx[0].memoryUsage, trx);
      } finally {
        t.abort();
      }
    },
    
  };
}

function GraphMemoryUsageSuite () {
  'use strict';

  const g = require("@arangodb/general-graph");
  const gn = "UnitTestsGraph2";
  const vn = "UnitTestsVertex";
  const en = "UnitTestsEdge";
  
  return {
    setUp: function () {
      const rel = g._relation(en, vn, vn);
      g._create(gn, [rel], [], {});
    },

    tearDown: function () {
      g._drop(gn, true);
    },
    
    testInsertSingleVertex: function () {
      clearTransactions();

      let graph = g._graph(gn);
      graph[vn].insert({});

      let trx = getTransactions('_system', [vn], 'REST', 'inserting document(s)');
      assertEqual(1, trx.length);
      assertEqual([vn], trx[0].collections, trx);
      assertEqual('REST', trx[0].type, trx);
      assertEqual('inserting document(s)', trx[0].origin, trx);
      assertTrue(trx[0].peakMemoryUsage > 100, trx);
      assertTrue(trx[0].peakMemoryUsage >= trx[0].memoryUsage, trx);
    },
    
    testInsertMultipleVertexes: function () {
      clearTransactions();
      
      const n = 10000;

      let graph = g._graph(gn);
      let docs = [];
      for (let i = 0; i < n; ++i) {
        docs.push({});
      }
      graph[vn].insert(docs);

      let trx = getTransactions('_system', [vn], 'REST', 'inserting document(s)');
      assertEqual(1, trx.length);
      assertEqual([vn], trx[0].collections, trx);
      assertEqual('REST', trx[0].type, trx);
      assertEqual('inserting document(s)', trx[0].origin, trx);
      assertTrue(trx[0].peakMemoryUsage > n * 500, trx);
      assertTrue(trx[0].peakMemoryUsage >= trx[0].memoryUsage, trx);
    },

    testInsertSingleEdge: function () {
      clearTransactions();

      let graph = g._graph(gn);
      graph[en].insert({_from: vn + "/test0", _to: vn + "/test1"});

      let trx = getTransactions('_system', [en], 'REST', 'inserting document(s)');
      assertEqual(1, trx.length);
      assertEqual([en], trx[0].collections, trx);
      assertEqual('REST', trx[0].type, trx);
      assertEqual('inserting document(s)', trx[0].origin, trx);
      assertTrue(trx[0].peakMemoryUsage > 100, trx);
      assertTrue(trx[0].peakMemoryUsage >= trx[0].memoryUsage, trx);
    },
    
  };
}

if (db._version(true)['details']['maintainer-mode'] === 'true') {
  jsunity.run(MemoryUsageSuite);
  jsunity.run(GraphMemoryUsageSuite);
}
return jsunity.done();
