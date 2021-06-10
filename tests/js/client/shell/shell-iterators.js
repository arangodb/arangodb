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
const db = arangodb.db;
  
const cn = 'UnitTestsIterators';
const pad = function(value) {
  value = String(value);
  return Array(5 - value.length).join("0") + value;
};

function StandaloneAqlIteratorSuite() {
  'use strict';
      
  return {
    setUp: function () {
      db._drop(cn);
      let c = db._create(cn, { numberOfShards: 3 });
      let docs = [];
      for (let i = 0; i < 5000; ++i) {
        docs.push({ _key: "test" + pad(i), value1: i, value2: i % 113, value3: i });
      }
      c.insert(docs);
      c.ensureIndex({ type: "persistent", fields: ["value1"] });
      c.ensureIndex({ type: "persistent", fields: ["value2"] });
    },

    tearDown: function () {
      db._drop(cn);
    },
    
    testAqlForwardIterationReadOnly: function () {
      const opts = {};

      // full scan 
      let result = db._query('FOR doc IN @@c SORT doc.value3 ASC RETURN doc', { '@c': cn }, opts).toArray();
      assertEqual(5000, result.length);
      result.forEach((doc, index) => {
        assertEqual("test" + pad(index), doc._key);
        assertEqual(index, doc.value1);
        assertEqual(index % 113, doc.value2);
      });
      
      // full scan over a filtered range
      result = db._query('FOR doc IN @@c FILTER doc.value3 > 1 && doc.value3 <= 4995 SORT doc.value3 ASC RETURN doc', { '@c': cn }, opts).toArray();
      assertEqual(4994, result.length);
      result.forEach((doc, index) => {
        index = index + 2;
        assertEqual("test" + pad(index), doc._key);
        assertEqual(index, doc.value1);
        assertEqual(index % 113, doc.value2);
      });
     
      // full scan using primary index 
      result = db._query('FOR doc IN @@c SORT doc._key ASC RETURN doc._key', { '@c': cn }, opts).toArray();
      assertEqual(5000, result.length);
      result.forEach((value, index) => {
        assertEqual("test" + pad(index), value);
      });
      
      // full scan using primary index and a filter
      result = db._query('FOR doc IN @@c FILTER doc._key > "test0001" && doc._key <= "test4995" SORT doc._key ASC RETURN doc._key', { '@c': cn }, opts).toArray();
      assertEqual(4994, result.length);
      result.forEach((value, index) => {
        index = index + 2;
        assertEqual("test" + pad(index), value);
      });

      // full scan using secondary index 
      result = db._query('FOR doc IN @@c SORT doc.value1 ASC RETURN doc', { '@c': cn }, opts).toArray();
      assertEqual(5000, result.length);
      result.forEach((doc, index) => {
        assertEqual("test" + pad(index), doc._key);
        assertEqual(index, doc.value1);
        assertEqual(index % 113, doc.value2);
      });
      
      // full scan using secondary index and a filter
      result = db._query('FOR doc IN @@c FILTER doc.value1 > 1 && doc.value1 <= 4995 SORT doc.value1 ASC RETURN doc', { '@c': cn }, opts).toArray();
      assertEqual(4994, result.length);
      result.forEach((doc, index) => {
        index = index + 2;
        assertEqual("test" + pad(index), doc._key);
        assertEqual(index, doc.value1);
        assertEqual(index % 113, doc.value2);
      });
    },
    
    testAqlReverseIterationReadOnly: function () {
      const opts = {};

      // full scan 
      let result = db._query('FOR doc IN @@c SORT doc.value3 DESC RETURN doc', { '@c': cn }, opts).toArray();
      assertEqual(5000, result.length);
      result.forEach((doc, index) => {
        index = 5000 - index - 1;
        assertEqual("test" + pad(index), doc._key);
        assertEqual(index, doc.value1);
        assertEqual(index % 113, doc.value2);
      });
      
      // full scan over a filtered range
      result = db._query('FOR doc IN @@c FILTER doc.value3 > 1 && doc.value3 <= 4995 SORT doc.value3 DESC RETURN doc', { '@c': cn }, opts).toArray();
      assertEqual(4994, result.length);
      result.forEach((doc, index) => {
        index = 4995 - index;
        assertEqual("test" + pad(index), doc._key);
        assertEqual(index, doc.value1);
        assertEqual(index % 113, doc.value2);
      });
     
      // full scan using primary index 
      result = db._query('FOR doc IN @@c SORT doc._key DESC RETURN doc._key', { '@c': cn }, opts).toArray();
      assertEqual(5000, result.length);
      result.forEach((value, index) => {
        index = 5000 - index - 1;
        assertEqual("test" + pad(index), value);
      });
      
      // full scan using primary index and a filter
      result = db._query('FOR doc IN @@c FILTER doc._key > "test0001" && doc._key <= "test4995" SORT doc._key DESC RETURN doc._key', { '@c': cn }, opts).toArray();
      assertEqual(4994, result.length);
      result.forEach((value, index) => {
        index = 4995 - index;
        assertEqual("test" + pad(index), value);
      });

      // full scan using secondary index 
      result = db._query('FOR doc IN @@c SORT doc.value1 DESC RETURN doc', { '@c': cn }, opts).toArray();
      assertEqual(5000, result.length);
      result.forEach((doc, index) => {
        index = 5000 - index - 1;
        assertEqual("test" + pad(index), doc._key);
        assertEqual(index, doc.value1);
        assertEqual(index % 113, doc.value2);
      });
      
      // full scan using secondary index and a filter
      result = db._query('FOR doc IN @@c FILTER doc.value1 > 1 && doc.value1 <= 4995 SORT doc.value1 DESC RETURN doc', { '@c': cn }, opts).toArray();
      assertEqual(4994, result.length);
      result.forEach((doc, index) => {
        index = 4995 - index;
        assertEqual("test" + pad(index), doc._key);
        assertEqual(index, doc.value1);
        assertEqual(index % 113, doc.value2);
      });
    },
    
    testAqlForwardIterationInsert1: function () {
      const opts = { intermediateCommitCount: 127 };

      // full scan 
      let result = db._query('FOR doc IN @@c SORT doc.value3 ASC INSERT { _key: CONCAT("y", doc.value3) } INTO @@c RETURN doc', { '@c': cn }, opts).toArray();
      assertEqual(5000 + 5000, db[cn].count());
      assertEqual(5000, result.length);
      result.forEach((doc, index) => {
        assertEqual("test" + pad(index), doc._key);
        assertEqual(index, doc.value1);
        assertEqual(index % 113, doc.value2);
      });
    },
      
    testAqlForwardIterationInsert2: function () {
      const opts = { intermediateCommitCount: 127 };

      // full scan over a filtered range
      let result = db._query('FOR doc IN @@c FILTER doc.value3 > 1 && doc.value3 <= 4995 SORT doc.value3 ASC INSERT { _key: CONCAT("y", doc.value3) } INTO @@c RETURN doc', { '@c': cn }, opts).toArray();
      
      assertEqual(5000 + 4994, db[cn].count());
      assertEqual(4994, result.length);
      result.forEach((doc, index) => {
        index = index + 2;
        assertEqual("test" + pad(index), doc._key);
        assertEqual(index, doc.value1);
        assertEqual(index % 113, doc.value2);
      });
    },
     
    testAqlForwardIterationInsert3: function () {
      const opts = { intermediateCommitCount: 127 };

      // full scan using primary index 
      let result = db._query('FOR doc IN @@c SORT doc._key ASC INSERT { _key: CONCAT("y", doc._key) } INTO @@c RETURN doc._key', { '@c': cn }, opts).toArray();
      assertEqual(5000 + 5000, db[cn].count());
      assertEqual(5000, result.length);
      result.forEach((value, index) => {
        assertEqual("test" + pad(index), value);
      });
    },
      
    testAqlForwardIterationInsert4: function () {
      const opts = { intermediateCommitCount: 127 };

      // full scan using primary index and a filter
      let result = db._query('FOR doc IN @@c FILTER doc._key > "test0001" && doc._key <= "test4995" SORT doc._key ASC INSERT { _key: CONCAT("y", doc._key) } INTO @@c RETURN doc._key', { '@c': cn }, opts).toArray();
      assertEqual(5000 + 4994, db[cn].count());
      assertEqual(4994, result.length);
      result.forEach((value, index) => {
        index = index + 2;
        assertEqual("test" + pad(index), value);
      });
    },

    testAqlForwardIterationInsert5: function () {
      const opts = { intermediateCommitCount: 127 };

      // full scan using secondary index 
      let result = db._query('FOR doc IN @@c SORT doc.value1 ASC INSERT { _key: CONCAT("y", doc.value1) } INTO @@c RETURN doc', { '@c': cn }, opts).toArray();
      assertEqual(5000 + 5000, db[cn].count());
      assertEqual(5000, result.length);
      result.forEach((doc, index) => {
        assertEqual("test" + pad(index), doc._key);
        assertEqual(index, doc.value1);
        assertEqual(index % 113, doc.value2);
      });
    },
      
    testAqlForwardIterationInsert6: function () {
      const opts = { intermediateCommitCount: 127 };

      // full scan using secondary index and a filter
      let result = db._query('FOR doc IN @@c FILTER doc.value1 > 1 && doc.value1 <= 4995 SORT doc.value1 ASC INSERT { _key: CONCAT("y", doc.value1) } INTO @@c RETURN doc', { '@c': cn }, opts).toArray();
      assertEqual(5000 + 4994, db[cn].count());
      assertEqual(4994, result.length);
      result.forEach((doc, index) => {
        index = index + 2;
        assertEqual("test" + pad(index), doc._key);
        assertEqual(index, doc.value1);
        assertEqual(index % 113, doc.value2);
      });
    },

  };
}

function TransactionIteratorSuite() {
  'use strict';

  let prepareTransaction = function(opts) {
    const trxOpts = {
      collections: {
        write: [cn]
      },
    };
    const trx = internal.db._createTransaction(Object.assign(trxOpts, opts));
    const tc = trx.collection(cn);
    let docs = [];
    for (let i = 0; i < 5000; ++i) {
      docs.push({ _key: "test" + pad(i), value1: i, value2: i % 113, value3: i });
    }
    tc.insert(docs);
    return trx;
  };

  let permute = function(test) {
    [ {}, {intermediateCommitCount: 127} ].forEach((trxOpts) => {
      [ {}, {intermediateCommitCount: 111} ].forEach((opts) => {
        let trx = prepareTransaction(trxOpts);

        try {
          test(trx, opts);
        } finally {
          trx.abort();
        }
      });
    });
  };
      
  return {
    setUp: function () {
      db._drop(cn);
      let c = db._create(cn, { numberOfShards: 3 });
      c.ensureIndex({ type: "persistent", fields: ["value1"] });
      c.ensureIndex({ type: "persistent", fields: ["value2"] });
    },

    tearDown: function () {
      db._drop(cn);
    },
    
    testTrxForwardIterationReadOnly: function () {
      permute((trx, opts) => {
        // full scan 
        let result = trx.query('FOR doc IN @@c SORT doc.value3 ASC RETURN doc', { '@c': cn }, opts).toArray();
        assertEqual(5000, result.length);
        result.forEach((doc, index) => {
          assertEqual("test" + pad(index), doc._key);
          assertEqual(index, doc.value1);
          assertEqual(index % 113, doc.value2);
        });
        
        // full scan over a filtered range
        result = trx.query('FOR doc IN @@c FILTER doc.value3 > 1 && doc.value3 <= 4995 SORT doc.value3 ASC RETURN doc', { '@c': cn }, opts).toArray();
        assertEqual(4994, result.length);
        result.forEach((doc, index) => {
          index = index + 2;
          assertEqual("test" + pad(index), doc._key);
          assertEqual(index, doc.value1);
          assertEqual(index % 113, doc.value2);
        });
       
        // full scan using primary index 
        result = trx.query('FOR doc IN @@c SORT doc._key ASC RETURN doc._key', { '@c': cn }, opts).toArray();
        assertEqual(5000, result.length);
        result.forEach((value, index) => {
          assertEqual("test" + pad(index), value);
        });
        
        // full scan using primary index and a filter
        result = trx.query('FOR doc IN @@c FILTER doc._key > "test0001" && doc._key <= "test4995" SORT doc._key ASC RETURN doc._key', { '@c': cn }, opts).toArray();
        assertEqual(4994, result.length);
        result.forEach((value, index) => {
          index = index + 2;
          assertEqual("test" + pad(index), value);
        });

        // full scan using secondary index 
        result = trx.query('FOR doc IN @@c SORT doc.value1 ASC RETURN doc', { '@c': cn }, opts).toArray();
        assertEqual(5000, result.length);
        result.forEach((doc, index) => {
          assertEqual("test" + pad(index), doc._key);
          assertEqual(index, doc.value1);
          assertEqual(index % 113, doc.value2);
        });
        
        // full scan using secondary index and a filter
        result = trx.query('FOR doc IN @@c FILTER doc.value1 > 1 && doc.value1 <= 4995 SORT doc.value1 ASC RETURN doc', { '@c': cn }, opts).toArray();
        assertEqual(4994, result.length);
        result.forEach((doc, index) => {
          index = index + 2;
          assertEqual("test" + pad(index), doc._key);
          assertEqual(index, doc.value1);
          assertEqual(index % 113, doc.value2);
        });
      });
    },

    testTrxReverseIterationReadOnly: function () {
      permute((trx, opts) => {
        // full scan 
        let result = trx.query('FOR doc IN @@c SORT doc.value3 DESC RETURN doc', { '@c': cn }, opts).toArray();
        assertEqual(5000, result.length);
        result.forEach((doc, index) => {
          index = 5000 - index - 1;
          assertEqual("test" + pad(index), doc._key);
          assertEqual(index, doc.value1);
          assertEqual(index % 113, doc.value2);
        });
        
        // full scan over a filtered range
        result = trx.query('FOR doc IN @@c FILTER doc.value3 > 1 && doc.value3 <= 4995 SORT doc.value3 DESC RETURN doc', { '@c': cn }, opts).toArray();
        assertEqual(4994, result.length);
        result.forEach((doc, index) => {
          index = 4995 - index;
          assertEqual("test" + pad(index), doc._key);
          assertEqual(index, doc.value1);
          assertEqual(index % 113, doc.value2);
        });
       
        // full scan using primary index 
        result = trx.query('FOR doc IN @@c SORT doc._key DESC RETURN doc._key', { '@c': cn }, opts).toArray();
        assertEqual(5000, result.length);
        result.forEach((value, index) => {
          index = 5000 - index - 1;
          assertEqual("test" + pad(index), value);
        });
        
        // full scan using primary index and a filter
        result = trx.query('FOR doc IN @@c FILTER doc._key > "test0001" && doc._key <= "test4995" SORT doc._key DESC RETURN doc._key', { '@c': cn }, opts).toArray();
        assertEqual(4994, result.length);
        result.forEach((value, index) => {
          index = 4995 - index;
          assertEqual("test" + pad(index), value);
        });

        // full scan using secondary index 
        result = trx.query('FOR doc IN @@c SORT doc.value1 DESC RETURN doc', { '@c': cn }, opts).toArray();
        assertEqual(5000, result.length);
        result.forEach((doc, index) => {
          index = 5000 - index - 1;
          assertEqual("test" + pad(index), doc._key);
          assertEqual(index, doc.value1);
          assertEqual(index % 113, doc.value2);
        });
        
        // full scan using secondary index and a filter
        result = trx.query('FOR doc IN @@c FILTER doc.value1 > 1 && doc.value1 <= 4995 SORT doc.value1 DESC RETURN doc', { '@c': cn }, opts).toArray();
        assertEqual(4994, result.length);
        result.forEach((doc, index) => {
          index = 4995 - index;
          assertEqual("test" + pad(index), doc._key);
          assertEqual(index, doc.value1);
          assertEqual(index % 113, doc.value2);
        });
      });
    },
    
    testTrxForwardIterationInsert1: function () {
      permute((trx, opts) => {
        // full scan 
        const tc = trx.collection(cn);
        let result = trx.query('FOR doc IN @@c SORT doc.value3 ASC INSERT { _key: CONCAT("y", doc.value3) } INTO @@c RETURN doc', { '@c': cn }, opts).toArray();
        assertEqual(5000 + 5000, tc.count());
        assertEqual(5000, result.length);
        result.forEach((doc, index) => {
          assertEqual("test" + pad(index), doc._key);
          assertEqual(index, doc.value1);
          assertEqual(index % 113, doc.value2);
        });
      });
    },
      
    testTrxForwardIterationInsert2: function () {
      permute((trx, opts) => {
        // full scan over a filtered range
        const tc = trx.collection(cn);
        let result = trx.query('FOR doc IN @@c FILTER doc.value3 > 1 && doc.value3 <= 4995 SORT doc.value3 ASC INSERT { _key: CONCAT("y", doc.value3) } INTO @@c RETURN doc', { '@c': cn }, opts).toArray();
        
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
     
    testTrxForwardIterationInsert3: function () {
      permute((trx, opts) => {
        const tc = trx.collection(cn);
        // full scan using primary index 
        let result = trx.query('FOR doc IN @@c SORT doc._key ASC INSERT { _key: CONCAT("y", doc._key) } INTO @@c RETURN doc._key', { '@c': cn }, opts).toArray();
        assertEqual(5000 + 5000, tc.count());
        assertEqual(5000, result.length);
        result.forEach((value, index) => {
          assertEqual("test" + pad(index), value);
        });
      });
    },
    
    testTrxForwardIterationInsert3Before: function () {
      permute((trx, opts) => {
        const tc = trx.collection(cn);
        // full scan using primary index 
        let result = trx.query('FOR doc IN @@c SORT doc._key ASC INSERT { _key: CONCAT("a", doc._key) } INTO @@c RETURN doc._key', { '@c': cn }, opts).toArray();
        assertEqual(5000 + 5000, tc.count());
        assertEqual(5000, result.length);
        result.forEach((value, index) => {
          assertEqual("test" + pad(index), value);
        });
      });
    },
      
    testTrxForwardIterationInsert4: function () {
      permute((trx, opts) => {
        const tc = trx.collection(cn);
        // full scan using primary index and a filter
        let result = trx.query('FOR doc IN @@c FILTER doc._key > "test0001" && doc._key <= "test4995" SORT doc._key ASC INSERT { _key: CONCAT("y", doc._key) } INTO @@c RETURN doc._key', { '@c': cn }, opts).toArray();
        assertEqual(5000 + 4994, tc.count());
        assertEqual(4994, result.length);
        result.forEach((value, index) => {
          index = index + 2;
          assertEqual("test" + pad(index), value);
        });
      });
    },
    
    testTrxForwardIterationInsert4Before: function () {
      permute((trx, opts) => {
        const tc = trx.collection(cn);
        // full scan using primary index and a filter
        let result = trx.query('FOR doc IN @@c FILTER doc._key > "test0001" && doc._key <= "test4995" SORT doc._key ASC INSERT { _key: CONCAT("a", doc._key) } INTO @@c RETURN doc._key', { '@c': cn }, opts).toArray();
        assertEqual(5000 + 4994, tc.count());
        assertEqual(4994, result.length);
        result.forEach((value, index) => {
          index = index + 2;
          assertEqual("test" + pad(index), value);
        });
      });
    },

    testTrxForwardIterationInsert5: function () {
      permute((trx, opts) => {
        const tc = trx.collection(cn);
        // full scan using secondary index 
        let result = trx.query('FOR doc IN @@c SORT doc.value1 ASC INSERT { _key: CONCAT("y", doc.value1) } INTO @@c RETURN doc', { '@c': cn }, opts).toArray();
        assertEqual(5000 + 5000, tc.count());
        assertEqual(5000, result.length);
        result.forEach((doc, index) => {
          assertEqual("test" + pad(index), doc._key);
          assertEqual(index, doc.value1);
          assertEqual(index % 113, doc.value2);
        });
      });
    },
      
    testTrxForwardIterationInsert6: function () {
      permute((trx, opts) => {
        const tc = trx.collection(cn);
        // full scan using secondary index and a filter
        let result = trx.query('FOR doc IN @@c FILTER doc.value1 > 1 && doc.value1 <= 4995 SORT doc.value1 ASC INSERT { _key: CONCAT("y", doc.value1) } INTO @@c RETURN doc', { '@c': cn }, opts).toArray();
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

  };
}    

jsunity.run(StandaloneAqlIteratorSuite);
jsunity.run(TransactionIteratorSuite);

return jsunity.done();
