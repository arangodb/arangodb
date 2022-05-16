/* jshint globalstrict:false, strict:false, maxlen: 200 */
/* global fail, assertTrue, assertFalse, assertEqual, assertNotUndefined, arango */

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

const jsunity = require('jsunity');
const internal = require('internal');
const arangodb = require('@arangodb');
const testHelper = require('@arangodb/test-helper');
const deriveTestSuite = testHelper.deriveTestSuite;
const db = arangodb.db;

const cn = 'UnitTestsIterators';
const ecn = 'UnitTestsEdgeIterators';
const pad = function(value) {
  value = String(value);
  return Array(6 - value.length).join("0") + value;
};

function IteratorSuite(permuteConfigs) {
  'use strict';

  const prepare = function(ctx) {
    const col = ctx.collection(cn);
    let docs = [];
    for (let i = 0; i < 5000; ++i) {
      docs.push({ _key: "test" + pad(i), value1: i, value2: i % 113, value3: i });
    }
    col.insert(docs);

    let ecol = ctx.collection(ecn);
    for (let i = 0; i < 5000; ++i) {
      docs.push({
        _from: cn + "/blubb",
        _to: cn + "/test" + pad(i),
        value1: i,
        value2: i % 113,
        value3: i,
        uniqueValue: i,
      });
    }
    ecol.insert(docs);
  };

  const permute = function(test) {
    const run = function(ctx, opts) {
      prepare(ctx);
      try {
        test(ctx, opts);
      } finally {
        ctx.abort();
      }
      
      db[cn].truncate();
      db[ecn].truncate();
    };
    permuteConfigs(run);
  };

  return {
    setUp: function () {
      db._drop(cn);
      let c = db._create(cn, { numberOfShards: 3 });
      c.ensureIndex({ type: "persistent", fields: ["value1"] });
      c.ensureIndex({ type: "persistent", fields: ["value2"] });
      if (!internal.isCluster()) {
        c.ensureIndex({ type: "persistent", fields: ["uniqueValue"], unique: true, sparse: true });
      }

      db._drop(ecn);
      db._createEdgeCollection(ecn, { numberOfShards: 3 });
    },

    tearDown: function () {
      db._drop(cn);
      db._drop(ecn);
    },
    
    testForwardIterationReadOnly: function () {
      permute((ctx, opts) => {
        // full scan
        let result = ctx.query('FOR doc IN @@c SORT doc.value3 ASC RETURN doc', { '@c': cn }, opts).toArray();
        assertEqual(5000, result.length);
        result.forEach((doc, index) => {
          assertEqual("test" + pad(index), doc._key);
          assertEqual(index, doc.value1);
          assertEqual(index % 113, doc.value2);
        });

        // full scan over a filtered range
        result = ctx.query('FOR doc IN @@c FILTER doc.value3 > 1 && doc.value3 <= 4995 SORT doc.value3 ASC RETURN doc', { '@c': cn }, opts).toArray();
        assertEqual(4994, result.length);
        result.forEach((doc, index) => {
          index = index + 2;
          assertEqual("test" + pad(index), doc._key);
          assertEqual(index, doc.value1);
          assertEqual(index % 113, doc.value2);
        });

        // full scan using primary index
        result = ctx.query('FOR doc IN @@c SORT doc._key ASC RETURN doc._key', { '@c': cn }, opts).toArray();
        assertEqual(5000, result.length);
        result.forEach((value, index) => {
          assertEqual("test" + pad(index), value);
        });

        // full scan using primary index and a filter
        result = ctx.query('FOR doc IN @@c FILTER doc._key > "test00001" && doc._key <= "test04995" SORT doc._key ASC RETURN doc._key', { '@c': cn }, opts).toArray();
        assertEqual(4994, result.length);
        result.forEach((value, index) => {
          index = index + 2;
          assertEqual("test" + pad(index), value);
        });

        // full scan using secondary index
        result = ctx.query('FOR doc IN @@c SORT doc.value1 ASC RETURN doc', { '@c': cn }, opts).toArray();
        assertEqual(5000, result.length);
        result.forEach((doc, index) => {
          assertEqual("test" + pad(index), doc._key);
          assertEqual(index, doc.value1);
          assertEqual(index % 113, doc.value2);
        });

        // full scan using secondary index and a filter
        result = ctx.query('FOR doc IN @@c FILTER doc.value1 > 1 && doc.value1 <= 4995 SORT doc.value1 ASC RETURN doc', { '@c': cn }, opts).toArray();
        assertEqual(4994, result.length);
        result.forEach((doc, index) => {
          index = index + 2;
          assertEqual("test" + pad(index), doc._key);
          assertEqual(index, doc.value1);
          assertEqual(index % 113, doc.value2);
        });
      });
    },

    testReverseIterationReadOnly: function () {
      permute((ctx, opts) => {
        // full scan
        let result = ctx.query('FOR doc IN @@c SORT doc.value3 DESC RETURN doc', { '@c': cn }, opts).toArray();
        assertEqual(5000, result.length);
        result.forEach((doc, index) => {
          index = 5000 - index - 1;
          assertEqual("test" + pad(index), doc._key);
          assertEqual(index, doc.value1);
          assertEqual(index % 113, doc.value2);
        });

        // full scan over a filtered range
        result = ctx.query('FOR doc IN @@c FILTER doc.value3 > 1 && doc.value3 <= 4995 SORT doc.value3 DESC RETURN doc', { '@c': cn }, opts).toArray();
        assertEqual(4994, result.length);
        result.forEach((doc, index) => {
          index = 4995 - index;
          assertEqual("test" + pad(index), doc._key);
          assertEqual(index, doc.value1);
          assertEqual(index % 113, doc.value2);
        });

        // full scan using primary index
        result = ctx.query('FOR doc IN @@c SORT doc._key DESC RETURN doc._key', { '@c': cn }, opts).toArray();
        assertEqual(5000, result.length);
        result.forEach((value, index) => {
          index = 5000 - index - 1;
          assertEqual("test" + pad(index), value);
        });

        // full scan using primary index and a filter
        result = ctx.query('FOR doc IN @@c FILTER doc._key > "test00001" && doc._key <= "test04995" SORT doc._key DESC RETURN doc._key', { '@c': cn }, opts).toArray();
        assertEqual(4994, result.length);
        result.forEach((value, index) => {
          index = 4995 - index;
          assertEqual("test" + pad(index), value);
        });

        // full scan using secondary index
        result = ctx.query('FOR doc IN @@c SORT doc.value1 DESC RETURN doc', { '@c': cn }, opts).toArray();
        assertEqual(5000, result.length);
        result.forEach((doc, index) => {
          index = 5000 - index - 1;
          assertEqual("test" + pad(index), doc._key);
          assertEqual(index, doc.value1);
          assertEqual(index % 113, doc.value2);
        });

        // full scan using secondary index and a filter
        result = ctx.query('FOR doc IN @@c FILTER doc.value1 > 1 && doc.value1 <= 4995 SORT doc.value1 DESC RETURN doc', { '@c': cn }, opts).toArray();
        assertEqual(4994, result.length);
        result.forEach((doc, index) => {
          index = 4995 - index;
          assertEqual("test" + pad(index), doc._key);
          assertEqual(index, doc.value1);
          assertEqual(index % 113, doc.value2);
        });
      });
    },

    testForwardIterationInsertFullScan: function () {
      permute((ctx, opts) => {
        // full scan
        const tc = ctx.collection(cn);
        let result = ctx.query('FOR doc IN @@c SORT doc.value3 ASC INSERT { _key: CONCAT("y", doc.value3) } INTO @@c SORT doc._key RETURN doc', { '@c': cn }, opts).toArray();
        assertEqual(5000 + 5000, tc.count());
        assertEqual(5000, result.length);
        result.forEach((doc, index) => {
          assertEqual("test" + pad(index), doc._key);
          assertEqual(index, doc.value1);
          assertEqual(index % 113, doc.value2);
        });
      });
    },

    testForwardIterationInsertFullScanFiltered: function () {
      permute((ctx, opts) => {
        // full scan over a filtered range
        const tc = ctx.collection(cn);
        const q =`
          FOR doc IN @@c
            FILTER doc.value3 > 1 && doc.value3 <= 4995
            SORT doc.value3 ASC
            INSERT { _key: CONCAT("y", doc.value3) } INTO @@c
            SORT doc._key
            RETURN doc`;
        let result = ctx.query(q, { '@c': cn }, opts).toArray();

        assertEqual(5000 + 4994, tc.count());
        assertEqual(4994, result.length);
        result.forEach((doc, index) => {
          index = index + 2;
          assertEqual("test" + pad(index), doc._key);
          assertEqual(index, doc.value1);
          assertEqual(index % 113, doc.value2);
        });
      });
    },

    testForwardIterationInsertPrimaryIndex: function () {
      permute((ctx, opts) => {
        const tc = ctx.collection(cn);
        // full scan using primary index
        let result = ctx.query('FOR doc IN @@c SORT doc._key ASC INSERT { _key: CONCAT("y", doc._key) } INTO @@c SORT doc._key RETURN doc._key', { '@c': cn }, opts).toArray();
        assertEqual(5000 + 5000, tc.count());
        assertEqual(5000, result.length);
        result.forEach((value, index) => {
          assertEqual("test" + pad(index), value);
        });
      });
    },

    testForwardIterationInsertPrimaryIndexBefore: function () {
      permute((ctx, opts) => {
        const tc = ctx.collection(cn);
        // full scan using primary index
        let result = ctx.query('FOR doc IN @@c SORT doc._key ASC INSERT { _key: CONCAT("a", doc._key) } INTO @@c SORT doc._key RETURN doc._key', { '@c': cn }, opts).toArray();
        assertEqual(5000 + 5000, tc.count());
        assertEqual(5000, result.length);
        result.forEach((value, index) => {
          assertEqual("test" + pad(index), value);
        });
      });
    },

    testForwardIterationInsertPrimaryIndexFiltered: function () {
      permute((ctx, opts) => {
        const tc = ctx.collection(cn);
        // full scan using primary index and a filter
        const q = `
          FOR doc IN @@c
            FILTER doc._key > "test00001" && doc._key <= "test04995"
            SORT doc._key ASC
            INSERT { _key: CONCAT("y", doc._key) } INTO @@c
            SORT doc._key
            RETURN doc._key`;
        let result = ctx.query(q, { '@c': cn }, opts).toArray();
        assertEqual(5000 + 4994, tc.count());
        assertEqual(4994, result.length);
        result.forEach((value, index) => {
          index = index + 2;
          assertEqual("test" + pad(index), value);
        });
      });
    },

    testForwardIterationInsertPrimaryIndexFilteredBefore: function () {
      permute((ctx, opts) => {
        const tc = ctx.collection(cn);
        // full scan using primary index and a filter
        const q = `
          FOR doc IN @@c
            FILTER doc._key > "test00001" && doc._key <= "test04995"
            SORT doc._key ASC
            INSERT { _key: CONCAT("a", doc._key) } INTO @@c
            SORT doc._key
            RETURN doc._key`;
        let result = ctx.query(q, { '@c': cn }, opts).toArray();
        assertEqual(5000 + 4994, tc.count());
        assertEqual(4994, result.length);
        result.forEach((value, index) => {
          index = index + 2;
          assertEqual("test" + pad(index), value);
        });
      });
    },

    testForwardIterationInsertSecondaryIndex: function () {
      permute((ctx, opts) => {
        const tc = ctx.collection(cn);
        // full scan using secondary index
        const q = `
          FOR doc IN @@c
            SORT doc.value1 ASC
            INSERT { _key: CONCAT("y", doc.value1) } INTO @@c
            SORT doc._key
            RETURN doc`;
        let result = ctx.query(q, { '@c': cn }, opts).toArray();
        assertEqual(5000 + 5000, tc.count());
        assertEqual(5000, result.length);
        result.forEach((doc, index) => {
          assertEqual("test" + pad(index), doc._key);
          assertEqual(index, doc.value1);
          assertEqual(index % 113, doc.value2);
        });
      });
    },

    testForwardIterationInsertSecondaryIndexFiltered: function () {
      permute((ctx, opts) => {
        const tc = ctx.collection(cn);
        // full scan using secondary index and a filter
        const q = `
          FOR doc IN @@c
            FILTER doc.value1 > 1 && doc.value1 <= 4995
            SORT doc.value1 ASC
            INSERT { _key: CONCAT("y", doc.value1) } INTO @@c
            SORT doc._key
            RETURN doc`;
        let result = ctx.query(q, { '@c': cn }, opts).toArray();
        assertEqual(5000 + 4994, tc.count());
        assertEqual(4994, result.length);
        result.forEach((doc, index) => {
          index = index + 2;
          assertEqual("test" + pad(index), doc._key);
          assertEqual(index, doc.value1);
          assertEqual(index % 113, doc.value2);
        });
      });
    },

    testForwardIterationInsertEdgeIndex: function () {
      permute((ctx, opts) => {
        const tc = ctx.collection(ecn);
        // full scan using edge index
        let result = ctx.query(`
          FOR doc IN @@c
            FILTER doc._from == "${cn}/blubb"
            INSERT { _from: "${cn}/blubb", _to: CONCAT("y", doc._to) } INTO @@c
            SORT doc._to ASC
            RETURN doc`,
          { '@c': ecn }, opts).toArray();
        assertEqual(5000 + 5000, tc.count());
        assertEqual(5000, result.length);
        result.forEach((doc, index) => {
          assertEqual(cn + "/test" + pad(index), doc._to);
          assertEqual(index, doc.value1);
          assertEqual(index % 113, doc.value2);
        });
      });
    },

    testForwardIterationInsertEdgeIndexFiltered: function () {
      permute((ctx, opts) => {
        const tc = ctx.collection(ecn);
        // full scan using edge index and a filter
        // we have to wrap the range scan on _to with a NOOPT to prevent the optimizer
        // from picking an index for it. Edge indexes are not fully ordered and therefore
        // do not support range scans, but we currently have a bug that does not prevent
        // this, which can potential produce incorrect results.
        let result = ctx.query(`
          FOR doc IN @@c
            FILTER doc._from == "${cn}/blubb"
            FILTER NOOPT(doc._to > "${cn}/test00001" AND doc._to <= "${cn}/test04995")
            INSERT { _from: "${cn}/blubb", _to: CONCAT("y", doc._to) } INTO @@c
            SORT doc._to ASC
            RETURN doc`,
          { '@c': ecn }, opts).toArray();
        assertEqual(5000 + 4994, tc.count());
        assertEqual(4994, result.length);
        result.forEach((doc, index) => {
          index = index + 2;
          assertEqual(cn + "/test" + pad(index), doc._to);
          assertEqual(index, doc.value1);
          assertEqual(index % 113, doc.value2);
        });
      });
    },

    testDocumentFunction: function () {
      permute((ctx, opts) => {
        const tc = ctx.collection(cn);
        // lookup documents via DOCUMENT function
        let q = `
        FOR i in 1..2000
          LET doc = (FOR j IN 0..0 RETURN DOCUMENT(@@col, "dummy"))[0]
          LET unused = (FOR j in 1..(doc == null ? 1 : 0)
            INSERT { _key: "dummy" } INTO @@col OPTIONS { ignoreErrors: true })
          COLLECT isNull = doc == null WITH COUNT INTO cnt
          RETURN {isNull, cnt}
        `;
        let result = ctx.query(q, { '@col': cn }, opts).toArray();
        assertEqual(5000 + 1, tc.count()); // only a single insert may succeed!
        assertEqual(1, result.length);
        assertEqual(2000, result[0].cnt);
        
        let values = ctx.query('FOR doc in @@c FILTER doc._key == "dummy" RETURN doc', { '@c': cn }, opts).toArray();
        assertEqual(1, values.length);
      });
    },
    
    testSubqueryWithPrimaryIndex: function () {
      permute((ctx, opts) => {
        const tc = ctx.collection(cn);
        // lookup documents via subquery with primary index
        let q = `
        FOR i in 1..2000
          LET doc = (FOR d IN @@col FILTER d._key == "dummy" RETURN d)[0]
          LET unused = (FOR j in 1..(doc == null ? 1 : 0)
            INSERT { _key: "dummy" } INTO @@col OPTIONS { ignoreErrors: true })
          COLLECT isNull = doc == null WITH COUNT INTO cnt
          RETURN {isNull, cnt}
        `;
        let result = ctx.query(q, { '@col': cn }, opts).toArray();
        assertEqual(5000 + 1, tc.count()); // only a single insert may succeed!
        assertEqual(1, result.length);
        assertEqual(2000, result[0].cnt);
        
        let values = ctx.query('FOR doc in @@c FILTER doc._key == "dummy" RETURN doc', { '@c': cn }, opts).toArray();
        assertEqual(1, values.length);
      });
    },

    testSubqueryWithUniqueSecondaryIndex: function () {
      if (internal.isCluster()) {
        return;
      }

      permute((ctx, opts) => {
        const tc = ctx.collection(cn);
        // lookup documents via subquery with unique secondary index
        let q = `
        FOR i in 1..2000
          LET doc = (FOR d IN @@col FILTER d.uniqueValue == "dummy" RETURN d)[0]
          LET unused = (FOR j in 1..(doc == null ? 1 : 0)
            INSERT { uniqueValue: "dummy" } INTO @@col OPTIONS { ignoreErrors: true })
          COLLECT isNull = doc == null WITH COUNT INTO cnt
          RETURN {isNull, cnt}
        `;
        let result = ctx.query(q, { '@col': cn }, opts).toArray();
        assertEqual(5000 + 1, tc.count()); // only a single insert may succeed!
        assertEqual(1, result.length);
        assertEqual(2000, result[0].cnt);
        
        let values = ctx.query('FOR doc in @@c FILTER doc.uniqueValue == "dummy" RETURN doc', { '@c': cn }, opts).toArray();
        assertEqual(1, values.length);
      });
    },

    testUpsertFullScan: function () {
      if (internal.isCluster()) {
        // temporarily disabled because ATM UPSERT is broken in the cluster and needs to be fixed separately
        return;
      }

      permute((ctx, opts) => {
        const tc = ctx.collection(cn);
        // full scan
        let result = ctx.query('FOR i IN 1..2000 UPSERT { v: "dummy" } INSERT { v: "dummy", cnt: 1, idx: i } UPDATE { cnt: OLD.cnt + 1 } IN @@c RETURN OLD', { '@c': cn }, opts).toArray();
        assertEqual(5000 + 1, tc.count());
        assertEqual(2000, result.length);

        let values = ctx.query('FOR doc in @@c FILTER doc.v == "dummy" RETURN doc', { '@c': cn }, opts).toArray();
        assertEqual(1, values.length);
        assertEqual(2000, values[0].cnt);
      });
    },

    testUpsertPrimaryIndex: function () {
      if (internal.isCluster()) {
        // temporarily disabled because ATM UPSERT is broken in the cluster and needs to be fixed separately
        return;
      }

      permute((ctx, opts) => {
        const tc = ctx.collection(cn);
        // primary index
        let q = 'FOR i IN 1..2000 UPSERT { _key: "key" } INSERT { _key: "key", cnt: 1 } UPDATE { cnt: OLD.cnt + 1 } IN @@c RETURN OLD';
        let result = ctx.query(q, { '@c': cn }, opts).toArray();
        assertEqual(5000 + 1, tc.count());
        assertEqual(2000, result.length);

        let values = ctx.query('FOR doc in @@c FILTER doc._key == "key" RETURN doc', { '@c': cn }, opts).toArray();
        assertEqual(1, values.length);
        assertEqual(2000, values[0].cnt);
      });
    },

    testUpsertSecondaryIndex: function () {
      if (internal.isCluster()) {
        // temporarily disabled because ATM UPSERT is broken in the cluster and needs to be fixed separately
        return;
      }

      permute((ctx, opts) => {
        const tc = ctx.collection(cn);
        // secondary index
        const q = 'FOR i IN 1..2000 UPSERT { value1: "blubb" } INSERT { value1: "blubb" , cnt: 1 } UPDATE { cnt: OLD.cnt + 1} IN @@c RETURN OLD';
        let result = ctx.query(q, { '@c': cn }, opts).toArray();
        assertEqual(5000 + 1, tc.count());
        assertEqual(2000, result.length);

        let values = ctx.query('FOR doc in @@c FILTER doc.value1 == "blubb" RETURN doc', { '@c': cn }, opts).toArray();
        assertEqual(1, values.length);
        assertEqual(2000, values[0].cnt);
      });
    },

    testUpsertUniqueSecondaryIndex: function () {
      if (internal.isCluster()) {
        return;
      }

      permute((ctx, opts) => {
        const tc = ctx.collection(cn);
        // secondary index
        let result = ctx.query(
          'FOR i IN 1..2000 UPSERT { uniqueValue: "blubb" } INSERT { uniqueValue: "blubb" , cnt: 1 } UPDATE { cnt: OLD.cnt + 1} IN @@c RETURN OLD', { '@c': cn }, opts).toArray();
        assertEqual(5000 + 1, tc.count());
        assertEqual(2000, result.length);

        let values = ctx.query('FOR doc in @@c FILTER doc.uniqueValue == "blubb" RETURN doc', { '@c': cn }, opts).toArray();
        assertEqual(1, values.length);
        assertEqual(2000, values[0].cnt);
      });
    },
    
    testUpsertEdgeIndex: function () {
      if (internal.isCluster()) {
        // temporarily disabled because ATM UPSERT is broken in the cluster and needs to be fixed separately
        return;
      }

      permute((ctx, opts) => {
        const tc = ctx.collection(ecn);
        // edge index
        let result = ctx.query(
          `FOR i IN 1..2000 UPSERT { _from: "${cn}/xyz" } INSERT { _from: "${cn}/xyz", _to: "${cn}/blubb", cnt: 1 } UPDATE { cnt: OLD.cnt + 1} IN @@c RETURN OLD`,
          { '@c': ecn }, opts).toArray();
        assertEqual(5000 + 1, tc.count());
        assertEqual(2000, result.length);

        let values = ctx.query(`FOR doc in @@c FILTER doc._from == "${cn}/xyz" RETURN doc`, { '@c': ecn }, opts).toArray();
        assertEqual(1, values.length);
        assertEqual(2000, values[0].cnt);
      });
    },
  };
}

function StandaloneAqlIteratorSuite() {
  'use strict';

  const ctx = {
    query: (...args) => db._query(...args),
    collection: (name) => db[name],
    abort: () => {},
  };
  
  let permute = function(run) {
    [ {}, {intermediateCommitCount: 111} ].forEach((opts) => {
      run(ctx, opts);
    });
  };

  let suite = {};
  deriveTestSuite(IteratorSuite(permute), suite, '_StandaloneAql');
  return suite;
}

function TransactionIteratorSuite() {
  'use strict';
  
  let permute = function(run) {
    [ {}, /*{intermediateCommitCount: 111}*/ ].forEach((opts) => {
      const trxOpts = {
        collections: {
          write: [cn, ecn]
        },
      };
      const ctx = db._createTransaction(Object.assign(trxOpts, opts));
      run(ctx, opts);
    });
  };
  
  let suite = {};
  deriveTestSuite(IteratorSuite(permute), suite, '_StreamingTrx');
  return suite;
}

jsunity.run(StandaloneAqlIteratorSuite);
jsunity.run(TransactionIteratorSuite);

return jsunity.done();
