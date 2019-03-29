/* jshint globalstrict:false, strict:false, maxlen: 200 */
/* global fail, assertTrue, assertFalse, assertEqual, assertNotUndefined */

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

var jsunity = require('jsunity');
var internal = require('internal');
var arangodb = require('@arangodb');
var db = arangodb.db;
var testHelper = require('@arangodb/test-helper').Helper;

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

    setUp: function () {
      db._drop(cn);
      c = db._create(cn);
    },

    tearDown: function () {

      if (c !== null) {
        c.drop();
      }

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
        fail();
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
        fail();
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
        fail();
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
        fail();
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
        fail();
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
        fail();
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
        fail();
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
  return {

    // //////////////////////////////////////////////////////////////////////////////
    // / @brief set up
    // //////////////////////////////////////////////////////////////////////////////

    setUp: function () {
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
        0,
        1,
        'foo',
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
            trx.abort();
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
          fail();
        } finally {
          if (trx) {
            trx.abort();
          }
        }
      });
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

    setUp: function () {
      db._drop(cn1);
      c1 = db._create(cn1);

      db._drop(cn2);
      c2 = db._create(cn2);
    },

    // //////////////////////////////////////////////////////////////////////////////
    // / @brief tear down
    // //////////////////////////////////////////////////////////////////////////////

    tearDown: function () {
      if (c1 !== null) {
        c1.drop();
      }

      c1 = null;

      if (c2 !== null) {
        c2.drop();
      }

      c2 = null;
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
        fail();
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
        fail();
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
        fail();
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
        fail();
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
        fail();
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
        fail();
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
        fail();
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
        fail();
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
        fail();
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
        fail();
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
        fail();
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
        fail();
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
        fail();
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
        fail();
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
        fail();
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
        fail();
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
        assertEqual(10, tc2.count());

      } catch(err) {
        fail();
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
        fail();
      } finally {
        if (trx) {
          trx.commit();
        }
      }

      assertEqual(0, c1.count());
      assertEqual(0, c2.count());
    }

  };
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief test suite
// //////////////////////////////////////////////////////////////////////////////

function transactionOperationsSuite () {
  'use strict';
  var cn1 = 'UnitTestsTransaction1';
  var cn2 = 'UnitTestsTransaction2';

  var c1 = null;
  var c2 = null;

  return {

    // //////////////////////////////////////////////////////////////////////////////
    // / @brief set up
    // //////////////////////////////////////////////////////////////////////////////

    setUp: function () {
      db._drop(cn1);
      db._drop(cn2);
    },

    // //////////////////////////////////////////////////////////////////////////////
    // / @brief tear down
    // //////////////////////////////////////////////////////////////////////////////

    tearDown: function () {
      if (c1 !== null) {
        c1.drop();
      }

      c1 = null;

      if (c2 !== null) {
        c2.drop();
      }

      c2 = null;
      internal.wait(0);
    },

    // //////////////////////////////////////////////////////////////////////////////
    // / @brief test: trx with read operation
    // //////////////////////////////////////////////////////////////////////////////

    testSingleRead1: function () {
      c1 = db._create(cn1);
      c1.save({ _key: 'foo', a: 1 });

      let obj = {
        collections: {
          read: [ cn1 ]
        }
      };

      let trx;
      try {
        trx = db._createTransaction(obj);
        let tc1 = trx.collection(c1.name());
        assertEqual(1, tc1.document('foo').a);
      } catch(err) {
        fail();
      } finally {
        if (trx) {
          trx.commit();
        }
      }

      assertEqual(1, c1.count());
      assertEqual([ 'foo' ], sortedKeys(c1));
    },

    // //////////////////////////////////////////////////////////////////////////////
    // / @brief test: trx with read operation
    // //////////////////////////////////////////////////////////////////////////////

    testSingleRead2: function () {
      c1 = db._create(cn1);
      c1.save({ _key: 'foo', a: 1 });

      let obj = {
        collections: {
          write: [ cn1 ]
        }
      };

      let trx;
      try {
        trx = db._createTransaction(obj);
        let tc1 = trx.collection(c1.name());
        assertEqual(1, tc1.document('foo').a);
      } catch(err) {
        fail();
      } finally {
        if (trx) {
          trx.commit();
        }
      }

      assertEqual(1, c1.count());
      assertEqual([ 'foo' ], sortedKeys(c1));
    },

    // //////////////////////////////////////////////////////////////////////////////
    // / @brief test: trx with read operation
    // //////////////////////////////////////////////////////////////////////////////

    testScan1: function () {
      c1 = db._create(cn1);
      for (let i = 0; i < 100; ++i) {
        c1.save({ _key: 'foo' + i, a: i });
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
        // TODO enable with cursor el-cheapification
        //assertEqual(100, c1.toArray().length);
        assertEqual(100, tc1.count());
        for (let i = 0; i < 100; ++i) {
          assertEqual(i, tc1.document('foo' + i).a);
        }
      } catch(err) {
        fail();
      } finally {
        if (trx) {
          trx.commit();
        }
      }

      assertEqual(100, c1.count());
    },

    // //////////////////////////////////////////////////////////////////////////////
    // / @brief test: trx with read operation
    // //////////////////////////////////////////////////////////////////////////////

    testScan2: function () {
      c1 = db._create(cn1);
      for (let i = 0; i < 100; ++i) {
        c1.save({ _key: 'foo' + i, a: i });
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
        
        //assertEqual(100, tc1.toArray().length);
        assertEqual(100, tc1.count());

        for (let i = 0; i < 100; ++i) {
          assertEqual(i, tc1.document('foo' + i).a);
        }

      } catch(err) {
        fail();
      } finally {
        if (trx) {
          trx.commit();
        }
      }

      assertEqual(100, c1.count());
    },

    // //////////////////////////////////////////////////////////////////////////////
    // / @brief test: trx with insert operation
    // //////////////////////////////////////////////////////////////////////////////

    testSingleInsert: function () {
      c1 = db._create(cn1);

      let obj = {
        collections: {
          write: [ cn1 ]
        }
      };

      let trx;
      try {
        trx = db._createTransaction(obj);
        let tc1 = trx.collection(c1.name());
        
        tc1.save({ _key: 'foo' });

      } catch(err) {
        fail();
      } finally {
        if (trx) {
          trx.commit();
        }
      }

      assertEqual(1, c1.count());
      assertEqual([ 'foo' ], sortedKeys(c1));
    },

    // //////////////////////////////////////////////////////////////////////////////
    // / @brief test: trx with insert operation
    // //////////////////////////////////////////////////////////////////////////////

    testMultiInsert: function () {
      c1 = db._create(cn1);

      let obj = {
        collections: {
          write: [ cn1 ]
        }
      };
      
      let trx;
      try {
        trx = db._createTransaction(obj);
        let tc1 = trx.collection(c1.name());
        
        tc1.save({ _key: 'foo' });
        tc1.save({ _key: 'bar' });

      } catch(err) {
        fail();
      } finally {
        if (trx) {
          trx.commit();
        }
      }

      assertEqual(2, c1.count());
      assertEqual([ 'bar', 'foo' ], sortedKeys(c1));
    },

    // //////////////////////////////////////////////////////////////////////////////
    // / @brief test: trx with insert operation
    // //////////////////////////////////////////////////////////////////////////////

    testInsertWithExisting: function () {
      c1 = db._create(cn1);
      c1.save({ _key: 'foo' });
      c1.save({ _key: 'bar' });

      let obj = {
        collections: {
          write: [ cn1 ]
        }
      };

      let trx;
      try {
        trx = db._createTransaction(obj);
        let tc1 = trx.collection(c1.name());
        
        tc1.save({ _key: 'baz' });
        tc1.save({ _key: 'bam' });

      } catch(err) {
        fail();
      } finally {
        if (trx) {
          trx.commit();
        }
      }

      assertEqual(4, c1.count());
      assertEqual([ 'bam', 'bar', 'baz', 'foo' ], sortedKeys(c1));
    },

    // //////////////////////////////////////////////////////////////////////////////
    // / @brief test: trx with replace operation
    // //////////////////////////////////////////////////////////////////////////////

    testReplace: function () {
      c1 = db._create(cn1);
      c1.save({ _key: 'foo', a: 1 });
      c1.save({ _key: 'bar', b: 2, c: 3 });

      let obj = {
        collections: {
          write: [ cn1 ]
        }
      };

      let trx;
      try {
        trx = db._createTransaction(obj);
        let tc1 = trx.collection(c1.name());
        
        assertEqual(1, tc1.document('foo').a);
        tc1.replace('foo', { a: 3 });

        assertEqual(2, tc1.document('bar').b);
        assertEqual(3, tc1.document('bar').c);
        tc1.replace('bar', { b: 9 });

      } catch(err) {
        fail();
      } finally {
        if (trx) {
          trx.commit();
        }
      }

      assertEqual(2, c1.count());
      assertEqual([ 'bar', 'foo' ], sortedKeys(c1));
      assertEqual(3, c1.document('foo').a);
      assertEqual(9, c1.document('bar').b);
      assertEqual(undefined, c1.document('bar').c);
    },

    // //////////////////////////////////////////////////////////////////////////////
    // / @brief test: trx with replace operation
    // //////////////////////////////////////////////////////////////////////////////

    testReplaceReplace: function () {
      c1 = db._create(cn1);
      c1.save({ _key: 'foo', a: 1 });

      let obj = {
        collections: {
          write: [ cn1 ]
        }
      };

      let trx;
      try {
        trx = db._createTransaction(obj);
        let tc1 = trx.collection(c1.name());
        
        assertEqual(1, tc1.document('foo').a);
        tc1.replace('foo', { a: 3 });
        assertEqual(3, tc1.document('foo').a);
        tc1.replace('foo', { a: 4 });
        assertEqual(4, tc1.document('foo').a);
        tc1.replace('foo', { a: 5 });
        assertEqual(5, tc1.document('foo').a);
        tc1.replace('foo', { a: 6, b: 99 });
        assertEqual(6, tc1.document('foo').a);
        assertEqual(99, tc1.document('foo').b);

      } catch(err) {
        fail();
      } finally {
        if (trx) {
          trx.commit();
        }
      }

      assertEqual(1, c1.count());
      assertEqual(6, c1.document('foo').a);
      assertEqual(99, c1.document('foo').b);
    },

    // //////////////////////////////////////////////////////////////////////////////
    // / @brief test: trx with update operation
    // //////////////////////////////////////////////////////////////////////////////

    testUpdate: function () {
      c1 = db._create(cn1);
      c1.save({ _key: 'foo', a: 1 });
      c1.save({ _key: 'bar', b: 2 });
      c1.save({ _key: 'baz', c: 3 });
      c1.save({ _key: 'bam', d: 4 });

      let obj = {
        collections: {
          write: [ cn1 ]
        }
      };
      
      let trx;
      try {
        trx = db._createTransaction(obj);
        let tc1 = trx.collection(c1.name());
        
        assertEqual(1, tc1.document('foo').a);
        tc1.update('foo', { a: 3 });

        assertEqual(2, tc1.document('bar').b);
        tc1.update('bar', { b: 9 });

        assertEqual(3, tc1.document('baz').c);
        tc1.update('baz', { b: 9, c: 12 });

        assertEqual(4, tc1.document('bam').d);

      } catch(err) {
        fail();
      } finally {
        if (trx) {
          trx.commit();
        }
      }

      assertEqual(4, c1.count());
      assertEqual([ 'bam', 'bar', 'baz', 'foo' ], sortedKeys(c1));
      assertEqual(3, c1.document('foo').a);
      assertEqual(9, c1.document('bar').b);
      assertEqual(9, c1.document('baz').b);
      assertEqual(12, c1.document('baz').c);
      assertEqual(4, c1.document('bam').d);
    },

    // //////////////////////////////////////////////////////////////////////////////
    // / @brief test: trx with remove operation
    // //////////////////////////////////////////////////////////////////////////////

    testRemove: function () {
      c1 = db._create(cn1);
      c1.save({ _key: 'foo', a: 1 });
      c1.save({ _key: 'bar', b: 2 });
      c1.save({ _key: 'baz', c: 3 });

      let obj = {
        collections: {
          write: [ cn1 ]
        }
      };
      
      let trx;
      try {
        trx = db._createTransaction(obj);
        let tc1 = trx.collection(c1.name());
        
        tc1.remove('foo');
        tc1.remove('baz');

      } catch(err) {
        fail();
      } finally {
        if (trx) {
          trx.commit();
        }
      }

      assertEqual(1, c1.count());
      assertEqual([ 'bar' ], sortedKeys(c1));
    },

    // //////////////////////////////////////////////////////////////////////////////
    // / @brief test: trx with truncate operation
    // //////////////////////////////////////////////////////////////////////////////

    testTruncateEmpty: function () {
      c1 = db._create(cn1);

      let obj = {
        collections: {
          write: [ cn1 ]
        }
      };

           
      let trx;
      try {
        trx = db._createTransaction(obj);
        let tc1 = trx.collection(c1.name());
        
        tc1.truncate();

      } catch(err) {
        fail();
      } finally {
        if (trx) {
          trx.commit();
        }
      }

      assertEqual(0, c1.count());
      assertEqual([ ], sortedKeys(c1));
    },

    // //////////////////////////////////////////////////////////////////////////////
    // / @brief test: trx with truncate operation
    // //////////////////////////////////////////////////////////////////////////////

    testTruncateNonEmpty: function () {
      c1 = db._create(cn1);

      for (let i = 0; i < 100; ++i) {
        c1.save({ a: i });
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
        
        tc1.truncate();

      } catch(err) {
        fail();
      } finally {
        if (trx) {
          trx.commit();
        }
      }

      assertEqual(0, c1.count());
      assertEqual([ ], sortedKeys(c1));
    },

    // //////////////////////////////////////////////////////////////////////////////
    // / @brief test: trx with truncate operation
    // //////////////////////////////////////////////////////////////////////////////

    testTruncateAndAdd: function () {
      c1 = db._create(cn1);

      for (let i = 0; i < 100; ++i) {
        c1.save({ a: i });
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
        
        tc1.truncate();
        tc1.save({ _key: 'foo' });

      } catch(err) {
        fail();
      } finally {
        if (trx) {
          trx.commit();
        }
      }

      assertEqual(1, c1.count());
      assertEqual([ 'foo' ], sortedKeys(c1));
    }

  };
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief test suite
// //////////////////////////////////////////////////////////////////////////////

function transactionBarriersSuite () {
  'use strict';
  var cn1 = 'UnitTestsTransaction1';
  var cn2 = 'UnitTestsTransaction2';

  var c1 = null;
  var c2 = null;

  return {

    // //////////////////////////////////////////////////////////////////////////////
    // / @brief set up
    // //////////////////////////////////////////////////////////////////////////////

    setUp: function () {
      db._drop(cn1);
      db._drop(cn2);
    },

    // //////////////////////////////////////////////////////////////////////////////
    // / @brief tear down
    // //////////////////////////////////////////////////////////////////////////////

    tearDown: function () {
      if (c1 !== null) {
        c1.drop();
      }

      c1 = null;

      if (c2 !== null) {
        c2.drop();
      }

      c2 = null;
      internal.wait(0);
    },

    // //////////////////////////////////////////////////////////////////////////////
    // / @brief test: usage of barriers outside of transaction
    // //////////////////////////////////////////////////////////////////////////////

    testBarriersOutsideCommit: function () {
      c1 = db._create(cn1);

      let docs = [ ];

      let obj = {
        collections: {
          write: [ cn1 ]
        }
      };

      let result;
      // begin trx
      let trx = db._createTransaction(obj);
      try {
        let tc1 = trx.collection(c1.name());
        
        for (let i = 0; i < 100; ++i) {
          tc1.save({ _key: 'foo' + i, value1: i, value2: 'foo' + i + 'x' });
        }

        for (let i = 0; i < 100; ++i) {
          docs.push(tc1.document('foo' + i));
        }
        result = tc1.document('foo0');

      } finally {
        trx.commit();
      }
      // end trx

      assertEqual(100, docs.length);
      assertEqual(100, c1.count());

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
      c1 = db._create(cn1);

      let docs = [ ];
      let obj = {
        collections: {
          write: [ cn1 ]
        }
      };

      let result;
      // begin trx
      let trx = db._createTransaction(obj);
      try {
        let tc1 = trx.collection(c1.name());
        
        for (let i = 0; i < 100; ++i) {
          tc1.save({ _key: 'foo' + i, value1: i, value2: 'foo' + i + 'x' });
        }

        for (let i = 0; i < 100; ++i) {
          docs.push(tc1.document('foo' + i));
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
      assertEqual(c1.count(), 0);
    }

  };
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief test suite
// //////////////////////////////////////////////////////////////////////////////

function transactionRollbackSuite () {
  'use strict';
  var cn1 = 'UnitTestsTransaction1';

  var c1 = null;

  return {

    // //////////////////////////////////////////////////////////////////////////////
    // / @brief set up
    // //////////////////////////////////////////////////////////////////////////////

    setUp: function () {
      db._drop(cn1);
    },

    // //////////////////////////////////////////////////////////////////////////////
    // / @brief tear down
    // //////////////////////////////////////////////////////////////////////////////

    tearDown: function () {
      if (c1 !== null) {
        c1.drop();
      }

      c1 = null;
      internal.wait(0);
    },

    // //////////////////////////////////////////////////////////////////////////////
    // / @brief test: rollback after flush
    // //////////////////////////////////////////////////////////////////////////////

    testRollbackAfterFlush: function () {
      c1 = db._create(cn1);

      // begin trx
      let trx = db._createTransaction({
        collections: {
          write: [ cn1 ]
        }
      });
      try {
        let tc1 = trx.collection(c1.name());
        
        tc1.save({ _key: 'tom' });
        tc1.save({ _key: 'tim' });
        tc1.save({ _key: 'tam' });

        internal.wal.flush(true);
        assertEqual(3, tc1.count());
      } finally {
        trx.abort(); // rollback
      }
      // end trx

      assertEqual(0, c1.count());

      testHelper.waitUnload(c1);

      assertEqual(0, c1.count());
    },

    // //////////////////////////////////////////////////////////////////////////////
    // / @brief test: the collection revision id
    // //////////////////////////////////////////////////////////////////////////////

    // TODO revision is not a supported El-Cheapo API
    /*testRollbackRevision: function () {
      c1 = db._create(cn1);
      c1.save({ _key: 'foo' });

      let r = c1.revision();

      let obj = {
        collections: {
          write: [ cn1 ]
        }
      };

      // begin trx
      let trx = db._createTransaction(obj);
      try {
        let tc1 = trx.collection(c1.name());
        
        var _r = r;

        tc1.save({ _key: 'tom' });
        assertEqual(1, compareStringIds(tc1.revision(), _r));
        _r = tc1.revision();

        tc1.save({ _key: 'tim' });
        assertEqual(1, compareStringIds(tc1.revision(), _r));
        _r = tc1.revision();

        tc1.save({ _key: 'tam' });
        assertEqual(1, compareStringIds(tc1.revision(), _r));
        _r = c1.revision();

        tc1.remove('tam');
        assertEqual(1, compareStringIds(tc1.revision(), _r));
        _r = c1.revision();

        tc1.update('tom', { 'bom': true });
        assertEqual(1, compareStringIds(tc1.revision(), _r));
        _r = tc1.revision();

        tc1.remove('tom');
        assertEqual(1, compareStringIds(tc1.revision(), _r));
        // _r = c1.revision();
        
      } finally {
        trx.abort(); // rollback
      }
      // end trx

      assertEqual(1, c1.count());
      assertEqual(r, c1.revision());
    },*/

    // //////////////////////////////////////////////////////////////////////////////
    // / @brief test: rollback inserts
    // //////////////////////////////////////////////////////////////////////////////

    testRollbackInsert: function () {
      c1 = db._create(cn1);
      c1.save({ _key: 'foo' });
      c1.save({ _key: 'bar' });
      c1.save({ _key: 'meow' });

      let obj = {
        collections: {
          write: [ cn1 ]
        }
      };

      // begin trx
      let trx = db._createTransaction(obj);
      try {
        let tc1 = trx.collection(c1.name());
        
        tc1.save({ _key: 'tom' });
        tc1.save({ _key: 'tim' });
        tc1.save({ _key: 'tam' });

        assertEqual(6, tc1.count());
        
      } finally {
        trx.abort(); // rollback
      }
      // end trx

      assertEqual(3, c1.count());
      assertEqual([ 'bar', 'foo', 'meow' ], sortedKeys(c1));
    },

    // //////////////////////////////////////////////////////////////////////////////
    // / @brief test: rollback inserts w/ secondary indexes
    // //////////////////////////////////////////////////////////////////////////////

    testRollbackInsertSecondaryIndexes: function () {
      c1 = db._create(cn1);
      c1.save({ _key: 'foo', value: 'foo', a: 1 });
      c1.save({ _key: 'bar', value: 'bar', a: 1 });
      c1.save({ _key: 'meow', value: 'meow' });

      c1.ensureHashIndex('value');
      c1.ensureSkiplist('value');
      let good = false;

      let obj = {
        collections: {
          write: [ cn1 ]
        }
      };

      // begin trx
      let trx = db._createTransaction(obj);
      try {
        let tc1 = trx.collection(c1.name());
        
        tc1.save({ _key: 'tom', value: 'tom' });
        tc1.save({ _key: 'tim', value: 'tim' });
        tc1.save({ _key: 'tam', value: 'tam' });
        tc1.save({ _key: 'troet', value: 'foo', a: 2 });
        tc1.save({ _key: 'floxx', value: 'bar', a: 2 });

        assertEqual(8, tc1.count());

        good = true;
      } finally {
        trx.abort(); // rollback
      }
      // end trx

      assertEqual(true, good);

      assertEqual(3, c1.count());
    },

    // //////////////////////////////////////////////////////////////////////////////
    // / @brief test: rollback inserts
    // //////////////////////////////////////////////////////////////////////////////

    testRollbackInsertUpdate: function () {
      c1 = db._create(cn1);
      c1.save({ _key: 'foo' });
      c1.save({ _key: 'bar' });
      c1.save({ _key: 'meow' });

      let obj = {
        collections: {
          write: [ cn1 ]
        }
      };

      // begin trx
      let trx = db._createTransaction(obj);
      try {
        let tc1 = trx.collection(c1.name());
        
        tc1.save({ _key: 'tom' });
        tc1.save({ _key: 'tim' });
        tc1.save({ _key: 'tam' });

        tc1.update('tom', { });
        tc1.update('tim', { });
        tc1.update('tam', { });
        tc1.update('bar', { });
        tc1.remove('foo');
        tc1.remove('bar');
        tc1.remove('meow');
        tc1.remove('tom');

        assertEqual(2, tc1.count());
      } finally {
        trx.abort(); // rollback
      }
      // end trx

      assertEqual(3, c1.count());
      assertEqual([ 'bar', 'foo', 'meow' ], sortedKeys(c1));
    },

    // //////////////////////////////////////////////////////////////////////////////
    // / @brief test: rollback update
    // //////////////////////////////////////////////////////////////////////////////

    testRollbackUpdate: function () {
      var d1, d2, d3;

      c1 = db._create(cn1);
      d1 = c1.save({ _key: 'foo', a: 1 });
      d2 = c1.save({ _key: 'bar', a: 2 });
      d3 = c1.save({ _key: 'meow', a: 3 });

      let obj = {
        collections: {
          write: [ cn1 ]
        }
      };

      // begin trx
      let trx = db._createTransaction(obj);
      try {
        let tc1 = trx.collection(c1.name());
        
        tc1.update(d1, { a: 4 });
        tc1.update(d2, { a: 5 });
        tc1.update(d3, { a: 6 });

        assertEqual(3, tc1.count());

      } finally {
        trx.abort(); // rollback
      }
      // end trx

      assertEqual(3, c1.count());
      assertEqual([ 'bar', 'foo', 'meow' ], sortedKeys(c1));
      assertEqual(d1._rev, c1.document('foo')._rev);
      assertEqual(d2._rev, c1.document('bar')._rev);
      assertEqual(d3._rev, c1.document('meow')._rev);
    },

    // //////////////////////////////////////////////////////////////////////////////
    // / @brief test: rollback update
    // //////////////////////////////////////////////////////////////////////////////

    testRollbackUpdateUpdate: function () {
      var d1, d2, d3;

      c1 = db._create(cn1);
      d1 = c1.save({ _key: 'foo', a: 1 });
      d2 = c1.save({ _key: 'bar', a: 2 });
      d3 = c1.save({ _key: 'meow', a: 3 });

      let obj = {
        collections: {
          write: [ cn1 ]
        }
      };

      // begin trx
      let trx = db._createTransaction(obj);
      try {
        let tc1 = trx.collection(c1.name());
        
        for (let i = 0; i < 100; ++i) {
          tc1.replace('foo', { a: i });
          tc1.replace('bar', { a: i + 42 });
          tc1.replace('meow', { a: i + 23 });

          assertEqual(i, tc1.document('foo').a);
          assertEqual(i + 42, tc1.document('bar').a);
          assertEqual(i + 23, tc1.document('meow').a);
        }

        assertEqual(3, tc1.count());

      } finally {
        trx.abort(); // rollback
      }
      // end trx

      assertEqual(3, c1.count());
      assertEqual([ 'bar', 'foo', 'meow' ], sortedKeys(c1));
      assertEqual(1, c1.document('foo').a);
      assertEqual(2, c1.document('bar').a);
      assertEqual(3, c1.document('meow').a);
      assertEqual(d1._rev, c1.document('foo')._rev);
      assertEqual(d2._rev, c1.document('bar')._rev);
      assertEqual(d3._rev, c1.document('meow')._rev);
    },

    // //////////////////////////////////////////////////////////////////////////////
    // / @brief test: rollback remove w/ secondary indexes
    // //////////////////////////////////////////////////////////////////////////////

    testRollbackUpdateSecondaryIndexes: function () {
      c1 = db._create(cn1);
      c1.save({ _key: 'foo', value: 'foo', a: 1 });
      c1.save({ _key: 'bar', value: 'bar', a: 1 });
      c1.save({ _key: 'meow', value: 'meow' });

      c1.ensureHashIndex('value');
      c1.ensureSkiplist('value');
      let good = false;

      let obj = {
        collections: {
          write: [ cn1 ]
        }
      };

      // begin trx
      let trx = db._createTransaction(obj);
      try {
        let tc1 = trx.collection(c1.name());
        
        tc1.update('foo', { value: 'foo', a: 2 });
        tc1.update('bar', { value: 'bar', a: 2 });
        tc1.update('meow', { value: 'troet' });

        assertEqual(3, tc1.count());
        good = true;

      } finally {
        trx.abort(); // rollback
      }
      // end trx

      assertEqual(true, good);
      assertEqual(3, c1.count());
    },

    // //////////////////////////////////////////////////////////////////////////////
    // / @brief test: rollback remove
    // //////////////////////////////////////////////////////////////////////////////

    testRollbackRemove: function () {
      c1 = db._create(cn1);
      c1.save({ _key: 'foo' });
      c1.save({ _key: 'bar' });
      c1.save({ _key: 'meow' });

      let obj = {
        collections: {
          write: [ cn1 ]
        }
      };

      // begin trx
      let trx = db._createTransaction(obj);
      try {
        let tc1 = trx.collection(c1.name());
        
        tc1.remove('meow');
        tc1.remove('foo');

        assertEqual(1, tc1.count());
        assertTrue(tc1.document('bar') !== undefined);

      } finally {
        trx.abort(); // rollback
      }
      // end trx

      assertEqual(3, c1.count());
      assertEqual([ 'bar', 'foo', 'meow' ], sortedKeys(c1));
    },

    // //////////////////////////////////////////////////////////////////////////////
    // / @brief test: rollback remove
    // //////////////////////////////////////////////////////////////////////////////

    testRollbackRemoveMulti: function () {
      c1 = db._create(cn1);
      c1.save({ _key: 'foo' });

      let obj = {
        collections: {
          write: [ cn1 ]
        }
      };

      // begin trx
      let trx = db._createTransaction(obj);
      try {
        let tc1 = trx.collection(c1.name());
        
        for (let i = 0; i < 100; ++i) {
          tc1.save({ _key: 'foo' + i });
        }

        assertEqual(101, tc1.count());
      } finally {
        trx.abort(); // rollback
      }
      // end trx

      assertEqual(1, c1.count());
      assertEqual([ 'foo' ], sortedKeys(c1));
    },

    // //////////////////////////////////////////////////////////////////////////////
    // / @brief test: rollback remove w/ secondary indexes
    // //////////////////////////////////////////////////////////////////////////////

    testRollbackRemoveSecondaryIndexes: function () {
      c1 = db._create(cn1);
      c1.save({ _key: 'foo', value: 'foo', a: 1 });
      c1.save({ _key: 'bar', value: 'bar', a: 1 });
      c1.save({ _key: 'meow', value: 'meow' });

      c1.ensureHashIndex('value');
      c1.ensureSkiplist('value');
      var good = false;

      let obj = {
        collections: {
          write: [ cn1 ]
        }
      };

      // begin trx
      let trx = db._createTransaction(obj);
      try {
        let tc1 = trx.collection(c1.name());
        
        tc1.remove('meow');
        tc1.remove('bar');
        tc1.remove('foo');

        assertEqual(0, tc1.count());

        good = true;
        
      } finally {
        trx.abort(); // rollback
      }
      // end trx

      assertEqual(true, good);
      assertEqual(3, c1.count());
    },

    // //////////////////////////////////////////////////////////////////////////////
    // / @brief test: rollback insert/remove w/ secondary indexes
    // //////////////////////////////////////////////////////////////////////////////

    testRollbackRemoveInsertSecondaryIndexes: function () {
      c1 = db._create(cn1);
      c1.save({ _key: 'foo', value: 'foo', a: 1 });
      c1.save({ _key: 'bar', value: 'bar', a: 1 });
      c1.save({ _key: 'meow', value: 'meow' });

      c1.ensureHashIndex('value');
      c1.ensureSkiplist('value');
      let good = false;

      let obj = {
        collections: {
          write: [ cn1 ]
        }
      };

      // begin trx
      let trx = db._createTransaction(obj);
      try {
        let tc1 = trx.collection(c1.name());
        
        tc1.remove('meow');
        tc1.remove('bar');
        tc1.remove('foo');
        assertEqual(0, tc1.count());

        tc1.save({ _key: 'foo2', value: 'foo', a: 2 });
        tc1.save({ _key: 'bar2', value: 'bar', a: 2 });
        assertEqual(2, tc1.count());

        good = true;
      
      } finally {
        trx.abort(); // rollback
      }
      // end trx

      assertEqual(true, good);
      assertEqual(3, c1.count());
    },

    // //////////////////////////////////////////////////////////////////////////////
    // / @brief test: rollback truncate
    // //////////////////////////////////////////////////////////////////////////////

    testRollbackTruncateEmpty: function () {
      c1 = db._create(cn1);

      let obj = {
        collections: {
          write: [ cn1 ]
        }
      };

      // begin trx
      let trx = db._createTransaction(obj);
      try {
        let tc1 = trx.collection(c1.name());
        
        // truncate often...
        for (let i = 0; i < 100; ++i) {
          tc1.truncate();
        }
      
      } finally {
        trx.abort(); // rollback
      }
      // end trx

      assertEqual(0, c1.count());
      assertEqual([ ], sortedKeys(c1));
    },

    // //////////////////////////////////////////////////////////////////////////////
    // / @brief test: rollback truncate
    // //////////////////////////////////////////////////////////////////////////////

    testRollbackTruncateNonEmpty: function () {
      c1 = db._create(cn1);
      for (let i = 0; i < 100; ++i) {
        c1.save({ _key: 'foo' + i });
      }
      assertEqual(100, c1.count());

      let obj = {
        collections: {
          write: [ cn1 ]
        }
      };

      // begin trx
      let trx = db._createTransaction(obj);
      try {
        let tc1 = trx.collection(c1.name());
        
        // truncate often...
        for (let i = 0; i < 100; ++i) {
          tc1.truncate();
        }
        tc1.save({ _key: 'bar' });
      
      } finally {
        trx.abort(); // rollback
      }
      // end trx

      assertEqual(100, c1.count());
    },

    // //////////////////////////////////////////////////////////////////////////////
    // / @brief test: trx rollback with unique constraint violation
    // //////////////////////////////////////////////////////////////////////////////

    testRollbackUniquePrimary: function () {
      c1 = db._create(cn1);
      let d1 = c1.save({ _key: 'foo' });

      let obj = {
        collections: {
          write: [ cn1 ]
        }
      };

      // begin trx
      let trx = db._createTransaction(obj);
      try {
        let tc1 = trx.collection(c1.name());
        
        tc1.save({ _key: 'bar' });
        tc1.save({ _key: 'foo' });
        fail();

      } catch (err) {
        assertEqual(internal.errors.ERROR_ARANGO_UNIQUE_CONSTRAINT_VIOLATED.code, err.errorNum);
      } finally {
        trx.abort(); // rollback
      }
      // end trx

      assertEqual(1, c1.count());
      assertEqual('foo', c1.toArray()[0]._key);
      assertEqual(d1._rev, c1.toArray()[0]._rev);
    },

    // //////////////////////////////////////////////////////////////////////////////
    // / @brief test: trx rollback with unique constraint violation
    // //////////////////////////////////////////////////////////////////////////////

    testRollbackUniqueSecondary: function () {
      c1 = db._create(cn1);
      c1.ensureUniqueConstraint('name');
      let d1 = c1.save({ name: 'foo' });

      let obj = {
        collections: {
          write: [ cn1 ]
        }
      };

      // begin trx
      let trx = db._createTransaction(obj);
      try {
        let tc1 = trx.collection(c1.name());
        
        tc1.save({ name: 'bar' });
        tc1.save({ name: 'baz' });
        tc1.save({ name: 'foo' });
        fail();
      
      } catch (err) {
        assertEqual(internal.errors.ERROR_ARANGO_UNIQUE_CONSTRAINT_VIOLATED.code, err.errorNum);
      } finally {
        trx.abort(); // rollback
      }
      // end trx
      
      assertEqual(1, c1.count());
      assertEqual('foo', c1.toArray()[0].name);
      assertEqual(d1._rev, c1.toArray()[0]._rev);
    },

    // //////////////////////////////////////////////////////////////////////////////
    // / @brief test: rollback a mixed workload
    // //////////////////////////////////////////////////////////////////////////////

    testRollbackMixed1: function () {
      c1 = db._create(cn1);

      for (let i = 0; i < 100; ++i) {
        c1.save({ _key: 'key' + i, value: i });
      }

      let obj = {
        collections: {
          write: [ cn1 ]
        }
      };

      // begin trx
      let trx = db._createTransaction(obj);
      try {
        let tc1 = trx.collection(c1.name());
        
        for (let i = 0; i < 50; ++i) {
          tc1.remove('key' + i);
        }

        for (let i = 50; i < 100; ++i) {
          tc1.update('key' + i, { value: i - 50 });
        }

        tc1.remove('key50');
      
      } finally {
        trx.abort(); // rollback
      }
      // end trx

      assertEqual(100, c1.count());
    },

    // //////////////////////////////////////////////////////////////////////////////
    // / @brief test: rollback a mixed workload
    // //////////////////////////////////////////////////////////////////////////////

    testRollbackMixed2: function () {
      c1 = db._create(cn1);

      c1.save({ _key: 'foo' });
      c1.save({ _key: 'bar' });

      // begin trx
      let trx = db._createTransaction({
        collections: {
          write: [ cn1 ]
        }
      });
      try {
        let tc1 = trx.collection(c1.name());
        
        for (let i = 0; i < 10; ++i) {
          tc1.save({ _key: 'key' + i, value: i });
        }

        for (let i = 0; i < 5; ++i) {
          tc1.remove('key' + i);
        }

        for (let i = 5; i < 10; ++i) {
          tc1.update('key' + i, { value: i - 5 });
        }

        tc1.remove('key5');
      
      } finally {
        trx.abort(); // rollback
      }
      // end trx

      assertEqual(2, c1.count());
      assertEqual('foo', c1.document('foo')._key);
      assertEqual('bar', c1.document('bar')._key);
    },

    // //////////////////////////////////////////////////////////////////////////////
    // / @brief test: rollback a mixed workload
    // //////////////////////////////////////////////////////////////////////////////

    testRollbackMixed3: function () {
      c1 = db._create(cn1);

      c1.save({ _key: 'foo' });
      c1.save({ _key: 'bar' });

      let obj = {
        collections: {
          write: [ cn1 ]
        }
      };

      // begin trx
      let trx = db._createTransaction(obj);
      try {
        let tc1 = trx.collection(c1.name());
        
        for (let i = 0; i < 10; ++i) {
          tc1.save({ _key: 'key' + i, value: i });
        }

        for (let i = 0; i < 10; ++i) {
          tc1.remove('key' + i);
        }

        for (let i = 0; i < 10; ++i) {
          tc1.save({ _key: 'key' + i, value: i });
        }

        for (let i = 0; i < 10; ++i) {
          tc1.update('key' + i, { value: i - 5 });
        }

        for (let i = 0; i < 10; ++i) {
          tc1.update('key' + i, { value: i + 5 });
        }

        for (let i = 0; i < 10; ++i) {
          tc1.remove('key' + i);
        }

        for (let i = 0; i < 10; ++i) {
          tc1.save({ _key: 'key' + i, value: i });
        }
      
      } finally {
        trx.abort(); // rollback
      }
      // end trx

      assertEqual(2, c1.count());
      assertEqual('foo', c1.document('foo')._key);
      assertEqual('bar', c1.document('bar')._key);
    }

  };
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief test suite
// //////////////////////////////////////////////////////////////////////////////

function transactionCountSuite () {
  'use strict';
  var cn1 = 'UnitTestsTransaction1';

  var c1 = null;

  return {

    // //////////////////////////////////////////////////////////////////////////////
    // / @brief set up
    // //////////////////////////////////////////////////////////////////////////////

    setUp: function () {
      db._drop(cn1);
    },

    // //////////////////////////////////////////////////////////////////////////////
    // / @brief tear down
    // //////////////////////////////////////////////////////////////////////////////

    tearDown: function () {
      if (c1 !== null) {
        c1.drop();
      }

      c1 = null;
      internal.wait(0);
    },

    // //////////////////////////////////////////////////////////////////////////////
    // / @brief test: test counts during trx
    // //////////////////////////////////////////////////////////////////////////////

    testCountDuring: function () {
      c1 = db._create(cn1);
      assertEqual(0, c1.count());

      let obj = {
        collections: {
          write: [ cn1 ]
        }
      };

      // begin trx
      let trx = db._createTransaction(obj);
      try {
        let tc1 = trx.collection(c1.name());
        var d1, d2;

        tc1.save({ a: 1 });
        assertEqual(1, tc1.count());

        d1 = tc1.save({ a: 2 });
        assertEqual(2, tc1.count());

        d2 = tc1.update(d1, { a: 3 });
        assertEqual(2, tc1.count());

        assertEqual(3, tc1.document(d2).a);

        tc1.remove(d2);
        assertEqual(1, tc1.count());

        tc1.truncate();
        assertEqual(0, tc1.count());

        tc1.truncate();
        assertEqual(0, tc1.count());
      } finally {
        trx.commit();
      }
      
      // end trx
      
      assertEqual(0, c1.count());
    },

    // //////////////////////////////////////////////////////////////////////////////
    // / @brief test: test counts during and after trx
    // //////////////////////////////////////////////////////////////////////////////

    testCountCommitAfterFlush: function () {
      c1 = db._create(cn1);
      c1.save({ _key: 'foo' });
      c1.save({ _key: 'bar' });
      assertEqual(2, c1.count());

      let obj = {
        collections: {
          write: [ cn1 ]
        }
      };

      // begin trx
      let trx = db._createTransaction(obj);
      try {
        let tc1 = trx.collection(c1.name());

        tc1.save({ _key: 'baz' });
        assertEqual(3, tc1.count());

        internal.wal.flush(true, false);

        tc1.save({ _key: 'meow' });
        assertEqual(4, tc1.count());

        internal.wal.flush(true, false);

        tc1.remove('foo');
        assertEqual(3, tc1.count());
      } finally {
        trx.commit();
      }
      // end trx

      assertEqual(3, c1.count());
    },

    // //////////////////////////////////////////////////////////////////////////////
    // / @brief test: test counts during and after trx
    // //////////////////////////////////////////////////////////////////////////////

    testCountCommit: function () {
      c1 = db._create(cn1);
      c1.save({ _key: 'foo' });
      c1.save({ _key: 'bar' });
      assertEqual(2, c1.count());

      let obj = {
        collections: {
          write: [ cn1 ]
        }
      };

      // begin trx
      let trx = db._createTransaction(obj);
      try {
        let tc1 = trx.collection(c1.name());

        tc1.save({ _key: 'baz' });
        assertEqual(3, tc1.count());

        tc1.save({ _key: 'meow' });
        assertEqual(4, tc1.count());

        tc1.remove('foo');
        assertEqual(3, tc1.count());
      } finally {
        trx.commit();
      }
      // end trx

      assertEqual(3, c1.count());
    },

    // //////////////////////////////////////////////////////////////////////////////
    // / @brief test: test counts during and after trx
    // //////////////////////////////////////////////////////////////////////////////

    testCountRollback: function () {
      c1 = db._create(cn1);
      c1.save({ _key: 'foo' });
      c1.save({ _key: 'bar' });
      assertEqual(2, c1.count());

      let obj = {
        collections: {
          write: [ cn1 ]
        }
      };

      // begin trx
      let trx = db._createTransaction(obj);
      try {
        let tc1 = trx.collection(c1.name());

        tc1.save({ _key: 'baz' });
        assertEqual(3, tc1.count());

        tc1.save({ _key: 'meow' });
        assertEqual(4, tc1.count());

        tc1.remove('foo');
        assertEqual(3, tc1.count());
      } finally {
        trx.abort(); // rollback
      }
      // end trx

      assertEqual(2, c1.count());

      var keys = [ ];
      c1.toArray().forEach(function (d) {
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
  var cn1 = 'UnitTestsTransaction1';
  var cn2 = 'UnitTestsTransaction2';

  var c1 = null;
  var c2 = null;

  return {

    // //////////////////////////////////////////////////////////////////////////////
    // / @brief set up
    // //////////////////////////////////////////////////////////////////////////////

    setUp: function () {
      db._drop(cn1);
      db._drop(cn2);
    },

    // //////////////////////////////////////////////////////////////////////////////
    // / @brief tear down
    // //////////////////////////////////////////////////////////////////////////////

    tearDown: function () {
      if (c1 !== null) {
        c1.drop();
      }

      c1 = null;

      if (c2 !== null) {
        c2.drop();
      }

      c2 = null;
      internal.wait(0);
    },

    // //////////////////////////////////////////////////////////////////////////////
    // / @brief test: test cross collection commit
    // //////////////////////////////////////////////////////////////////////////////

    testInserts: function () {
      c1 = db._create(cn1);
      c2 = db._create(cn2);

      let obj = {
        collections: {
          write: [ cn1, cn2 ]
        }
      };

      // begin trx
      let trx = db._createTransaction(obj);
      try {
        let tc1 = trx.collection(c1.name());
        let tc2 = trx.collection(c2.name());

        for (let i = 0; i < 10; ++i) {
          tc1.save({ _key: 'a' + i });
          tc2.save({ _key: 'b' + i });
        }
      } finally {
        trx.commit();
      }
      // end trx

      assertEqual(10, c1.count());
      assertEqual(10, c2.count());
    },

    // //////////////////////////////////////////////////////////////////////////////
    // / @brief test: test cross collection commit
    // //////////////////////////////////////////////////////////////////////////////

    testUpdates: function () {
      c1 = db._create(cn1);
      c2 = db._create(cn2);

      var i;
      for (i = 0; i < 10; ++i) {
        c1.save({ _key: 'a' + i, a: i });
        c2.save({ _key: 'b' + i, b: i });
      }

      let obj = {
        collections: {
          write: [ cn1, cn2 ]
        }
      };

      // begin trx
      let trx = db._createTransaction(obj);
      try {
        let tc1 = trx.collection(c1.name());
        let tc2 = trx.collection(c2.name());

        for (i = 0; i < 10; ++i) {
          tc1.update('a' + i, { a: i + 20 });

          tc2.update('b' + i, { b: i + 20 });
          tc2.remove('b' + i);
        }
      } finally {
        trx.commit();
      }
      // end trx

      assertEqual(10, c1.count());
      assertEqual(0, c2.count());
    },

    // //////////////////////////////////////////////////////////////////////////////
    // / @brief test: test cross collection commit
    // //////////////////////////////////////////////////////////////////////////////

    testDeletes: function () {
      c1 = db._create(cn1);
      c2 = db._create(cn2);

      var i;
      for (i = 0; i < 10; ++i) {
        c1.save({ _key: 'a' + i, a: i });
        c2.save({ _key: 'b' + i, b: i });
      }

      let obj = {
        collections: {
          write: [ cn1, cn2 ]
        }
      };

      // begin trx
      let trx = db._createTransaction(obj);
      try {
        let tc1 = trx.collection(c1.name());
        let tc2 = trx.collection(c2.name());

        for (i = 0; i < 10; ++i) {
          tc1.remove('a' + i);
          tc2.remove('b' + i);
        }

      } finally {
        trx.commit();
      }
      // end trx

      assertEqual(0, c1.count());
      assertEqual(0, c2.count());
    },

    // //////////////////////////////////////////////////////////////////////////////
    // / @brief test: test cross collection commit
    // //////////////////////////////////////////////////////////////////////////////

    testDeleteReload: function () {
      c1 = db._create(cn1);
      c2 = db._create(cn2);

      var i;
      for (i = 0; i < 10; ++i) {
        c1.save({ _key: 'a' + i, a: i });
        c2.save({ _key: 'b' + i, b: i });
      }

      let obj = {
        collections: {
          write: [ cn1, cn2 ]
        }
      };

      // begin trx
      let trx = db._createTransaction(obj);
      try {
        let tc1 = trx.collection(c1.name());
        let tc2 = trx.collection(c2.name());

        for (i = 0; i < 10; ++i) {
          tc1.remove('a' + i);
          tc2.remove('b' + i);
        }

      } finally {
        trx.commit();
      }
      // end trx

      assertEqual(0, c1.count());
      assertEqual(0, c2.count());

      testHelper.waitUnload(c1);
      testHelper.waitUnload(c2);

      assertEqual(0, c1.count());
      assertEqual(0, c2.count());
    },

    // //////////////////////////////////////////////////////////////////////////////
    // / @brief test: test cross collection commit
    // //////////////////////////////////////////////////////////////////////////////

    testCommitReload: function () {
      c1 = db._create(cn1);
      c2 = db._create(cn2);

      let obj = {
        collections: {
          write: [ cn1, cn2 ]
        }
      };

      // begin trx
      let trx = db._createTransaction(obj);
      try {
        let tc1 = trx.collection(c1.name());
        let tc2 = trx.collection(c2.name());

        for (let i = 0; i < 10; ++i) {
          tc1.save({ _key: 'a' + i });
        }

        for (let i = 0; i < 10; ++i) {
          tc2.save({ _key: 'b' + i });
        }

        assertEqual(10, tc1.count());
        assertEqual(10, tc2.count());

        tc1.remove('a4');
        tc2.remove('b6');

      } finally {
        trx.commit();
      }
      // end trx

      assertEqual(9, c1.count());
      assertEqual(9, c2.count());

      testHelper.waitUnload(c1);
      testHelper.waitUnload(c2);

      assertEqual(9, c1.count());
      assertEqual(9, c2.count());
    },

    // //////////////////////////////////////////////////////////////////////////////
    // / @brief test: test cross collection rollback
    // //////////////////////////////////////////////////////////////////////////////

    testRollbackReload: function () {
      c1 = db._create(cn1);
      c2 = db._create(cn2);

      c1.save({ _key: 'a1' });
      c2.save({ _key: 'b1', a: 1 });

      let obj = {
        collections: {
          write: [ cn1, cn2 ]
        }
      };

      // begin trx
      let trx = db._createTransaction(obj);
      try {
        let tc1 = trx.collection(c1.name());
        let tc2 = trx.collection(c2.name());

        tc1.save({ _key: 'a2' });
        tc1.save({ _key: 'a3' });

        tc2.save({ _key: 'b2' });
        tc2.update('b1', { a: 2 });
        assertEqual(2, tc2.document('b1').a);

      } catch (err) {
        fail();
      } finally {
        trx.abort(); // rollback
      }
      // end trx

      assertEqual(1, c1.count());
      assertEqual(1, c2.count());

      assertEqual([ 'a1' ], sortedKeys(c1));
      assertEqual([ 'b1' ], sortedKeys(c2));
      assertEqual(1, c2.document('b1').a);

      c1.unload();
      c2.unload();
      testHelper.waitUnload(c1);
      testHelper.waitUnload(c2);

      assertEqual(1, c1.count());
      assertEqual(1, c2.count());

      assertEqual([ 'a1' ], sortedKeys(c1));
      assertEqual([ 'b1' ], sortedKeys(c2));
      assertEqual(1, c2.document('b1').a);
    },

    // //////////////////////////////////////////////////////////////////////////////
    // / @brief test: unload / reload after aborting transactions
    // //////////////////////////////////////////////////////////////////////////////

    testUnloadReloadAbortedTrx: function () {
      c1 = db._create(cn1);

      for (let i = 0; i < 10; ++i) {
        c1.save({ _key: 'a' + i, a: i });
      }

      let obj = {
        collections: {
          write: [ cn1 ]
        }
      };

      for (let i = 0; i < 50; ++i) {

        // begin trx
        let trx = db._createTransaction(obj);
        try {
          let tc1 = trx.collection(c1.name());

          for (let x = 0; x < 100; ++x) {
            tc1.save({ _key: 'test' + x });
          }
  
        } catch (err) {
          fail();
        } finally {
          trx.abort(); // rollback
        }
        // end trx
      }

      assertEqual(10, c1.count());

      testHelper.waitUnload(c1);

      assertEqual(10, c1.count());
    }

  };
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief test suite
// //////////////////////////////////////////////////////////////////////////////

function transactionTraversalSuite () {
  'use strict';
  var cn = 'UnitTestsTransaction';

  return {

    // //////////////////////////////////////////////////////////////////////////////
    // / @brief set up
    // //////////////////////////////////////////////////////////////////////////////

    setUp: function () {
      db._drop(cn + 'Vertex');
      db._drop(cn + 'Edge');
      db._create(cn + 'Vertex');
      db._createEdgeCollection(cn + 'Edge');

      var i;
      for (i = 0; i < 100; ++i) {
        db.UnitTestsTransactionVertex.insert({ _key: String(i) });
      }

      for (i = 1; i < 100; ++i) {
        db.UnitTestsTransactionEdge.insert(cn + 'Vertex/' + i, cn + 'Vertex/' + (i + 1), { });
      }
    },

    // //////////////////////////////////////////////////////////////////////////////
    // / @brief tear down
    // //////////////////////////////////////////////////////////////////////////////

    tearDown: function () {
      db._drop(cn + 'Vertex');
      db._drop(cn + 'Edge');
    },

    // //////////////////////////////////////////////////////////////////////////////
    // / @brief test: use of undeclared traversal collection in transaction
    // //////////////////////////////////////////////////////////////////////////////

    testUndeclaredTraversalCollection: function () {

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
                            'Vertex/21" LIMIT 1 RETURN e').toArray();

        if (results.length > 0) {
          result = results[0];
          tedge.remove(result);
          return 1;
        }
      } catch (err) {
        fail();
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
      for (let i = 0; i < 100; ++i) {
        db[cn + 'Edge'].insert(cn + 'Edge/test' + (i % 21), cn + 'Edge/test' + (i % 7), { });
      }

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
        const qq = "FOR e IN " + cn + "Edge FILTER e._from == @a AND e.request == false RETURN e";

        let from = cn + 'Edge/test1';
        let to = cn + 'Edge/test8';

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

    setUp: function () {
      db._drop(cn);
      db._drop(cn + 'Vertex');
      db._drop(cn + 'Edge');
      c = db._create(cn, {numberOfShards: 2, replicationFactor: 2});
      db._create(cn + 'Vertex', {numberOfShards: 2, replicationFactor: 2});
      db._createEdgeCollection(cn + 'Edge', {numberOfShards: 2, replicationFactor: 2});

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

      c.ensureIndex({ type: 'skiplist', fields: ["value1"]});
      c.ensureIndex({ type: 'skiplist', fields: ["value2"]});

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
      db._drop(cn);
      db._drop(cn + 'Vertex');
      db._drop(cn + 'Edge');
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

      } catch(err) {
        fail();
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
  
        } catch(err) {
          fail();
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

      } catch(err) {
        fail();
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
        fail();
      } finally {
        trx.commit();
      }
      // end trx

      assertEqual(1, result);
    }
  };
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief executes the test suites
// //////////////////////////////////////////////////////////////////////////////

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

return jsunity.done();
