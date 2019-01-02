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

var jsunity = require("jsunity");
var arangodb = require("@arangodb");
var internal = require("internal");
var ERRORS = arangodb.errors;
var ArangoError = require("@arangodb").ArangoError;
var cluster;
var isOnServer = (typeof ArangoClusterComm === "object");
if (isOnServer) {
  cluster = require("@arangodb/cluster");
} else {
  cluster = {};
}
var db = arangodb.db;

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////

function TransactionsInvocationsSuite () {
  'use strict';

  return {

////////////////////////////////////////////////////////////////////////////////
/// @brief set up
////////////////////////////////////////////////////////////////////////////////

    setUp : function () {
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief tear down
////////////////////////////////////////////////////////////////////////////////

    tearDown : function () {
      internal.wait(0);
    },

    testErrorHandling : function () {
      try {
        db._executeTransaction({
          collections: {},
          action: function() {
            var err = new Error('test');
            err.errorNum = 1234;
            Object.defineProperty(err, 'name', {
                get: function() { throw new Error('Error in getter'); }
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
    
    testErrorHandlingArangoError : function () {
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

    testInvokeActionString : function () {
      var result = db._executeTransaction({
        collections: { },
        action: "function () { return 23; }"
      });

      assertEqual(23, result);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief execute a transaction with a string function action
////////////////////////////////////////////////////////////////////////////////

    testInvokeActionStringFunction : function () {
      var result = db._executeTransaction({
        collections: { },
        action: "function () { return function () { return 11; }(); }"
      });

      assertEqual(11, result);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief execute a transaction with a function action
////////////////////////////////////////////////////////////////////////////////

    testInvokeActionFunction : function () {
      var result = db._executeTransaction({
        collections: { },
        action: function () { return 42; }
      });

      assertEqual(42, result);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief execute a transaction with an invalid action
////////////////////////////////////////////////////////////////////////////////

    testInvokeActionInvalid1 : function () {
      try {
        db._executeTransaction({
          collections: { },
          action: null,
        });
        fail();
      }
      catch (err) {
        assertEqual(ERRORS.ERROR_BAD_PARAMETER.code, err.errorNum);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief execute a transaction without an action
////////////////////////////////////////////////////////////////////////////////

    testInvokeNoAction : function () {
      try {
        db._executeTransaction({
          collections: { }
        });
        fail();
      }
      catch (err) {
        assertEqual(ERRORS.ERROR_BAD_PARAMETER.code, err.errorNum);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief execute a transaction with a non-working action declaration
////////////////////////////////////////////////////////////////////////////////

    testInvokeActionBroken1 : function () {
      try {
        db._executeTransaction({
          collections: { },
          action: "return 11;"
        });
        fail();
      }
      catch (err) {
        assertEqual(ERRORS.ERROR_BAD_PARAMETER.code, err.errorNum);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief execute a transaction with a non-working action declaration
////////////////////////////////////////////////////////////////////////////////

    testInvokeActionBroken2 : function () {
      try {
        db._executeTransaction({
          collections: { },
          action: "function () { "
        });
        fail();
      }
      catch (err) {
        assertEqual(ERRORS.ERROR_BAD_PARAMETER.code, err.errorNum);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief execute a transaction with an invalid action
////////////////////////////////////////////////////////////////////////////////

    testInvokeActionInvalid2 : function () {
      try {
        db._executeTransaction({
          collections: { },
          action: null,
        });
        fail();
      }
      catch (err) {
        assertEqual(ERRORS.ERROR_BAD_PARAMETER.code, err.errorNum);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief parameters
////////////////////////////////////////////////////////////////////////////////

    testParametersString : function () {
      var result = db._executeTransaction({
        collections: { },
        action: "function (params) { return [ params[1], params[4] ]; }",
        params: [ 1, 2, 3, 4, 5 ]
      });

      assertEqual([ 2, 5 ], result);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief parameters
////////////////////////////////////////////////////////////////////////////////

    testParametersFunction : function () {
      var result = db._executeTransaction({
        collections: { },
        action: function (params) {
          return [ params[1], params[4] ];
        },
        params: [ 1, 2, 3, 4, 5 ]
      });

      assertEqual([ 2, 5 ], result);
    }

  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////

function TransactionsImplicitCollectionsSuite () {
  'use strict';

  var cn1 = "UnitTestsTransaction1";
  var cn2 = "UnitTestsTransaction2";
  var c1 = null;
  var c2 = null;

  return {

////////////////////////////////////////////////////////////////////////////////
/// @brief set up
////////////////////////////////////////////////////////////////////////////////

    setUp : function () {
      db._drop(cn1);
      db._drop(cn2);
      c1 = db._create(cn1);
      c2 = db._createEdgeCollection(cn2);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief tear down
////////////////////////////////////////////////////////////////////////////////

    tearDown : function () {
      c1 = null;
      c2 = null;
      db._drop(cn1);
      db._drop(cn2);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test allowImplicit
////////////////////////////////////////////////////////////////////////////////

    testSingleReadOnly : function () {
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

    testSingleReadCollectionObject : function () {
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

    testSingleReadCollectionArray : function () {
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

    testSingleWriteOnly : function () {
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

    testSingleWriteCollectionObject : function () {
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

    testSingleWriteCollectionArray : function () {
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

    testSingleReadWrite : function () {
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

    testMultiRead : function () {
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

    testUseInAqlTraversal : function () {
      var result = db._executeTransaction({
        collections: { allowImplicit: false, read: cn2 },
        action: "function (params) { " +
          "return require('internal').db._query('FOR i IN ANY @start @@cn RETURN i', { '@cn' : params.cn, start: params.cn + '/1' }).toArray(); }",
        params: { cn: cn2 }
      });
      assertEqual([ ], result);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief uses implicitly declared collections in AQL
////////////////////////////////////////////////////////////////////////////////

    testUseInAqlTraversalTwoCollections : function () {
      var result = db._executeTransaction({
        collections: { allowImplicit: false, read: [ cn1, cn2 ] },
        action: "function (params) { " +
          "return require('internal').db._query('FOR i IN ANY @start @@cn RETURN i', { '@cn' : params.cn, start: params.cn + '/1' }).toArray(); }",
        params: { cn: cn2 }
      });
      assertEqual([ ], result);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief uses implicitly declared collections in AQL
////////////////////////////////////////////////////////////////////////////////

    testUseInAqlTraversalUndeclared : function () {
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

    testUseInAqlTraversalUndeclared2 : function () {
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

    testUseInAqlTraversalUndeclared3 : function () {
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

    testUseInAqlDocument : function () {
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

    testUseInAql : function () {
      var result = db._executeTransaction({
        collections: { allowImplicit: false, read: cn1 },
        action: "function (params) { " +
          "return require('internal').db._query('FOR i IN @@cn1 RETURN i', { '@cn1' : params.cn1 }).toArray(); }",
        params: { cn1: cn1 }
      });
      assertEqual([ ], result);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief uses implicitly declared collections in AQL
////////////////////////////////////////////////////////////////////////////////

    testUseInAqlUndeclared : function () {
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

    testUseImplicitAql : function () {
      var result = db._executeTransaction({
        collections: { allowImplicit: true, read: cn1 },
        action: "function (params) { " + 
          "return require('internal').db._query('FOR i IN @@cn1 RETURN i', { '@cn1' : params.cn1 }).toArray(); }",
        params: { cn1: cn1 }
      });
      assertEqual([ ], result);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief uses implicitly declared collections in AQL
////////////////////////////////////////////////////////////////////////////////

    testUseNoImplicitAql : function () {
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
    /// @brief perform an infinite loop
    ////////////////////////////////////////////////////////////////////////////////

    testUseQueryStreamCursorInAql : function () {
      let docs = [];
      for(let i = 0; i < 100000; i++) {
        docs.push({value: i});
        if (i % 5000 === 0) {
          c1.save(docs);
          docs = [];
        }
      }
      c1.save(docs);
      
      var result = db._executeTransaction({
        collections: { allowImplicit: false, read: cn1 },
        action: `function (params) {
          const db = require('internal').db;
          let cc = db._query('FOR i IN @@cn1 RETURN i', { '@cn1' : params.cn1 }, {stream: true});
          let xx = 0;
          while (cc.hasNext()) {cc.next(); xx++;}
          let cc2 = db._query('FOR i IN 1..1000000000 RETURN i', {}, {stream: true});
          return xx; }`,
        params: { cn1: cn1 }
      });
      assertEqual(100000, result);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief uses an explicitly declared collection for reading
////////////////////////////////////////////////////////////////////////////////

    testUseForRead : function () {
      var result = db._executeTransaction({
        collections: { read: cn1 },
        action: "function (params) { var db = require('internal').db; return db._collection(params.cn1).toArray(); }",
        params: { cn1: cn1 }
      });

      assertEqual([ ], result);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief uses an explicitly declared collection for writing
////////////////////////////////////////////////////////////////////////////////

    testUseForWriteAllowImplicit : function () {
      db._executeTransaction({
        collections: { write: cn1, allowImplicit: true },
        action: "function (params) { var db = require('internal').db; db._collection(params.cn1).truncate(); }",
        params: { cn1: cn1 }
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief uses an explicitly declared collection for writing
////////////////////////////////////////////////////////////////////////////////

    testUseForWriteNoAllowImplicit : function () {
      db._executeTransaction({
        collections: { write: cn1, allowImplicit: false },
        action: "function (params) { var db = require('internal').db; db._collection(params.cn1).truncate(); }",
        params: { cn1: cn1 }
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief uses an implicitly declared collection for reading
////////////////////////////////////////////////////////////////////////////////

    testUseOtherForRead : function () {
      var result = db._executeTransaction({
        collections: { read: cn1 },
        action: "function (params) { var db = require('internal').db; return db._collection(params.cn2).toArray(); }",
        params: { cn2: cn2 }
      });
      
      assertEqual([ ], result);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief uses an implicitly declared collection for reading
////////////////////////////////////////////////////////////////////////////////

    testUseOtherForReadAllowImplicit : function () {
      var result = db._executeTransaction({
        collections: { read: cn1, allowImplicit : true },
        action: "function (params) { var db = require('internal').db; return db._collection(params.cn2).toArray(); }",
        params: { cn2: cn2 }
      });
      
      assertEqual([ ], result);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief uses an implicitly declared collection for reading
////////////////////////////////////////////////////////////////////////////////

    testUseOtherForReadNoAllowImplicit : function () {
      try {
        db._executeTransaction({
          collections: { read: cn1, allowImplicit : false },
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

    testUseOtherForWriteNoAllowImplicit : function () {
      if ((cluster && cluster.isCluster && cluster.isCluster()) || !cluster.isCluster) {
        return;
      }

      try {
        db._executeTransaction({
          collections: { read: cn1, allowImplicit : false },
          action: "function (params) { var db = require('internal').db; db._collection(params.cn2).truncate(); }",
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
/// @brief executes the test suite
////////////////////////////////////////////////////////////////////////////////

jsunity.run(TransactionsInvocationsSuite);
jsunity.run(TransactionsImplicitCollectionsSuite);

return jsunity.done();

