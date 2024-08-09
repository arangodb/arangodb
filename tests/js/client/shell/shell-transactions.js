/* jshint globalstrict:false, strict:false, maxlen: 200 */
/* global fail, assertTrue, assertEqual, TRANSACTION, params, assertNotEqual, arango */

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
// / @author Jan Steemann
// / @author Copyright 2013, triAGENS GmbH, Cologne, Germany
// //////////////////////////////////////////////////////////////////////////////

const jsunity = require('jsunity');
const internal = require('internal');
const arangodb = require('@arangodb');
const db = arangodb.db;
const testHelper = require('@arangodb/test-helper').helper;
const {activateFailure} = require('@arangodb/test-helper');
const isCluster = internal.isCluster();
let IM = global.instanceManager;

let compareStringIds = function (l, r) {
  'use strict';
  if (l.length !== r.length) {
    return l.length - r.length < 0 ? -1 : 1;
  }

  // length is equal
  for (let i = 0; i < l.length; ++i) {
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

function transactionFailuresSuite () {
  'use strict';
  const cn = 'UnitTestsTransaction';
  let c = null;

  return {

    setUp: function () {
      IM.debugClearFailAt();
      db._drop(cn);
      c = db._create(cn);
    },

    tearDown: function () {
      IM.debugClearFailAt();

      db._drop(cn);
      c = null;
    },
    
    testCommitEmptyTransactionFailure : function () {
      c.insert({ _key: "foobar", value: "baz" });
      assertEqual(1, c.count());
      activateFailure("TransactionCommitFail");
      try {
        db._executeTransaction({
          collections: {
            write: cn 
          },
          action: function () {}
        });

        fail();
      } catch (err) {
        assertEqual(internal.errors.ERROR_DEBUG.code, err.errorNum);
      }

      IM.debugClearFailAt();
      assertEqual(1, c.count());
      assertEqual("baz", c.document("foobar").value);
    },

    testCommitTransactionWithRemovalsFailure : function () {
      let docs = [];
      for (let i = 0; i < 100; ++i) {
        docs.push({ _key: "test" + i });
      }
      c.insert(docs);
      assertEqual(100, c.count());
      
      activateFailure("TransactionCommitFail");
      try {
        db._executeTransaction({ 
          collections: {
            write: cn 
          },
          action: function () {
            let c = require('@arangodb').db._collection(params.cn);
            for (var i = 0; i < 100; ++i) {
              c.remove("test" + i);
            }
            require("jsunity").jsUnity.assertions.assertEqual(0, c.count());
          },
          params: {
            cn: cn
          }
        });

        fail();
      } catch (err) {
        assertEqual(internal.errors.ERROR_DEBUG.code, err.errorNum);
      }

      IM.debugClearFailAt();
      assertEqual(100, c.count());
    },
    
    testCommitTransactionWithFailuresInsideFailure : function () {
      c.insert({ _key: "foobar", value: "baz" });

      activateFailure("TransactionCommitFail");
      try {
        db._executeTransaction({ 
          collections: {
            write: cn 
          },
          action: function () {
            let c = require('@arangodb').db._collection(params.cn);
            for (var i = 0; i < 100; ++i) {
              try {
                // insert conflicting document
                c.insert({ _key: "foobar" });
                fail();
              } catch (err) {
              }
            }

            require("jsunity").jsUnity.assertions.assertEqual(1, c.count());
          },
          params: {
            cn: cn
          }
        });

        fail();
      } catch (err) {
        assertEqual(internal.errors.ERROR_DEBUG.code, err.errorNum);
      }

      IM.debugClearFailAt();
      assertEqual(1, c.count());
      assertEqual("baz", c.document("foobar").value);
    }

  };
}

function transactionRevisionsSuite () {
  'use strict';
  const cn = 'UnitTestsTransaction';
  let c = null;

  return {

    setUp: function () {
      IM.debugClearFailAt();
      db._drop(cn);
      c = db._create(cn);
    },

    tearDown: function () {
      IM.debugClearFailAt();
      db._drop(cn);
    },

    testInsertUniqueFailing: function () {
      c.insert({ _key: 'test', value: 1 });
      try {
        db._executeTransaction({
          collections: { write: c.name() },
          action: function () {
            let c = require('@arangodb').db._collection(params.cn);
            c.insert({ _key: 'test', value: 2 });
          },
          params: {
            cn: cn
          }
        });
        fail();
      } catch (err) {
      }

      assertEqual(1, c.count());
      assertEqual(1, c.toArray().length);
      assertEqual(1, c.document('test').value);
    },

    testInsertUniqueSingleFailing: function () {
      c.insert({ _key: 'test', value: 1 });
      try {
        c.insert({ _key: 'test', value: 2 });
        fail();
      } catch (err) {
      }

      assertEqual(1, c.count());
      assertEqual(1, c.toArray().length);
      assertEqual(1, c.document('test').value);
    },

    testInsertTransactionFailing: function () {
      c.insert({ _key: 'test', value: 1 });
      try {
        db._executeTransaction({
          collections: { write: c.name() },
          action: function () {
            let c = require('@arangodb').db._collection(params.cn);
            c.insert({ _key: 'test2', value: 2 });
            throw new Error('foo');
          },
          params: {
            cn: cn
          }
        });
        fail();
      } catch (err) {
      }

      assertEqual(1, c.toArray().length);
      assertEqual(1, c.document('test').value);
    },

    testRemoveTransactionFailing: function () {
      c.insert({ _key: 'test', value: 1 });
      try {
        db._executeTransaction({
          collections: { write: c.name() },
          action: function () {
            let c = require('@arangodb').db._collection(params.cn);
            c.remove('test');
            throw new Error('foo');
          }
        });
        fail();
      } catch (err) {
      }

      assertEqual(1, c.toArray().length);
      assertEqual(1, c.document('test').value);
    },

    testRemoveInsertWithSameRev: function () {
      if (isCluster) {
        // this test uses isRestore=true, which can interfere with the
        // LocalDocumentId values created by coordinators/DB servers.
        // running this test in cluster can trigger assertion failures
        // that validate LocalDocumentId values, which are faithfully
        // picked up from the incoming requests if isRestore=true.
        return;
      }
      var doc = c.insert({ _key: 'test', value: 1 });
      db._executeTransaction({
        collections: { write: cn },
        action: function () {
          let c = require('@arangodb').db._collection(params.cn);
          c.remove('test');
          c.insert({ _key: 'test', _rev: params.doc._rev, value: 2 }, { isRestore: true });
        },
        params: {
          cn: cn,
          doc: doc
        }
      });

      assertEqual(1, c.toArray().length);
      assertEqual(2, c.document('test').value);
    },

    testUpdateWithSameRev: function () {
      if (isCluster) {
        // this test uses isRestore=true, which can interfere with the
        // LocalDocumentId values created by coordinators/DB servers.
        // running this test in cluster can trigger assertion failures
        // that validate LocalDocumentId values, which are faithfully
        // picked up from the incoming requests if isRestore=true.
        return;
      }
      var doc = c.insert({ _key: 'test', value: 1 });
      c.update('test', { _key: 'test', _rev: doc._rev, value: 2 }, { isRestore: true });

      assertEqual(1, c.toArray().length);
      assertEqual(2, c.document('test').value);
    },

    testUpdateWithSameRevTransaction: function () {
      if (isCluster) {
        // this test uses isRestore=true, which can interfere with the
        // LocalDocumentId values created by coordinators/DB servers.
        // running this test in cluster can trigger assertion failures
        // that validate LocalDocumentId values, which are faithfully
        // picked up from the incoming requests if isRestore=true.
        return;
      }
      var doc = c.insert({ _key: 'test', value: 1 });
      db._executeTransaction({
        collections: { write: c.name() },
        action: function () {
          let c = require('@arangodb').db._collection(params.cn);
          c.update('test', { _key: 'test', _rev: params.doc._rev, value: 2 }, { isRestore: true });
        },
        params: {
          doc: doc,
          cn: cn
        }
      });

      assertEqual(1, c.toArray().length);
      assertEqual(2, c.document('test').value);
    },

    testUpdateFailingWithSameRev: function () {
      if (isCluster) {
        // running this test in cluster will trigger an assertion failure
        return;
      }
      var doc = c.insert({ _key: 'test', value: 1 });
      try {
        db._executeTransaction({
          collections: { write: c.name() },
          action: function () {
            let c = require('@arangodb').db._collection(params.cn);
            c.update('test', { _key: 'test', _rev: params.doc._rev, value: 2 }, { isRestore: true });
            throw new Error('foo');
          },
          params: {
            doc: doc,
            cn: cn
          }
        });
        fail();
      } catch (err) {
      }

      assertEqual(1, c.toArray().length);
      assertEqual(1, c.document('test').value);
    },

    testUpdateFailing: function () {
      c.insert({ _key: 'test', value: 1 });
      try {
        db._executeTransaction({
          collections: { write: c.name() },
          action: function () {
            c.update({ _key: 'test', value: 2 });
            throw new Error('foo');
          }
        });
        fail();
      } catch (err) {
      }

      assertEqual(1, c.toArray().length);
      assertEqual(1, c.document('test').value);
    },

    testUpdateAndInsertFailing: function () {
      c.insert({ _key: 'test', value: 1 });
      try {
        db._executeTransaction({
          collections: { write: c.name() },
          action: function () {
            c.update({ _key: 'test', value: 2 });
            c.insert({ _key: 'test', value: 3 });
            throw new Error('foo');
          }
        });
        fail();
      } catch (err) {
      }

      assertEqual(1, c.toArray().length);
      assertEqual(1, c.document('test').value);
    },

    testRemoveAndInsert: function () {
      c.insert({ _key: 'test', value: 1 });
      db._executeTransaction({
        collections: { write: cn },
        action: function () {
          let c = require('@arangodb').db._collection(params.cn);
          c.remove('test');
          c.insert({ _key: 'test', value: 2 });
        },
        params: {
          cn: cn
        }
      });

      assertEqual(1, c.toArray().length);
      assertEqual(2, c.document('test').value);
    },

    testRemoveAndInsertFailing: function () {
      c.insert({ _key: 'test', value: 1 });
      try {
        db._executeTransaction({
          collections: { write: c.name() },
          action: function () {
            c.remove('test');
            c.insert({ _key: 'test', value: 3 });
            throw new Error('foo');
          }
        });
        fail();
      } catch (err) {
      }

      assertEqual(1, c.toArray().length);
      assertEqual(1, c.document('test').value);
    }

  };
}

function transactionInvocationSuite () {
  'use strict';
  return {

    // //////////////////////////////////////////////////////////////////////////////
    // / @brief test: invalid invocations of TRANSACTION() function
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
        { collections: { } },
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
        try {
          require('@arangodb').db._executeTransaction(test);
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
        }
      });
    },

    // //////////////////////////////////////////////////////////////////////////////
    // / @brief test: valid invocations of TRANSACTION() function
    // //////////////////////////////////////////////////////////////////////////////

    testValidEmptyInvocations: function () {
      var tests = [
        { collections: { }, action: function () { var result = 1; return result; } },
        { collections: { read: [ ] }, action: function () { var result = 1; return result; } },
        { collections: { write: [ ] }, action: function () { var result = 1; return result; } },
        { collections: { read: [ ], write: [ ] }, action: function () { var result = 1; return result; } },
        { collections: { read: [ ], write: [ ] }, lockTimeout: 5.0, action: function () { var result = 1; return result; } },
        { collections: { read: [ ], write: [ ] }, lockTimeout: 0.0, action: function () { var result = 1; return result; } },
        { collections: { read: [ ], write: [ ] }, waitForSync: true, action: function () { var result = 1; return result; } },
        { collections: { read: [ ], write: [ ] }, waitForSync: false, action: function () { var result = 1; return result; } }
      ];

      tests.forEach(function (test) {
        var result = 0;

        result = require('@arangodb').db._executeTransaction(test);
        assertEqual(1, result);
      });
    },

    // //////////////////////////////////////////////////////////////////////////////
    // / @brief test: return values
    // //////////////////////////////////////////////////////////////////////////////

    testReturnValues: function () {
      var tests = [
        { expected: 1, trx: { collections: { }, action: function () { return 1; } } },
        { expected: null, trx: { collections: { }, action: function () { } } },
        { expected: [ ], trx: { collections: { read: [ ] }, action: function () { return [ ]; } } },
        { expected: [ null, true, false ], trx: { collections: { write: [ ] }, action: function () { return [ null, true, false ]; } } },
        { expected: 'foo', trx: { collections: { read: [ ], write: [ ] }, action: function () { return 'foo'; } } },
        { expected: { 'a': 1, 'b': 2 }, trx: { collections: { read: [ ], write: [ ] }, action: function () { return { 'a': 1, 'b': 2 }; } } }
      ];

      tests.forEach(function (test) {
        assertEqual(test.expected, require('@arangodb').db._executeTransaction(test.trx));
      });
    },

    // //////////////////////////////////////////////////////////////////////////////
    // / @brief test: action
    // //////////////////////////////////////////////////////////////////////////////

    testActionFunction: function () {
      var obj = {
        collections: {
        },
        action: function () {
          return 42;
        }
      };

      assertEqual(42, require('@arangodb').db._executeTransaction(obj));
    },

    // //////////////////////////////////////////////////////////////////////////////
    // / @brief test: action
    // //////////////////////////////////////////////////////////////////////////////

    testActionInvalidString: function () {
      try {
        require('@arangodb').db._executeTransaction({
          collections: {
          },
          action: 'return 42;'
        });
        fail();
      } catch (err) {
        assertEqual(arangodb.errors.ERROR_BAD_PARAMETER.code, err.errorNum);
      }
    },

    // //////////////////////////////////////////////////////////////////////////////
    // / @brief test: action
    // //////////////////////////////////////////////////////////////////////////////

    testActionString: function () {
      var obj = {
        collections: {
        },
        action: 'function () { return 42; }'
      };

      assertEqual(42, require('@arangodb').db._executeTransaction(obj));
    },

    // //////////////////////////////////////////////////////////////////////////////
    // / @brief test: nesting
    // //////////////////////////////////////////////////////////////////////////////

    testNesting: function () {
      var obj = {
        collections: {
        },
        action: function () {
          TRANSACTION({
            collections: {
            },
            action: 'function () { return 1; }'
          });
        }
      };

      try {
        db._executeTransaction(obj);
        fail();
      } catch (err) {
        assertEqual(arangodb.errors.ERROR_TRANSACTION_NESTED.code, err.errorNum);
      }
    },

    // //////////////////////////////////////////////////////////////////////////////
    // / @brief test: nesting
    // //////////////////////////////////////////////////////////////////////////////

    testNestingEmbedFlag: function () {
      var obj = {
        collections: {
        },
        action: function () {
          return 19 + TRANSACTION({
            collections: {
            },
            embed: true,
            action: 'function () { return 23; }'
          });
        }
      };

      assertEqual(42, require('@arangodb').db._executeTransaction(obj));
    },

    // //////////////////////////////////////////////////////////////////////////////
    // / @brief test: params
    // //////////////////////////////////////////////////////////////////////////////

    testParamsFunction: function () {
      var obj = {
        collections: {
        },
        action: function (params) {
          return [ params[1], params[4] ];
        },
        params: [ 1, 2, 3, 4, 5 ]
      };

      assertEqual([ 2, 5 ], require('@arangodb').db._executeTransaction(obj));
    },

    // //////////////////////////////////////////////////////////////////////////////
    // / @brief test: params
    // //////////////////////////////////////////////////////////////////////////////

    testParamsString: function () {
      var obj = {
        collections: {
        },
        action: 'function (params) { return [ params[1], params[4] ]; }',
        params: [ 1, 2, 3, 4, 5 ]
      };

      assertEqual([ 2, 5 ], require('@arangodb').db._executeTransaction(obj));
    }

  };
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief test suite
// //////////////////////////////////////////////////////////////////////////////

function transactionCollectionsSuite () {
  'use strict';
  const cn1 = 'UnitTestsTransaction1';
  const cn2 = 'UnitTestsTransaction2';
  let c1 = null;
  let c2 = null;

  return {

    setUp: function () {
      db._drop(cn1);
      c1 = db._create(cn1);

      db._drop(cn2);
      c2 = db._create(cn2);
    },

    tearDown: function () {
      db._drop(cn1);
      c1 = null;

      db._drop(cn2);
      c2 = null;
    },

    // //////////////////////////////////////////////////////////////////////////////
    // / @brief test: trx using non-existing collections
    // //////////////////////////////////////////////////////////////////////////////

    testNonExistingCollectionsArray: function () {
      var obj = {
        collections: {
          read: [ 'UnitTestsTransactionNonExisting' ]
        },
        action: function () {
          return true;
        }
      };

      try {
        db._executeTransaction(obj);
        fail();
      } catch (err) {
        assertEqual(arangodb.errors.ERROR_ARANGO_DATA_SOURCE_NOT_FOUND.code, err.errorNum);
      }
    },

    // //////////////////////////////////////////////////////////////////////////////
    // / @brief test: trx using non-existing collections
    // //////////////////////////////////////////////////////////////////////////////

    testNonExistingCollectionsString: function () {
      var obj = {
        collections: {
          read: 'UnitTestsTransactionNonExisting'
        },
        action: function () {
          return true;
        }
      };

      try {
        db._executeTransaction(obj);
        fail();
      } catch (err) {
        assertEqual(arangodb.errors.ERROR_ARANGO_DATA_SOURCE_NOT_FOUND.code, err.errorNum);
      }
    },

    // //////////////////////////////////////////////////////////////////////////////
    // / @brief test: trx using non-declared collections
    // //////////////////////////////////////////////////////////////////////////////

    testNonDeclaredCollections1: function () {
      var obj = {
        collections: {
        },
        action: function () {
          const db = require('@arangodb').db;
          let c1 = db._collection(params.cn1);
          c1.save({ _key: 'foo' });
          return true;
        },
        params: {
          cn1: cn1
        }
      };

      try {
        db._executeTransaction(obj);
        fail();
      } catch (err) {
        assertEqual(arangodb.errors.ERROR_TRANSACTION_UNREGISTERED_COLLECTION.code, err.errorNum);
      }
    },

    // //////////////////////////////////////////////////////////////////////////////
    // / @brief test: trx using non-declared collections
    // //////////////////////////////////////////////////////////////////////////////

    testNonDeclaredCollections2: function () {
      var obj = {
        collections: {
          write: [ cn2 ]
        },
        action: function () {
          const db = require('@arangodb').db;
          let c1 = db._collection(params.cn1);
          c1.save({ _key: 'foo' });
          return true;
        },
        params: {
          cn1: cn1
        }
      };

      try {
        db._executeTransaction(obj);
        fail();
      } catch (err) {
        assertEqual(arangodb.errors.ERROR_TRANSACTION_UNREGISTERED_COLLECTION.code, err.errorNum);
      }
    },

    // //////////////////////////////////////////////////////////////////////////////
    // / @brief test: trx using wrong mode
    // //////////////////////////////////////////////////////////////////////////////

    testNonDeclaredCollections3: function () {
      var obj = {
        collections: {
          read: [ cn1 ]
        },
        action: function () {
          const db = require('@arangodb').db;
          let c1 = db._collection(params.cn1);
          c1.save({ _key: 'foo' });
          return true;
        },
        params: {
          cn1: cn1
        }
      };

      try {
        db._executeTransaction(obj);
        fail();
      } catch (err) {
        assertEqual(arangodb.errors.ERROR_TRANSACTION_UNREGISTERED_COLLECTION.code, err.errorNum);
      }
    },

    // //////////////////////////////////////////////////////////////////////////////
    // / @brief test: trx using no collections
    // //////////////////////////////////////////////////////////////////////////////

    testNoCollections: function () {
      var obj = {
        collections: {
        },
        action: function () {
          return true;
        }
      };

      assertTrue(require('@arangodb').db._executeTransaction(obj));
    },

    // //////////////////////////////////////////////////////////////////////////////
    // / @brief test: trx using no collections
    // //////////////////////////////////////////////////////////////////////////////

    testNoCollectionsAql: function () {
      var obj = {
        collections: {
        },
        action: function () {
          var result = require('@arangodb/aql-helper').getQueryResults('FOR i IN [ 1, 2, 3 ] RETURN i');
          return result;
        }
      };

      var result = require('@arangodb').db._executeTransaction(obj);
      assertEqual([ 1, 2, 3 ], result);
    },

    // //////////////////////////////////////////////////////////////////////////////
    // / @brief test: trx using valid collections
    // //////////////////////////////////////////////////////////////////////////////

    testValidCollectionsArray: function () {
      var obj = {
        collections: {
          write: [ cn1 ]
        },
        action: function () {
          const db = require('@arangodb').db;
          let c1 = db._collection(params.cn1);
          c1.save({ _key: 'foo' });
          return true;
        },
        params: {
          cn1: cn1
        }
      };

      assertTrue(require('@arangodb').db._executeTransaction(obj));
    },
    // //////////////////////////////////////////////////////////////////////////////
    // / @brief test: trx using valid collections
    // //////////////////////////////////////////////////////////////////////////////

    testValidCollectionsString: function () {
      var obj = {
        collections: {
          write: cn1
        },
        action: function () {
          const db = require('@arangodb').db;
          let c1 = db._collection(params.cn1);
          c1.save({ _key: 'foo' });
          return true;
        },
        params: {
          cn1: cn1
        }
      };

      assertTrue(require('@arangodb').db._executeTransaction(obj));
    },

    // //////////////////////////////////////////////////////////////////////////////
    // / @brief test: trx using valid collections
    // //////////////////////////////////////////////////////////////////////////////

    testValidMultipleCollectionsArray: function () {
      var obj = {
        collections: {
          write: [ cn1, cn2 ]
        },
        action: function () {
          const db = require('@arangodb').db;
          let c1 = db._collection(params.cn1);
          let c2 = db._collection(params.cn2);
          c1.save({ _key: 'foo' });
          c2.save({ _key: 'foo' });
          return true;
        },
        params: {
          cn1: cn1,
          cn2: cn2
        }
      };

      assertTrue(require('@arangodb').db._executeTransaction(obj));
    },

    // //////////////////////////////////////////////////////////////////////////////
    // / @brief test: trx using valid collections
    // //////////////////////////////////////////////////////////////////////////////

    testValidMultipleCollectionsString: function () {
      c2.save({ _key: 'foo' });

      var obj = {
        collections: {
          write: cn1,
          read: cn2
        },
        action: function () {
          const db = require('@arangodb').db;
          let c1 = db._collection(params.cn1);
          let c2 = db._collection(params.cn2);
          c1.save({ _key: 'foo' });
          c2.document('foo');
          return true;
        },
        params: {
          cn1: cn1,
          cn2: cn2
        }
      };

      assertTrue(require('@arangodb').db._executeTransaction(obj));
    },

    // //////////////////////////////////////////////////////////////////////////////
    // / @brief test: trx using valid collections
    // //////////////////////////////////////////////////////////////////////////////

    testRedeclareCollectionArray: function () {
      var obj = {
        collections: {
          read: [ cn1 ],
          write: [ cn1 ]
        },
        action: function () {
          const db = require('@arangodb').db;
          let c1 = db._collection(params.cn1);
          c1.save({ _key: 'foo' });
          return true;
        },
        params: {
          cn1: cn1
        }
      };

      assertTrue(require('@arangodb').db._executeTransaction(obj));
    },

    // //////////////////////////////////////////////////////////////////////////////
    // / @brief test: trx using valid collections
    // //////////////////////////////////////////////////////////////////////////////

    testRedeclareCollectionString: function () {
      var obj = {
        collections: {
          read: cn1,
          write: cn1
        },
        action: function () {
          const db = require('@arangodb').db;
          let c1 = db._collection(params.cn1);
          c1.save({ _key: 'foo' });
          return true;
        },
        params: {
          cn1: cn1
        }
      };

      assertTrue(require('@arangodb').db._executeTransaction(obj));
    },

    // //////////////////////////////////////////////////////////////////////////////
    // / @brief test: trx using valid collections
    // //////////////////////////////////////////////////////////////////////////////

    testReadWriteCollections: function () {
      var obj = {
        collections: {
          read: [ cn1 ],
          write: [ cn2 ]
        },
        action: function () {
          const db = require('@arangodb').db;
          let c2 = db._collection(params.cn2);
          c2.save({ _key: 'foo' });
          return true;
        },
        params: {
          cn2: cn2
        }
      };

      assertTrue(require('@arangodb').db._executeTransaction(obj));
    },

    // //////////////////////////////////////////////////////////////////////////////
    // / @brief test: trx using waitForSync
    // //////////////////////////////////////////////////////////////////////////////

    testWaitForSyncTrue: function () {
      var obj = {
        collections: {
          write: [ cn1 ]
        },
        waitForSync: true,
        action: function () {
          const db = require('@arangodb').db;
          let c1 = db._collection(params.cn1);
          c1.save({ _key: 'foo' });
          return true;
        },
        params: {
          cn1: cn1
        }
      };

      assertTrue(require('@arangodb').db._executeTransaction(obj));
    },

    // //////////////////////////////////////////////////////////////////////////////
    // / @brief test: trx using waitForSync
    // //////////////////////////////////////////////////////////////////////////////

    testWaitForSyncFalse: function () {
      var obj = {
        collections: {
          write: [ cn1 ]
        },
        waitForSync: false,
        action: function () {
          const db = require('@arangodb').db;
          let c1 = db._collection(params.cn1);
          c1.save({ _key: 'foo' });
          return true;
        },
        params: {
          cn1: cn1
        }
      };

      assertTrue(require('@arangodb').db._executeTransaction(obj));
    },

    // //////////////////////////////////////////////////////////////////////////////
    // / @brief test: trx with embedded AQL
    // //////////////////////////////////////////////////////////////////////////////

    testAqlRead: function () {
      let docs = [];
      for (let i = 0; i < 10; ++i) {
        docs.push({ _key: 'test' + i });
      }
      c1.insert(docs);

      var obj = {
        collections: {
          read: [ cn1 ]
        },
        action: function () {
          var docs = require('@arangodb').db._query('FOR i IN @@cn1 RETURN i', { '@cn1': params.cn1 }).toArray();
          require("jsunity").jsUnity.assertions.assertEqual(10, docs.length);
          return true;
        },
        params: {
          cn1: cn1
        }
      };

      assertTrue(require('@arangodb').db._executeTransaction(obj));
    },

    // //////////////////////////////////////////////////////////////////////////////
    // / @brief test: trx with embedded AQL
    // //////////////////////////////////////////////////////////////////////////////

    testAqlReadMulti: function () {
      let docs = [];
      for (let i = 0; i < 10; ++i) {
        docs.push({ _key: 'test' + i });
      }
      c1.insert(docs);
      c2.insert(docs);

      var obj = {
        collections: {
          read: [ cn1, cn2 ]
        },
        action: function () {
          var docs = require('@arangodb').db._query('FOR i IN @@cn1 FOR j IN @@cn2 RETURN i', { '@cn1': params.cn1, '@cn2': params.cn2 }).toArray();
          require("jsunity").jsUnity.assertions.assertEqual(100, docs.length);
          return true;
        },
        params: {
          cn1: cn1,
          cn2: cn2,
        }
      };

      assertTrue(require('@arangodb').db._executeTransaction(obj));
    },

    // //////////////////////////////////////////////////////////////////////////////
    // / @brief test: trx with embedded AQL
    // //////////////////////////////////////////////////////////////////////////////

    testAqlReadMultiUndeclared: function () {
      let docs = [];
      for (let i = 0; i < 10; ++i) {
        docs.push({ _key: 'test' + i });
      }
      c1.insert(docs);
      c2.insert(docs);

      var obj = {
        collections: {
          // intentionally empty
        },
        action: function () {
          var docs = require('@arangodb').db._query('FOR i IN @@cn1 FOR j IN @@cn2 RETURN i', { '@cn1': params.cn1, '@cn2': params.cn2 }).toArray();
          require("jsunity").jsUnity.assertions.assertEqual(100, docs.length);
          return true;
        },
        params: {
          cn1: cn1,
          cn2: cn2,
        }
      };

      assertTrue(require('@arangodb').db._executeTransaction(obj));
    },

    // //////////////////////////////////////////////////////////////////////////////
    // / @brief test: trx with embedded AQL
    // //////////////////////////////////////////////////////////////////////////////

    testAqlWrite: function () {
      let docs = [];
      for (let i = 0; i < 10; ++i) {
        docs.push({ _key: 'test' + i });
      }
      c1.insert(docs);

      var obj = {
        collections: {
          write: [ cn1 ]
        },
        action: function () {
          const db = require('@arangodb').db;
          let c1 = db._collection(params.cn1);
          require("jsunity").jsUnity.assertions.assertEqual(10, c1.count());
          var ops = require('@arangodb').db._query('FOR i IN @@cn1 REMOVE i._key IN @@cn1', { '@cn1': params.cn1 }).getExtra().stats;
          require("jsunity").jsUnity.assertions.assertEqual(10, ops.writesExecuted);
          require("jsunity").jsUnity.assertions.assertEqual(0, c1.count());
          return true;
        },
        params: {
          cn1: cn1
        }
      };

      assertTrue(require('@arangodb').db._executeTransaction(obj));
      assertEqual(0, c1.count());
    },

    // //////////////////////////////////////////////////////////////////////////////
    // / @brief test: trx with embedded AQL
    // //////////////////////////////////////////////////////////////////////////////

    testAqlReadWrite: function () {
      let docs = [];
      for (let i = 0; i < 10; ++i) {
        docs.push({ _key: 'test' + i });
      }
      c1.insert(docs);
      c2.insert(docs);

      var obj = {
        collections: {
          read: [ cn1 ],
          write: [ cn2 ]
        },
        action: function () {
          var ops = require('@arangodb').db._query('FOR i IN @@cn1 REMOVE i._key IN @@cn2', { '@cn1': params.cn1, '@cn2': params.cn2 }).getExtra().stats;
          require("jsunity").jsUnity.assertions.assertEqual(10, ops.writesExecuted);
          return true;
        },
        params: {
          cn1: cn1,
          cn2: cn2,
        }
      };

      assertTrue(require('@arangodb').db._executeTransaction(obj));

      assertEqual(10, c1.count());
      assertEqual(0, c2.count());
    },

    // //////////////////////////////////////////////////////////////////////////////
    // / @brief test: trx with embedded AQL
    // //////////////////////////////////////////////////////////////////////////////

    testAqlWriteUndeclared: function () {
      let docs = [];
      for (let i = 0; i < 10; ++i) {
        docs.push({ _key: 'test' + i });
      }
      c1.insert(docs);
      c2.insert(docs);

      var obj = {
        collections: {
          read: [ cn1 ]
        },
        action: function () {
          const db = require('@arangodb').db;
          let c1 = db._collection(params.cn1);
          try {
            require('@arangodb').db._query('FOR i IN @@cn1 REMOVE i._key IN @@cn2', { '@cn1': params.cn1, '@cn2': params.cn2 });
            fail();
          } catch (err) {
            require("jsunity").jsUnity.assertions.assertEqual(require('@arangodb').errors.ERROR_TRANSACTION_UNREGISTERED_COLLECTION.code, err.errorNum);
          }

          require("jsunity").jsUnity.assertions.assertEqual(10, c1.count());
          return true;
        },
        params: {
          cn1: cn1,
          cn2: cn2
        }
      };

      assertTrue(require('@arangodb').db._executeTransaction(obj));

      assertEqual(10, c1.count());
      assertEqual(10, c2.count());
    },

    // //////////////////////////////////////////////////////////////////////////////
    // / @brief test: trx with embedded AQL
    // //////////////////////////////////////////////////////////////////////////////

    testAqlMultiWrite: function () {
      let docs = [];
      for (let i = 0; i < 10; ++i) {
        docs.push({ _key: 'test' + i });
      }
      c1.insert(docs);
      c2.insert(docs);

      var obj = {
        collections: {
          write: [ cn1, cn2 ]
        },
        action: function () {
          var ops;
          const db = require('@arangodb').db;
          let c1 = db._collection(params.cn1);
          let c2 = db._collection(params.cn2);
          ops = require('@arangodb').db._query('FOR i IN @@cn1 REMOVE i._key IN @@cn1', { '@cn1': params.cn1 }).getExtra().stats;
          require("jsunity").jsUnity.assertions.assertEqual(10, ops.writesExecuted);
          require("jsunity").jsUnity.assertions.assertEqual(0, c1.count());

          ops = require('@arangodb').db._query('FOR i IN @@cn2 REMOVE i._key IN @@cn2', { '@cn2': params.cn2 }).getExtra().stats;
          require("jsunity").jsUnity.assertions.assertEqual(10, ops.writesExecuted);
          require("jsunity").jsUnity.assertions.assertEqual(0, c2.count());
          return true;
        },
        params: {
          cn1: cn1,
          cn2: cn2
        }
      };

      assertTrue(require('@arangodb').db._executeTransaction(obj));

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
  const cn1 = 'UnitTestsTransaction1';
  const cn2 = 'UnitTestsTransaction2';
  let c1 = null;
  let c2 = null;

  return {

    setUp: function () {
      db._drop(cn1);
      db._drop(cn2);
    },

    tearDown: function () {
      db._drop(cn1);
      c1 = null;

      db._drop(cn2);
      c2 = null;
    },

    // //////////////////////////////////////////////////////////////////////////////
    // / @brief test: trx with create operation
    // //////////////////////////////////////////////////////////////////////////////

    testCreate: function () {
      var obj = {
        collections: {
        },
        action: function () {
          require('@arangodb').db._create(params.cn1);
          fail();
        },
        params: {
          cn1: cn1
        }
      };

      try {
        db._executeTransaction(obj);
        fail();
      } catch (err) {
        assertEqual(arangodb.errors.ERROR_TRANSACTION_DISALLOWED_OPERATION.code, err.errorNum);
      }
    },

    // //////////////////////////////////////////////////////////////////////////////
    // / @brief test: trx with drop operation
    // //////////////////////////////////////////////////////////////////////////////

    testDrop: function () {
      c1 = db._create(cn1);

      var obj = {
        collections: {
        },
        action: function () {
          const db = require('@arangodb').db;
          let c1 = db._collection(params.cn1);
          c1.drop();
          fail();
        },
        params: {
          cn1: cn1
        }
      };

      try {
        db._executeTransaction(obj);
        fail();
      } catch (err) {
        assertEqual(arangodb.errors.ERROR_TRANSACTION_DISALLOWED_OPERATION.code, err.errorNum);
      }
    },

    // //////////////////////////////////////////////////////////////////////////////
    // / @brief test: trx with rename operation
    // //////////////////////////////////////////////////////////////////////////////

    testRename: function () {
      c1 = db._create(cn1);

      var obj = {
        collections: {
        },
        action: function () {
          const db = require('@arangodb').db;
          let c1 = db._collection(params.cn1);
          c1.rename(params.cn2);
          fail();
        },
        params: {
          cn1: cn1,
          cn2: cn2
        }
      };

      try {
        db._executeTransaction(obj);
        fail();
      } catch (err) {
        assertEqual(arangodb.errors.ERROR_TRANSACTION_DISALLOWED_OPERATION.code, err.errorNum);
      }
    },

    // //////////////////////////////////////////////////////////////////////////////
    // / @brief test: trx with create index operation
    // //////////////////////////////////////////////////////////////////////////////

    testCreateUniqueIndex: function () {
      c1 = db._create(cn1);

      let obj = {
        collections: {
        },
        action: function () {
          const db = require('@arangodb').db;
          let c1 = db._collection(params.cn1);
          c1.ensureIndex({ type: "persistent", fields: ["foo"], unique: true });
          fail();
        },
        params: {
          cn1: cn1
        }
      };

      try {
        db._executeTransaction(obj);
        fail();
      } catch (err) {
        assertEqual(arangodb.errors.ERROR_TRANSACTION_DISALLOWED_OPERATION.code, err.errorNum);
      }
    },

    // //////////////////////////////////////////////////////////////////////////////
    // / @brief test: trx with create index operation
    // //////////////////////////////////////////////////////////////////////////////

    testCreateNonUniqueIndex: function () {
      c1 = db._create(cn1);

      let obj = {
        collections: {
        },
        action: function () {
          const db = require('@arangodb').db;
          let c1 = db._collection(params.cn1);
          c1.ensureIndex({ type: "persistent", fields: ["foo"] });
          fail();
        },
        params: {
          cn1: cn1
        }
      };

      try {
        db._executeTransaction(obj);
        fail();
      } catch (err) {
        assertEqual(arangodb.errors.ERROR_TRANSACTION_DISALLOWED_OPERATION.code, err.errorNum);
      }
    },

    // //////////////////////////////////////////////////////////////////////////////
    // / @brief test: trx with create index operation
    // //////////////////////////////////////////////////////////////////////////////

    testCreateFulltextIndex: function () {
      c1 = db._create(cn1);

      let obj = {
        collections: {
        },
        action: function () {
          const db = require('@arangodb').db;
          let c1 = db._collection(params.cn1);
          c1.ensureIndex({ type: "fulltext", fields: ["foo"] });
          fail();
        },
        params: {
          cn1: cn1
        }
      };

      try {
        db._executeTransaction(obj);
        fail();
      } catch (err) {
        assertEqual(arangodb.errors.ERROR_TRANSACTION_DISALLOWED_OPERATION.code, err.errorNum);
      }
    },

    // //////////////////////////////////////////////////////////////////////////////
    // / @brief test: trx with drop index operation
    // //////////////////////////////////////////////////////////////////////////////

    testDropIndex: function () {
      c1 = db._create(cn1);
      var idx = c1.ensureIndex({ type: "persistent", fields: ["foo"], unique: true });

      var obj = {
        collections: {
        },
        action: function () {
          const db = require('@arangodb').db;
          let c1 = db._collection(params.cn1);
          c1.dropIndex(params.idx.id);
          return true;
        },
        params: {
          cn1: cn1,
          idx: idx
        }
      };

      try {
        db._executeTransaction(obj);
        fail();
      } catch (err) {
        assertEqual(arangodb.errors.ERROR_TRANSACTION_DISALLOWED_OPERATION.code, err.errorNum);
      }
    },

    // //////////////////////////////////////////////////////////////////////////////
    // / @brief test: trx with read operation
    // //////////////////////////////////////////////////////////////////////////////

    testSingleRead1: function () {
      c1 = db._create(cn1);
      c1.save({ _key: 'foo', a: 1 });

      var obj = {
        collections: {
          read: [ cn1 ]
        },
        action: function () {
          const db = require('@arangodb').db;
          let c1 = db._collection(params.cn1);
          require("jsunity").jsUnity.assertions.assertEqual(1, c1.document('foo').a);

          return true;
        },
        params: {
          cn1: cn1
        }
      };

      require('@arangodb').db._executeTransaction(obj);
      assertEqual(1, c1.count());
      assertEqual([ 'foo' ], sortedKeys(c1));
    },

    // //////////////////////////////////////////////////////////////////////////////
    // / @brief test: trx with read operation
    // //////////////////////////////////////////////////////////////////////////////

    testSingleRead2: function () {
      c1 = db._create(cn1);
      c1.save({ _key: 'foo', a: 1 });

      var obj = {
        collections: {
          write: [ cn1 ]
        },
        action: function () {
          const db = require('@arangodb').db;
          let c1 = db._collection(params.cn1);
          require("jsunity").jsUnity.assertions.assertEqual(1, c1.document('foo').a);
          return true;
        },
        params: {
          cn1: cn1
        }
      };

      require('@arangodb').db._executeTransaction(obj);
      assertEqual(1, c1.count());
      assertEqual([ 'foo' ], sortedKeys(c1));
    },

    // //////////////////////////////////////////////////////////////////////////////
    // / @brief test: trx with read operation
    // //////////////////////////////////////////////////////////////////////////////

    testScan1: function () {
      c1 = db._create(cn1);
      let docs = [];
      for (let i = 0; i < 100; ++i) {
        docs.push({ _key: 'foo' + i, a: i });
      }
      c1.insert(docs);

      var obj = {
        collections: {
          read: [ cn1 ]
        },
        action: function () {
          const db = require('@arangodb').db;
          let c1 = db._collection(params.cn1);
          require("jsunity").jsUnity.assertions.assertEqual(100, c1.toArray().length);
          require("jsunity").jsUnity.assertions.assertEqual(100, c1.count());

          for (var i = 0; i < 100; ++i) {
            require("jsunity").jsUnity.assertions.assertEqual(i, c1.document('foo' + i).a);
          }
          return true;
        },
        params: {
          cn1: cn1
        }
      };

      require('@arangodb').db._executeTransaction(obj);
      assertEqual(100, c1.count());
    },

    // //////////////////////////////////////////////////////////////////////////////
    // / @brief test: trx with read operation
    // //////////////////////////////////////////////////////////////////////////////

    testScan2: function () {
      c1 = db._create(cn1);
      let docs = [];
      for (let i = 0; i < 100; ++i) {
        docs.push({ _key: 'foo' + i, a: i });
      }
      c1.insert(docs);

      var obj = {
        collections: {
          write: [ cn1 ]
        },
        action: function () {
          const db = require('@arangodb').db;
          let c1 = db._collection(params.cn1);
          require("jsunity").jsUnity.assertions.assertEqual(100, c1.toArray().length);
          require("jsunity").jsUnity.assertions.assertEqual(100, c1.count());

          for (var i = 0; i < 100; ++i) {
            require("jsunity").jsUnity.assertions.assertEqual(i, c1.document('foo' + i).a);
          }
          return true;
        },
        params: {
          cn1: cn1
        }
      };

      require('@arangodb').db._executeTransaction(obj);
      assertEqual(100, c1.count());
    },

    // //////////////////////////////////////////////////////////////////////////////
    // / @brief test: trx with insert operation
    // //////////////////////////////////////////////////////////////////////////////

    testSingleInsert: function () {
      c1 = db._create(cn1);

      var obj = {
        collections: {
          write: [ cn1 ]
        },
        action: function () {
          const db = require('@arangodb').db;
          let c1 = db._collection(params.cn1);
          c1.save({ _key: 'foo' });
          return true;
        },
        params: {
          cn1: cn1
        }
      };

      require('@arangodb').db._executeTransaction(obj);
      assertEqual(1, c1.count());
      assertEqual([ 'foo' ], sortedKeys(c1));
    },

    // //////////////////////////////////////////////////////////////////////////////
    // / @brief test: trx with insert operation
    // //////////////////////////////////////////////////////////////////////////////

    testMultiInsert: function () {
      c1 = db._create(cn1);

      var obj = {
        collections: {
          write: [ cn1 ]
        },
        action: function () {
          const db = require('@arangodb').db;
          let c1 = db._collection(params.cn1);
      
          c1.save({ _key: 'foo' });
          c1.save({ _key: 'bar' });
          return true;
        },
        params: {
          cn1: cn1
        }
      };

      require('@arangodb').db._executeTransaction(obj);
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

      var obj = {
        collections: {
          write: [ cn1 ]
        },
        action: function () {
          const db = require('@arangodb').db;
          let c1 = db._collection(params.cn1);
          c1.save({ _key: 'baz' });
          c1.save({ _key: 'bam' });
          return true;
        },
        params: {
          cn1: cn1
        }
      };

      require('@arangodb').db._executeTransaction(obj);
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

      var obj = {
        collections: {
          write: [ cn1 ]
        },
        action: function () {
          const db = require('@arangodb').db;
          let c1 = db._collection(params.cn1);
          require("jsunity").jsUnity.assertions.assertEqual(1, c1.document('foo').a);
          c1.replace('foo', { a: 3 });
          require("jsunity").jsUnity.assertions.assertEqual(2, c1.document('bar').b);
          require("jsunity").jsUnity.assertions.assertEqual(3, c1.document('bar').c);
          c1.replace('bar', { b: 9 });
          return true;
        },
        params: {
          cn1: cn1
        }
      };

      require('@arangodb').db._executeTransaction(obj);
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

      var obj = {
        collections: {
          write: [ cn1 ]
        },
        action: function () {
          const db = require('@arangodb').db;
          let c1 = db._collection(params.cn1);
          require("jsunity").jsUnity.assertions.assertEqual(1, c1.document('foo').a);
          c1.replace('foo', { a: 3 });
          require("jsunity").jsUnity.assertions.assertEqual(3, c1.document('foo').a);
          c1.replace('foo', { a: 4 });
          require("jsunity").jsUnity.assertions.assertEqual(4, c1.document('foo').a);
          c1.replace('foo', { a: 5 });
          require("jsunity").jsUnity.assertions.assertEqual(5, c1.document('foo').a);
          c1.replace('foo', { a: 6, b: 99 });
          require("jsunity").jsUnity.assertions.assertEqual(6, c1.document('foo').a);
          require("jsunity").jsUnity.assertions.assertEqual(99, c1.document('foo').b);
          return true;
        },
        params: {
          cn1: cn1
        }
      };

      require('@arangodb').db._executeTransaction(obj);
      assertEqual(1, c1.count());
      assertEqual(6, c1.document('foo').a);
      assertEqual(99, c1.document('foo').b);
    },

    // //////////////////////////////////////////////////////////////////////////////
    // / @brief test: trx with update operation
    // //////////////////////////////////////////////////////////////////////////////

    testUpdate: function () {
      c1 = db._create(cn1);
      c1.insert([{ _key: 'foo', a: 1 },
                 { _key: 'bar', b: 2 },
                 { _key: 'baz', c: 3 },
                 { _key: 'bam', d: 4 }]);

      var obj = {
        collections: {
          write: [ cn1 ]
        },
        action: function () {
          const db = require('@arangodb').db;
          let c1 = db._collection(params.cn1);
          require("jsunity").jsUnity.assertions.assertEqual(1, c1.document('foo').a);
          c1.update('foo', { a: 3 });

          require("jsunity").jsUnity.assertions.assertEqual(2, c1.document('bar').b);
          c1.update('bar', { b: 9 });

          require("jsunity").jsUnity.assertions.assertEqual(3, c1.document('baz').c);
          c1.update('baz', { b: 9, c: 12 });

          require("jsunity").jsUnity.assertions.assertEqual(4, c1.document('bam').d);
          return true;
        },
        params: {
          cn1: cn1
        }
      };

      require('@arangodb').db._executeTransaction(obj);
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
      c1.insert([{ _key: 'foo', a: 1 },
                 { _key: 'bar', b: 2 },
                 { _key: 'baz', c: 3 }]);

      var obj = {
        collections: {
          write: [ cn1 ]
        },
        action: function () {
          const db = require('@arangodb').db;
          let c1 = db._collection(params.cn1);
          c1.remove('foo');
          c1.remove('baz');
          return true;
        },
        params: {
          cn1: cn1
        }
      };

      require('@arangodb').db._executeTransaction(obj);
      assertEqual(1, c1.count());
      assertEqual([ 'bar' ], sortedKeys(c1));
    },

    // //////////////////////////////////////////////////////////////////////////////
    // / @brief test: trx with truncate operation
    // //////////////////////////////////////////////////////////////////////////////

    testTruncateEmpty: function () {
      c1 = db._create(cn1);

      var obj = {
        collections: {
          write: [ cn1 ]
        },
        action: function () {
          const db = require('@arangodb').db;
          let c1 = db._collection(params.cn1);
          c1.truncate({ compact: false });
          return true;
        },
        params: {
          cn1: cn1
        }
      };

      require('@arangodb').db._executeTransaction(obj);
      assertEqual(0, c1.count());
      assertEqual([ ], sortedKeys(c1));
    },

    // //////////////////////////////////////////////////////////////////////////////
    // / @brief test: trx with truncate operation
    // //////////////////////////////////////////////////////////////////////////////

    testTruncateNonEmpty: function () {
      c1 = db._create(cn1);

      let docs = [];
      for (let i = 0; i < 100; ++i) {
        docs.push({ a: i });
      }
      c1.insert(docs);

      var obj = {
        collections: {
          write: [ cn1 ]
        },
        action: function () {
          const db = require('@arangodb').db;
          let c1 = db._collection(params.cn1);
          c1.truncate({ compact: false });
          return true;
        },
        params: {
          cn1: cn1
        }
      };

      require('@arangodb').db._executeTransaction(obj);
      assertEqual(0, c1.count());
      assertEqual([ ], sortedKeys(c1));
    },

    // //////////////////////////////////////////////////////////////////////////////
    // / @brief test: trx with truncate operation
    // //////////////////////////////////////////////////////////////////////////////

    testTruncateAndAdd: function () {
      c1 = db._create(cn1);

      let docs = [];
      for (let i = 0; i < 100; ++i) {
        docs.push({ a: i });
      }
      c1.insert(docs);

      var obj = {
        collections: {
          write: [ cn1 ]
        },
        action: function () {
          const db = require('@arangodb').db;
          let c1 = db._collection(params.cn1);
          c1.truncate({ compact: false });
          c1.save({ _key: 'foo' });
          return true;
        },
        params: {
          cn1: cn1
        }
      };

      require('@arangodb').db._executeTransaction(obj);
      assertEqual(1, c1.count());
      assertEqual([ 'foo' ], sortedKeys(c1));
    },

    // //////////////////////////////////////////////////////////////////////////////
    // / @brief test: trx with byExample operation
    // //////////////////////////////////////////////////////////////////////////////

    testByExample: function () {
      c1 = db._create(cn1);
      c1.ensureIndex({ type: "persistent", fields: ["name"], unique: true });

      let docs = [];
      for (let i = 0; i < 100; ++i) {
        docs.push({ name: 'test' + i });
      }
      c1.insert(docs);

      let obj = {
        collections: {
          write: [ cn1 ]
        },
        action: function () {
          const db = require('@arangodb').db;
          let c1 = db._collection(params.cn1);
          var r = c1.byExample({ name: 'test99' }).toArray();
          require("jsunity").jsUnity.assertions.assertEqual(r.length, 1);
          require("jsunity").jsUnity.assertions.assertEqual('test99', r[0].name);
        },
        params: {
          cn1: cn1
        }
      };

      require('@arangodb').db._executeTransaction(obj);
    },

    // //////////////////////////////////////////////////////////////////////////////
    // / @brief test: trx with firstExample operation
    // //////////////////////////////////////////////////////////////////////////////

    testFirstExample1: function () {
      c1 = db._create(cn1);
      c1.ensureIndex({ type: "persistent", fields: ["name"], unique: true });

      let docs = [];
      for (let i = 0; i < 100; ++i) {
        docs.push({ name: 'test' + i });
      }
      c1.insert(docs);

      let obj = {
        collections: {
          write: [ cn1 ]
        },
        action: function () {
          const db = require('@arangodb').db;
          let c1 = db._collection(params.cn1);
          var r = c1.firstExample({ name: 'test99' });
          require("jsunity").jsUnity.assertions.assertEqual('test99', r.name);
        },
        params: {
          cn1: cn1
        }
      };

      require('@arangodb').db._executeTransaction(obj);
    },

    // //////////////////////////////////////////////////////////////////////////////
    // / @brief test: trx with firstExample operation
    // //////////////////////////////////////////////////////////////////////////////

    testFirstExample2: function () {
      c1 = db._create(cn1);
      c1.ensureIndex({ type: "persistent", fields: ["name"] });

      let docs = [];
      for (var i = 0; i < 100; ++i) {
        docs.push({ name: 'test' + i });
      }
      c1.insert(docs);

      let obj = {
        collections: {
          write: [ cn1 ]
        },
        action: function () {
          const db = require('@arangodb').db;
          let c1 = db._collection(params.cn1);
          var r = c1.firstExample({ name: 'test99' });
          require("jsunity").jsUnity.assertions.assertEqual('test99', r.name);
        },
        params: {
          cn1: cn1
        }
      };

      require('@arangodb').db._executeTransaction(obj);
    },

    // //////////////////////////////////////////////////////////////////////////////
    // / @brief test: trx with fulltext operation
    // //////////////////////////////////////////////////////////////////////////////

    testFulltext: function () {
      c1 = db._create(cn1);
      var idx = c1.ensureIndex({ type: "fulltext", fields: ["text"] });

      c1.save({ text: 'steam', other: 1 });
      c1.save({ text: 'steamboot', other: 2 });

      let obj = {
        collections: {
          write: [ cn1 ]
        },
        action: function () {
          const db = require('@arangodb').db;
          let c1 = db._collection(params.cn1);
          var r = c1.fulltext('text', 'prefix:steam', params.idx).toArray();
          require("jsunity").jsUnity.assertions.assertEqual(2, r.length);

          r = c1.fulltext('text', 'steam', params.idx).toArray();
          require("jsunity").jsUnity.assertions.assertEqual(1, r.length);
        },
        params: {
          cn1: cn1,
          idx: idx
        }
      };

      require('@arangodb').db._executeTransaction(obj);
    }

  };
}

function transactionBarriersSuite () {
  'use strict';
  const cn1 = 'UnitTestsTransaction1';
  const cn2 = 'UnitTestsTransaction2';
  let c1 = null;
  let c2 = null;

  return {

    setUp: function () {
      db._drop(cn1);
      db._drop(cn2);
    },

    tearDown: function () {
      if (c1 !== null) {
        c1.drop();
      }

      c1 = null;

      if (c2 !== null) {
        c2.drop();
      }

      c2 = null;
    },

    // //////////////////////////////////////////////////////////////////////////////
    // / @brief test: usage of barriers outside of transaction
    // //////////////////////////////////////////////////////////////////////////////

    testBarriersOutsideCommit: function () {
      c1 = db._create(cn1);

      var i;

      var obj = {
        collections: {
          write: [ cn1 ]
        },
        action: function () {
          var docs = [ ];
          const db = require('@arangodb').db;
          let c1 = db._collection(params.cn1);
          for (i = 0; i < 100; ++i) {
            c1.save({ _key: 'foo' + i, value1: i, value2: 'foo' + i + 'x' });
          }

          for (i = 0; i < 100; ++i) {
            docs.push(c1.document('foo' + i));
          }

          return [c1.document('foo0'), docs];
        },
        params: {
          cn1: cn1
        }
      };

      var [result, docs] = require('@arangodb').db._executeTransaction(obj);

      assertEqual(100, docs.length);
      assertEqual(100, c1.count());

      assertEqual('foo0', result._key);
      assertEqual(0, result.value1);
      assertEqual('foo0x', result.value2);

      for (i = 0; i < 100; ++i) {
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

      var i;

      var obj = {
        collections: {
          write: [ cn1 ]
        },
        action: function () {
          var docs = [ ];

          for (i = 0; i < 100; ++i) {
            c1.save({ _key: 'foo' + i, value1: i, value2: 'foo' + i + 'x' });
          }

          for (i = 0; i < 100; ++i) {
            docs.push(c1.document('foo' + i));
          }

          throw new Error('doh!');
        }
      };

      try {
        db._executeTransaction(obj);
        fail();
      } catch (err) {
      }

      assertEqual(0, c1.count());
    }

  };
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief test suite
// //////////////////////////////////////////////////////////////////////////////

function transactionGraphSuite () {
  'use strict';
  var cn1 = 'UnitTestsVertices';
  var cn2 = 'UnitTestsEdges';

  var G = require('@arangodb/general-graph');

  var c1 = null;
  var c2 = null;

  return {

    // //////////////////////////////////////////////////////////////////////////////
    // / @brief set up
    // //////////////////////////////////////////////////////////////////////////////

    setUp: function () {
      try {
        G._drop('UnitTestsGraph');
      } catch (err) {
      }
      db._drop(cn1);
      db._drop(cn2);

      c1 = db._create(cn1);
      c2 = db._createEdgeCollection(cn2);
    },

    // //////////////////////////////////////////////////////////////////////////////
    // / @brief tear down
    // //////////////////////////////////////////////////////////////////////////////

    tearDown: function () {
      try {
        G._drop('UnitTestsGraph');
      } catch (err) {
      }
      db._drop(cn1);
      c1 = null;

      db._drop(cn2);
      c2 = null;
    },

    // //////////////////////////////////////////////////////////////////////////////
    // / @brief test: rollback updates in a graph transaction
    // //////////////////////////////////////////////////////////////////////////////

    testRollbackGraphUpdates: function () {
      var graph = G._create('UnitTestsGraph', [G._relation(cn2, cn1, cn1)]);

      assertEqual(0, db[cn1].count());

      var obj = {
        collections: {
          write: [ cn1, cn2 ]
        },
        action: function () {
          var result = { };
          result.enxirvp = graph[cn1].save({});
          result.biitqtk = graph[cn1].save({});
          result.oboyuhh = graph[cn2].save({_from: result.enxirvp._id, _to: result.biitqtk._id, name: 'john smith'});
          result.cvwmkym = db[cn1].replace(result.enxirvp._id, { _rev: null });
          result.gsalfxu = db[cn1].replace(result.biitqtk._id, { _rev: null });
          result.xsjzbst = (function () {
            graph[cn2].remove(result.oboyuhh._id);
            return true;
          }());

          result.thizhdd = graph[cn2].save({_from: result.cvwmkym._id, _to: result.gsalfxu._id, _key: result.oboyuhh._key, name: 'david smith'});
          
          // gotHere = 1;
          
          result.rldfnre = graph[cn2].save({_from: result.cvwmkym._id, _to: result.gsalfxu._id, _key: result.oboyuhh._key, name: 'david smith'});
          
          // In case last `save` call will not throw error, we will throw it by ourself
          var err = new Error('gotHere = 2');
          err.errorNum = 2;
          throw(err);
        }
      };

      try {
        db._executeTransaction(obj);
        fail();
      } catch (err) {
        assertNotEqual(err.errorNum, 2);
      }

      assertEqual(0, db[cn1].count());
      assertEqual(0, db[cn2].count());
    },

    // //////////////////////////////////////////////////////////////////////////////
    // / @brief test: usage of barriers outside of graph transaction
    // //////////////////////////////////////////////////////////////////////////////

    testUseBarriersOutsideGraphTransaction: function () {
      const graphName = 'UnitTestsGraph';
      G._create(graphName, [G._relation(cn2, cn1, cn1)]);

      var obj = {
        collections: {
          write: [ cn1, cn2 ]
        },
        action: function () {
          var graph = require('@arangodb/general-graph')._graph(params.graphName);
          var result = { };

          result.enxirvp = graph[params.cn1].save({});
          result.biitqtk = graph[params.cn1].save({});
          result.oboyuhh = graph[params.cn2].save({_from: result.enxirvp._id, _to: result.biitqtk._id, name: 'john smith'});
          result.oboyuhh = graph[params.cn2].document(result.oboyuhh);
          result.cvwmkym = require('@arangodb').db[params.cn1].replace(result.enxirvp._id, { _rev: null });
          result.gsalfxu = require('@arangodb').db[params.cn1].replace(result.biitqtk._id, { _rev: null });
          result.xsjzbst = (function () {
            graph[params.cn2].remove(result.oboyuhh._id);
            return true;
          }());

          graph[params.cn2].save({_from: result.cvwmkym._id, _to: result.gsalfxu._id, _key: result.oboyuhh._key, name: 'david smith'});
          result.rldfnre = graph[params.cn2].document(result.oboyuhh._key);

          return result;
        },
        params: {
          graphName: graphName,
          cn1: cn1,
          cn2: cn2
        }
      };

      var result = require('@arangodb').db._executeTransaction(obj);
      assertTrue(result.enxirvp._key.length > 0);
      assertEqual(undefined, result.enxirvp.name);

      assertTrue(result.biitqtk._key.length > 0);
      assertEqual(undefined, result.biitqtk.name);

      assertTrue(result.oboyuhh._key.length > 0);
      assertEqual('john smith', result.oboyuhh.name);
      assertEqual(undefined, result.oboyuhh.$label);
      assertTrue(result.oboyuhh._from.length > 0);
      assertTrue(result.oboyuhh._to.length > 0);

      assertTrue(result.cvwmkym._key.length > 0);
      assertEqual(undefined, result.cvwmkym.name);
      assertEqual(result.enxirvp._rev, result.cvwmkym._oldRev);

      assertTrue(result.gsalfxu._key.length > 0);
      assertEqual(undefined, result.gsalfxu.name);
      assertEqual(result.biitqtk._rev, result.gsalfxu._oldRev);

      assertEqual(true, result.xsjzbst);

      assertTrue(result.rldfnre._key.length > 0);
      assertEqual(result.oboyuhh._key, result.rldfnre._key);
      assertEqual('david smith', result.rldfnre.name);
      assertEqual(undefined, result.rldfnre.$label);
    },

    // //////////////////////////////////////////////////////////////////////////////
    // / @brief test: usage of read-copied documents inside write-transaction
    // //////////////////////////////////////////////////////////////////////////////

    testReadWriteDocumentsList: function () {
      c1.save({ _key: 'bar' });
      c1.save({ _key: 'baz' });
      c2.save(cn1 + '/bar', cn1 + '/baz', { type: 'one' });
      c2.save(cn1 + '/baz', cn1 + '/bar', { type: 'two' });

      var obj = {
        collections: {
          write: [ cn2 ]
        },
        action: function () {
          const db = require('@arangodb').db;
          var result = [ ];
          let c2 = db._collection(params.cn2);
          result.push(c2.inEdges(params.cn1 + '/baz'));
          result.push(c2.inEdges(params.cn1 + '/bar'));

          c2.save(params.cn1 + '/foo', params.cn1 + '/baz', { type: 'three' });
          c2.save(params.cn1 + '/foo', params.cn1 + '/bar', { type: 'four' });

          result.push(c2.inEdges(params.cn1 + '/baz'));
          result.push(c2.inEdges(params.cn1 + '/bar'));

          return result;
        },
        params: {
          cn1: cn1,
          cn2: cn2
        }
      };

      var sorter = function (l, r) {
        if (l.type !== r.type) {
          if (l.type < r.type) {
            return -1;
          }
          return 1;
        }
        return 0;
      };

      var result = require('@arangodb').db._executeTransaction(obj);

      assertEqual(4, result.length);
      var r = result[0];
      assertEqual(1, r.length);
      assertEqual(cn1 + '/bar', r[0]._from);
      assertEqual(cn1 + '/baz', r[0]._to);
      assertEqual('one', r[0].type);

      r = result[1];
      assertEqual(1, r.length);
      assertEqual(cn1 + '/baz', r[0]._from);
      assertEqual(cn1 + '/bar', r[0]._to);
      assertEqual('two', r[0].type);

      r = result[2];
      r.sort(sorter);

      assertEqual(2, r.length);
      assertEqual(cn1 + '/bar', r[0]._from);
      assertEqual(cn1 + '/baz', r[0]._to);
      assertEqual('one', r[0].type);

      assertEqual(cn1 + '/foo', r[1]._from);
      assertEqual(cn1 + '/baz', r[1]._to);
      assertEqual('three', r[1].type);

      r = result[3];
      r.sort(sorter);

      assertEqual(cn1 + '/foo', r[0]._from);
      assertEqual(cn1 + '/bar', r[0]._to);
      assertEqual('four', r[0].type);

      assertEqual(2, r.length);
      assertEqual(cn1 + '/baz', r[1]._from);
      assertEqual(cn1 + '/bar', r[1]._to);
      assertEqual('two', r[1].type);
    }

  };
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief test suite
// //////////////////////////////////////////////////////////////////////////////

function transactionRollbackSuite () {
  'use strict';
  const cn1 = 'UnitTestsTransaction1';

  let c1 = null;

  return {

    setUp: function () {
      db._drop(cn1);
    },

    tearDown: function () {
      db._drop(cn1);
      c1 = null;
    },

    // //////////////////////////////////////////////////////////////////////////////
    // / @brief test: rollback after flush
    // //////////////////////////////////////////////////////////////////////////////

    testRollbackAfterFlush: function () {
      c1 = db._create(cn1);

      var obj = {
        collections: {
          write: [ cn1 ]
        },
        action: function () {
          const db = require('@arangodb').db;
          let c1 = db._collection(params.cn1);
          c1.save({ _key: 'tom' });
          c1.save({ _key: 'tim' });
          c1.save({ _key: 'tam' });

          internal.wal.flush(true);
          assertEqual(3, c1.count());
          throw new Error('rollback');
        },
        params: {
          cn1: cn1
        }
      };

      try {
        db._executeTransaction(obj);
        fail();
      } catch (err) {
      }

      assertEqual(0, c1.count());

      testHelper.waitUnload(c1);

      assertEqual(0, c1.count());
    },

    // //////////////////////////////////////////////////////////////////////////////
    // / @brief test: the collection revision id
    // //////////////////////////////////////////////////////////////////////////////

    testRollbackRevision: function () {
      c1 = db._create(cn1);
      c1.save({ _key: 'foo' });

      var r = c1.revision();

      var obj = {
        collections: {
          write: [ cn1 ]
        },
        action: function () {
          var _r = r;
          const db = require('@arangodb').db;
          let c1 = db._collection(params.cn1);

          c1.save({ _key: 'tom' });
          require("jsunity").jsUnity.assertions.assertEqual(1, compareStringIds(c1.revision(), _r));
          _r = c1.revision();

          c1.save({ _key: 'tim' });
          require("jsunity").jsUnity.assertions.assertEqual(1, compareStringIds(c1.revision(), _r));
          _r = c1.revision();

          c1.save({ _key: 'tam' });
          require("jsunity").jsUnity.assertions.assertEqual(1, compareStringIds(c1.revision(), _r));
          _r = c1.revision();

          c1.remove('tam');
          require("jsunity").jsUnity.assertions.assertEqual(1, compareStringIds(c1.revision(), _r));
          _r = c1.revision();

          c1.update('tom', { 'bom': true });
          require("jsunity").jsUnity.assertions.assertEqual(1, compareStringIds(c1.revision(), _r));
          _r = c1.revision();

          c1.remove('tom');
          require("jsunity").jsUnity.assertions.assertEqual(1, compareStringIds(c1.revision(), _r));
          // _r = c1.revision();

          throw new Error('rollback');
        },
        params: {
          cn1: cn1
        }
      };

      try {
        db._executeTransaction(obj);
        fail();
      } catch (err) {
      }

      assertEqual(1, c1.count());
      assertEqual(r, c1.revision());
    },

    // //////////////////////////////////////////////////////////////////////////////
    // / @brief test: rollback inserts
    // //////////////////////////////////////////////////////////////////////////////

    testRollbackInsert: function () {
      c1 = db._create(cn1);
      c1.save({ _key: 'foo' });
      c1.save({ _key: 'bar' });
      c1.save({ _key: 'meow' });

      var obj = {
        collections: {
          write: [ cn1 ]
        },
        action: function () {
          const db = require('@arangodb').db;
          let c1 = db._collection(params.cn1);

          c1.save({ _key: 'tom' });
          c1.save({ _key: 'tim' });
          c1.save({ _key: 'tam' });

          require("jsunity").jsUnity.assertions.assertEqual(6, c1.count());
          throw new Error('rollback');
        },
        params: {
          cn1: cn1
        }
      };

      try {
        db._executeTransaction(obj);
        fail();
      } catch (err) {
      }

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

      c1.ensureIndex({ type: "persistent", fields: ["value"] });

      let obj = {
        collections: {
          write: [ cn1 ]
        },
        action: function () {
          const db = require('@arangodb').db;
          let c1 = db._collection(params.cn1);

          c1.save({ _key: 'tom', value: 'tom' });
          c1.save({ _key: 'tim', value: 'tim' });
          c1.save({ _key: 'tam', value: 'tam' });
          c1.save({ _key: 'troet', value: 'foo', a: 2 });
          c1.save({ _key: 'floxx', value: 'bar', a: 2 });
          require("jsunity").jsUnity.assertions.assertEqual(8, c1.count());

          let err = new Error('rollback');
          err.errorNum = 42;
          throw err;
        },
        params: {
          cn1: cn1
        }
      };

      try {
        db._executeTransaction(obj);
        fail();
      } catch (err) {
        assertEqual(err.errorNum, 42);
      }

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

      var obj = {
        collections: {
          write: [ cn1 ]
        },
        action: function () {
          const db = require('@arangodb').db;
          let c1 = db._collection(params.cn1);

          c1.save({ _key: 'tom' });
          c1.save({ _key: 'tim' });
          c1.save({ _key: 'tam' });

          c1.update('tom', { });
          c1.update('tim', { });
          c1.update('tam', { });
          c1.update('bar', { });
          c1.remove('foo');
          c1.remove('bar');
          c1.remove('meow');
          c1.remove('tom');

          require("jsunity").jsUnity.assertions.assertEqual(2, c1.count());
          throw new Error('rollback');
        },
        params: {
          cn1: cn1
        }
      };

      try {
        db._executeTransaction(obj);
        fail();
      } catch (err) {
      }

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

      var obj = {
        collections: {
          write: [ cn1 ]
        },
        action: function () {
          const db = require('@arangodb').db;
          let c1 = db._collection(params.cn1);

          c1.update(d1, { a: 4 });
          c1.update(d2, { a: 5 });
          c1.update(d3, { a: 6 });

          require("jsunity").jsUnity.assertions.assertEqual(3, c1.count());
          throw new Error('rollback');
        },
        params: {
          cn1: cn1
        }
      };

      try {
        db._executeTransaction(obj);
        fail();
      } catch (err) {
      }

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

      var obj = {
        collections: {
          write: [ cn1 ]
        },
        action: function () {
          const db = require('@arangodb').db;
          let c1 = db._collection(params.cn1);

          for (var i = 0; i < 100; ++i) {
            c1.replace('foo', { a: i });
            c1.replace('bar', { a: i + 42 });
            c1.replace('meow', { a: i + 23 });

            require("jsunity").jsUnity.assertions.assertEqual(i, c1.document('foo').a);
            require("jsunity").jsUnity.assertions.assertEqual(i + 42, c1.document('bar').a);
            require("jsunity").jsUnity.assertions.assertEqual(i + 23, c1.document('meow').a);
          }

          require("jsunity").jsUnity.assertions.assertEqual(3, c1.count());

          throw new Error('rollback');
        },
        params: {
          cn1: cn1
        }
      };

      try {
        db._executeTransaction(obj);
        fail();
      } catch (err) {
      }

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

      c1.ensureIndex({ type: "persistent", fields: ["value"] });
      let obj = {
        collections: {
          write: [ cn1 ]
        },
        action: function () {
          const db = require('@arangodb').db;
          let c1 = db._collection(params.cn1);

          c1.update('foo', { value: 'foo', a: 2 });
          c1.update('bar', { value: 'bar', a: 2 });
          c1.update('meow', { value: 'troet' });

          require("jsunity").jsUnity.assertions.assertEqual(3, c1.count());

          throw new Error('rollback');
        },
        params: {
          cn1: cn1
        }
      };

      try {
        db._executeTransaction(obj);
        fail();
      } catch (err) {
      }

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

      var obj = {
        collections: {
          write: [ cn1 ]
        },
        action: function () {
          const db = require('@arangodb').db;
          let c1 = db._collection(params.cn1);

          c1.remove('meow');
          c1.remove('foo');

          require("jsunity").jsUnity.assertions.assertEqual(1, c1.count());
          require("jsunity").jsUnity.assertions.assertEqual([ 'bar' ], sortedKeys(c1));

          throw new Error('rollback');
        },
        params: {
          cn1: cn1
        }
      };

      try {
        db._executeTransaction(obj);
        fail();
      } catch (err) {
      }

      assertEqual(3, c1.count());
      assertEqual([ 'bar', 'foo', 'meow' ], sortedKeys(c1));
    },

    // //////////////////////////////////////////////////////////////////////////////
    // / @brief test: rollback remove
    // //////////////////////////////////////////////////////////////////////////////

    testRollbackRemoveMulti: function () {
      c1 = db._create(cn1);
      c1.save({ _key: 'foo' });

      var obj = {
        collections: {
          write: [ cn1 ]
        },
        action: function () {
          const db = require('@arangodb').db;
          let c1 = db._collection(params.cn1);

          for (var i = 0; i < 100; ++i) {
            c1.save({ _key: 'foo' + i });
          }

          require("jsunity").jsUnity.assertions.assertEqual(101, c1.count());

          throw new Error('rollback');
        },
        params: {
          cn1: cn1
        }
      };

      try {
        db._executeTransaction(obj);
        fail();
      } catch (err) {
      }

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

      c1.ensureIndex({ type: "persistent", fields: ["value"] });
      let obj = {
        collections: {
          write: [ cn1 ]
        },
        action: function () {
          const db = require('@arangodb').db;
          let c1 = db._collection(params.cn1);

          c1.remove('meow');
          c1.remove('bar');
          c1.remove('foo');

          require("jsunity").jsUnity.assertions.assertEqual(0, c1.count());

          throw new Error('rollback');
        },
        params: {
          cn1: cn1
        }
      };

      try {
        db._executeTransaction(obj);
        fail();
      } catch (err) {
      }

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

      c1.ensureIndex({ type: "persistent", fields: ["value"] });

      let obj = {
        collections: {
          write: [ cn1 ]
        },
        action: function () {
          const db = require('@arangodb').db;
          let c1 = db._collection(params.cn1);

          c1.remove('meow');
          c1.remove('bar');
          c1.remove('foo');
          require("jsunity").jsUnity.assertions.assertEqual(0, c1.count());

          c1.save({ _key: 'foo2', value: 'foo', a: 2 });
          c1.save({ _key: 'bar2', value: 'bar', a: 2 });
          require("jsunity").jsUnity.assertions.assertEqual(2, c1.count());

          throw new Error('rollback');
        },
        params: {
          cn1: cn1
        }
      };

      try {
        db._executeTransaction(obj);
        fail();
      } catch (err) {
      }

      assertEqual(3, c1.count());
    },

    // //////////////////////////////////////////////////////////////////////////////
    // / @brief test: rollback truncate
    // //////////////////////////////////////////////////////////////////////////////

    testRollbackTruncateEmpty: function () {
      c1 = db._create(cn1);

      var obj = {
        collections: {
          write: [ cn1 ]
        },
        action: function () {
          const db = require('@arangodb').db;
          let c1 = db._collection(params.cn1);

          // truncate often...
          for (var i = 0; i < 100; ++i) {
            c1.truncate();
          }

          throw new Error('rollback');
        },
        params: {
          cn1: cn1
        }
      };

      try {
        db._executeTransaction(obj);
        fail();
      } catch (err) {
      }

      assertEqual(0, c1.count());
      assertEqual([ ], sortedKeys(c1));
    },

    // //////////////////////////////////////////////////////////////////////////////
    // / @brief test: rollback truncate
    // //////////////////////////////////////////////////////////////////////////////

    testRollbackTruncateNonEmpty: function () {
      c1 = db._create(cn1);
      let docs = [];
      for (let i = 0; i < 100; ++i) {
        docs.push({ _key: 'foo' + i });
      }
      c1.insert(docs);
      assertEqual(100, c1.count());

      var obj = {
        collections: {
          write: [ cn1 ]
        },
        action: function () {
          const db = require('@arangodb').db;
          let c1 = db._collection(params.cn1);

          // truncate often...
          for (var i = 0; i < 100; ++i) {
            c1.truncate();
          }
          c1.save({ _key: 'bar' });

          throw new Error('rollback');
        },
        params: {
          cn1: cn1
        }
      };

      try {
        db._executeTransaction(obj);
        fail();
      } catch (err) {
      }

      assertEqual(100, c1.count());
    },

    // //////////////////////////////////////////////////////////////////////////////
    // / @brief test: trx rollback with unique constraint violation
    // //////////////////////////////////////////////////////////////////////////////

    testRollbackUniquePrimary: function () {
      c1 = db._create(cn1);
      var d1 = c1.save({ _key: 'foo' });

      var obj = {
        collections: {
          write: [ cn1 ]
        },
        action: function () {
          const db = require('@arangodb').db;
          let c1 = db._collection(params.cn1);

          c1.save({ _key: 'bar' });
          c1.save({ _key: 'foo' });
          fail();
        },
        params: {
          cn1: cn1
        }
      };

      try {
        db._executeTransaction(obj);
        fail();
      } catch (err) {
      }

      assertEqual(1, c1.count());
      assertEqual('foo', c1.toArray()[0]._key);
      assertEqual(d1._rev, c1.toArray()[0]._rev);
    },

    // //////////////////////////////////////////////////////////////////////////////
    // / @brief test: trx rollback with unique constraint violation
    // //////////////////////////////////////////////////////////////////////////////

    testRollbackUniqueSecondary: function () {
      c1 = db._create(cn1);
      c1.ensureIndex({ type: "persistent", fields: ["name"], unique: true });
      let d1 = c1.save({ name: 'foo' });

      let obj = {
        collections: {
          write: [ cn1 ]
        },
        action: function () {
          const db = require('@arangodb').db;
          let c1 = db._collection(params.cn1);

          c1.save({ name: 'bar' });
          c1.save({ name: 'baz' });
          c1.save({ name: 'foo' });
          fail();
        },
        params: {
          cn1: cn1
        }
      };

      try {
        db._executeTransaction(obj);
        fail();
      } catch (err) {
      }
      assertEqual(1, c1.count());
      assertEqual('foo', c1.toArray()[0].name);
      assertEqual(d1._rev, c1.toArray()[0]._rev);
    },

    // //////////////////////////////////////////////////////////////////////////////
    // / @brief test: rollback a mixed workload
    // //////////////////////////////////////////////////////////////////////////////

    testRollbackMixed1: function () {
      c1 = db._create(cn1);

      let docs = [];
      for (let i = 0; i < 100; ++i) {
        docs.push({_key: 'key' + i, value: i });
      }
      c1.insert(docs);

      var obj = {
        collections: {
          write: [ cn1 ]
        },
        action: function () {
          const db = require('@arangodb').db;
          let c1 = db._collection(params.cn1);

          var i;

          for (i = 0; i < 50; ++i) {
            c1.remove('key' + i);
          }

          for (i = 50; i < 100; ++i) {
            c1.update('key' + i, { value: i - 50 });
          }

          c1.remove('key50');
          throw new Error('doh!');
        },
        params: {
          cn1: cn1
        }
      };

      try {
        db._executeTransaction(obj);
        fail();
      } catch (err) {
      }

      assertEqual(100, c1.count());
    },

    // //////////////////////////////////////////////////////////////////////////////
    // / @brief test: rollback a mixed workload
    // //////////////////////////////////////////////////////////////////////////////

    testRollbackMixed2: function () {
      c1 = db._create(cn1);

      c1.save({ _key: 'foo' });
      c1.save({ _key: 'bar' });

      var obj = {
        collections: {
          write: [ cn1 ]
        },
        action: function () {
          var i;
          const db = require('@arangodb').db;
          let c1 = db._collection(params.cn1);

          for (i = 0; i < 10; ++i) {
            c1.save({ _key: 'key' + i, value: i });
          }

          for (i = 0; i < 5; ++i) {
            c1.remove('key' + i);
          }

          for (i = 5; i < 10; ++i) {
            c1.update('key' + i, { value: i - 5 });
          }

          c1.remove('key5');
          throw new Error('doh!');
        },
        params: {
          cn1: cn1
        }
      };

      try {
        db._executeTransaction(obj);
        fail();
      } catch (err) {
      }

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

      var obj = {
        collections: {
          write: [ cn1 ]
        },
        action: function () {
          var i;
          const db = require('@arangodb').db;
          let c1 = db._collection(params.cn1);

          for (i = 0; i < 10; ++i) {
            c1.save({ _key: 'key' + i, value: i });
          }

          for (i = 0; i < 10; ++i) {
            c1.remove('key' + i);
          }

          for (i = 0; i < 10; ++i) {
            c1.save({ _key: 'key' + i, value: i });
          }

          for (i = 0; i < 10; ++i) {
            c1.update('key' + i, { value: i - 5 });
          }

          for (i = 0; i < 10; ++i) {
            c1.update('key' + i, { value: i + 5 });
          }

          for (i = 0; i < 10; ++i) {
            c1.remove('key' + i);
          }

          for (i = 0; i < 10; ++i) {
            c1.save({ _key: 'key' + i, value: i });
          }

          throw new Error('doh!');
        },
        params: {
          cn1: cn1
        }
      };

      try {
        db._executeTransaction(obj);
        fail();
      } catch (err) {
      }

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
  const cn1 = 'UnitTestsTransaction1';

  let c1 = null;

  return {

    setUp: function () {
      db._drop(cn1);
    },

    tearDown: function () {
      db._drop(cn1);
      c1 = null;
    },

    // //////////////////////////////////////////////////////////////////////////////
    // / @brief test: test counts during trx
    // //////////////////////////////////////////////////////////////////////////////

    testCountDuring: function () {
      c1 = db._create(cn1);
      assertEqual(0, c1.count());

      var obj = {
        collections: {
          write: [ cn1 ]
        },
        action: function () {
          var d1, d2;
          const db = require('@arangodb').db;
          let c1 = db._collection(params.cn1);

          c1.save({ a: 1 });
          require("jsunity").jsUnity.assertions.assertEqual(1, c1.count());

          d1 = c1.save({ a: 2 });
          require("jsunity").jsUnity.assertions.assertEqual(2, c1.count());

          d2 = c1.update(d1, { a: 3 });
          require("jsunity").jsUnity.assertions.assertEqual(2, c1.count());

          require("jsunity").jsUnity.assertions.assertEqual(3, c1.document(d2).a);

          c1.remove(d2);
          require("jsunity").jsUnity.assertions.assertEqual(1, c1.count());

          c1.truncate({ compact: false });
          require("jsunity").jsUnity.assertions.assertEqual(0, c1.count());

          c1.truncate({ compact: false });
          require("jsunity").jsUnity.assertions.assertEqual(0, c1.count());

          return true;
        },
        params: {
          cn1: cn1
        }
      };

      require('@arangodb').db._executeTransaction(obj);
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

      var obj = {
        collections: {
          write: [ cn1 ]
        },
        action: function () {
          const db = require('@arangodb').db;
          let c1 = db._collection(params.cn1);
          c1.save({ _key: 'baz' });
          require("jsunity").jsUnity.assertions.assertEqual(3, c1.count());

          require("internal").wal.flush(true, false);

          c1.save({ _key: 'meow' });
          require("jsunity").jsUnity.assertions.assertEqual(4, c1.count());

          require("internal").wal.flush(true, false);

          c1.remove('foo');
          require("jsunity").jsUnity.assertions.assertEqual(3, c1.count());

          return true;
        },
        params: {
          cn1: cn1
        }
      };

      require('@arangodb').db._executeTransaction(obj);
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

      var obj = {
        collections: {
          write: [ cn1 ]
        },
        action: function () {
          const db = require('@arangodb').db;
          let c1 = db._collection(params.cn1);
          c1.save({ _key: 'baz' });
          require("jsunity").jsUnity.assertions.assertEqual(3, c1.count());

          c1.save({ _key: 'meow' });
          require("jsunity").jsUnity.assertions.assertEqual(4, c1.count());

          c1.remove('foo');
          require("jsunity").jsUnity.assertions.assertEqual(3, c1.count());
          return true;
        },
        params: {
          cn1: cn1
        }
      };

      require('@arangodb').db._executeTransaction(obj);
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

      var obj = {
        collections: {
          write: [ cn1 ]
        },
        action: function () {
          c1.save({ _key: 'baz' });
          require("jsunity").jsUnity.assertions.assertEqual(3, c1.count());

          c1.save({ _key: 'meow' });
          require("jsunity").jsUnity.assertions.assertEqual(4, c1.count());

          c1.remove('foo');
          require("jsunity").jsUnity.assertions.assertEqual(3, c1.count());

          throw new Error('rollback');
        }
      };

      try {
        db._executeTransaction(obj);
        fail();
      } catch (err) {
      }
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
  const cn1 = 'UnitTestsTransaction1';
  const cn2 = 'UnitTestsTransaction2';

  let c1 = null;
  let c2 = null;

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
      db._drop(cn1);
      c1 = null;

      db._drop(cn2);
      c2 = null;
    },

    // //////////////////////////////////////////////////////////////////////////////
    // / @brief test: test cross collection commit
    // //////////////////////////////////////////////////////////////////////////////

    testInserts: function () {
      c1 = db._create(cn1);
      c2 = db._create(cn2);

      var obj = {
        collections: {
          write: [ cn1, cn2 ]
        },
        action: function () {
          var i;
          const db = require('@arangodb').db;
          let c1 = db._collection(params.cn1);
          let c2 = db._collection(params.cn2);
          for (i = 0; i < 10; ++i) {
            c1.save({ _key: 'a' + i });
            c2.save({ _key: 'b' + i });
          }

          return true;
        },
        params: {
          cn1: cn1,
          cn2: cn2
        }
      };

      require('@arangodb').db._executeTransaction(obj);
      assertEqual(10, c1.count());
      assertEqual(10, c2.count());
    },

    // //////////////////////////////////////////////////////////////////////////////
    // / @brief test: test cross collection commit
    // //////////////////////////////////////////////////////////////////////////////

    testUpdates: function () {
      c1 = db._create(cn1);
      c2 = db._create(cn2);

      let docs1 = [];
      let docs2 = [];
      for (let i = 0; i < 10; ++i) {
        docs1.push({ _key: 'a' + i, a: i });
        docs2.push({ _key: 'b' + i, b: i });
      }
      c1.insert(docs1);
      c2.insert(docs2);

      var obj = {
        collections: {
          write: [ cn1, cn2 ]
        },
        action: function () {
          var i;
          const db = require('@arangodb').db;
          let c1 = db._collection(params.cn1);
          let c2 = db._collection(params.cn2);
          for (i = 0; i < 10; ++i) {
            c1.update('a' + i, { a: i + 20 });

            c2.update('b' + i, { b: i + 20 });
            c2.remove('b' + i);
          }

          return true;
        },
        params: {
          cn1: cn1,
          cn2: cn2
        }
      };

      require('@arangodb').db._executeTransaction(obj);
      assertEqual(10, c1.count());
      assertEqual(0, c2.count());
    },

    // //////////////////////////////////////////////////////////////////////////////
    // / @brief test: test cross collection commit
    // //////////////////////////////////////////////////////////////////////////////

    testDeletes: function () {
      c1 = db._create(cn1);
      c2 = db._create(cn2);

      let docs1 = [];
      let docs2 = [];
      for (let i = 0; i < 10; ++i) {
        docs1.push({ _key: 'a' + i, a: i });
        docs2.push({ _key: 'b' + i, b: i });
      }
      c1.insert(docs1);
      c2.insert(docs2);

      var obj = {
        collections: {
          write: [ cn1, cn2 ]
        },
        action: function () {
          var i;
          const db = require('@arangodb').db;
          let c1 = db._collection(params.cn1);
          let c2 = db._collection(params.cn2);
          for (i = 0; i < 10; ++i) {
            c1.remove('a' + i);
            c2.remove('b' + i);
          }

          return true;
        },
        params: {
          cn1: cn1,
          cn2: cn2
        }
      };

      require('@arangodb').db._executeTransaction(obj);
      assertEqual(0, c1.count());
      assertEqual(0, c2.count());
    },

    // //////////////////////////////////////////////////////////////////////////////
    // / @brief test: test cross collection commit
    // //////////////////////////////////////////////////////////////////////////////

    testDeleteReload: function () {
      c1 = db._create(cn1);
      c2 = db._create(cn2);

      let docs1 = [];
      let docs2 = [];
      for (let i = 0; i < 10; ++i) {
        docs1.push({ _key: 'a' + i, a: i });
        docs2.push({ _key: 'b' + i, b: i });
      }
      c1.insert(docs1);
      c2.insert(docs2);

      var obj = {
        collections: {
          write: [ cn1, cn2 ]
        },
        action: function () {
          var i;
          const db = require('@arangodb').db;
          let c1 = db._collection(params.cn1);
          let c2 = db._collection(params.cn2);
          for (i = 0; i < 10; ++i) {
            c1.remove('a' + i);
            c2.remove('b' + i);
          }

          return true;
        },
        params: {
          cn1: cn1,
          cn2: cn2
        }
      };

      require('@arangodb').db._executeTransaction(obj);
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

      var obj = {
        collections: {
          write: [ cn1, cn2 ]
        },
        action: function () {
          var i;
          const db = require('@arangodb').db;
          let c1 = db._collection(params.cn1);
          let c2 = db._collection(params.cn2);

          for (i = 0; i < 10; ++i) {
            c1.save({ _key: 'a' + i });
          }

          for (i = 0; i < 10; ++i) {
            c2.save({ _key: 'b' + i });
          }

          if (10 !== c1.count()) {
            throw new Error(`10 !== c1.count()`);
          }
          if (10 !== c2.count()) {
            throw new Error(`10 !== c2.count()`);
          }

          c1.remove('a4');
          c2.remove('b6');

          return true;
        },
        params: {
          cn1: cn1,
          cn2: cn2
        }
      };

      require('@arangodb').db._executeTransaction(obj);
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

      var obj = {
        collections: {
          write: [ cn1, cn2 ]
        },
        action: function () {
          c1.save({ _key: 'a2' });
          c1.save({ _key: 'a3' });

          c2.save({ _key: 'b2' });
          c2.update('b1', { a: 2 });
          assertEqual(2, c2.document('b1').a);

          throw new Error('rollback');
        }
      };

      try {
        db._executeTransaction(obj);
        fail();
      } catch (err) {
      }

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
    // / @brief test: unload / reload after failed transactions
    // //////////////////////////////////////////////////////////////////////////////

    testUnloadReloadFailedTrx: function () {
      c1 = db._create(cn1);

      let docs1 = [];
      for (let i = 0; i < 10; ++i) {
        docs1.push({ _key: 'a' + i, a: i });
      }
      c1.insert(docs1);

      var obj = {
        collections: {
          write: [ cn1 ]
        },
        action: function () {
          var i;
          for (i = 0; i < 100; ++i) {
            c1.save({ _key: 'test' + i });
          }

          throw new Error('rollback');
        }
      };

      for (let i = 0; i < 50; ++i) {
        try {
          require('@arangodb').db._executeTransaction(obj);
          fail();
        } catch (err) {
        }
      }

      assertEqual(10, c1.count());

      testHelper.waitUnload(c1);

      assertEqual(10, c1.count());
    }

  };
}

function transactionConstraintsSuite () {
  'use strict';
  const cn = 'UnitTestsTransaction';
  let c = null;

  return {

    setUp: function () {
      db._drop(cn);
    },

    tearDown: function () {
      db._drop(cn);
      c = null;
    },

    // //////////////////////////////////////////////////////////////////////////////
    // / @brief test: rollback in case of a server-side fail
    // //////////////////////////////////////////////////////////////////////////////

    testMultiUniqueConstraintInsert1: function () {
      c = db._create(cn);
      c.ensureIndex({ type: "persistent", fields: ["value1"], unique: true });
      c.ensureIndex({ type: "persistent", fields: ["value2"], unique: true });

      let docs = [];
      for (let i = 0; i < 10; ++i) {
        docs.push({ _key: 'test' + i, value1: i, value2: i });
      }
      c.insert(docs);
      assertEqual(10, c.count());

      try {
        c.insert({ value1: 9, value2: 17 });
        fail();
      } catch (err) {
        assertEqual(internal.errors.ERROR_ARANGO_UNIQUE_CONSTRAINT_VIOLATED.code, err.errorNum);
      }

      assertEqual(10, c.count());
      assertEqual(9, c.document('test9').value1);
      assertEqual(9, c.document('test9').value2);
    },

    // //////////////////////////////////////////////////////////////////////////////
    // / @brief test: rollback in case of a server-side fail
    // //////////////////////////////////////////////////////////////////////////////

    testMultiUniqueConstraintInsert2: function () {
      c = db._create(cn);
      c.ensureIndex({ type: "persistent", fields: ["value1"], unique: true });
      c.ensureIndex({ type: "persistent", fields: ["value2"], unique: true });

      let docs = [];
      for (let i = 0; i < 10; ++i) {
        docs.push({ _key: 'test' + i, value1: i, value2: i });
      }
      c.insert(docs);
      assertEqual(10, c.count());

      try {
        c.insert({ value1: 17, value2: 9 });
        fail();
      } catch (err) {
        assertEqual(internal.errors.ERROR_ARANGO_UNIQUE_CONSTRAINT_VIOLATED.code, err.errorNum);
      }

      assertEqual(10, c.count());
      assertEqual(9, c.document('test9').value1);
      assertEqual(9, c.document('test9').value2);
    },

  };
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief test suite
// //////////////////////////////////////////////////////////////////////////////

function transactionTraversalSuite () {
  'use strict';
  const cn = 'UnitTestsTransaction';

  return {

    setUp: function () {
      db._drop(cn + 'Vertex');
      db._drop(cn + 'Edge');
      db._create(cn + 'Vertex');
      db._createEdgeCollection(cn + 'Edge');

      let docs = [];
      for (let i = 0; i < 100; ++i) {
        docs.push({ _key: String(i) });
      }
      db.UnitTestsTransactionVertex.insert(docs);

      docs = [];
      for (let i = 1; i < 100; ++i) {
        docs.push({_from: cn + 'Vertex/' + i, _to: cn + 'Vertex/' + (i + 1) });
      }
      db.UnitTestsTransactionEdge.insert(docs);
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
      var result = db._executeTransaction({
        collections: {
          read: [ cn + 'Edge' ],
          write: [ cn + 'Edge' ]
        },
        action: function () {
          var db = require('internal').db;

          var results = db._query('WITH ' + params.cn + 'Vertex FOR v, e IN ANY "' + params.cn + 'Vertex/20" ' + 
                                  params.cn + 'Edge FILTER v._id == "' + params.cn + 
                                  'Vertex/21" LIMIT 1 RETURN e').toArray();

          if (results.length > 0) {
            var result = results[0];
            db[params.cn + 'Edge'].remove(result);
            return 1;
          }
          return 0;
        },
        params: {
          cn: cn
        }
      });

      assertEqual(1, result);
    },

    // //////////////////////////////////////////////////////////////////////////////
    // / @brief test: use of undeclared traversal collection in transaction
    // //////////////////////////////////////////////////////////////////////////////

    testTestCount: function () {
      for (var i = 0; i < 100; ++i) {
        db[cn + 'Edge'].insert(cn + 'Edge/test' + (i % 21), cn + 'Edge/test' + (i % 7), { });
      }

      var result = db._executeTransaction({
        collections: {
          read: [ cn + 'Edge' ],
          write: [ cn + 'Edge' ]
        },
        action: function () {
          var db = require('internal').db;
          var from = params.cn + 'Edge/test1';
          var to = params.cn + 'Edge/test8';

          var newDoc = db[params.cn + 'Edge'].insert(from, to, { request: true });
          var fromCount1 = db[params.cn + 'Edge'].byExample({ _from: from, request: false }).count();

          newDoc.request = false;
          db[params.cn + 'Edge'].update({ _id: newDoc._id }, newDoc);

          var fromCount2 = db[params.cn + 'Edge'].byExample({ _from: from, request: false }).count();
          return [ fromCount1, fromCount2 ];
        },
        params: {
          cn: cn
        }
      });

      assertEqual(result[0] + 1, result[1]);
    }

  };
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief executes the test suites
// //////////////////////////////////////////////////////////////////////////////

if (IM.debugCanUseFailAt()) {
  jsunity.run(transactionFailuresSuite);
}
jsunity.run(transactionRevisionsSuite);
jsunity.run(transactionRollbackSuite);
jsunity.run(transactionInvocationSuite);
jsunity.run(transactionCollectionsSuite);
jsunity.run(transactionOperationsSuite);
jsunity.run(transactionBarriersSuite);
jsunity.run(transactionGraphSuite);
jsunity.run(transactionCountSuite);
jsunity.run(transactionCrossCollectionSuite);
jsunity.run(transactionConstraintsSuite);
jsunity.run(transactionTraversalSuite);

return jsunity.done();
