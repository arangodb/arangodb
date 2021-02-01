/*jshint globalstrict:false, strict:false */
/*global assertEqual, assertTrue, assertMatch, fail */

////////////////////////////////////////////////////////////////////////////////
/// @brief tests for client/server side transaction invocation
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
/// Copyright holder is triAGENS GmbH, Cologne, Germany
///
/// @author Jan Steemann
/// @author Copyright 2012, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

const jsunity = require("jsunity");
const arangodb = require("@arangodb");
const internal = require("internal");
const ERRORS = arangodb.errors;
const ArangoError = require("@arangodb").ArangoError;
const db = arangodb.db;

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////

function TransactionsInvocationsSuite() {
  'use strict';

  return {

    tearDown: function () {
      internal.wait(0);
    },
    
    testNestingLevelArrayOk: function () {
      let params = { doc: [] };
      let level = 0;
      let start = params.doc;
      while (level < 64) {
        start.push([]);
        start = start[0];
        ++level;
      }

      let obj = {
        collections: {},
        action: function (params) {
          return params.doc;
        },
        params
      };

      let result = db._executeTransaction(obj);
      assertEqual(params.doc, result);
    },
    
    testNestingLevelArrayBorderline: function () {
      let params = { doc: [] };
      let level = 0;
      let start = params.doc;
      while (level < 77) {
        start.push([]);
        start = start[0];
        ++level;
      }

      let obj = {
        collections: {},
        action: function (params) {
          return params.doc;
        },
        params
      };

      let result = db._executeTransaction(obj);
      assertEqual(params.doc, result);
    },
    
    testNestingLevelArrayTooDeep: function () {
      if (!global.ARANGOSH_PATH) {
        // we are arangod... in this case the JS -> JSON -> JS 
        // conversion will not take place, and we will not run into
        // an error here
        return;
      }

      let params = { doc: [] };
      let level = 0;
      let start = params.doc;
      while (level < 100) {
        start.push([]);
        start = start[0];
        ++level;
      }

      let obj = {
        collections: {},
        action: function (params) {
          return params.doc;
        },
        params
      };

      try {
        db._executeTransaction(obj);
        fail();
      } catch (err) {
        assertEqual(arangodb.errors.ERROR_BAD_PARAMETER.code, err.errorNum);
      }
    },
    
    testNestingLevelObjectOk: function () {
      let params = { doc: {} };
      let level = 0;
      let start = params.doc;
      while (level < 64) {
        start.doc = {};
        start = start.doc;
        ++level;
      }

      let obj = {
        collections: {},
        action: function (params) {
          return params.doc;
        },
        params
      };

      let result = db._executeTransaction(obj);
      assertEqual(params.doc, result);
    },
    
    testNestingLevelObjectBorderline: function () {
      let params = { doc: {} };
      let level = 0;
      let start = params.doc;
      while (level < 77) {
        start.doc = {};
        start = start.doc;
        ++level;
      }

      let obj = {
        collections: {},
        action: function (params) {
          return params.doc;
        },
        params
      };

      let result = db._executeTransaction(obj);
      assertEqual(params.doc, result);
    },
    
    testNestingLevelObjectTooDeep: function () {
      if (!global.ARANGOSH_PATH) {
        // we are arangod... in this case the JS -> JSON -> JS 
        // conversion will not take place, and we will not run into
        // an error here
        return;
      }

      let params = { doc: {} };
      let level = 0;
      let start = params.doc;
      while (level < 100) {
        start.doc = {};
        start = start.doc;
        ++level;
      }

      let obj = {
        collections: {},
        action: function (params) {
          return params.doc;
        },
        params
      };

      try {
        db._executeTransaction(obj);
        fail();
      } catch (err) {
        assertEqual(arangodb.errors.ERROR_BAD_PARAMETER.code, err.errorNum);
      }
    },

    testErrorHandling: function () {
      try {
        db._executeTransaction({
          collections: {},
          action: function () {
            var err = new Error('test');
            err.errorNum = 1234;
            Object.defineProperty(err, 'name', {
              get: function () { throw new Error('Error in getter'); }
            });
            throw err;
          }
        });
        fail();
      } catch (err) {
        assertTrue(err instanceof ArangoError);
        assertMatch(/test/, err.errorMessage);
        assertEqual(1234, err.errorNum);
      }
    },

    testErrorHandlingArangoError: function () {
      try {
        db._executeTransaction({
          collections: {},
          action: function () {
            const arangodb = require('@arangodb');
            var err = new arangodb.ArangoError();
            err.errorNum = arangodb.ERROR_BAD_PARAMETER;
            err.errorMessage = "who's bad?";
            throw err;
          }
        });
        fail();
      } catch (err) {
        assertTrue(err instanceof ArangoError);
        assertMatch(/who's bad?/, err.errorMessage);
        assertEqual(arangodb.ERROR_BAD_PARAMETER, err.errorNum);
      }
    },
    
    ////////////////////////////////////////////////////////////////////////////////
    /// @brief execute a transaction with a string action
    ////////////////////////////////////////////////////////////////////////////////

    testInvokeActionString: function () {
      let result = db._executeTransaction({
        collections: {},
        action: "function () { return 23; }"
      });

      assertEqual(23, result);
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief execute a transaction with a string function action
    ////////////////////////////////////////////////////////////////////////////////

    testInvokeActionStringFunction: function () {
      var result = db._executeTransaction({
        collections: {},
        action: "function () { return function () { return 11; }(); }"
      });

      assertEqual(11, result);
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief execute a transaction with a function action
    ////////////////////////////////////////////////////////////////////////////////

    testInvokeActionFunction: function () {
      var result = db._executeTransaction({
        collections: {},
        action: function () { return 42; }
      });

      assertEqual(42, result);
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief execute a transaction with an invalid action
    ////////////////////////////////////////////////////////////////////////////////

    testInvokeActionInvalid1: function () {
      try {
        db._executeTransaction({
          collections: {},
          action: null,
        });
        fail();
      } catch (err) {
        assertEqual(ERRORS.ERROR_BAD_PARAMETER.code, err.errorNum);
      }
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief execute a transaction without an action
    ////////////////////////////////////////////////////////////////////////////////

    testInvokeNoAction: function () {
      try {
        db._executeTransaction({
          collections: {}
        });
        fail();
      } catch (err) {
        assertEqual(ERRORS.ERROR_BAD_PARAMETER.code, err.errorNum);
      }
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief execute a transaction with a non-working action declaration
    ////////////////////////////////////////////////////////////////////////////////

    testInvokeActionBroken1: function () {
      try {
        db._executeTransaction({
          collections: {},
          action: "return 11;"
        });
        fail();
      } catch (err) {
        assertEqual(ERRORS.ERROR_BAD_PARAMETER.code, err.errorNum);
      }
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief execute a transaction with a non-working action declaration
    ////////////////////////////////////////////////////////////////////////////////

    testInvokeActionBroken2: function () {
      try {
        db._executeTransaction({
          collections: {},
          action: "function () { "
        });
        fail();
      } catch (err) {
        assertEqual(ERRORS.ERROR_BAD_PARAMETER.code, err.errorNum);
      }
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief execute a transaction with an invalid action
    ////////////////////////////////////////////////////////////////////////////////

    testInvokeActionInvalid2: function () {
      try {
        db._executeTransaction({
          collections: {},
          action: null,
        });
        fail();
      } catch (err) {
        assertEqual(ERRORS.ERROR_BAD_PARAMETER.code, err.errorNum);
      }
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief parameters
    ////////////////////////////////////////////////////////////////////////////////

    testParametersString: function () {
      var result = db._executeTransaction({
        collections: {},
        action: "function (params) { return [ params[1], params[4] ]; }",
        params: [1, 2, 3, 4, 5]
      });

      assertEqual([2, 5], result);
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief parameters
    ////////////////////////////////////////////////////////////////////////////////

    testParametersFunction: function () {
      var result = db._executeTransaction({
        collections: {},
        action: function (params) {
          return [params[1], params[4]];
        },
        params: [1, 2, 3, 4, 5]
      });

      assertEqual([2, 5], result);
    }

  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////

function TransactionsImplicitCollectionsSuite() {
  'use strict';

  var cn1 = "UnitTestsTransaction1";
  var cn2 = "UnitTestsTransaction2";
  var c1 = null;
  var c2 = null;

  return {

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief set up
    ////////////////////////////////////////////////////////////////////////////////

    setUp: function () {
      db._drop(cn1);
      db._drop(cn2);
      c1 = db._create(cn1);
      c2 = db._createEdgeCollection(cn2);
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief tear down
    ////////////////////////////////////////////////////////////////////////////////

    tearDown: function () {
      c1 = null;
      c2 = null;
      db._drop(cn1);
      db._drop(cn2);
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief test allowImplicit
    ////////////////////////////////////////////////////////////////////////////////

    testSingleReadOnly: function () {
      assertEqual([], db._executeTransaction({
        collections: { allowImplicit: false, read: cn1 },
        action: "function (params) { " +
          "return require('internal').db._query('FOR doc IN @@cn1 RETURN doc', { '@cn1' : params.cn }).toArray(); }",
        params: { cn: cn1 }
      }));
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief test read collection object
    ////////////////////////////////////////////////////////////////////////////////

    testSingleReadCollectionObject: function () {
      assertEqual([], db._executeTransaction({
        collections: { read: db._collection(cn1) },
        action: "function (params) { " +
          "return require('internal').db._query('FOR doc IN @@cn1 RETURN doc', { '@cn1' : params.cn }).toArray(); }",
        params: { cn: cn1 }
      }));
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief test read collection object in array
    ////////////////////////////////////////////////////////////////////////////////

    testSingleReadCollectionArray: function () {
      assertEqual([], db._executeTransaction({
        collections: { read: [db._collection(cn1)] },
        action: "function (params) { " +
          "return require('internal').db._query('FOR doc IN @@cn1 RETURN doc', { '@cn1' : params.cn }).toArray(); }",
        params: { cn: cn1 }
      }));
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief test allowImplicit
    ////////////////////////////////////////////////////////////////////////////////

    testSingleWriteOnly: function () {
      assertEqual([], db._executeTransaction({
        collections: { allowImplicit: false, write: cn1 },
        action: "function (params) { " +
          "return require('internal').db._query('FOR doc IN @@cn RETURN doc', { '@cn' : params.cn }).toArray(); }",
        params: { cn: cn1 }
      }));
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief test write collection object
    ////////////////////////////////////////////////////////////////////////////////

    testSingleWriteCollectionObject: function () {
      assertEqual([], db._executeTransaction({
        collections: { write: db._collection(cn1) },
        action: "function (params) { " +
          "return require('internal').db._query('FOR doc IN @@cn RETURN doc', { '@cn' : params.cn }).toArray(); }",
        params: { cn: cn1 }
      }));
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief test write collection object in array
    ////////////////////////////////////////////////////////////////////////////////

    testSingleWriteCollectionArray: function () {
      assertEqual([], db._executeTransaction({
        collections: { write: [db._collection(cn1)] },
        action: "function (params) { " +
          "return require('internal').db._query('FOR doc IN @@cn RETURN doc', { '@cn' : params.cn }).toArray(); }",
        params: { cn: cn1 }
      }));
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief test allowImplicit
    ////////////////////////////////////////////////////////////////////////////////

    testSingleReadWrite: function () {
      assertEqual([], db._executeTransaction({
        collections: { allowImplicit: false, write: cn1, read: cn1 },
        action: "function (params) { " +
          "return require('internal').db._query('FOR doc IN @@cn RETURN doc', { '@cn' : params.cn }).toArray(); }",
        params: { cn: cn1 }
      }));
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief test allowImplicit
    ////////////////////////////////////////////////////////////////////////////////

    testMultiRead: function () {
      try {
        db._executeTransaction({
          collections: { allowImplicit: false, read: cn1 },
          action: "function (params) { " +
            "return require('internal').db._query('FOR doc IN @@cn RETURN doc', { '@cn' : params.cn }).toArray(); }",
          params: { cn: cn2 }
        });
        fail();
      }
      catch (err) {
        assertEqual(ERRORS.ERROR_TRANSACTION_UNREGISTERED_COLLECTION.code, err.errorNum);
      }
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief uses implicitly declared collections in AQL
    ////////////////////////////////////////////////////////////////////////////////

    testUseInAqlTraversal: function () {
      var result = db._executeTransaction({
        collections: { allowImplicit: false, read: cn2 },
        action: "function (params) { " +
          "return require('internal').db._query('FOR i IN ANY @start @@cn RETURN i', { '@cn' : params.cn, start: params.cn + '/1' }).toArray(); }",
        params: { cn: cn2 }
      });
      assertEqual([], result);
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief uses implicitly declared collections in AQL
    ////////////////////////////////////////////////////////////////////////////////

    testUseInAqlTraversalTwoCollections: function () {
      var result = db._executeTransaction({
        collections: { allowImplicit: false, read: [cn1, cn2] },
        action: "function (params) { " +
          "return require('internal').db._query('FOR i IN ANY @start @@cn RETURN i', { '@cn' : params.cn, start: params.cn + '/1' }).toArray(); }",
        params: { cn: cn2 }
      });
      assertEqual([], result);
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief uses implicitly declared collections in AQL
    ////////////////////////////////////////////////////////////////////////////////

    testUseInAqlTraversalUndeclared: function () {
      try {
        db._executeTransaction({
          collections: { allowImplicit: false, read: cn1 },
          action: "function (params) { " +
            "return require('internal').db._query('FOR i IN ANY @start @@cn RETURN i', { '@cn' : params.cn, start: params.cn + '/1' }).toArray(); }",
          params: { cn: cn2 }
        });
      }
      catch (err) {
        assertEqual(ERRORS.ERROR_TRANSACTION_UNREGISTERED_COLLECTION.code, err.errorNum);
      }
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief uses implicitly declared collections in AQL
    ////////////////////////////////////////////////////////////////////////////////

    testUseInAqlTraversalUndeclared2: function () {
      try {
        db._executeTransaction({
          collections: { allowImplicit: false, read: cn2 },
          action: "function (params) { " +
            "return require('internal').db._query('FOR i IN ANY @start @@cn RETURN i', { '@cn' : params.cn2, start: params.cn1 + '/1' }).toArray(); }",
          params: { cn1: cn1, cn2: cn2 }
        });
      }
      catch (err) {
        assertEqual(ERRORS.ERROR_TRANSACTION_UNREGISTERED_COLLECTION.code, err.errorNum);
      }
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief uses implicitly declared collections in AQL
    ////////////////////////////////////////////////////////////////////////////////

    testUseInAqlTraversalUndeclared3: function () {
      try {
        db._executeTransaction({
          collections: { allowImplicit: false, read: cn1 },
          action: "function (params) { " +
            "return require('internal').db._query('FOR i IN ANY @start @@cn RETURN i', { '@cn' : params.cn2, start: params.cn1 + '/1' }).toArray(); }",
          params: { cn1: cn1, cn2: cn2 }
        });
      }
      catch (err) {
        assertEqual(ERRORS.ERROR_TRANSACTION_UNREGISTERED_COLLECTION.code, err.errorNum);
      }
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief uses implicitly declared collections in AQL
    ////////////////////////////////////////////////////////////////////////////////

    testUseInAqlDocument: function () {
      try {
        db._executeTransaction({
          collections: { allowImplicit: false, read: cn1 },
          action: "function (params) { " +
            "return require('internal').db._query('RETURN DOCUMENT(@v)', { v: params.cn + '/1' }).toArray(); }",
          params: { cn: cn2 }
        });
      }
      catch (err) {
        assertEqual(ERRORS.ERROR_TRANSACTION_UNREGISTERED_COLLECTION.code, err.errorNum);
      }
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief uses implicitly declared collections in AQL
    ////////////////////////////////////////////////////////////////////////////////

    testUseInAql: function () {
      var result = db._executeTransaction({
        collections: { allowImplicit: false, read: cn1 },
        action: "function (params) { " +
          "return require('internal').db._query('FOR i IN @@cn1 RETURN i', { '@cn1' : params.cn1 }).toArray(); }",
        params: { cn1: cn1 }
      });
      assertEqual([], result);
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief uses implicitly declared collections in AQL
    ////////////////////////////////////////////////////////////////////////////////

    testUseInAqlUndeclared: function () {
      try {
        db._executeTransaction({
          collections: { allowImplicit: false },
          action: "function (params) { " +
            "return require('internal').db._query('FOR i IN @@cn1 RETURN i', { '@cn1' : params.cn1 }).toArray(); }",
          params: { cn1: cn1 }
        });
        fail();
      }
      catch (err) {
        assertEqual(ERRORS.ERROR_TRANSACTION_UNREGISTERED_COLLECTION.code, err.errorNum);
      }
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief uses implicitly declared collections in AQL
    ////////////////////////////////////////////////////////////////////////////////

    testUseImplicitAql: function () {
      var result = db._executeTransaction({
        collections: { allowImplicit: true, read: cn1 },
        action: "function (params) { " +
          "return require('internal').db._query('FOR i IN @@cn1 RETURN i', { '@cn1' : params.cn1 }).toArray(); }",
        params: { cn1: cn1 }
      });
      assertEqual([], result);
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief uses implicitly declared collections in AQL
    ////////////////////////////////////////////////////////////////////////////////

    testUseNoImplicitAql: function () {
      try {
        db._executeTransaction({
          collections: { allowImplicit: false, read: cn2 },
          action: "function (params) { " +
            "return require('internal').db._query('FOR i IN @@cn1 RETURN i', { '@cn1' : params.cn1 }).toArray(); }",
          params: { cn1: cn1 }
        });
        fail();
      }
      catch (err) {
        assertEqual(ERRORS.ERROR_TRANSACTION_UNREGISTERED_COLLECTION.code, err.errorNum);
      }
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief uses an explicitly declared collection for reading
    ////////////////////////////////////////////////////////////////////////////////

    testUseForRead: function () {
      var result = db._executeTransaction({
        collections: { read: cn1 },
        action: "function (params) { var db = require('internal').db; return db._collection(params.cn1).toArray(); }",
        params: { cn1: cn1 }
      });

      assertEqual([], result);
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief uses an explicitly declared collection for writing
    ////////////////////////////////////////////////////////////////////////////////

    testUseForWriteAllowImplicit: function () {
      db._executeTransaction({
        collections: { write: cn1, allowImplicit: true },
        action: "function (params) { var db = require('internal').db; db._collection(params.cn1).truncate({ compact: false }); }",
        params: { cn1: cn1 }
      });
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief uses an explicitly declared collection for writing
    ////////////////////////////////////////////////////////////////////////////////

    testUseForWriteNoAllowImplicit: function () {
      db._executeTransaction({
        collections: { write: cn1, allowImplicit: false },
        action: "function (params) { var db = require('internal').db; db._collection(params.cn1).truncate({ compact: false }); }",
        params: { cn1: cn1 }
      });
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief uses an implicitly declared collection for reading
    ////////////////////////////////////////////////////////////////////////////////

    testUseOtherForRead: function () {
      var result = db._executeTransaction({
        collections: { read: cn1 },
        action: "function (params) { var db = require('internal').db; return db._collection(params.cn2).toArray(); }",
        params: { cn2: cn2 }
      });

      assertEqual([], result);
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief uses an implicitly declared collection for reading
    ////////////////////////////////////////////////////////////////////////////////

    testUseOtherForReadAllowImplicit: function () {
      var result = db._executeTransaction({
        collections: { read: cn1, allowImplicit: true },
        action: "function (params) { var db = require('internal').db; return db._collection(params.cn2).toArray(); }",
        params: { cn2: cn2 }
      });

      assertEqual([], result);
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief uses an implicitly declared collection for reading
    ////////////////////////////////////////////////////////////////////////////////

    testUseOtherForReadNoAllowImplicit: function () {
      try {
        db._executeTransaction({
          collections: { read: cn1, allowImplicit: false },
          action: "function (params) { var db = require('internal').db; return db._collection(params.cn2).toArray(); }",
          params: { cn2: cn2 }
        });
        fail();
      }
      catch (err) {
        assertEqual(ERRORS.ERROR_TRANSACTION_UNREGISTERED_COLLECTION.code, err.errorNum);
      }
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief uses an implicitly declared collection for writing
    ////////////////////////////////////////////////////////////////////////////////

    testUseOtherForWriteNoAllowImplicit: function () {
      try {
        db._executeTransaction({
          collections: { read: cn1, allowImplicit: false },
          action: "function (params) { var db = require('internal').db; db._collection(params.cn2).truncate({ compact: false }); }",
          params: { cn2: cn2 }
        });
        fail();
      }
      catch (err) {
        assertEqual(ERRORS.ERROR_TRANSACTION_UNREGISTERED_COLLECTION.code, err.errorNum);
      }
    },


    ////////////////////////////////////////////////////////////////////////////////
    /// @brief uses an implicitly declared collection for writing
    ////////////////////////////////////////////////////////////////////////////////

    testUseForWriting: function () {
      try {
        db._executeTransaction({
          collections: {},
          action: "function (params) { var db = require('internal').db; db._collection(params.cn1).truncate({ compact: false }); }",
          params: { cn1: cn1 }
        });
        fail();
      }
      catch (err) {
        assertEqual(ERRORS.ERROR_TRANSACTION_UNREGISTERED_COLLECTION.code, err.errorNum);
      }
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief uses an implicitly declared collection for writing
    ////////////////////////////////////////////////////////////////////////////////

    testUseReadForWriting: function () {
      try {
        db._executeTransaction({
          collections: { read: cn1 },
          action: "function (params) { var db = require('internal').db; db._collection(params.cn1).truncate({ compact: false }); }",
          params: { cn1: cn1 }
        });
        fail();
      }
      catch (err) {
        assertEqual(ERRORS.ERROR_TRANSACTION_UNREGISTERED_COLLECTION.code, err.errorNum);
      }
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief uses an implicitly declared collection for writing
    ////////////////////////////////////////////////////////////////////////////////

    testUseOtherForWriting: function () {
      try {
        db._executeTransaction({
          collections: { write: cn2 },
          action: "function (params) { var db = require('internal').db; db._collection(params.cn1).truncate({ compact: false }); }",
          params: { cn1: cn1 }
        });
        fail();
      }
      catch (err) {
        assertEqual(ERRORS.ERROR_TRANSACTION_UNREGISTERED_COLLECTION.code, err.errorNum);
      }
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief uses an implicitly declared collection for writing
    ////////////////////////////////////////////////////////////////////////////////

    testUseOtherForWriteAllowImplicit: function () {
      try {
        db._executeTransaction({
          collections: { read: cn1, allowImplicit: true },
          action: "function (params) { var db = require('internal').db; db._collection(params.cn2).truncate({ compact: false }); }",
          params: { cn2: cn2 }
        });
        fail();
      }
      catch (err) {
        assertEqual(ERRORS.ERROR_TRANSACTION_UNREGISTERED_COLLECTION.code, err.errorNum);
      }
    }
  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////

function TransactionsInvocationsParametersSuite () {
  'use strict';

  var c = null;
  var cn = "UnitTestsTransaction";

  return {

////////////////////////////////////////////////////////////////////////////////
/// @brief set up
////////////////////////////////////////////////////////////////////////////////

    setUp : function () {
      db._drop(cn);
      c = db._create(cn);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief tear down
////////////////////////////////////////////////////////////////////////////////

    tearDown : function () {
      db._drop(cn);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief transactions
////////////////////////////////////////////////////////////////////////////////
    
    testSmallMaxTransactionSize : function () {
      try {
        db._query("FOR i IN 1..10000 INSERT { someValue: i} INTO @@cn", 
        {"@cn": cn}, {maxTransactionSize: 100 * 1000}); // 100 KB => not enough!
        fail();
      } catch (err) {
        assertEqual(ERRORS.ERROR_RESOURCE_LIMIT.code, err.errorNum);
      }
      
      assertEqual(0, db._collection(cn).count());
      assertEqual(0, db._collection(cn).toArray().length);
    },

    testBigMaxTransactionSize : function () {
      db._query("FOR i IN 1..10000 INSERT { someValue: i} INTO @@cn", 
        {"@cn": cn}, {maxTransactionSize: 10 * 1000 * 1000}); // 10 MB => enough!
      assertEqual(10000, db._collection(cn).count());
      assertEqual(10000, db._collection(cn).toArray().length);
    },
    
    testIntermediateCommitCountVerySmall : function () {
      db._query("FOR i IN 1..1000 INSERT { someValue: i} INTO @@cn", {"@cn": cn}, 
      { intermediateCommitCount: 1 });
      // this should produce 1000 intermediate commits
      assertEqual(1000, db._collection(cn).count());
      assertEqual(1000, db._collection(cn).toArray().length);
    },

    testIntermediateCommitCountBigger : function () {
      db._query("FOR i IN 1..10000 INSERT { someValue: i} INTO @@cn", {"@cn": cn}, 
      { intermediateCommitCount: 1000 });
      // this should produce 10 intermediate commits
      assertEqual(10000, db._collection(cn).count());
      assertEqual(10000, db._collection(cn).toArray().length);
    },
    
    testIntermediateCommitCountWithFail : function () {
      var failed = false;

      try {
        db._query("FOR i IN 1..10001 FILTER i < 10001 OR FAIL('peng') INSERT { someValue: i} INTO @@cn ", {"@cn": cn}, 
        { intermediateCommitCount: 1000 });
        fail();
        // this should produce 10 intermediate commits
      } catch (err) {
        assertEqual(ERRORS.ERROR_QUERY_FAIL_CALLED.code, err.errorNum);
        failed = true;
      }

      assertTrue(failed);
      assertEqual(10000, db._collection(cn).count());
      assertEqual(10000, db._collection(cn).toArray().length);
    },

    testIntermediateCommitCountWithFailInTheMiddle : function () {
      var failed = false;

      try {
        db._query("FOR i IN 1..10000 FILTER i != 6532 OR FAIL('peng') INSERT { someValue: i} INTO @@cn ", {"@cn": cn}, 
        { intermediateCommitCount: 1000 });
        // this should produce 6 intermediate commits
        fail();
      } catch (err) {
        failed = true;
        assertEqual(ERRORS.ERROR_QUERY_FAIL_CALLED.code, err.errorNum);
      }

      assertTrue(failed);
      assertEqual(6000, db._collection(cn).count());
      assertEqual(6000, db._collection(cn).toArray().length);
    },


    testIntermediateCommitSizeVerySmall : function () {
      db._query("FOR i IN 1..1000 INSERT { someValue: i} INTO @@cn", {"@cn": cn}, 
      { intermediateCommitSize: 10 });
      // this should produce a lot of intermediate commits
      assertEqual(1000, db._collection(cn).count());
      assertEqual(1000, db._collection(cn).toArray().length);
    },
    
    testIntermediateCommitSizeBigger : function () {
      db._query("FOR i IN 1..10000 INSERT { someValue: i} INTO @@cn", {"@cn": cn}, 
      { intermediateCommitSize: 1000 });
      // this should produce a lot of intermediate commits
      assertEqual(10000, db._collection(cn).count());
      assertEqual(10000, db._collection(cn).toArray().length);
    },
    
    testIntermediateCommitSizeWithFail : function () {
      var failed = false;

      try {
        db._query("FOR i IN 1..10001 FILTER i < 10001 OR FAIL('peng') INSERT { someValue: i} INTO @@cn", {"@cn": cn}, 
        { intermediateCommitSize: 10 });
        // this should produce a lot of intermediate commits
        fail();
      } catch (err) {
        failed = true;
        assertEqual(ERRORS.ERROR_QUERY_FAIL_CALLED.code, err.errorNum);
      }

      assertTrue(failed);
      assertEqual(10000, db._collection(cn).count());
      assertEqual(10000, db._collection(cn).toArray().length);
    },
    
    testIntermediateCommitDuplicateKeys1 : function () {
      var failed = false;

      try { // should fail because intermediate commits are not allowed
        db._executeTransaction({
          collections: { write: cn },
          action: "function (params) { " +
            "var db = require('internal').db; " +
            "var c = db._collection(params.cn); " +
            "for (var i = 0; i < 10; ++i) { c.insert({ _key: 'test' + i }); } " +
            "for (var i = 0; i < 10; ++i) { c.insert({ _key: 'test' + i }); } " +
            "}",
          params: { cn },
          intermediateCommitCount: 10 
        });
        fail();
      } catch (err) {
        failed = true;
        assertEqual(ERRORS.ERROR_ARANGO_UNIQUE_CONSTRAINT_VIOLATED.code, err.errorNum);
      }

      assertTrue(failed);
      assertEqual(0, db._collection(cn).count());
      assertEqual(0, db._collection(cn).toArray().length);
    },

        
    testIntermediateCommitDuplicateKeys2 : function () {
      var failed = false;

      try { // should fail because intermediate commits are not allowed
        db._query("FOR i IN ['a', 'b', 'a', 'b'] INSERT { _key: i} INTO @@cn", {"@cn": cn}, {intermediateCommitCount: 2 });
        fail();
      } catch (err) {
        failed = true;
        assertEqual(ERRORS.ERROR_ARANGO_UNIQUE_CONSTRAINT_VIOLATED.code, err.errorNum);
      }

      assertTrue(failed);
      assertEqual(2, db._collection(cn).count());
      assertEqual(2, db._collection(cn).toArray().length);
    },
    
    testIntermediateCommitDuplicateKeys3 : function () {
      var failed = false;

      try {
        db._query("FOR i IN ['a', 'b', 'a', 'b'] INSERT { _key: i} INTO @@cn", {"@cn": cn}, {intermediateCommitCount: 10 });
        fail();
      } catch (err) {
        failed = true;
        assertEqual(ERRORS.ERROR_ARANGO_UNIQUE_CONSTRAINT_VIOLATED.code, err.errorNum);
      }

      assertTrue(failed);
      assertEqual(0, db._collection(cn).count());
      assertEqual(0, db._collection(cn).toArray().length);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief aql queries
////////////////////////////////////////////////////////////////////////////////
    
    testAqlSmallMaxTransactionSize : function () {
      try {
        db._query({ query: "FOR i IN 1..10000 INSERT {} INTO " + cn, options: { maxTransactionSize: 1000 } });
        fail();
      } catch (err) {
        assertEqual(ERRORS.ERROR_RESOURCE_LIMIT.code, err.errorNum);
      }
      
      assertEqual(0, db._collection(cn).count());
      assertEqual(0, db._collection(cn).toArray().length);
    },

    testAqlBigMaxTransactionSize : function () {
      db._query({ query: "FOR i IN 1..10000 INSERT {} INTO " + cn, options: { maxTransactionSize: 10 * 1000 * 1000 } });
      assertEqual(10000, db._collection(cn).count());
      assertEqual(10000, db._collection(cn).toArray().length);
    },
    
    testAqlIntermediateCommitCountVerySmall : function () {
      db._query({ query: "FOR i IN 1..10000 INSERT {} INTO " + cn, options: { intermediateCommitCount: 1 } });
      assertEqual(10000, db._collection(cn).count());
      assertEqual(10000, db._collection(cn).toArray().length);
    },

    testAqlIntermediateCommitCountBigger : function () {
      db._query({ query: "FOR i IN 1..10000 INSERT {} INTO " + cn, options: { intermediateCommitCount: 1000 } });
      assertEqual(10000, db._collection(cn).count());
      assertEqual(10000, db._collection(cn).toArray().length);
    },
    
    testAqlIntermediateCommitCountWithFail : function () {
      var failed = false;

      try {
        db._query({ query: "FOR i IN 1..10000 LET x = NOOPT(i == 8533 ? FAIL('peng!') : i) INSERT { value: x } INTO " + cn, options: { intermediateCommitCount: 1000 } });
        fail();
      } catch (err) {
        failed = true;
        assertEqual(ERRORS.ERROR_QUERY_FAIL_CALLED.code, err.errorNum);
      }

      assertTrue(failed);
      assertEqual(8000, db._collection(cn).count());
      assertEqual(8000, db._collection(cn).toArray().length);
    },

    testAqlIntermediateCommitSizeVerySmall : function () {
      db._query({ query: "FOR i IN 1..10000 INSERT { someValue: i } INTO " + cn, options: { intermediateCommitSize: 1000 } });
      assertEqual(10000, db._collection(cn).count());
      assertEqual(10000, db._collection(cn).toArray().length);
    },
    
    testAqlIntermediateCommitSizeBigger : function () {
      db._query({ query: "FOR i IN 1..10000 INSERT { someValue: i } INTO " + cn, options: { intermediateCommitSize: 10000 } });
      assertEqual(10000, db._collection(cn).count());
      assertEqual(10000, db._collection(cn).toArray().length);
    },
    
    testAqlIntermediateCommitSizeWithFailInTheMiddle : function () {
      var failed = false;

      try {
        db._query({ query: "FOR i IN 1..10000 LET x = NOOPT(i == 8533 ? FAIL('peng!') : i) INSERT { value: x } INTO " + cn, options: { intermediateCommitSize: 10 } });
        fail();
      } catch (err) {
        failed = true;
        assertEqual(ERRORS.ERROR_QUERY_FAIL_CALLED.code, err.errorNum);
      }

      assertTrue(failed);
      // not 8533 as expected, because the CalculationBlock will
      // execute 1000 expressions at a time, and when done, the
      // INSERTs will be applied as a whole. However, the CalcBlock
      // will now fail somewhere in a batch of 1000, and no inserts
      // will be done for this batch
      assertEqual(8000, db._collection(cn).count());
      assertEqual(8000, db._collection(cn).toArray().length);
    }

  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suite
////////////////////////////////////////////////////////////////////////////////

jsunity.run(TransactionsInvocationsSuite);
jsunity.run(TransactionsImplicitCollectionsSuite);
jsunity.run(TransactionsInvocationsParametersSuite);

return jsunity.done();

