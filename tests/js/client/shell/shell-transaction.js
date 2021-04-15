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
// / @author Simon Grätzer
// //////////////////////////////////////////////////////////////////////////////

// tests for streaming transactions

var jsunity = require('jsunity');
var internal = require('internal');
var arangodb = require('@arangodb');
var db = arangodb.db;
var analyzers = require("@arangodb/analyzers");
let ArangoTransaction = require('@arangodb/arango-transaction').ArangoTransaction;
const isCluster = internal.isCluster();

var compareStringIds = function (l, r) {
  'use strict';
  var i;
  if (l.length !== r.length) {
    return l.length - r.length < 0 ? -1 : 1;
  }

  // length is equal
  for (i = 0; i < l.length; ++i) {
    if (l[i] !== r[i]) {
      return l[i] < r[i] ? -1 : 1;
    }
  }

  return 0;
};

var sortedKeys = function (col) {
  'use strict';
  var keys = [ ];

  col.toArray().forEach(function (d) {
    keys.push(d._key);
  });

  keys.sort();
  return keys;
};

function transactionRevisionsSuite () {
  'use strict';
  var cn = 'UnitTestsTransaction';
  var c = null;

  return {

    setUpAll: function () {
      db._drop(cn);
      c = db._create(cn, {numberOfShards: 4});
    },

    setUp: function () {
      c.truncate({ compact: false });
    },

    tearDown: function () {
      c.truncate({ compact: false });
      internal.wait(0);
    },
    tearDownAll: function () {
      c.drop();
      c = null;
      internal.wait(0);
     },

    testInsertUniqueFailing: function () {
      c.insert({ _key: 'test', value: 1 });

      let trx = db._createTransaction({ 
        collections: { write: c.name()  }
      });
      try {
        let tc = trx.collection(c.name());
        tc.insert({ _key: 'test', value: 2 }); // should fail
        trx.commit(); // should not get here
        fail();
      } catch(err) {
        assertEqual(internal.errors.ERROR_ARANGO_UNIQUE_CONSTRAINT_VIOLATED.code, err.errorNum);
      } finally {
        trx.abort(); // otherwise drop hangs
      }
      
      assertEqual(1, c.count());
      assertEqual(1, c.toArray().length);
      assertEqual(1, c.document('test').value);
    },

    testInsertTransactionAbort: function () {
      c.insert({ _key: 'test', value: 1 });

      let trx = db._createTransaction({ 
        collections: { write: c.name()  }
      });
      try {
        let tc = trx.collection(c.name());
        tc.insert({ _key: 'test2', value: 2 });
        assertEqual(2, tc.count());
      } finally {
        trx.abort();
      }
      
      assertEqual(1, c.toArray().length);
      assertEqual(1, c.document('test').value);
    },

    testRemoveTransactionAbort: function () {
      c.insert({ _key: 'test', value: 1 });

      let trx = db._createTransaction({ 
        collections: { write: c.name()  }
      });

      try {
        let tc = trx.collection(c.name());
        tc.remove('test');
        assertEqual(0, tc.count());
      } catch(e) {
        fail("Transaction failed with: " + JSON.stringify(e));
      } finally {
        trx.abort();
      }

      assertEqual(1, c.toArray().length);
      assertEqual(1, c.document('test').value);
    },

    testRemoveInsertWithSameRev: function () {
      var doc = c.insert({ _key: 'test', value: 1 });

      let trx = db._createTransaction({ 
        collections: { write: c.name()  }
      });
      try {
        let tc = trx.collection(c.name());
        tc.remove('test');
        tc.insert({ _key: 'test', _rev: doc._rev, value: 2 }, { isRestore: true });
      } catch(e) {
        fail("Transaction failed with: " + JSON.stringify(e));
      } finally {
        trx.commit();
      }

      assertEqual(1, c.toArray().length);
      assertEqual(2, c.document('test').value);
    },

    testUpdateWithSameRevTransaction: function () {
      var doc = c.insert({ _key: 'test', value: 1 });

      let trx = db._createTransaction({ 
        collections: { write: c.name()  }
      });
      try {
        let tc = trx.collection(c.name());
        tc.update('test', { _key: 'test', _rev: doc._rev, value: 2 }, { isRestore: true });
      } catch(e) {
        fail("Transaction failed with: " + JSON.stringify(e));
      } finally {
        trx.commit();
      }

      assertEqual(1, c.toArray().length);
      assertEqual(2, c.document('test').value);
    },

    testUpdateAbortWithSameRev: function () {
      var doc = c.insert({ _key: 'test', value: 1 });

      let trx = db._createTransaction({ 
        collections: { write: c.name()  }
      });
      try {
        let tc = trx.collection(c.name());
        tc.update('test', { _key: 'test', _rev: doc._rev, value: 2 }, { isRestore: true });
      } catch(err) {
        fail("Transaction failed with: " + JSON.stringify(err));
      } finally {
        trx.abort();
      }

      assertEqual(1, c.toArray().length);
      assertEqual(1, c.document('test').value);
    },

    testUpdateFailing: function () {
      c.insert({ _key: 'test', value: 1 });

      let trx = db._createTransaction({ 
        collections: { write: c.name()  }
      });
      try {
        let tc = trx.collection(c.name());
        tc.update('test', { _key: 'test', value: 2 });
      } catch(err) {
        fail("Transaction failed with: " + JSON.stringify(err));
      } finally {
        trx.abort();
      }

      assertEqual(1, c.toArray().length);
      assertEqual(1, c.document('test').value);
    },

    testUpdateAndInsertFailing: function () {
      c.insert({ _key: 'test', value: 1 });

      let trx = db._createTransaction({ 
        collections: { write: c.name()  }
      });
      try {
        let tc = trx.collection(c.name());
        tc.update('test', {value: 2 });
        tc.insert({ _key: 'test', value: 3 });
        fail();
      } catch(err) {
        assertEqual(internal.errors.ERROR_ARANGO_UNIQUE_CONSTRAINT_VIOLATED.code, err.errorNum);
      } finally {
        trx.abort();
      }

      assertEqual(1, c.toArray().length);
      assertEqual(1, c.document('test').value);
    },

    testRemoveAndInsert: function () {
      c.insert({ _key: 'test', value: 1 });

      let trx = db._createTransaction({ 
        collections: { write: c.name()  }
      });
      try {
        let tc = trx.collection(c.name());
        tc.remove('test');
        tc.insert({ _key: 'test', value: 2 });
      } catch(err) {
        fail("Transaction failed with: " + JSON.stringify(err));
      } finally {
        trx.commit();
      }

      assertEqual(1, c.toArray().length);
      assertEqual(2, c.document('test').value);
    },

    testRemoveAndInsertAbort: function () {
      c.insert({ _key: 'test', value: 1 });

      let trx = db._createTransaction({ 
        collections: { write: c.name()  }
      });
      try {
        let tc = trx.collection(c.name());
        tc.remove('test');
        tc.insert({ _key: 'test', value: 3 });
      } catch(err) {
        fail("Transaction failed with: " + JSON.stringify(err));
      } finally {
        trx.abort();
      }

      assertEqual(1, c.toArray().length);
      assertEqual(1, c.document('test').value);
    }

  };
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief test suite
// //////////////////////////////////////////////////////////////////////////////

function transactionInvocationSuite () {
  'use strict';
  const cn = "UnitTestsCollection";
  const cn2 = "UnitTestsCollection2";
  const cn4 = "UnitTestsCollection4";
  
  let assertInList = function(list, trx) {
    assertTrue(list.filter(function(data) { return data.id === trx._id; }).length > 0,
               "transaction " + trx._id + " is not contained in list of transactions " + JSON.stringify(list)); 
  };

  let assertNotInList = function(list, trx) {
    assertFalse(list.filter(function(data) { return data.id === trx._id; }).length > 0,
               "transaction " + trx._id + " is contained in list of transactions " + JSON.stringify(list)); 
  };

  return {

    // //////////////////////////////////////////////////////////////////////////////
    // / @brief set up
    // //////////////////////////////////////////////////////////////////////////////

    setUpAll: function () {
      db._create(cn);
      db._create(cn2, {numberOfShards: 2});
      db._create(cn4, {numberOfShards: 4});
    },

    tearDownAll: function () {
      db._drop(cn);
      db._drop(cn2);
      db._drop(cn4);
    },
    
    setUp: function () {
      db[cn].truncate({ compact: false });
      db[cn2].truncate({ compact: false });
      db[cn4].truncate({ compact: false });
    },

    tearDown: function () {
      db[cn].truncate({ compact: false });
      db[cn2].truncate({ compact: false });
      db[cn4].truncate({ compact: false });
    },
    
    // //////////////////////////////////////////////////////////////////////////////
    // / @brief test: invalid invocations of _createTransaction() function
    // //////////////////////////////////////////////////////////////////////////////

    testInvalidInvocations: function () {
      var tests = [
        undefined,
        null,
        true,
        false,
        { }, { },
        { }, { }, { },
        false, true,
        [ ],
        [ 'action' ],
        [ 'collections' ],
        [ 'collections', 'action' ],
        { },
        { collections: true },
        { action: true },
        { action: function () { } },
        { collections: true, action: true },
        { collections: { }, action: true },
        { collections: true, action: function () { } },
        { collections: { read: true }, action: function () { } },
        { collections: { }, lockTimeout: -1, action: function () { } },
        { collections: { }, lockTimeout: -30.0, action: function () { } },
        { collections: { }, lockTimeout: null, action: function () { } },
        { collections: { }, lockTimeout: true, action: function () { } },
        { collections: { }, lockTimeout: 'foo', action: function () { } },
        { collections: { }, lockTimeout: [ ], action: function () { } },
        { collections: { }, lockTimeout: { }, action: function () { } },
        { collections: { }, waitForSync: null, action: function () { } },
        { collections: { }, waitForSync: 0, action: function () { } },
        { collections: { }, waitForSync: 'foo', action: function () { } },
        { collections: { }, waitForSync: [ ], action: function () { } },
        { collections: { }, waitForSync: { }, action: function () { } }
      ];

      let localDebug = false;
      tests.forEach(function (test) {
        if (localDebug) {
          require('internal').print(test);
        }
        let trx;
        try {

          trx = db._createTransaction(test);

          if (localDebug) {
            require('internal').print('no exception failing');
          }
          fail();
        } catch (err) {
          var expected = internal.errors.ERROR_BAD_PARAMETER.code;
          if (test && test.hasOwnProperty('exErr')) {
            expected = test.exErr;
          }
          if (localDebug) {
            require('internal').print('exp: ' + expected + ' real: ' + err.errorNum);
          }
          assertEqual(expected, err.errorNum);
        } finally {
          if (trx) {
            try { trx.abort(); } catch (err) {}
          }
        }
      });
    },

    // //////////////////////////////////////////////////////////////////////////////
    // / @brief test: valid invocations of _createTransaction() function
    // //////////////////////////////////////////////////////////////////////////////

    testValidEmptyInvocations: function () {
      var result;

      var tests = [
        { collections: { },  },
        { collections: { read: [ ] } },
        { collections: { write: [ ] } },
        { collections: { read: [ ], write: [ ] } },
        { collections: { read: [ ], write: [ ] }, lockTimeout: 5.0 },
        { collections: { read: [ ], write: [ ] }, lockTimeout: 0.0 },
        { collections: { read: [ ], write: [ ] }, waitForSync: true },
        { collections: { read: [ ], write: [ ] }, waitForSync: false }
      ];

      tests.forEach(function (test) {
        let trx;
        try {
          trx = db._createTransaction(test);
        } catch(err) {
          fail("Transaction failed with: " + JSON.stringify(err));
        } finally {
          if (trx) {
            trx.abort();
          }
        }
      });
    },
    
    // //////////////////////////////////////////////////////////////////////////////
    // / @brief test: _transactions() function
    // //////////////////////////////////////////////////////////////////////////////

    testListTransactions: function () {
      let trx1, trx2, trx3;
      
      let obj = {
        collections: {
          read: [ cn4 ]
        }
      };
     
      try {
        // create a single trx
        trx1 = db._createTransaction(obj);
      
        let trx = db._transactions();
        assertInList(trx, trx1);
            
        trx1.commit();
        // trx is committed now - list should be empty

        trx = db._transactions();
        assertNotInList(trx, trx1);

        // create two more
        trx2 = db._createTransaction(obj);
      
        trx = db._transactions();
        assertInList(trx, trx2);
        assertNotInList(trx, trx1);
            
        trx3 = db._createTransaction(obj);
      
        trx = db._transactions();
        assertInList(trx, trx2);
        assertInList(trx, trx3);
        assertNotInList(trx, trx1);

        trx2.commit();
        
        trx = db._transactions();
        assertInList(trx, trx3);
        assertNotInList(trx, trx2);
        assertNotInList(trx, trx1);

        trx3.commit();
        
        trx = db._transactions();
        assertNotInList(trx, trx3);
        assertNotInList(trx, trx2);
        assertNotInList(trx, trx1);
      } finally {
        if (trx1 && trx1._id) {
          try { trx1.abort(); } catch (err) {}
        }
        if (trx2 && trx2._id) {
          try { trx2.abort(); } catch (err) {}
        }
        if (trx3 && trx3._id) {
          try { trx3.abort(); } catch (err) {}
        }
      }
    },
    
    // //////////////////////////////////////////////////////////////////////////////
    // / @brief test: _createTransaction() function
    // //////////////////////////////////////////////////////////////////////////////

    testcreateTransaction: function () {
      let values = [ "aaaaaaaaaaaaaaaaaaaaaaaa", "der-fuchs-der-fuchs", 99999999999999999999999, 1 ];

      values.forEach(function(data) {
        try {
          let trx = db._createTransaction(data);
          trx.status();
          fail();
        } catch (err) {
          assertTrue(err.errorNum === internal.errors.ERROR_BAD_PARAMETER.code ||
                     err.errorNum === internal.errors.ERROR_TRANSACTION_NOT_FOUND.code);
        }
      });
    },
    
    // //////////////////////////////////////////////////////////////////////////////
    // / @brief test: abort
    // //////////////////////////////////////////////////////////////////////////////

    testAbortTransaction: function () {
      let cleanup = [];
      
      let obj = {
        collections: {
          read: [ cn4 ]
        }
      };
     
      try {
        let trx1 = db._createTransaction(obj);
        cleanup.push(trx1);

        assertInList(db._transactions(), trx1);

        // abort using trx object
        let result = db._createTransaction(trx1).abort();
        assertEqual(trx1._id, result.id);
        assertEqual("aborted", result.status);
        
        assertNotInList(db._transactions(), trx1);
        
        let trx2 = db._createTransaction(obj);
        cleanup.push(trx2);

        assertInList(db._transactions(), trx2);

        // abort by id
        result = db._createTransaction(trx2._id).abort();
        assertEqual(trx2._id, result.id);
        assertEqual("aborted", result.status);
        
        assertNotInList(db._transactions(), trx1);
        assertNotInList(db._transactions(), trx2);

      } finally {
        cleanup.forEach(function(trx) {
          try { trx.abort(); } catch (err) {}
        });
      }
    },

    // //////////////////////////////////////////////////////////////////////////////
    // / @brief test: commit
    // //////////////////////////////////////////////////////////////////////////////

    testCommitTransaction: function () {
      let cleanup = [];
      
      let obj = {
        collections: {
          read: [ cn4 ]
        }
      };
     
      try {
        let trx1 = db._createTransaction(obj);
        cleanup.push(trx1);

        assertInList(db._transactions(), trx1);

        // commit using trx object
        let result = db._createTransaction(trx1).commit();
        assertEqual(trx1._id, result.id);
        assertEqual("committed", result.status);
        
        assertNotInList(db._transactions(), trx1);
        
        let trx2 = db._createTransaction(obj);
        cleanup.push(trx2);

        assertInList(db._transactions(), trx2);

        // commit by id
        result = db._createTransaction(trx2._id).commit();
        assertEqual(trx2._id, result.id);
        assertEqual("committed", result.status);
        
        assertNotInList(db._transactions(), trx1);
        assertNotInList(db._transactions(), trx2);
      } finally {
        cleanup.forEach(function(trx) {
          try { trx.abort(); } catch (err) {}
        });
      }
    },
    
    // //////////////////////////////////////////////////////////////////////////////
    // / @brief test: status
    // //////////////////////////////////////////////////////////////////////////////

    testStatusTransaction: function () {
      let cleanup = [];
      
      let obj = {
        collections: {
          read: [ cn4 ]
        }
      };
     
      try {
        let trx1 = db._createTransaction(obj);
        cleanup.push(trx1);

        let result = trx1.status();
        assertEqual(trx1._id, result.id);
        assertEqual("running", result.status);

        result = db._createTransaction(trx1._id).commit();
        assertEqual(trx1._id, result.id);
        assertEqual("committed", result.status);
        
        result = trx1.status();
        assertEqual(trx1._id, result.id);
        assertEqual("committed", result.status);
        
        let trx2 = db._createTransaction(obj);
        cleanup.push(trx2);

        result = trx2.status();
        assertEqual(trx2._id, result.id);
        assertEqual("running", result.status);

        result = db._createTransaction(trx2._id).abort();
        assertEqual(trx2._id, result.id);
        assertEqual("aborted", result.status);
        
        result = trx2.status();
        assertEqual(trx2._id, result.id);
        assertEqual("aborted", result.status);
      } finally {
        cleanup.forEach(function(trx) {
          try { trx.abort(); } catch (err) {}
        });
      }
    },

    // //////////////////////////////////////////////////////////////////////////////
    // / @brief test: abort write transactions
    // //////////////////////////////////////////////////////////////////////////////

    testAbortWriteTransactions: function () {
      let trx1, trx2, trx3;
      
      let obj = {
        collections: {
          write: [ cn2 ]
        }
      };
     
      try {
        trx1 = db._createTransaction(obj);
        trx2 = db._createTransaction(obj);
        trx3 = db._createTransaction(obj);
        
        let trx = db._transactions();
        // the following assertions are not safe, as transactions have
        // an idle timeout of 10 seconds, and we cannot guarantee any
        // runtime performance in our test environment
        // assertInList(trx, trx1);
        // assertInList(trx, trx2);
        // assertInList(trx, trx3);
        
        let result = arango.DELETE("/_api/transaction/write");
        assertEqual(result.code, 200);

        trx = db._transactions();
        assertNotInList(trx, trx1);
        assertNotInList(trx, trx2);
        assertNotInList(trx, trx3);
      } finally {
        if (trx1 && trx1._id) {
          try { trx1.abort(); } catch (err) {}
        }
        if (trx2 && trx2._id) {
          try { trx2.abort(); } catch (err) {}
        }
        if (trx3 && trx3._id) {
          try { trx3.abort(); } catch (err) {}
        }
      }
    },
    
    // //////////////////////////////////////////////////////////////////////////////
    // / @brief test: abort write transactions
    // //////////////////////////////////////////////////////////////////////////////

    testAbortWriteTransactionAQL: function () {
      let trx1;
      
      let obj = {
        collections: {
          write: [ cn2 ]
        }
      };
     
      try {
        trx1 = db._createTransaction(obj);
        let result = arango.POST_RAW("/_api/cursor", {
          query: "FOR i IN 1..10000000 INSERT {} INTO " + cn2
        }, { 
          "x-arango-trx-id" : trx1._id,
          "x-arango-async" : "store"
        });

        let jobId = result.headers["x-arango-async-id"];

        // wait until job has started...
        let tries = 0;
        while (++tries < 60) {
          require("internal").wait(0.5, false);
          result = arango.PUT_RAW("/_api/job/" + jobId, {});

          if (result.code === 204) {
            break;
          }
        }
        
        let trx = db._transactions();
        assertInList(trx, trx1);
       
        result = arango.DELETE("/_api/transaction/write");
        assertEqual(result.code, 200);

        // wait until job has been canceled
        tries = 0;
        while (++tries < 60) {
          result = arango.PUT_RAW("/_api/job/" + jobId, {});
        
          if (result.code === 410 || result.code === 404) {
            break;
          }
          require("internal").wait(0.5, false);

          // timing issues may occur when canceling transactions
          result = arango.DELETE("/_api/transaction/write");
          assertEqual(result.code, 200);
        }
        assertTrue(result.code === 410 || result.code === 404);
      } finally {
        if (trx1 && trx1._id) {
          try { trx1.abort(); } catch (err) {}
        }
      }
    },

  };
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief test suite
// //////////////////////////////////////////////////////////////////////////////

function transactionCollectionsSuite () {
  'use strict';
  var cn1 = 'UnitTestsTransaction1';
  var cn2 = 'UnitTestsTransaction2';

  var c1 = null;
  var c2 = null;

  return {

    // //////////////////////////////////////////////////////////////////////////////
    // / @brief set up
    // //////////////////////////////////////////////////////////////////////////////

    setUpAll: function () {
      db._drop(cn1);
      c1 = db._create(cn1, {numberOfShards: 4});

      db._drop(cn2);
      c2 = db._create(cn2, {numberOfShards: 4});
    },

    // //////////////////////////////////////////////////////////////////////////////
    // / @brief tear down
    // //////////////////////////////////////////////////////////////////////////////

    tearDownAll: function () {
      c1.drop();
      c2.drop();
    },

    // //////////////////////////////////////////////////////////////////////////////
    // / @brief set up
    // //////////////////////////////////////////////////////////////////////////////

    setUp: function () {
      c1.truncate({ compact: false });
      c2.truncate({ compact: false });
    },

    // //////////////////////////////////////////////////////////////////////////////
    // / @brief tear down
    // //////////////////////////////////////////////////////////////////////////////

    tearDown: function () {
      c1.truncate({ compact: false });
      c2.truncate({ compact: false });
      internal.wait(0);
    },

    // //////////////////////////////////////////////////////////////////////////////
    // / @brief test: trx using non-existing collections
    // //////////////////////////////////////////////////////////////////////////////

    testNonExistingCollectionsArray: function () {
      let obj = {
        collections: {
          read: [ 'UnitTestsTransactionNonExisting' ]
        }
      };

      let trx;
      try {
        trx = db._createTransaction(obj);
      } catch(err) {
        assertEqual(arangodb.errors.ERROR_ARANGO_DATA_SOURCE_NOT_FOUND.code, err.errorNum);
      } finally {
        if (trx) {
          trx.abort();
        }
      }
    },

    // //////////////////////////////////////////////////////////////////////////////
    // / @brief test: trx using non-existing collections
    // //////////////////////////////////////////////////////////////////////////////

    testNonExistingCollectionsString: function () {
      let obj = {
        collections: {
          read: 'UnitTestsTransactionNonExisting'
        }
      };

      let trx;
      try {
        trx = db._createTransaction(obj);
      } catch(err) {
        assertEqual(arangodb.errors.ERROR_ARANGO_DATA_SOURCE_NOT_FOUND.code, err.errorNum);
      } finally {
        if (trx) {
          trx.abort();
        }
      }
    },

    // //////////////////////////////////////////////////////////////////////////////
    // / @brief test: trx using non-declared collections
    // //////////////////////////////////////////////////////////////////////////////

    testNonDeclaredCollections1: function () {
      let obj = {
        collections: {}
      };

      let trx;
      try {
        trx = db._createTransaction(obj);
        let tc = trx.collection(c1.name());
        tc.save({ _key: 'foo' });
        fail();
      } catch(err) {
        assertEqual(arangodb.errors.ERROR_TRANSACTION_UNREGISTERED_COLLECTION.code, err.errorNum);
      } finally {
        if (trx) {
          trx.abort();
        }
      }
    },

    // //////////////////////////////////////////////////////////////////////////////
    // / @brief test: trx using non-declared collections
    // //////////////////////////////////////////////////////////////////////////////

    testNonDeclaredCollections2: function () {
      let obj = {
        collections: {
          write: [ cn2 ]
        }
      };

      let trx;
      try {
        trx = db._createTransaction(obj);
        let tc = trx.collection(c1.name());
        tc.save({ _key: 'foo' });
        fail();
      } catch(err) {
        assertEqual(arangodb.errors.ERROR_TRANSACTION_UNREGISTERED_COLLECTION.code, err.errorNum);
      } finally {
        if (trx) {
          trx.abort();
        }
      }
    },

    // //////////////////////////////////////////////////////////////////////////////
    // / @brief test: trx using wrong mode
    // //////////////////////////////////////////////////////////////////////////////

    testNonDeclaredCollections3: function () {
      let obj = {
        collections: {
          read: [ cn1 ]
        }
      };

      let trx;
      try {
        trx = db._createTransaction(obj);
        let tc = trx.collection(c1.name());
        tc.save({ _key: 'foo' });
        fail();
      } catch(err) {
        assertEqual(arangodb.errors.ERROR_TRANSACTION_UNREGISTERED_COLLECTION.code, err.errorNum);
      } finally {
        if (trx) {
          trx.abort();
        }
      }
    },

    // //////////////////////////////////////////////////////////////////////////////
    // / @brief test: trx using no collections
    // //////////////////////////////////////////////////////////////////////////////

    testNoCollections: function () {
      let obj = {
        collections: {}
      };

      let trx;
      try {
        trx = db._createTransaction(obj);
      } catch(err) {
        fail("Transaction failed with: " + JSON.stringify(err));
      } finally {
        if (trx) {
          trx.commit();
        }
      }
    },

    // //////////////////////////////////////////////////////////////////////////////
    // / @brief test: trx using no collections
    // //////////////////////////////////////////////////////////////////////////////

    testNoCollectionsAql: function () {
      let result;

      let obj = {
        collections: {
        }
      };

      let trx;
      try {
        trx = db._createTransaction(obj);
        result = trx.query('FOR i IN [ 1, 2, 3 ] RETURN i').toArray();
      } catch(err) {
        fail("Transaction failed with: " + JSON.stringify(err));
      } finally {
        if (trx) {
          trx.commit();
        }
      }

      assertEqual([ 1, 2, 3 ], result);
    },

    // //////////////////////////////////////////////////////////////////////////////
    // / @brief test: trx using valid collections
    // //////////////////////////////////////////////////////////////////////////////

    testValidCollectionsArray: function () {
      let obj = {
        collections: {
          write: [ cn1 ]
        }
      };

      let trx;
      try {
        trx = db._createTransaction(obj);
        let tc = trx.collection(c1.name());
        tc.save({ _key: 'foo' });
      } catch(err) {
        fail("Transaction failed with: " + JSON.stringify(err));
      } finally {
        if (trx) {
          trx.commit();
        }
      }

      assertEqual(c1.count(), 1);
    },
    // //////////////////////////////////////////////////////////////////////////////
    // / @brief test: trx using valid collections
    // //////////////////////////////////////////////////////////////////////////////

    testValidCollectionsString: function () {
      let obj = {
        collections: {
          write: cn1
        }
      };
      
      let trx;
      try {
        trx = db._createTransaction(obj);
        let tc = trx.collection(c1.name());
        tc.save({ _key: 'foo' });
      } catch(err) {
        fail("Transaction failed with: " + JSON.stringify(err));
      } finally {
        if (trx) {
          trx.commit();
        }
      }

      assertEqual(c1.count(), 1);
    },

    // //////////////////////////////////////////////////////////////////////////////
    // / @brief test: trx using valid collections
    // //////////////////////////////////////////////////////////////////////////////

    testValidMultipleCollectionsArray: function () {
      let obj = {
        collections: {
          write: [ cn1, cn2 ]
        }
      };

      let trx;
      try {
        trx = db._createTransaction(obj);
        let tc1 = trx.collection(c1.name());
        let tc2 = trx.collection(c2.name());
        tc1.save({ _key: 'foo' });
        tc2.save({ _key: 'foo' });
      } catch(err) {
        fail("Transaction failed with: " + JSON.stringify(err));
      } finally {
        if (trx) {
          trx.commit();
        }
      }

      assertEqual(c1.count(), 1);
      assertEqual(c2.count(), 1);
    },

    // //////////////////////////////////////////////////////////////////////////////
    // / @brief test: trx using valid collections
    // //////////////////////////////////////////////////////////////////////////////

    testValidMultipleCollectionsString: function () {
      c2.save({ _key: 'foo' });

      let obj = {
        collections: {
          write: cn1,
          read: cn2
        }
      };

      let trx;
      try {
        trx = db._createTransaction(obj);
        let tc1 = trx.collection(c1.name());
        let tc2 = trx.collection(c2.name());
        tc1.save({ _key: 'foo' });
        tc2.document('foo');
      } catch(err) {
        fail("Transaction failed with: " + JSON.stringify(err));
      } finally {
        if (trx) {
          trx.commit();
        }
      }

      assertEqual(c1.count(), 1);
      assertEqual(c2.count(), 1);
    },

    // //////////////////////////////////////////////////////////////////////////////
    // / @brief test: trx using valid collections
    // //////////////////////////////////////////////////////////////////////////////

    testRedeclareCollectionArray: function () {
      let obj = {
        collections: {
          read: [ cn1 ],
          write: [ cn1 ]
        }
      };

      let trx;
      try {
        trx = db._createTransaction(obj);
        let tc1 = trx.collection(c1.name());
        tc1.save({ _key: 'foo' });
      } catch(err) {
        fail("Transaction failed with: " + JSON.stringify(err));
      } finally {
        if (trx) {
          trx.commit();
        }
      }

      assertEqual(c1.count(), 1);
      assertEqual(c2.count(), 0);
    },

    // //////////////////////////////////////////////////////////////////////////////
    // / @brief test: trx using valid collections
    // //////////////////////////////////////////////////////////////////////////////

    testRedeclareCollectionString: function () {
      let obj = {
        collections: {
          read: cn1,
          write: cn1
        }
      };

      let trx;
      try {
        trx = db._createTransaction(obj);
        let tc1 = trx.collection(c1.name());
        tc1.save({ _key: 'foo' });
      } catch(err) {
        fail("Transaction failed with: " + JSON.stringify(err));
      } finally {
        if (trx) {
          trx.commit();
        }
      }

      assertEqual(c1.count(), 1);
      assertEqual(c2.count(), 0);
    },

    // //////////////////////////////////////////////////////////////////////////////
    // / @brief test: trx using valid collections
    // //////////////////////////////////////////////////////////////////////////////

    testReadWriteCollections: function () {
      let obj = {
        collections: {
          read: [ cn1 ],
          write: [ cn2 ]
        }
      };

      let trx;
      try {
        trx = db._createTransaction(obj);
        let tc2 = trx.collection(c2.name());
        tc2.save({ _key: 'foo' });
      } catch(err) {
        fail("Transaction failed with: " + JSON.stringify(err));
      } finally {
        if (trx) {
          trx.commit();
        }
      }

      assertEqual(c1.count(), 0);
      assertEqual(c2.count(), 1);
    },

    // //////////////////////////////////////////////////////////////////////////////
    // / @brief test: trx using waitForSync
    // //////////////////////////////////////////////////////////////////////////////

    testWaitForSyncTrue: function () {
      let obj = {
        collections: {
          write: [ cn1 ]
        },
        waitForSync: true
      };

      let trx;
      try {
        trx = db._createTransaction(obj);
        let tc1 = trx.collection(c1.name());
        tc1.save({ _key: 'foo' });
      } catch(err) {
        fail("Transaction failed with: " + JSON.stringify(err));
      } finally {
        if (trx) {
          trx.commit();
        }
      }

      assertEqual(c1.count(), 1);
      assertEqual(c2.count(), 0);
    },

    // //////////////////////////////////////////////////////////////////////////////
    // / @brief test: trx using waitForSync
    // //////////////////////////////////////////////////////////////////////////////

    testWaitForSyncFalse: function () {
      let obj = {
        collections: {
          write: [ cn1 ]
        },
        waitForSync: false
      };

      let trx;
      try {
        trx = db._createTransaction(obj);
        let tc1 = trx.collection(c1.name());
        tc1.save({ _key: 'foo' });
      } catch(err) {
        fail("Transaction failed with: " + JSON.stringify(err));
      } finally {
        if (trx) {
          trx.commit();
        }
      }

      assertEqual(c1.count(), 1);
      assertEqual(c2.count(), 0);
    },

    // //////////////////////////////////////////////////////////////////////////////
    // / @brief test: trx with embedded AQL
    // //////////////////////////////////////////////////////////////////////////////

    testAqlRead: function () {
      for (let i = 0; i < 10; ++i) {
        c1.save({ _key: 'test' + i });
      }

      let obj = {
        collections: {
          read: [ cn1 ]
        }
      };

      let trx;
      try {
        trx = db._createTransaction(obj);

        let docs = trx.query('FOR i IN @@cn1 RETURN i', { '@cn1': cn1 }).toArray();
        assertEqual(10, docs.length);

      } catch(err) {
        fail("Transaction failed with: " + JSON.stringify(err));
      } finally {
        if (trx) {
          trx.commit();
        }
      }
      
    },

    // //////////////////////////////////////////////////////////////////////////////
    // / @brief test: trx with embedded AQL
    // //////////////////////////////////////////////////////////////////////////////

    testAqlReadMulti: function () {
      var i = 0;
      for (i = 0; i < 10; ++i) {
        c1.save({ _key: 'test' + i });
        c2.save({ _key: 'test' + i });
      }

      let obj = {
        collections: {
          read: [ cn1, cn2 ]
        }
      };
      
      let trx;
      try {
        trx = db._createTransaction(obj);

        let docs = trx.query('FOR i IN @@cn1 FOR j IN @@cn2 RETURN i', { '@cn1': cn1, '@cn2': cn2 }).toArray();
        assertEqual(100, docs.length);

      } catch(err) {
        fail("Transaction failed with: " + JSON.stringify(err));
      } finally {
        if (trx) {
          trx.commit();
        }
      }
    },

    // //////////////////////////////////////////////////////////////////////////////
    // / @brief test: trx with embedded AQL
    // //////////////////////////////////////////////////////////////////////////////

    testAqlReadMultiUndeclared: function () {
      var i = 0;
      for (i = 0; i < 10; ++i) {
        c1.save({ _key: 'test' + i });
        c2.save({ _key: 'test' + i });
      }

      let obj = {
        collections: {
          // intentionally empty
        }
      };

      let trx;
      try {
        trx = db._createTransaction(obj);

        let docs = trx.query('FOR i IN @@cn1 FOR j IN @@cn2 RETURN i', { '@cn1': cn1, '@cn2': cn2 }).toArray();
        assertEqual(100, docs.length);

      } catch(err) {
        fail("Transaction failed with: " + JSON.stringify(err));
      } finally {
        if (trx) {
          trx.commit();
        }
      }
    },

    // //////////////////////////////////////////////////////////////////////////////
    // / @brief test: trx with embedded AQL
    // //////////////////////////////////////////////////////////////////////////////

    testAqlWrite: function () {
      for (let i = 0; i < 10; ++i) {
        c1.save({ _key: 'test' + i });
      }

      let obj = {
        collections: {
          write: [ cn1 ]
        }
      };

      let trx;
      try {
        trx = db._createTransaction(obj);
        let tc1 = trx.collection(c1.name());

        assertEqual(10, tc1.count());
        var ops = trx.query('FOR i IN @@cn1 REMOVE i._key IN @@cn1', { '@cn1': cn1 }).getExtra().stats;
        assertEqual(10, ops.writesExecuted);
        assertEqual(0, tc1.count());

      } catch(err) {
        fail("Transaction failed with: " + JSON.stringify(err));
      } finally {
        if (trx) {
          trx.commit();
        }
      }

      assertEqual(0, c1.count());
    },

    // //////////////////////////////////////////////////////////////////////////////
    // / @brief test: trx with embedded AQL
    // //////////////////////////////////////////////////////////////////////////////

    testAqlReadWrite: function () {
      for (let i = 0; i < 10; ++i) {
        c1.save({ _key: 'test' + i });
        c2.save({ _key: 'test' + i });
      }

      let obj = {
        collections: {
          read: [ cn1 ],
          write: [ cn2 ]
        }
      };

      let trx;
      try {
        trx = db._createTransaction(obj);
        let tc1 = trx.collection(c1.name());
        let tc2 = trx.collection(c2.name());
        assertEqual(10, tc1.count());
        assertEqual(10, tc2.count());

        var ops = trx.query('FOR i IN @@cn1 REMOVE i._key IN @@cn2', { '@cn1': cn1, '@cn2': cn2 }).getExtra().stats;
        assertEqual(10, ops.writesExecuted);
        assertEqual(10, tc1.count());
        assertEqual(0, tc2.count());

      } catch(err) {
        fail("Transaction failed with: " + JSON.stringify(err));
      } finally {
        if (trx) {
          trx.commit();
        }
      }

      assertEqual(10, c1.count());
      assertEqual(0, c2.count());
    },

    // //////////////////////////////////////////////////////////////////////////////
    // / @brief test: trx with embedded AQL
    // //////////////////////////////////////////////////////////////////////////////

    testAqlWriteUndeclared: function () {
      for (let i = 0; i < 10; ++i) {
        c1.save({ _key: 'test' + i });
        c2.save({ _key: 'test' + i });
      }

      let obj = {
        collections: {
          read: [ cn1 ]
        }
      };

      let trx;
      try {
        trx = db._createTransaction(obj);
        let tc1 = trx.collection(c1.name());
        let tc2 = trx.collection(c2.name());
        
        try {
          trx.query('FOR i IN @@cn1 REMOVE i._key IN @@cn2', { '@cn1': cn1, '@cn2': cn2 });
          fail();
        } catch (err) {
          assertEqual(arangodb.errors.ERROR_TRANSACTION_UNREGISTERED_COLLECTION.code, err.errorNum);
        }

        assertEqual(10, tc1.count());
      } catch(err) {
        fail("Transaction failed with: " + JSON.stringify(err));
      } finally {
        if (trx) {
          trx.commit();
        }
      }

      assertEqual(10, c1.count());
      assertEqual(10, c2.count());
    },

    // //////////////////////////////////////////////////////////////////////////////
    // / @brief test: trx with embedded AQL
    // //////////////////////////////////////////////////////////////////////////////

    testAqlMultiWrite: function () {
      for (let i = 0; i < 10; ++i) {
        c1.save({ _key: 'test' + i });
        c2.save({ _key: 'test' + i });
      }

      let obj = {
        collections: {
          write: [ cn1, cn2 ]
        }
      };

      let trx;
      try {
        trx = db._createTransaction(obj);
        let tc1 = trx.collection(c1.name());
        let tc2 = trx.collection(c2.name());
        
        var ops;
        ops = trx.query('FOR i IN @@cn1 REMOVE i._key IN @@cn1', { '@cn1': cn1 }).getExtra().stats;
        assertEqual(10, ops.writesExecuted);
        assertEqual(0, tc1.count());

        ops = trx.query('FOR i IN @@cn2 REMOVE i._key IN @@cn2', { '@cn2': cn2 }).getExtra().stats;
        assertEqual(10, ops.writesExecuted);
        assertEqual(0, tc2.count());

      } catch(err) {
        fail("Transaction failed with: " + JSON.stringify(err));
      } finally {
        if (trx) {
          trx.commit();
        }
      }

      assertEqual(0, c1.count());
      assertEqual(0, c2.count());
    },
    // //////////////////////////////////////////////////////////////////////////////
    // / @brief test: trx with analyzers usage
    // //////////////////////////////////////////////////////////////////////////////
    testAnalyzersRevisionReadSubstitutedAnalyzer: function () {
      if (!isCluster) {
        return; // we don`t have revisions for single server. Nothing to check
      }
      let analyzerName = "my_analyzer";
      let obj = {
        collections: {
          read: [ cn1 ]
        }
      };
      let trx;
      try { analyzers.remove(analyzerName, true); } catch(e) {}
      analyzers.save(analyzerName,"identity", {});
      let col = db._collection(cn1);
      let test_doc = col.save({"test_field":"some"});
      try {
        trx = db._createTransaction(obj);
        // recreating analyzer with same name but different properties
        analyzers.remove(analyzerName, true);
        analyzers.save(analyzerName,"ngram", {min:2, max:2, preserveOriginal:true}); 
        let res = trx.query("FOR d IN @@cn1 FILTER TOKENS(d.test_field, @an)[0] == 'some' RETURN d",
                   { '@cn1': cn1, 'an':analyzerName });  // query should fail due to substituted analyzer
        fail();      
      } catch(err) {
        assertEqual(require("internal").errors.ERROR_BAD_PARAMETER.code, err.errorNum);
      } finally {
        if (trx) {
          trx.commit();
        }
        col.remove(test_doc);
      }
    },
    testAnalyzersRevisionRead: function () {
      let analyzerName = "my_analyzer";
      let obj = {
        collections: {
          read: [ cn1 ]
        }
      };
      let trx;
      try { analyzers.remove(analyzerName, true); } catch(e) {}
      analyzers.save(analyzerName,"identity", {});
      let col = db._collection(cn1);
      let test_doc = col.save({"test_field":"some"});
      try {
        trx = db._createTransaction(obj);
        let res = trx.query("FOR d IN @@cn1 FILTER TOKENS(d.test_field, @an)[0] == 'some' RETURN d",
                   { '@cn1': cn1, 'an':analyzerName });  
        assertEqual(1, res.toArray().length);
      } finally {
        if (trx) {
          trx.commit();
        }
        col.remove(test_doc);
      }
    }
  };
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief test suite
// //////////////////////////////////////////////////////////////////////////////

function transactionOperationsSuite () {
  'use strict';
  var cn32 = 'UnitTestsTransaction32';
  var cn42 = 'UnitTestsTransaction42';

  var c32 = null;
  var c42 = null;

  return {

    // //////////////////////////////////////////////////////////////////////////////
    // / @brief set up
    // //////////////////////////////////////////////////////////////////////////////

    setUpAll: function () {
      c32 = db._create(cn32, {numberOfShards: 3, replicationFactor: 2});
      c42 = db._create(cn42, {numberOfShards: 4, replicationFactor: 2});
    },


    setUp: function () {
      c32.truncate({ compact: false });
      c42.truncate({ compact: false });
    },

    // //////////////////////////////////////////////////////////////////////////////
    // / @brief tear down
    // //////////////////////////////////////////////////////////////////////////////

    tearDown: function () {
      c32.truncate({ compact: false });
      c42.truncate({ compact: false });
      internal.wait(0);
    },

    tearDownAll: function () {
      db._drop(cn32);
      db._drop(cn42);
    },

    // //////////////////////////////////////////////////////////////////////////////
    // / @brief test: trx with negative lock timeout
    // //////////////////////////////////////////////////////////////////////////////

    testNegativeLockTimeout: function () {
      let obj = {
        collections: {
          read: [ cn32 ],
        },
        lockTimeout: -1
      };

      try {
        db._createTransaction(obj);
        fail();
      } catch (err) {
        assertEqual(internal.errors.ERROR_BAD_PARAMETER.code, err.errorNum);
      }
    },

    // //////////////////////////////////////////////////////////////////////////////
    // / @brief test: trx with read operation
    // //////////////////////////////////////////////////////////////////////////////

    testSingleRead1: function () {
      c32.save({ _key: 'foo', a: 1 });

      let obj = {
        collections: {
          read: [ cn32 ]
        }
      };

      let trx;
      try {
        trx = db._createTransaction(obj);
        let tc1 = trx.collection(c32.name());
        assertEqual(1, tc1.document('foo').a);
      } catch(err) {
        fail("Transaction failed with: " + JSON.stringify(err));
      } finally {
        if (trx) {
          trx.commit();
        }
      }

      assertEqual(1, c32.count());
      assertEqual([ 'foo' ], sortedKeys(c32));
    },

    // //////////////////////////////////////////////////////////////////////////////
    // / @brief test: trx with read operation
    // //////////////////////////////////////////////////////////////////////////////

    testSingleRead2: function () {
      c32.save({ _key: 'foo', a: 1 });

      let obj = {
        collections: {
          write: [ cn32 ]
        }
      };

      let trx;
      try {
        trx = db._createTransaction(obj);
        let tc32 = trx.collection(c32.name());
        assertEqual(1, tc32.document('foo').a);
      } catch(err) {
        fail("Transaction failed with: " + JSON.stringify(err));
      } finally {
        if (trx) {
          trx.commit();
        }
      }

      assertEqual(1, c32.count());
      assertEqual([ 'foo' ], sortedKeys(c32));
    },

    // //////////////////////////////////////////////////////////////////////////////
    // / @brief test: trx with read operation
    // //////////////////////////////////////////////////////////////////////////////

    testScan1: function () {
      let docs = [];
      for (let i = 0; i < 100; ++i) {
        docs.push({ _key: 'foo' + i, a: i });
      }
      c32.save(docs);

      let obj = {
        collections: {
          read: [ cn32 ]
        }
      };

      let trx;
      try {
        trx = db._createTransaction(obj);
        let tc32 = trx.collection(c32.name());
        // TODO enable with cursor el-cheapification
        //assertEqual(100, c32.toArray().length);
        assertEqual(100, tc32.count());
        for (let i = 0; i < 100; ++i) {
          assertEqual(i, tc32.document('foo' + i).a);
        }
      } catch(err) {
        fail("Transaction failed with: " + JSON.stringify(err));
      } finally {
        if (trx) {
          trx.commit();
        }
      }

      assertEqual(100, c32.count());
    },

    // //////////////////////////////////////////////////////////////////////////////
    // / @brief test: trx with read operation
    // //////////////////////////////////////////////////////////////////////////////

    testScan2: function () {
      let docs = [];
      for (let i = 0; i < 100; ++i) {
        docs.push({ _key: 'foo' + i, a: i });
      }
      c32.save(docs);

      let obj = {
        collections: {
          write: [ cn32 ]
        }
      };

      let trx;
      try {
        trx = db._createTransaction(obj);
        let tc32 = trx.collection(c32.name());
        
        //assertEqual(100, tc32.toArray().length);
        assertEqual(100, tc32.count());

        for (let i = 0; i < 100; ++i) {
          assertEqual(i, tc32.document('foo' + i).a);
        }

      } catch(err) {
        fail("Transaction failed with: " + JSON.stringify(err));
      } finally {
        if (trx) {
          trx.commit();
        }
      }

      assertEqual(100, c32.count());
    },

    // //////////////////////////////////////////////////////////////////////////////
    // / @brief test: trx with insert operation
    // //////////////////////////////////////////////////////////////////////////////

    testSingleInsert: function () {
      let obj = {
        collections: {
          write: [ cn32 ]
        }
      };

      let trx;
      try {
        trx = db._createTransaction(obj);
        let tc32 = trx.collection(cn32);
        
        tc32.save({ _key: 'foo' });

      } catch(err) {
        fail("Transaction failed with: " + JSON.stringify(err));
      } finally {
        if (trx) {
          trx.commit();
        }
      }

      assertEqual(1, c32.count());
      assertEqual([ 'foo' ], sortedKeys(c32));
    },

    // //////////////////////////////////////////////////////////////////////////////
    // / @brief test: trx with insert operation
    // //////////////////////////////////////////////////////////////////////////////

    testMultiInsert: function () {
      let obj = {
        collections: {
          write: [ cn32 ]
        }
      };
      
      let trx;
      try {
        trx = db._createTransaction(obj);
        let tc32 = trx.collection(c32.name());
        
        tc32.save({ _key: 'foo' });
        tc32.save({ _key: 'bar' });

      } catch(err) {
        fail("Transaction failed with: " + JSON.stringify(err));
      } finally {
        if (trx) {
          trx.commit();
        }
      }

      assertEqual(2, c32.count());
      assertEqual([ 'bar', 'foo' ], sortedKeys(c32));
    },

    // //////////////////////////////////////////////////////////////////////////////
    // / @brief test: trx with insert operation
    // //////////////////////////////////////////////////////////////////////////////

    testInsertWithExisting: function () {
      c32.save([
        { _key: 'foo' },
        { _key: 'bar' }]);

      let obj = {
        collections: {
          write: [ cn32 ]
        }
      };

      let trx;
      try {
        trx = db._createTransaction(obj);
        let tc32 = trx.collection(cn32);
        
        tc32.save({ _key: 'baz' });
        tc32.save({ _key: 'bam' });

      } catch(err) {
        fail("Transaction failed with: " + JSON.stringify(err));
      } finally {
        if (trx) {
          trx.commit();
        }
      }

      assertEqual(4, c32.count());
      assertEqual([ 'bam', 'bar', 'baz', 'foo' ], sortedKeys(c32));
    },

    // //////////////////////////////////////////////////////////////////////////////
    // / @brief test: trx with replace operation
    // //////////////////////////////////////////////////////////////////////////////

    testReplace: function () {
      c32.save([{ _key: 'foo', a: 1 },
                { _key: 'bar', b: 2, c: 3 }]);

      let obj = {
        collections: {
          write: [ cn32 ]
        }
      };

      let trx;
      try {
        trx = db._createTransaction(obj);
        let tc32 = trx.collection(c32.name());
        
        assertEqual(1, tc32.document('foo').a);
        tc32.replace('foo', { a: 3 });

        assertEqual(2, tc32.document('bar').b);
        assertEqual(3, tc32.document('bar').c);
        tc32.replace('bar', { b: 9 });

      } catch(err) {
        fail("Transaction failed with: " + JSON.stringify(err));
      } finally {
        if (trx) {
          trx.commit();
        }
      }

      assertEqual(2, c32.count());
      assertEqual([ 'bar', 'foo' ], sortedKeys(c32));
      assertEqual(3, c32.document('foo').a);
      assertEqual(9, c32.document('bar').b);
      assertEqual(undefined, c32.document('bar').c);
    },

    // //////////////////////////////////////////////////////////////////////////////
    // / @brief test: trx with replace operation
    // //////////////////////////////////////////////////////////////////////////////

    testReplaceReplace: function () {
      c32.save({ _key: 'foo', a: 1 });

      let obj = {
        collections: {
          write: [ cn32 ]
        }
      };

      let trx;
      try {
        trx = db._createTransaction(obj);
        let tc32 = trx.collection(c32.name());
        
        assertEqual(1, tc32.document('foo').a);
        tc32.replace('foo', { a: 3 });
        assertEqual(3, tc32.document('foo').a);
        tc32.replace('foo', { a: 4 });
        assertEqual(4, tc32.document('foo').a);
        tc32.replace('foo', { a: 5 });
        assertEqual(5, tc32.document('foo').a);
        tc32.replace('foo', { a: 6, b: 99 });
        assertEqual(6, tc32.document('foo').a);
        assertEqual(99, tc32.document('foo').b);

      } catch(err) {
        fail("Transaction failed with: " + JSON.stringify(err));
      } finally {
        if (trx) {
          trx.commit();
        }
      }

      assertEqual(1, c32.count());
      assertEqual(6, c32.document('foo').a);
      assertEqual(99, c32.document('foo').b);
    },

    // //////////////////////////////////////////////////////////////////////////////
    // / @brief test: trx with update operation
    // //////////////////////////////////////////////////////////////////////////////

    testUpdate: function () {
      c42.save([
        { _key: 'foo', a: 1 },
        { _key: 'bar', b: 2 },
        { _key: 'baz', c: 3 },
        { _key: 'bam', d: 4 }]);

      let obj = {
        collections: {
          write: [ cn42 ]
        }
      };
      
      let trx;
      try {
        trx = db._createTransaction(obj);
        let tc42 = trx.collection(cn42);
        
        assertEqual(1, tc42.document('foo').a);
        tc42.update('foo', { a: 3 });

        assertEqual(2, tc42.document('bar').b);
        tc42.update('bar', { b: 9 });

        assertEqual(3, tc42.document('baz').c);
        tc42.update('baz', { b: 9, c: 12 });

        assertEqual(4, tc42.document('bam').d);

      } catch(err) {
        fail("Transaction failed with: " + JSON.stringify(err));
      } finally {
        if (trx) {
          trx.commit();
        }
      }

      assertEqual(4, c42.count());
      assertEqual([ 'bam', 'bar', 'baz', 'foo' ], sortedKeys(c42));
      assertEqual(3, c42.document('foo').a);
      assertEqual(9, c42.document('bar').b);
      assertEqual(9, c42.document('baz').b);
      assertEqual(12, c42.document('baz').c);
      assertEqual(4, c42.document('bam').d);
    },

    // //////////////////////////////////////////////////////////////////////////////
    // / @brief test: trx with remove operation
    // //////////////////////////////////////////////////////////////////////////////

    testRemove: function () {
      c42.save([{ _key: 'foo', a: 1 },
                { _key: 'bar', b: 2 },
                { _key: 'baz', c: 3 }]);

      let obj = {
        collections: {
          write: [ cn42 ]
        }
      };
      
      let trx;
      try {
        trx = db._createTransaction(obj);
        let tc42 = trx.collection(c42.name());
        
        tc42.remove('foo');
        tc42.remove('baz');

      } catch(err) {
        fail("Transaction failed with: " + JSON.stringify(err));
      } finally {
        if (trx) {
          trx.commit();
        }
      }

      assertEqual(1, c42.count());
      assertEqual([ 'bar' ], sortedKeys(c42));
    },

    // //////////////////////////////////////////////////////////////////////////////
    // / @brief test: trx with truncate operation
    // //////////////////////////////////////////////////////////////////////////////

    testTruncateEmpty: function () {
      let obj = {
        collections: {
          write: [ cn42 ]
        }
      };

           
      let trx;
      try {
        trx = db._createTransaction(obj);
        let tc42 = trx.collection(cn42);
        
        tc42.truncate({ compact: false });

      } catch(err) {
        fail("Transaction failed with: " + JSON.stringify(err));
      } finally {
        if (trx) {
          trx.commit();
        }
      }

      assertEqual(0, c42.count());
      assertEqual([ ], sortedKeys(c42));
    },

    // //////////////////////////////////////////////////////////////////////////////
    // / @brief test: trx with truncate operation
    // //////////////////////////////////////////////////////////////////////////////

    testTruncateNonEmpty: function () {
      let docs = [];
      for (let i = 0; i < 100; ++i) {
        docs.push({ a: i });
      }
      c42.save(docs);

      let obj = {
        collections: {
          write: [ cn42 ]
        }
      };
      
      let trx;
      try {
        trx = db._createTransaction(obj);
        let tc42 = trx.collection(cn42);
        
        tc42.truncate({ compact: false });

      } catch(err) {
        fail("Transaction failed with: " + JSON.stringify(err));
      } finally {
        if (trx) {
          trx.commit();
        }
      }

      assertEqual(0, c42.count());
      assertEqual([ ], sortedKeys(c42));
    },

    // //////////////////////////////////////////////////////////////////////////////
    // / @brief test: trx with truncate operation
    // //////////////////////////////////////////////////////////////////////////////

    testTruncateAndAdd: function () {
      let docs = [];
      for (let i = 0; i < 100; ++i) {
        docs.push({ a: i });
      }
      c42.save(docs);

      let obj = {
        collections: {
          write: [ cn42 ]
        }
      };

      let trx;
      try {
        trx = db._createTransaction(obj);
        let tc42 = trx.collection(cn42);
        
        tc42.truncate({ compact: false });
        tc42.save({ _key: 'foo' });

      } catch(err) {
        fail("Transaction failed with: " + JSON.stringify(err));
      } finally {
        if (trx) {
          trx.commit();
        }
      }

      assertEqual(1, c42.count());
      assertEqual([ 'foo' ], sortedKeys(c42));
    },

    testAQLDocumentModify: function() {
      c42.save({_key:"1"});


      let obj = {
        collections: {
          write: [ cn42 ]
        }
      };

      let trx;
      try {
        trx = db._createTransaction(obj);
        let tc42 = trx.collection(cn42);
        
        trx.query(`LET g = NOOPT(DOCUMENT("${cn42}/1")) UPDATE g WITH {"updated":1} IN ${cn42}`);

        let doc = tc42.document("1");
        assertEqual(doc.updated, 1);

      } catch(err) {
        fail("Transaction failed with: " + JSON.stringify(err));
      } finally {   
        if (trx) {
          trx.commit();
        }
      }
      assertEqual(1, c42.count());
      let doc = c42.document("1");
      assertEqual(doc.updated, 1);
    }

  };
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief test suite
// //////////////////////////////////////////////////////////////////////////////

function transactionBarriersSuite () {
  'use strict';
  var cn32 = 'UnitTestsTransaction32';

  var c32 = null;

  return {

    // //////////////////////////////////////////////////////////////////////////////
    // / @brief set up
    // //////////////////////////////////////////////////////////////////////////////

    setUpAll: function () {
      c32 = db._create(cn32, {numberOfShards: 3, replicationFactor: 2});
    },


    setUp: function () {
      c32.truncate({ compact: false });
    },

    // //////////////////////////////////////////////////////////////////////////////
    // / @brief tear down
    // //////////////////////////////////////////////////////////////////////////////

    tearDown: function () {
      c32.truncate({ compact: false });
      internal.wait(0);
    },

    tearDownAll: function () {
      db._drop(cn32);
    },

    // //////////////////////////////////////////////////////////////////////////////
    // / @brief test: usage of barriers outside of transaction
    // //////////////////////////////////////////////////////////////////////////////

    testBarriersOutsideCommit: function () {
      let docs = [ ];

      let obj = {
        collections: {
          write: [ cn32 ]
        }
      };

      let result;
      // begin trx
      let trx = db._createTransaction(obj);
      try {
        let tc32 = trx.collection(cn32);
        
        for (let i = 0; i < 100; ++i) {
          tc32.save({ _key: 'foo' + i, value1: i, value2: 'foo' + i + 'x' });
        }

        for (let i = 0; i < 100; ++i) {
          docs.push(tc32.document('foo' + i));
        }
        result = tc32.document('foo0');

      } finally {
        trx.commit();
      }
      // end trx

      assertEqual(100, docs.length);
      assertEqual(100, c32.count());

      assertEqual('foo0', result._key);
      assertEqual(0, result.value1);
      assertEqual('foo0x', result.value2);

      for (let i = 0; i < 100; ++i) {
        assertEqual('foo' + i, docs[i]._key);
        assertEqual(i, docs[i].value1);
        assertEqual('foo' + i + 'x', docs[i].value2);
      }
    },

    // //////////////////////////////////////////////////////////////////////////////
    // / @brief test: usage of barriers outside of transaction
    // //////////////////////////////////////////////////////////////////////////////

    testBarriersOutsideRollback: function () {
      let docs = [ ];
      let obj = {
        collections: {
          write: [ cn32 ]
        }
      };

      let result;
      // begin trx
      let trx = db._createTransaction(obj);
      try {
        let tc32 = trx.collection(cn32);
        
        for (let i = 0; i < 100; ++i) {
          tc32.save({ _key: 'foo' + i, value1: i, value2: 'foo' + i + 'x' });
        }

        for (let i = 0; i < 100; ++i) {
          docs.push(tc32.document('foo' + i));
        }

      } finally {
        trx.abort(); // rollback
      }
      // end trx

      assertEqual(100, docs.length);

      for (let i = 0; i < 100; ++i) {
        assertEqual('foo' + i, docs[i]._key);
        assertEqual(i, docs[i].value1);
        assertEqual('foo' + i + 'x', docs[i].value2);
      }
      assertEqual(c32.count(), 0);
    }

  };
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief test suite
// //////////////////////////////////////////////////////////////////////////////

function transactionRollbackSuite () {
  'use strict';
  var cn12 = 'UnitTestsTransaction12';
  var cn22s = 'UnitTestsTransaction22s';
  var cn32 = 'UnitTestsTransaction32';
  var cn42 = 'UnitTestsTransaction42';
  var cn42i = 'UnitTestsTransaction42i';
  var cn42s = 'UnitTestsTransaction42s';

  var c12 = null;
  var c22s = null;
  var c32 = null;
  var c42 = null;
  var c42s = null;
  var c42i = null;

  return {

    // //////////////////////////////////////////////////////////////////////////////
    // / @brief set up
    // //////////////////////////////////////////////////////////////////////////////

    setUpAll: function () {
      c12 = db._create(cn12, {numberOfShards: 1, replicationFactor: 2});
      c12.ensureUniqueConstraint('name');
      c22s = db._create(cn22s, {numberOfShards: 2, replicationFactor: 2, shardKeys:['name']});
      c22s.ensureUniqueConstraint('name');
      c32 = db._create(cn32, {numberOfShards: 3, replicationFactor: 2});
      c42 = db._create(cn42, {numberOfShards: 4, replicationFactor: 2});
      c42s = db._create(cn42s, {numberOfShards: 4, replicationFactor: 2, shardKeys: ['value']});
      // todo: mmfiles isn't here anymore. does this make sense?:
      c42s.ensureHashIndex('value');
      c42s.ensureSkiplist('value');
      c42i = db._create(cn42i, {numberOfShards: 4, replicationFactor: 2});
      // TODO: mmfiles isn't here anymore. does this make sense?:
      c42i.ensureHashIndex('value');
      c42i.ensureSkiplist('value');
      
    },

    setUp: function () {
      c12.truncate({ compact: false });
      c22s.truncate({ compact: false });
      c32.truncate({ compact: false });
      c42.truncate({ compact: false });
      c42s.truncate({ compact: false });
      c42i.truncate({ compact: false });
    },

    // //////////////////////////////////////////////////////////////////////////////
    // / @brief tear down
    // //////////////////////////////////////////////////////////////////////////////

    tearDown: function () {
      c12.truncate({ compact: false });
      c22s.truncate({ compact: false });
      c32.truncate({ compact: false });
      c42.truncate({ compact: false });
      c42s.truncate({ compact: false });
      c42i.truncate({ compact: false });

      internal.wait(0);
    },

    tearDownAll: function () {
      c12.drop();
      c22s.drop();
      c32.drop();
      c42.drop();
      c42s.drop();
      c42i.drop();

      internal.wait(0);
    },

    // //////////////////////////////////////////////////////////////////////////////
    // / @brief test: rollback after flush
    // //////////////////////////////////////////////////////////////////////////////

    testRollbackAfterFlush: function () {
      // begin trx
      let trx = db._createTransaction({
        collections: {
          write: [ cn42 ]
        }
      });
      try {
        let tc42 = trx.collection(cn42);
        
        tc42.save([{ _key: 'tom' },
                   { _key: 'tim' },
                   { _key: 'tam' }]);

        internal.wal.flush(true);
        assertEqual(3, tc42.count());
      } finally {
        trx.abort(); // rollback
      }
      // end trx

      assertEqual(0, c42.count());

    },

    // //////////////////////////////////////////////////////////////////////////////
    // / @brief test: the collection revision id
    // //////////////////////////////////////////////////////////////////////////////

    // TODO revision is not a supported El-Cheapo API
    /*testRollbackRevision: function () {
      c32.save({ _key: 'foo' });

      let r = c32.revision();

      let obj = {
        collections: {
          write: [ cn32 ]
        }
      };

      // begin trx
      let trx = db._createTransaction(obj);
      try {
        let tc32 = trx.collection(c32);
        
        var _r = r;

        tc32.save({ _key: 'tom' });
        assertEqual(1, compareStringIds(tc32.revision(), _r));
        _r = tc1.revision();

        tc32.save({ _key: 'tim' });
        assertEqual(1, compareStringIds(tc32.revision(), _r));
        _r = tc32.revision();

        tc32.save({ _key: 'tam' });
        assertEqual(1, compareStringIds(tc32.revision(), _r));
        _r = c1.revision();

        tc32.remove('tam');
        assertEqual(1, compareStringIds(tc32.revision(), _r));
        _r = c1.revision();

        tc32.update('tom', { 'bom': true });
        assertEqual(1, compareStringIds(tc32.revision(), _r));
        _r = tc1.revision();

        tc32.remove('tom');
        assertEqual(1, compareStringIds(tc32.revision(), _r));
        // _r = c1.revision();
        
      } finally {
        trx.abort(); // rollback
      }
      // end trx

      assertEqual(1, c32.count());
      assertEqual(r, c32.revision());
    },*/

    // //////////////////////////////////////////////////////////////////////////////
    // / @brief test: rollback inserts
    // //////////////////////////////////////////////////////////////////////////////

    testRollbackInsert: function () {
      c42.save([{ _key: 'foo' },
                { _key: 'bar' },
                { _key: 'meow' }]);

      let obj = {
        collections: {
          write: [ cn42 ]
        }
      };

      // begin trx
      let trx = db._createTransaction(obj);
      try {
        let tc42 = trx.collection(cn42);
        
        tc42.save({ _key: 'tom' });
        tc42.save({ _key: 'tim' });
        tc42.save({ _key: 'tam' });

        assertEqual(6, tc42.count());
        
      } finally {
        trx.abort(); // rollback
      }
      // end trx

      assertEqual(3, c42.count());
      assertEqual([ 'bar', 'foo', 'meow' ], sortedKeys(c42));
    },

    // //////////////////////////////////////////////////////////////////////////////
    // / @brief test: rollback inserts w/ secondary indexes
    // //////////////////////////////////////////////////////////////////////////////

    testRollbackInsertSecondaryIndexes: function () {
      c42.save([{ _key: 'foo', value: 'foo', a: 1 },
                { _key: 'bar', value: 'bar', a: 1 },
                { _key: 'meow', value: 'meow' }]);

      c42.ensureHashIndex('value');
      c42.ensureSkiplist('value');
      let good = false;

      let obj = {
        collections: {
          write: [ cn42 ]
        }
      };

      // begin trx
      let trx = db._createTransaction(obj);
      try {
        let tc42 = trx.collection(cn42);
        
        tc42.save({ _key: 'tom', value: 'tom' });
        tc42.save({ _key: 'tim', value: 'tim' });
        tc42.save({ _key: 'tam', value: 'tam' });
        tc42.save({ _key: 'troet', value: 'foo', a: 2 });
        tc42.save({ _key: 'floxx', value: 'bar', a: 2 });

        assertEqual(8, tc42.count());

        good = true;
      } finally {
        trx.abort(); // rollback
      }
      // end trx

      assertEqual(true, good);

      assertEqual(3, c42.count());
    },


    // //////////////////////////////////////////////////////////////////////////////
    // / @brief test: rollback inserts w/ secondary indexes
    // //////////////////////////////////////////////////////////////////////////////

    testRollbackInsertSecondaryIndexesCustomSharding: function () {
      let d1 = c42s.save({ value: 'foo', a: 1 });
      let d2 = c42s.save({ value: 'bar', a: 1 });
      let d3 = c42s.save({ value: 'meow' });

      let good = false;

      let obj = {
        collections: {
          write: [ cn42s ]
        }
      };

      // begin trx
      let trx = db._createTransaction(obj);
      try {
        let tc42s = trx.collection(cn42s);
        
        tc42s.save({ value: 'tom' });
        tc42s.save({ value: 'tim' });
        tc42s.save({ value: 'tam' });
        tc42s.save({ value: 'troet', a: 2 });
        tc42s.save({ value: 'floxx', a: 2 });

        assertEqual(8, tc42s.count());

        good = true;
      } finally {
        trx.abort(); // rollback
      }
      // end trx

      assertEqual(true, good);

      assertEqual(3, c42s.count());
      assertEqual(d1._rev, c42s.document(d1._key)._rev);
      assertEqual(d2._rev, c42s.document(d2._key)._rev);
      assertEqual(d3._rev, c42s.document(d3._key)._rev);
    },

    // //////////////////////////////////////////////////////////////////////////////
    // / @brief test: rollback inserts
    // //////////////////////////////////////////////////////////////////////////////

    testRollbackInsertUpdate: function () {
      c42.save([{ _key: 'foo' },
                { _key: 'bar' },
                { _key: 'meow' }]);

      let obj = {
        collections: {
          write: [ cn42 ]
        }
      };

      // begin trx
      let trx = db._createTransaction(obj);
      try {
        let tc42 = trx.collection(cn42);
        
        tc42.save({ _key: 'tom' });
        tc42.save({ _key: 'tim' });
        tc42.save({ _key: 'tam' });

        tc42.update('tom', { });
        tc42.update('tim', { });
        tc42.update('tam', { });
        tc42.update('bar', { });
        tc42.remove('foo');
        tc42.remove('bar');
        tc42.remove('meow');
        tc42.remove('tom');

        assertEqual(2, tc42.count());
      } finally {
        trx.abort(); // rollback
      }
      // end trx

      assertEqual(3, c42.count());
      assertEqual([ 'bar', 'foo', 'meow' ], sortedKeys(c42));
    },

    // //////////////////////////////////////////////////////////////////////////////
    // / @brief test: rollback inserts
    // //////////////////////////////////////////////////////////////////////////////

    testRollbackInsertUpdateCustomSharding: function () {
      // TODO review: do the 2 indices hurt here? 
      let d1 = c42s.save({ value: 'foo' });
      let d2 = c42s.save({ value: 'bar' });
      let d3 = c42s.save({ value: 'meow' });

      let obj = {
        collections: {
          write: [ cn42s ]
        }
      };

      // begin trx
      let trx = db._createTransaction(obj);
      try {
        let tc42s = trx.collection(cn42s);
        
        let d4 = tc42s.save({ value: 'tom' });
        let d5 = tc42s.save({ value: 'tim' });
        let d6 = tc42s.save({ value: 'tam' });

        tc42s.update(d4._key, { });
        tc42s.update(d5._key, { });
        tc42s.update(d6._key, { });
        tc42s.update(d2._key, { });
        tc42s.remove(d1._key);
        tc42s.remove(d2._key);
        tc42s.remove(d3._key);
        tc42s.remove(d4._key);

        assertEqual(2, tc42s.count());
      } finally {
        trx.abort(); // rollback
      }
      // end trx

      assertEqual(3, c42s.count());
      assertEqual(d1._rev, c42s.document(d1._key)._rev);
      assertEqual(d2._rev, c42s.document(d2._key)._rev);
      assertEqual(d3._rev, c42s.document(d3._key)._rev);
    },

    // //////////////////////////////////////////////////////////////////////////////
    // / @brief test: rollback update
    // //////////////////////////////////////////////////////////////////////////////

    testRollbackUpdate: function () {
      var d1, d2, d3;

      d1 = c42.save({ _key: 'foo', a: 1 });
      d2 = c42.save({ _key: 'bar', a: 2 });
      d3 = c42.save({ _key: 'meow', a: 3 });

      let obj = {
        collections: {
          write: [ cn42 ]
        }
      };

      // begin trx
      let trx = db._createTransaction(obj);
      try {
        let tc42 = trx.collection(cn42);
        
        tc42.update(d1, { a: 4 });
        tc42.update(d2, { a: 5 });
        tc42.update(d3, { a: 6 });

        assertEqual(3, tc42.count());

      } finally {
        trx.abort(); // rollback
      }
      // end trx

      assertEqual(3, c42.count());
      assertEqual([ 'bar', 'foo', 'meow' ], sortedKeys(c42));
      assertEqual(d1._rev, c42.document('foo')._rev);
      assertEqual(d2._rev, c42.document('bar')._rev);
      assertEqual(d3._rev, c42.document('meow')._rev);
    },

    // //////////////////////////////////////////////////////////////////////////////
    // / @brief test: rollback update
    // //////////////////////////////////////////////////////////////////////////////

    testRollbackUpdateCustomSharding: function () {
      // todo review: does the additional index hurt here?
      let d1 = c42s.save({ value: 'foo', a: 1 });
      let d2 = c42s.save({ value: 'bar', a: 2 });
      let d3 = c42s.save({ value: 'meow', a: 3 });

      let obj = {
        collections: {
          write: [ cn42s ]
        }
      };

      // begin trx
      let trx = db._createTransaction(obj);
      try {
        let tc42s = trx.collection(cn42s);
        
        tc42s.update(d1, { a: 4 });
        tc42s.update(d2, { a: 5 });
        tc42s.update(d3, { a: 6 });

        assertEqual(3, tc42s.count());

      } finally {
        trx.abort(); // rollback
      }
      // end trx

      assertEqual(3, c42s.count());
      assertEqual([ d1._key, d2._key, d3._key ].sort(), sortedKeys(c42s));
      assertEqual(d1._rev, c42s.document(d1._key)._rev);
      assertEqual(d2._rev, c42s.document(d2._key)._rev);
      assertEqual(d3._rev, c42s.document(d3._key)._rev);
    },

    // //////////////////////////////////////////////////////////////////////////////
    // / @brief test: rollback update
    // //////////////////////////////////////////////////////////////////////////////

    testRollbackUpdateUpdate: function () {
      var d1, d2, d3;

      d1 = c42.save({ _key: 'foo', a: 1 });
      d2 = c42.save({ _key: 'bar', a: 2 });
      d3 = c42.save({ _key: 'meow', a: 3 });

      let obj = {
        collections: {
          write: [ cn42 ]
        }
      };

      // begin trx
      let trx = db._createTransaction(obj);
      try {
        let tc42 = trx.collection(cn42);
        
        for (let i = 0; i < 100; ++i) {
          tc42.replace('foo', { a: i });
          tc42.replace('bar', { a: i + 42 });
          tc42.replace('meow', { a: i + 23 });

          assertEqual(i, tc42.document('foo').a);
          assertEqual(i + 42, tc42.document('bar').a);
          assertEqual(i + 23, tc42.document('meow').a);
        }

        assertEqual(3, tc42.count());

      } finally {
        trx.abort(); // rollback
      }
      // end trx

      assertEqual(3, c42.count());
      assertEqual([ 'bar', 'foo', 'meow' ], sortedKeys(c42));
      assertEqual(1, c42.document('foo').a);
      assertEqual(2, c42.document('bar').a);
      assertEqual(3, c42.document('meow').a);
      assertEqual(d1._rev, c42.document('foo')._rev);
      assertEqual(d2._rev, c42.document('bar')._rev);
      assertEqual(d3._rev, c42.document('meow')._rev);
    },

    // //////////////////////////////////////////////////////////////////////////////
    // / @brief test: rollback remove w/ secondary indexes
    // //////////////////////////////////////////////////////////////////////////////

    testRollbackUpdateSecondaryIndexes: function () {
      c42i.save([{ _key: 'foo', value: 'foo', a: 1 },
                 { _key: 'bar', value: 'bar', a: 1 },
                 { _key: 'meow', value: 'meow' }]);

      let good = false;

      let obj = {
        collections: {
          write: [ cn42i ]
        }
      };

      // begin trx
      let trx = db._createTransaction(obj);
      try {
        let tc42i = trx.collection(cn42i);
        
        tc42i.update('foo', { value: 'foo', a: 2 });
        tc42i.update('bar', { value: 'bar', a: 2 });
        tc42i.update('meow', { value: 'troet' });

        assertEqual(3, tc42i.count());
        good = true;

      } finally {
        trx.abort(); // rollback
      }
      // end trx

      assertEqual(true, good);
      assertEqual(3, c42i.count());
    },

    // //////////////////////////////////////////////////////////////////////////////
    // / @brief test: rollback remove
    // //////////////////////////////////////////////////////////////////////////////

    testRollbackRemove: function () {
      c42.save([{ _key: 'foo' },
                { _key: 'bar' },
                { _key: 'meow' }]);

      let obj = {
        collections: {
          write: [ cn42 ]
        }
      };

      // begin trx
      let trx = db._createTransaction(obj);
      try {
        let tc42 = trx.collection(cn42);
        
        tc42.remove('meow');
        tc42.remove('foo');

        assertEqual(1, tc42.count());
        assertTrue(tc42.document('bar') !== undefined);

      } finally {
        trx.abort(); // rollback
      }
      // end trx

      assertEqual(3, c42.count());
      assertEqual([ 'bar', 'foo', 'meow' ], sortedKeys(c42));
    },

    // //////////////////////////////////////////////////////////////////////////////
    // / @brief test: rollback remove
    // //////////////////////////////////////////////////////////////////////////////

    testRollbackRemoveCustomSharding: function () {
/// TODO: will the indices harm this case?
      let d1 = c42s.save({ value: 'foo' });
      let d2 = c42s.save({ value: 'bar' });
      let d3 = c42s.save({ value: 'meow' });

      let obj = {
        collections: {
          write: [ cn42s ]
        }
      };

      // begin trx
      let trx = db._createTransaction(obj);
      try {
        let tc42s = trx.collection(cn42s);
        
        tc42s.remove(d3._key);
        tc42s.remove(d1._key);

        assertEqual(1, tc42s.count());
        assertTrue(tc42s.document(d2._key) !== undefined);

      } finally {
        trx.abort(); // rollback
      }
      // end trx

      assertEqual(3, c42s.count());
      assertEqual(d1._rev, c42s.document(d1._key)._rev);
      assertEqual(d2._rev, c42s.document(d2._key)._rev);
      assertEqual(d3._rev, c42s.document(d3._key)._rev);
    },

    // //////////////////////////////////////////////////////////////////////////////
    // / @brief test: rollback remove
    // //////////////////////////////////////////////////////////////////////////////

    testRollbackRemoveMulti: function () {
      c42.save({ _key: 'foo' });

      let obj = {
        collections: {
          write: [ cn42 ]
        }
      };

      // begin trx
      let trx = db._createTransaction(obj);
      try {
        let tc42 = trx.collection(cn42);
        
        for (let i = 0; i < 100; ++i) {
          tc42.save({ _key: 'foo' + i });
        }

        assertEqual(101, tc42.count());
      } finally {
        trx.abort(); // rollback
      }
      // end trx

      assertEqual(1, c42.count());
      assertEqual([ 'foo' ], sortedKeys(c42));
    },

    // //////////////////////////////////////////////////////////////////////////////
    // / @brief test: rollback remove
    // //////////////////////////////////////////////////////////////////////////////

    testRollbackRemoveMultiCustomSharding: function () {
      // TODO: do indices hurt this case?
      let d1 = c42s.save({ value: 'foo' });

      let obj = {
        collections: {
          write: [ cn42s ]
        }
      };

      // begin trx
      let trx = db._createTransaction(obj);
      try {
        let tc42s = trx.collection(cn42s);
        
        for (let i = 0; i < 100; ++i) {
          tc42s.save({ value: 'foo' + i });
        }

        assertEqual(101, tc42s.count());
      } finally {
        trx.abort(); // rollback
      }
      // end trx

      assertEqual(1, c42s.count());
      assertEqual([ d1._key ], sortedKeys(c42s));
    },

    // //////////////////////////////////////////////////////////////////////////////
    // / @brief test: rollback remove w/ secondary indexes
    // //////////////////////////////////////////////////////////////////////////////

    testRollbackRemoveSecondaryIndexes: function () {
      c42i.save([{ _key: 'foo', value: 'foo', a: 1 },
                 { _key: 'bar', value: 'bar', a: 1 },
                 { _key: 'meow', value: 'meow' }]);

      var good = false;

      let obj = {
        collections: {
          write: [ cn42i ]
        }
      };

      // begin trx
      let trx = db._createTransaction(obj);
      try {
        let tc42i = trx.collection(cn42i);
        
        tc42i.remove('meow');
        tc42i.remove('bar');
        tc42i.remove('foo');

        assertEqual(0, tc42i.count());

        good = true;
        
      } finally {
        trx.abort(); // rollback
      }
      // end trx

      assertEqual(true, good);
      assertEqual(3, c42i.count());
    },

    // //////////////////////////////////////////////////////////////////////////////
    // / @brief test: rollback insert/remove w/ secondary indexes
    // //////////////////////////////////////////////////////////////////////////////

    testRollbackRemoveInsertSecondaryIndexes: function () {
      c42i.save([{ _key: 'foo', value: 'foo', a: 1 },
                 { _key: 'bar', value: 'bar', a: 1 },
                 { _key: 'meow', value: 'meow' }]);

      let good = false;

      let obj = {
        collections: {
          write: [ cn42i ]
        }
      };

      // begin trx
      let trx = db._createTransaction(obj);
      try {
        let tc42i = trx.collection(c42i);
        
        tc42i.remove('meow');
        tc42i.remove('bar');
        tc42i.remove('foo');
        assertEqual(0, tc42i.count());

        tc42i.save({ _key: 'foo2', value: 'foo', a: 2 });
        tc42i.save({ _key: 'bar2', value: 'bar', a: 2 });
        assertEqual(2, tc42i.count());

        good = true;
      
      } finally {
        trx.abort(); // rollback
      }
      // end trx

      assertEqual(true, good);
      assertEqual(3, c42i.count());
    },

    // //////////////////////////////////////////////////////////////////////////////
    // / @brief test: rollback truncate
    // //////////////////////////////////////////////////////////////////////////////

    testRollbackTruncateEmpty: function () {
      let obj = {
        collections: {
          write: [ cn42 ]
        }
      };

      // begin trx
      let trx = db._createTransaction(obj);
      try {
        let tc42 = trx.collection(cn42);
        
        // truncate often...
        for (let i = 0; i < 100; ++i) {
          tc42.truncate({ compact: false });
        }
      
      } finally {
        trx.abort(); // rollback
      }
      // end trx

      assertEqual(0, c42.count());
      assertEqual([ ], sortedKeys(c42));
    },

    // //////////////////////////////////////////////////////////////////////////////
    // / @brief test: rollback truncate
    // //////////////////////////////////////////////////////////////////////////////

    testRollbackTruncateNonEmpty: function () {
      let docs = [];
      for (let i = 0; i < 100; ++i) {
        docs.push({ _key: 'foo' + i });
      }
      c32.save(docs);
      assertEqual(100, c32.count());

      let obj = {
        collections: {
          write: [ cn32 ]
        }
      };

      // begin trx
      let trx = db._createTransaction(obj);
      try {
        let tc32 = trx.collection(cn32);
        
        // truncate often...
        for (let i = 0; i < 100; ++i) {
          tc32.truncate({ compact: false });
        }
        tc32.save({ _key: 'bar' });
      
      } finally {
        trx.abort(); // rollback
      }
      // end trx

      assertEqual(100, c32.count());
    },

    // //////////////////////////////////////////////////////////////////////////////
    // / @brief test: trx rollback with unique constraint violation
    // //////////////////////////////////////////////////////////////////////////////

    testRollbackUniquePrimary: function () {
      let d1 = c32.save({ _key: 'foo' });

      let obj = {
        collections: {
          write: [ cn32 ]
        }
      };

      // begin trx
      let trx = db._createTransaction(obj);
      try {
        let tc32 = trx.collection(cn32);
        
        tc32.save({ _key: 'bar' });
        tc32.save({ _key: 'foo' });
        fail();

      } catch (err) {
        assertEqual(internal.errors.ERROR_ARANGO_UNIQUE_CONSTRAINT_VIOLATED.code, err.errorNum);
      } finally {
        trx.abort(); // rollback
      }
      // end trx

      assertEqual(1, c32.count());
      assertEqual('foo', c32.toArray()[0]._key);
      assertEqual(d1._rev, c32.toArray()[0]._rev);
    },

    // //////////////////////////////////////////////////////////////////////////////
    // / @brief test: trx rollback with unique constraint violation
    // //////////////////////////////////////////////////////////////////////////////

    testRollbackUniqueSecondary: function () {
      let d1 = c12.save({ name: 'foo' });

      let obj = {
        collections: {
          write: [ cn12 ]
        }
      };

      // begin trx
      let trx = db._createTransaction(obj);
      try {
        let tc12 = trx.collection(cn12);
        
        tc12.save({ name: 'bar' });
        tc12.save({ name: 'baz' });
        tc12.save({ name: 'foo' });
        fail();
      
      } catch (err) {
        assertEqual(internal.errors.ERROR_ARANGO_UNIQUE_CONSTRAINT_VIOLATED.code, err.errorNum);
      } finally {
        trx.abort(); // rollback
      }
      // end trx
      
      assertEqual(1, c12.count());
      assertEqual('foo', c12.toArray()[0].name);
      assertEqual(d1._rev, c12.toArray()[0]._rev);
    },


    // //////////////////////////////////////////////////////////////////////////////
    // / @brief test: trx rollback with unique constraint violation + custom sharding
    // //////////////////////////////////////////////////////////////////////////////

    testRollbackUniqueSecondaryCustomShard: function () {
      let d1 = c22s.save({ name: 'foo' });

      let obj = {
        collections: {
          write: [ cn22s ]
        }
      };

      // begin trx
      let trx = db._createTransaction(obj);
      try {
        let tc22s = trx.collection(c22s);
        
        tc22s.save({ name: 'bar' });
        tc22s.save({ name: 'baz' });
        tc22s.save({ name: 'foo' });
        fail();
      
      } catch (err) {
        assertEqual(internal.errors.ERROR_ARANGO_UNIQUE_CONSTRAINT_VIOLATED.code, err.errorNum);
      } finally {
        trx.abort(); // rollback
      }
      // end trx
      
      assertEqual(1, c22s.count());
      assertEqual('foo', c22s.toArray()[0].name);
      assertEqual(d1._rev, c22s.toArray()[0]._rev);
    },

    // //////////////////////////////////////////////////////////////////////////////
    // / @brief test: rollback a mixed workload
    // //////////////////////////////////////////////////////////////////////////////

    testRollbackMixed1: function () {
      let docs = [];
      for (let i = 0; i < 100; ++i) {
        docs.push({ _key: 'key' + i, value: i });
      }
      c32.save(docs);

      let obj = {
        collections: {
          write: [ cn32 ]
        }
      };

      // begin trx
      let trx = db._createTransaction(obj);
      try {
        let tc32 = trx.collection(cn32);
        
        for (let i = 0; i < 50; ++i) {
          tc32.remove('key' + i);
        }

        for (let i = 50; i < 100; ++i) {
          tc32.update('key' + i, { value: i - 50 });
        }

        tc32.remove('key50');
      
      } finally {
        trx.abort(); // rollback
      }
      // end trx

      assertEqual(100, c32.count());
    },

    // //////////////////////////////////////////////////////////////////////////////
    // / @brief test: rollback a mixed workload
    // //////////////////////////////////////////////////////////////////////////////

    testRollbackMixed2: function () {
      c32.save([{ _key: 'foo' },
                { _key: 'bar' }]);

      // begin trx
      let trx = db._createTransaction({
        collections: {
          write: [ cn32 ]
        }
      });
      try {
        let tc32 = trx.collection(cn32);
        
        for (let i = 0; i < 10; ++i) {
          tc32.save({ _key: 'key' + i, value: i });
        }

        for (let i = 0; i < 5; ++i) {
          tc32.remove('key' + i);
        }

        for (let i = 5; i < 10; ++i) {
          tc32.update('key' + i, { value: i - 5 });
        }

        tc32.remove('key5');
      
      } finally {
        trx.abort(); // rollback
      }
      // end trx

      assertEqual(2, c32.count());
      assertEqual('foo', c32.document('foo')._key);
      assertEqual('bar', c32.document('bar')._key);
    },

    // //////////////////////////////////////////////////////////////////////////////
    // / @brief test: rollback a mixed workload
    // //////////////////////////////////////////////////////////////////////////////

    testRollbackMixed3: function () {
      c32.save([{ _key: 'foo' },
                { _key: 'bar' }]);

      let obj = {
        collections: {
          write: [ cn32 ]
        }
      };

      // begin trx
      let trx = db._createTransaction(obj);
      try {
        let tc32 = trx.collection(cn32);
        
        for (let i = 0; i < 10; ++i) {
          tc32.save({ _key: 'key' + i, value: i });
        }

        for (let i = 0; i < 10; ++i) {
          tc32.remove('key' + i);
        }

        for (let i = 0; i < 10; ++i) {
          tc32.save({ _key: 'key' + i, value: i });
        }

        for (let i = 0; i < 10; ++i) {
          tc32.update('key' + i, { value: i - 5 });
        }

        for (let i = 0; i < 10; ++i) {
          tc32.update('key' + i, { value: i + 5 });
        }

        for (let i = 0; i < 10; ++i) {
          tc32.remove('key' + i);
        }

        for (let i = 0; i < 10; ++i) {
          tc32.save({ _key: 'key' + i, value: i });
        }
      
      } finally {
        trx.abort(); // rollback
      }
      // end trx

      assertEqual(2, c32.count());
      assertEqual('foo', c32.document('foo')._key);
      assertEqual('bar', c32.document('bar')._key);
    }

  };
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief test suite
// //////////////////////////////////////////////////////////////////////////////

function transactionCountSuite () {
  'use strict';
  var cn32 = 'UnitTestsTransaction32';

  var c32 = null;

  return {

    // //////////////////////////////////////////////////////////////////////////////
    // / @brief set up
    // //////////////////////////////////////////////////////////////////////////////

    setUpAll: function () {
      c32 = db._create(cn32, {numberOfShards: 3, replicationFactor: 2});
    },

    setUp: function () {
      c32.truncate({ compact: false });
    },

    // //////////////////////////////////////////////////////////////////////////////
    // / @brief tear down
    // //////////////////////////////////////////////////////////////////////////////

    tearDown: function () {
      c32.truncate({ compact: false });
      internal.wait(0);
    },

    tearDownAll: function () {
      c32.drop();
      internal.wait(0);
    },

    // //////////////////////////////////////////////////////////////////////////////
    // / @brief test: test counts during trx
    // //////////////////////////////////////////////////////////////////////////////

    testCountDuring: function () {
      assertEqual(0, c32.count());

      let obj = {
        collections: {
          write: [ cn32 ]
        }
      };

      // begin trx
      let trx = db._createTransaction(obj);
      try {
        let tc32 = trx.collection(cn32);
        var d1, d2;

        tc32.save({ a: 1 });
        assertEqual(1, tc32.count());

        d1 = tc32.save({ a: 2 });
        assertEqual(2, tc32.count());

        d2 = tc32.update(d1, { a: 3 });
        assertEqual(2, tc32.count());

        assertEqual(3, tc32.document(d2).a);

        tc32.remove(d2);
        assertEqual(1, tc32.count());

        tc32.truncate({ compact: false });
        assertEqual(0, tc32.count());

        tc32.truncate({ compact: false });
        assertEqual(0, tc32.count());
      } finally {
        trx.commit();
      }
      
      // end trx
      
      assertEqual(0, c32.count());
    },

    // //////////////////////////////////////////////////////////////////////////////
    // / @brief test: test counts during and after trx
    // //////////////////////////////////////////////////////////////////////////////

    testCountCommitAfterFlush: function () {
      c32.save([{ _key: 'foo' },
                { _key: 'bar' }]);
      assertEqual(2, c32.count());

      let obj = {
        collections: {
          write: [ cn32 ]
        }
      };

      // begin trx
      let trx = db._createTransaction(obj);
      try {
        let tc32 = trx.collection(cn32);

        tc32.save({ _key: 'baz' });
        assertEqual(3, tc32.count());

        internal.wal.flush(true, false);

        tc32.save({ _key: 'meow' });
        assertEqual(4, tc32.count());

        internal.wal.flush(true, false);

        tc32.remove('foo');
        assertEqual(3, tc32.count());
      } finally {
        trx.commit();
      }
      // end trx

      assertEqual(3, c32.count());
    },

    // //////////////////////////////////////////////////////////////////////////////
    // / @brief test: test counts during and after trx
    // //////////////////////////////////////////////////////////////////////////////

    testCountCommit: function () {
      c32.save([{ _key: 'foo' },
                { _key: 'bar' }]);
      assertEqual(2, c32.count());

      let obj = {
        collections: {
          write: [ cn32 ]
        }
      };

      // begin trx
      let trx = db._createTransaction(obj);
      try {
        let tc32 = trx.collection(cn32);

        tc32.save({ _key: 'baz' });
        assertEqual(3, tc32.count());

        tc32.save({ _key: 'meow' });
        assertEqual(4, tc32.count());

        tc32.remove('foo');
        assertEqual(3, tc32.count());
      } finally {
        trx.commit();
      }
      // end trx

      assertEqual(3, c32.count());
    },

    // //////////////////////////////////////////////////////////////////////////////
    // / @brief test: test counts during and after trx
    // //////////////////////////////////////////////////////////////////////////////

    testCountRollback: function () {
      c32.save([{ _key: 'foo' },
                { _key: 'bar' }]);
      assertEqual(2, c32.count());

      let obj = {
        collections: {
          write: [ cn32 ]
        }
      };

      // begin trx
      let trx = db._createTransaction(obj);
      try {
        let tc32 = trx.collection(cn32);

        tc32.save({ _key: 'baz' });
        assertEqual(3, tc32.count());

        tc32.save({ _key: 'meow' });
        assertEqual(4, tc32.count());

        tc32.remove('foo');
        assertEqual(3, tc32.count());
      } finally {
        trx.abort(); // rollback
      }
      // end trx

      assertEqual(2, c32.count());

      var keys = [ ];
      c32.toArray().forEach(function (d) {
        keys.push(d._key);
      });
      keys.sort();
      assertEqual([ 'bar', 'foo' ], keys);
    }

  };
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief test suite
// //////////////////////////////////////////////////////////////////////////////

function transactionCrossCollectionSuite () {
  'use strict';
  var cn32 = 'UnitTestsTransaction32';
  var cn2 = 'UnitTestsTransaction2';

  var c32 = null;
  var c2 = null;

  return {

    // //////////////////////////////////////////////////////////////////////////////
    // / @brief set up
    // //////////////////////////////////////////////////////////////////////////////

    setUpAll: function () {
      c32 = db._create(cn32, {numberOfShards: 3, replicationFactor: 2});
      c2 = db._create(cn2, {numberOfShards: 2});
    },
    setUp: function () {
      c32.truncate({ compact: false });
      c2.truncate({ compact: false });
    },

    // //////////////////////////////////////////////////////////////////////////////
    // / @brief tear down
    // //////////////////////////////////////////////////////////////////////////////

    tearDown: function () {
      c32.truncate({ compact: true });
      c2.truncate({ compact: true });
      internal.wait(0);
    },

    tearDownAll: function () {
      c32.drop();
      c2.drop();
      internal.wait(0);
    },
    // //////////////////////////////////////////////////////////////////////////////
    // / @brief test: test cross collection commit
    // //////////////////////////////////////////////////////////////////////////////

    testInserts: function () {
      let obj = {
        collections: {
          write: [ cn32, cn2 ]
        }
      };

      // begin trx
      let trx = db._createTransaction(obj);
      try {
        let tc32 = trx.collection(cn32);
        let tc2 = trx.collection(cn2);

        for (let i = 0; i < 10; ++i) {
          tc32.save({ _key: 'a' + i });
          tc2.save({ _key: 'b' + i });
        }
      } finally {
        trx.commit();
      }
      // end trx

      assertEqual(10, c32.count());
      assertEqual(10, c2.count());
    },

    // //////////////////////////////////////////////////////////////////////////////
    // / @brief test: test cross collection commit
    // //////////////////////////////////////////////////////////////////////////////

    testUpdates: function () {
      var i;
      var docs32 = [];
      var docs2 = [];
      for (i = 0; i < 10; ++i) {
        docs32.push({ _key: 'a' + i, a: i });
        docs2.push({ _key: 'b' + i, b: i });
      }
      c32.save(docs32);
      c2.save(docs2);
      
      let obj = {
        collections: {
          write: [ cn32, cn2 ]
        }
      };

      // begin trx
      let trx = db._createTransaction(obj);
      try {
        let tc32 = trx.collection(cn32);
        let tc2 = trx.collection(cn2);

        for (i = 0; i < 10; ++i) {
          tc32.update('a' + i, { a: i + 20 });

          tc2.update('b' + i, { b: i + 20 });
          tc2.remove('b' + i);
        }
      } finally {
        trx.commit();
      }
      // end trx

      assertEqual(10, c32.count());
      assertEqual(0, c2.count());
    },

    // //////////////////////////////////////////////////////////////////////////////
    // / @brief test: test cross collection commit
    // //////////////////////////////////////////////////////////////////////////////

    testDeletes: function () {
      var i;
      var docs32 = [];
      var docs2 = [];
      for (i = 0; i < 10; ++i) {
        docs32.push({ _key: 'a' + i, a: i });
        docs2.push({ _key: 'b' + i, b: i });
      }
      c32.save(docs32);
      c2.save(docs2);

      let obj = {
        collections: {
          write: [ cn32, cn2 ]
        }
      };

      // begin trx
      let trx = db._createTransaction(obj);
      try {
        let tc32 = trx.collection(cn32);
        let tc2 = trx.collection(cn2);

        for (i = 0; i < 10; ++i) {
          tc32.remove('a' + i);
          tc2.remove('b' + i);
        }

      } finally {
        trx.commit();
      }
      // end trx

      assertEqual(0, c32.count());
      assertEqual(0, c2.count());
    },

    // //////////////////////////////////////////////////////////////////////////////
    // / @brief test: test cross collection commit
    // //////////////////////////////////////////////////////////////////////////////

    testDeleteReload: function () {
      var i;
      var docs32 = [];
      var docs2 = [];
      for (i = 0; i < 10; ++i) {
        docs32.push({ _key: 'a' + i, a: i });
        docs2.push({ _key: 'b' + i, b: i });
      }
      c32.save(docs32);
      c2.save(docs2);

      let obj = {
        collections: {
          write: [ cn32, cn2 ]
        }
      };

      // begin trx
      let trx = db._createTransaction(obj);
      try {
        let tc32 = trx.collection(cn32);
        let tc2 = trx.collection(cn2);

        for (i = 0; i < 10; ++i) {
          tc32.remove('a' + i);
          tc2.remove('b' + i);
        }

      } finally {
        trx.commit();
      }
      // end trx

      assertEqual(0, c32.count());
      assertEqual(0, c2.count());
    },

    // //////////////////////////////////////////////////////////////////////////////
    // / @brief test: test cross collection commit
    // //////////////////////////////////////////////////////////////////////////////

    testCommitReload: function () {
      let obj = {
        collections: {
          write: [ cn32, cn2 ]
        }
      };

      // begin trx
      let trx = db._createTransaction(obj);
      try {
        let tc32 = trx.collection(cn32);
        let tc2 = trx.collection(cn2);

        for (let i = 0; i < 10; ++i) {
          tc32.save({ _key: 'a' + i });
        }
        for (let i = 0; i < 10; ++i) {
          tc2.save({ _key: 'b' + i });
        }
        assertEqual(10, tc32.count());
        assertEqual(10, tc2.count());
        tc32.remove('a4');
        tc2.remove('b6');
      } finally {
        trx.commit();
      }
      // end trx
      assertEqual(9, c32.count());
      assertEqual(9, c2.count());
    },

    // //////////////////////////////////////////////////////////////////////////////
    // / @brief test: test cross collection rollback
    // //////////////////////////////////////////////////////////////////////////////

    testRollbackReload: function () {
      c32.save({ _key: 'a1' });
      c2.save({ _key: 'b1', a: 1 });

      let obj = {
        collections: {
          write: [ cn32, cn2 ]
        }
      };

      // begin trx
      let trx = db._createTransaction(obj);
      try {
        let tc32 = trx.collection(cn32);
        let tc2 = trx.collection(cn2);

        tc32.save({ _key: 'a2' });
        tc32.save({ _key: 'a3' });

        tc2.save({ _key: 'b2' });
        tc2.update('b1', { a: 2 });
        assertEqual(2, tc2.document('b1').a);

      } catch (err) {
        fail("Transaction failed with: " + JSON.stringify(err));
      } finally {
        trx.abort(); // rollback
      }
      // end trx

      assertEqual(1, c32.count());
      assertEqual(1, c2.count());

      assertEqual([ 'a1' ], sortedKeys(c32));
      assertEqual([ 'b1' ], sortedKeys(c2));
      assertEqual(1, c2.document('b1').a);

      c32.unload();
      c2.unload();

      assertEqual(1, c32.count());
      assertEqual(1, c2.count());

      assertEqual([ 'a1' ], sortedKeys(c32));
      assertEqual([ 'b1' ], sortedKeys(c2));
      assertEqual(1, c2.document('b1').a);
    },

    // //////////////////////////////////////////////////////////////////////////////
    // / @brief test: unload / reload after aborting transactions
    // //////////////////////////////////////////////////////////////////////////////

    testUnloadReloadAbortedTrx: function () {
      let docs = [];
      for (let i = 0; i < 10; ++i) {
        docs.push({ _key: 'a' + i, a: i });
      }
      c32.save(docs);

      let obj = {
        collections: {
          write: [ cn32 ]
        }
      };

      for (let i = 0; i < 50; ++i) {
        // begin trx
        let trx = db._createTransaction(obj);
        try {
          let tc32 = trx.collection(cn32);

          for (let x = 0; x < 100; ++x) {
            tc32.save({ _key: 'test' + x });
          }
  
        } catch (err) {
          fail("Transaction failed with: " + JSON.stringify(err));
        } finally {
          trx.abort(); // rollback
        }
        // end trx
      }

      assertEqual(10, c32.count());
    }

  };
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief test suite
// //////////////////////////////////////////////////////////////////////////////

function transactionTraversalSuite () {
  'use strict';
  var cn = 'UnitTestsTransaction';
  var cne = cn + 'Edge';
  var cnv = cn + 'Vertex';
  var ce = null;
  var cv = null;
  return {

    // //////////////////////////////////////////////////////////////////////////////
    // / @brief set up
    // //////////////////////////////////////////////////////////////////////////////
    setUpAll: function () {
      db._drop(cnv);
      db._drop(cne);
      cv = db._create(cnv);
      ce = db._createEdgeCollection(cne);
    },
    setUp: function() {
      cv.truncate({ compact: false });
      ce.truncate({ compact: false });
      var i;
      var docs = []
      for (i = 0; i < 100; ++i) {
        docs.push({ _key: String(i) });
      }
      cv.insert(docs);
      docs = [];
      for (i = 1; i < 100; ++i) {
        docs.push({
          '_from': cnv + '/' + i,
          '_to': cnv + '/' + (i + 1)
        });
      }
      ce.insert(docs);
    },
    // //////////////////////////////////////////////////////////////////////////////
    // / @brief tear down
    // //////////////////////////////////////////////////////////////////////////////

    tearDown: function () {
      cv.truncate({ compact: false });
      ce.truncate({ compact: false });
    },

    tearDownAll: function () {
      db._drop(cnv);
      db._drop(cne);
    },

    // //////////////////////////////////////////////////////////////////////////////
    // / @brief test: use of undeclared traversal collection in transaction
    // //////////////////////////////////////////////////////////////////////////////

    testUndeclaredTraversalCollection: function () {

      let result;
      // begin trx
      let trx = db._createTransaction({
        collections: {
          read: [ cne ],
          write: [ cne ]
        }
      });
      try {
        let tedge = trx.collection(cne);

        let results = trx.query('WITH ' + cnv + ' FOR v, e IN ANY "' + cnv + '/20" ' + 
                            cne + ' FILTER v._id == "' + cnv + 
                            '/21" LIMIT 1 RETURN e').toArray();

        if (results.length > 0) {
          result = results[0];
          tedge.remove(result);
          return 1;
        }
      } catch (err) {
        fail("Transaction failed with: " + JSON.stringify(err));
      } finally {
        trx.commit();
      }
      // end trx

      assertEqual(1, result);
    },

    // //////////////////////////////////////////////////////////////////////////////
    // / @brief test: use of undeclared traversal collection in transaction
    // //////////////////////////////////////////////////////////////////////////////

    testTestCount: function () {
      let docs = [];
      for (let i = 0; i < 100; ++i) {
        docs.push({'_from': cne + '/test' + (i % 21), '_to': cne + '/test' + (i % 7)});
      }
      ce.insert(docs);

      let result;
      // begin trx
      let trx = db._createTransaction({
        collections: {
          read: [ cne ],
          write: [ cne ]
        }
      });
      try {
        let tedge = trx.collection(cne);
        const qq = "FOR e IN " + cne + " FILTER e._from == @a AND e.request == false RETURN e";

        let from = cne + '/test1';
        let to = cne + '/test8';

        let newDoc = tedge.insert({_from: from, _to: to, request: true});
        let fromCount1 = trx.query(qq, {a:from}, {count:true}).count();

        newDoc.request = false;
        tedge.update({ _id: newDoc._id }, newDoc);

        let fromCount2 = trx.query(qq, {a:from}, {count:true}).count();
        result = [ fromCount1, fromCount2 ];

      } finally {
        trx.commit();
      }
      // end trx

      assertEqual(result[0] + 1, result[1]);
    }

  };
}


// //////////////////////////////////////////////////////////////////////////////
// / @brief test suite
// //////////////////////////////////////////////////////////////////////////////

function transactionAQLStreamSuite () {
  'use strict';
  const cn = 'UnitTestsTransaction';
  let c;

  return {

    // //////////////////////////////////////////////////////////////////////////////
    // / @brief set up
    // //////////////////////////////////////////////////////////////////////////////
    setUpAll: function() {
      c = db._create(cn, {numberOfShards: 2, replicationFactor: 2});
      db._create(cn + 'Vertex', {numberOfShards: 2, replicationFactor: 2});
      db._createEdgeCollection(cn + 'Edge', {numberOfShards: 2, replicationFactor: 2});
      c.ensureIndex({ type: 'skiplist', fields: ["value1"]});
      c.ensureIndex({ type: 'skiplist', fields: ["value2"]});
    },
    
    setUp: function () {
      c.truncate({ compact: false });
      db[cn + 'Vertex'].truncate({ compact: false });
      db[cn + 'Edge'].truncate({ compact: false });

      let docs = [];
      for (let i = 0; i < 100; ++i) {
        docs.push({ _key: String(i) });
      }
      db.UnitTestsTransactionVertex.insert(docs);

      docs = [];
      for (let i = 1; i < 100; ++i) {
        docs.push({ _from: cn + 'Vertex/' + i, _to: cn + 'Vertex/' + (i + 1)});
      }
      db.UnitTestsTransactionEdge.insert(docs);

      docs = [];
      for (let i = 0; i < 5000; i++) {
        docs.push({value1: i % 10, value2: i % 25 , value3: i % 25 });
      }
      c.insert(docs);
    },

    // //////////////////////////////////////////////////////////////////////////////
    // / @brief tear down
    // //////////////////////////////////////////////////////////////////////////////

    tearDown: function () {
      c.truncate({ compact: false });
      db[cn + 'Vertex'].truncate({ compact: false });
      db[cn + 'Edge'].truncate({ compact: false });
    },
    tearDownAll: function () {
      db._drop(cn);
      db._drop(cn + 'Vertex');
      db._drop(cn + 'Edge');
    },

    // //////////////////////////////////////////////////////////////////////////////
    // / @brief test: trx using multiple open cursors
    // //////////////////////////////////////////////////////////////////////////////
    testMultipleCursorsInSameTransaction: function () {
      let trx, cursor1, cursor2;
      try {
        trx = db._createTransaction({
          collections: {}
        });
        cursor1 = trx.query('FOR i IN 1..10000000000000 RETURN i', {}, {}, {stream: true});
        try {
          cursor2 = trx.query('FOR i IN 1..10000000000000 RETURN i', {}, {}, {stream: true});
          fail();
        } catch (err) {
          assertEqual(internal.errors.ERROR_LOCKED.code, err.errorNum);
        }
      } catch (err) {
        fail("Transaction failed with: " + JSON.stringify(err));
      } finally {
        if (cursor1) {
          cursor1.dispose();
        }
        if (trx) {
          trx.abort();
        }
      }
    },

    // //////////////////////////////////////////////////////////////////////////////
    // / @brief test: trx using no collections
    // //////////////////////////////////////////////////////////////////////////////

    testNoCollectionsInfiniteAql: function () {
      let trx, cursor;
      try {
        trx = db._createTransaction({
          collections: {}
        });

        cursor = trx.query('FOR i IN 1..10000000000000 RETURN i', {}, {}, {stream: true});
        let i = 0;
        while (cursor.hasNext() && i++ < 2000) {
          assertEqual(i, cursor.next());
        }

      } catch (err) {
        fail("Transaction failed with: " + JSON.stringify(err));
      } finally {
        if (cursor) {
          cursor.dispose();
        }
        if (trx) {
          trx.commit();
        }
      }
    },

    // //////////////////////////////////////////////////////////////////////////////
    // / @brief test: trx with embedded AQL
    // //////////////////////////////////////////////////////////////////////////////

    testAqlStreamQueries: function () {
      const queries = [
        `FOR doc IN ${cn} RETURN doc`,
        `FOR doc IN ${cn} FILTER doc.value1 == 1 UPDATE doc WITH { value1: 1 } INTO ${cn}`,
        `FOR doc IN ${cn} FILTER doc.value1 == 1 REPLACE doc WITH { value1: 2 } INTO ${cn}`,
        `FOR doc IN ${cn} FILTER doc.value1 == 2 INSERT { value2: doc.value1 } INTO ${cn}`,
        `FOR doc IN ${cn} FILTER doc.value1 == 2 INSERT { value2: doc.value1 } INTO ${cn}`,
        `FOR doc IN ${cn} FILTER doc.value3 >= 4 RETURN doc`,
        `FOR doc IN ${cn} COLLECT a = doc.value1 INTO groups = doc RETURN { val: a, doc: groups }`
      ];

      queries.forEach(q => {
        let trx, cursor;
        try {
          trx = db._createTransaction({collections: {write: cn}});
  
          cursor = trx.query(q, {}, {}, {stream: true});
          assertEqual(undefined, cursor.count());
          while (cursor.hasNext()) {
            cursor.next();
          }
        } catch (err) {
          fail("Transaction failed with: " + JSON.stringify(err));
        } finally {
          if (cursor) {
            cursor.dispose();
          }
          if (trx) {
            trx.commit();
          }
        }
      });
    },

    // //////////////////////////////////////////////////////////////////////////////
    // / @brief test: trx with embedded AQL
    // //////////////////////////////////////////////////////////////////////////////

    testAqlMultiWriteStream: function () {
      let trx, cursor1, cursor2;
      try {
        trx = db._createTransaction({
          collections: {
            write: [ cn, cn + 'Vertex']
          }
        });
        let tc = trx.collection(cn);

        let cursor1 = trx.query('FOR i IN @@cn FILTER i.value1 == 1 REMOVE i._key IN @@cn', { '@cn': cn }, {}, {stream:true});
        while (cursor1.hasNext()) {
          cursor1.next();
        }
        let extras = cursor1.getExtra();
        assertNotUndefined(extras);
        assertNotUndefined(extras.stats);
        assertEqual(500, extras.stats.writesExecuted);
        assertEqual(4500, tc.count());

        tc = trx.collection(cn + 'Vertex');
        cursor2 = trx.query('FOR i IN @@vc UPDATE {_key: i._key, updated:1} IN @@vc', { '@vc': cn + 'Vertex' }, {}, {stream:true});
        while (cursor2.hasNext()) {
          cursor2.next();
        }
        extras = cursor2.getExtra();
        assertNotUndefined(extras);
        assertNotUndefined(extras.stats);
        assertEqual(100, extras.stats.writesExecuted);
        assertEqual(100, tc.count());

      } catch (err) {
        fail("Transaction failed with: " + JSON.stringify(err));
      } finally {
        if (cursor1) {
          cursor1.dispose();
        }
        if (cursor2) {
          cursor2.dispose();
        }
        if (trx) {
          trx.commit();
        }
      }
    },

    // //////////////////////////////////////////////////////////////////////////////
    // / @brief test: use of undeclared traversal collection in transaction
    // //////////////////////////////////////////////////////////////////////////////

    testUndeclaredTraversalCollectionStream: function () {
      let result;
      // begin trx
      let trx = db._createTransaction({
        collections: {
          read: [ cn + 'Edge' ],
          write: [ cn + 'Edge' ]
        }
      });
      try {
        let tedge = trx.collection(cn + 'Edge');

        let results = trx.query('WITH ' + cn + 'Vertex FOR v, e IN ANY "' + cn + 'Vertex/20" ' + 
                            cn + 'Edge FILTER v._id == "' + cn + 
                            'Vertex/21" LIMIT 1 RETURN e', {}, {}, {stream: true}).toArray();

        if (results.length > 0) {
          result = results[0];
          tedge.remove(result);
          return 1;
        }
      } catch (err) {
        fail("Transaction failed with: " + JSON.stringify(err));
      } finally {
        trx.commit();
      }
      // end trx

      assertEqual(1, result);
    }
  };
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief test suite
// //////////////////////////////////////////////////////////////////////////////

function transactionTTLStreamSuite () {
  'use strict';
  const cn = 'UnitTestsTransaction';
  let c;

  return {

    // //////////////////////////////////////////////////////////////////////////////
    // / @brief set up
    // //////////////////////////////////////////////////////////////////////////////

    setUp: function () {
      db._drop(cn);
      c = db._create(cn, {numberOfShards: 2, replicationFactor: 2});
    },

    // //////////////////////////////////////////////////////////////////////////////
    // / @brief tear down
    // //////////////////////////////////////////////////////////////////////////////

    tearDown: function () {
      db._drop(cn);
    },


    // //////////////////////////////////////////////////////////////////////////////
    // / @brief test: abort idle transactions
    // //////////////////////////////////////////////////////////////////////////////

    testAbortIdleTrx: function () {
      let trx = db._createTransaction({
        collections: { write: cn }
      });

      trx.collection(cn).save({value:'val'});

      let x = 60;
      do {
        internal.sleep(1);

        if (trx.status().status === "aborted") {
          return;
        }

      } while(--x > 0);
      if (x <= 0) {
        fail(); // should not be reached
      }
    }
  };
}

function transactionIteratorSuite() {
  'use strict';
  var cn = 'UnitTestsTransaction';
  var c = null;

  return {

    setUp: function () {
      db._drop(cn);
      c = db._create(cn, { numberOfShards: 4 });
    },

    tearDown: function () {
      db._drop(cn);
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief test: make sure forward iterators respect bounds in write transaction
    ////////////////////////////////////////////////////////////////////////////////

    testIteratorBoundsForward: function () {
      c.ensureIndex({ type: "persistent", fields: ["value1"] });
      c.ensureIndex({ type: "persistent", fields: ["value2"] });
      let res = c.getIndexes();
      assertEqual(3, res.length);

      const opts = {
        collections: {
          write: [cn]
        }
      };
      const trx = internal.db._createTransaction(opts);

      const tc = trx.collection(cn);
      let docs = [];
      for (let i = 0; i < 100; ++i) {
        docs.push({ value1: i, value2: (100 - i) });
      }
      tc.save(docs);

      const cur = trx.query('FOR doc IN @@c SORT doc.value1 ASC RETURN doc', { '@c': cn });

      const half = cur.toArray();
      assertEqual(half.length, 100);

      trx.commit();
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief test: make sure reverse iterators respect bounds in write transaction
    ////////////////////////////////////////////////////////////////////////////////

    testIteratorBoundsReverse: function () {
      c.ensureIndex({ type: "persistent", fields: ["value1"] });
      c.ensureIndex({ type: "persistent", fields: ["value2"] });
      let res = c.getIndexes();
      assertEqual(3, res.length);

      const opts = {
        collections: {
          write: [cn]
        }
      };
      const trx = internal.db._createTransaction(opts);

      const tc = trx.collection(cn);
      let docs = [];
      for (let i = 0; i < 100; ++i) {
        docs.push({ value1: i, value2: (100 - i) });
      }
      tc.save(docs);

      const cur = trx.query('FOR doc IN @@c SORT doc.value2 DESC RETURN doc', { '@c': cn });

      const half = cur.toArray();
      assertEqual(half.length, 100);

      trx.commit();
    },
    
  };
}

function transactionOverlapSuite() {
  'use strict';
  const cn = 'UnitTestsTransaction';

  return {

    setUpAll: function () {
      db._drop(cn);
      db._create(cn, { numberOfShards: 4 });
    },
    setUp: function () {
      db[cn].truncate({ compact: false });
    },
    tearDown: function () {
      db[cn].truncate({ compact: false });
    },
    tearDownAll: function () {
      db._drop(cn);
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief test: overlapping transactions writing to the same document
    ////////////////////////////////////////////////////////////////////////////////

    testOverlapInsert: function () {
      const opts = {
        collections: {
          write: [cn]
        }
      };
      const trx1 = internal.db._createTransaction(opts);
      try {
        const tc1 = trx1.collection(cn);
        tc1.insert({ _key: "test" });

        const trx2 = internal.db._createTransaction(opts);
        const tc2 = trx2.collection(cn);
        try { 
          // should produce a conflict
          tc2.insert({ _key: "test" });
          fail();
        } catch (err) {
          assertEqual(internal.errors.ERROR_ARANGO_CONFLICT.code, err.errorNum);
        } finally { 
          trx2.abort();
        }
      } finally {
        trx1.abort();
      }
    },
    
    testOverlapUpdate: function () {
      db[cn].insert({ _key: "test" });

      const opts = {
        collections: {
          write: [cn]
        }
      };
      const trx1 = internal.db._createTransaction(opts);
      try {
        const tc1 = trx1.collection(cn);
        tc1.update("test", { value: "der fux" });

        const trx2 = internal.db._createTransaction(opts);
        const tc2 = trx2.collection(cn);
        try { 
          // should produce a conflict
          tc2.update("test", { value: "der hans" });
          fail();
        } catch (err) {
          assertEqual(internal.errors.ERROR_ARANGO_CONFLICT.code, err.errorNum);
        } finally { 
          trx2.abort();
        }
      } finally {
        trx1.abort();
      }
    },
    
    testOverlapReplace: function () {
      db[cn].insert({ _key: "test" });

      const opts = {
        collections: {
          write: [cn]
        }
      };
      const trx1 = internal.db._createTransaction(opts);
      try {
        const tc1 = trx1.collection(cn);
        tc1.replace("test", { value: "der fux" });

        const trx2 = internal.db._createTransaction(opts);
        const tc2 = trx2.collection(cn);
        try { 
          // should produce a conflict
          tc2.replace("test", { value: "der hans" });
          fail();
        } catch (err) {
          assertEqual(internal.errors.ERROR_ARANGO_CONFLICT.code, err.errorNum);
        } finally { 
          trx2.abort();
        }
      } finally {
        trx1.abort();
      }
    },
    
    testOverlapRemove: function () {
      db[cn].insert({ _key: "test" });

      const opts = {
        collections: {
          write: [cn]
        }
      };
      const trx1 = internal.db._createTransaction(opts);
      try {
        const tc1 = trx1.collection(cn);
        tc1.remove("test");

        const trx2 = internal.db._createTransaction(opts);
        const tc2 = trx2.collection(cn);
        try { 
          // should produce a conflict
          tc2.remove("test");
          fail();
        } catch (err) {
          assertEqual(internal.errors.ERROR_ARANGO_CONFLICT.code, err.errorNum);
        } finally { 
          trx2.abort();
        }
      } finally {
        trx1.abort();
      }
    },

  };
}

function transactionDatabaseSuite() {
  'use strict';
  const cn = 'UnitTestsTransaction';
  const dbn1 = 'UnitTestsTransactionDatabase1';
  const dbn2 = 'UnitTestsTransactionDatabase2';

  return {

    setUpAll: function() {
      [dbn1, dbn2].map(dbn => {
        db._useDatabase("_system");
        db._createDatabase(dbn);
        db._useDatabase(dbn);
        db._drop(cn);
        db._create(cn, {
          numberOfShards: 4
        });
      });
    },

    tearDownAll: function() {
      [dbn1, dbn2].map(dbn => {
        db._useDatabase("_system");
        db._dropDatabase(dbn);
      });
    },

    testTransactionStatus: function() {
      db._useDatabase(dbn1);
      const opts = {
        collections: {
          write: [cn]
        }
      };
      const trx1 = db._createTransaction(opts);
      const status1 = trx1.status();
      try {
        db._useDatabase(dbn2);
        const trx2 = new ArangoTransaction(db, trx1);
        trx1._database = db;
        const status2 = trx2.status();
        assertFalse(true);
      } catch (err) {
        assertTrue(err.error);
        assertEqual(err.code, 404);
        assertEqual(err.errorNum, 1655);
      } finally {
        db._useDatabase(dbn1);
        trx1.abort();
      }
    },

    testTransactionList: function() {
      db._useDatabase(dbn1);
      const opts = {
        collections: {
          write: [cn]
        }
      };
      const trx1 = db._createTransaction(opts);
      try {
        db._useDatabase(dbn2);
        const res = db._transactions();
        assertTrue(Array.isArray(res));
        assertEqual(res.length, 0);
      } finally {
        db._useDatabase(dbn1);
        trx1.abort();
      }
    },

    testTransactionCommit: function() {
      db._useDatabase(dbn1);
      const opts = {
        collections: {
          write: [cn]
        }
      };
      const trx1 = db._createTransaction(opts);

      try {
        db._useDatabase(dbn2);
        const trx2 = new ArangoTransaction(db, trx1);
        trx1._database = db;
        const res = trx2.commit();
        assertFalse(true);
      } catch (err) {
        assertTrue(err.error);
        assertEqual(err.code, 404);
        assertEqual(err.errorNum, 1655);
      } finally {
        db._useDatabase(dbn1);
        trx1.abort();
      }
    },

    testTransactionAbort: function() {
      db._useDatabase(dbn1);
      const opts = {
        collections: {
          write: [cn]
        }
      };
      const trx1 = db._createTransaction(opts);

      try {
        db._useDatabase(dbn2);
        const trx2 = new ArangoTransaction(db, trx1);
        trx1._database = db;
        const res = trx2.abort();
        assertFalse(true);
      } catch (err) {
        assertTrue(err.error);
        assertEqual(err.code, 404);
        assertEqual(err.errorNum, 1655);
      } finally {
        db._useDatabase(dbn1);
        trx1.abort();
      }
    },

  };
}

jsunity.run(transactionRevisionsSuite);
jsunity.run(transactionRollbackSuite);
jsunity.run(transactionInvocationSuite);
jsunity.run(transactionCollectionsSuite);
jsunity.run(transactionOperationsSuite);
jsunity.run(transactionBarriersSuite);
jsunity.run(transactionCountSuite);
jsunity.run(transactionCrossCollectionSuite);
jsunity.run(transactionTraversalSuite);
jsunity.run(transactionAQLStreamSuite);
jsunity.run(transactionTTLStreamSuite);
jsunity.run(transactionIteratorSuite);
jsunity.run(transactionOverlapSuite);
jsunity.run(transactionDatabaseSuite);

return jsunity.done();
