////////////////////////////////////////////////////////////////////////////////
/// @brief tests for transactions
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
/// @author Copyright 2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

var internal = require("internal");
var arangodb = require("org/arangodb");
var db = arangodb.db;
var jsunity = require("jsunity");

// -----------------------------------------------------------------------------
// --SECTION--                                                        test suite
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////

function transactionInvocationSuite () {

  return {

////////////////////////////////////////////////////////////////////////////////
/// @brief set up
////////////////////////////////////////////////////////////////////////////////

    setUp : function () {
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: invalid invocations of TRANSACTION() function
////////////////////////////////////////////////////////////////////////////////

    testInvalidInvocations : function () {
      var tests = [
        undefined,
        null,
        true,
        false,
        0,
        1,
        "foo",
        { }, { },
        { }, { }, { },
        false, true,
        [ ],
        [ "action" ],
        [ "collections" ],
        [ "collections", "action" ],
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
        { collections: { }, lockTimeout: "foo", action: function () { } },
        { collections: { }, lockTimeout: [ ], action: function () { } },
        { collections: { }, lockTimeout: { }, action: function () { } },
        { collections: { }, waitForSync: null, action: function () { } },
        { collections: { }, waitForSync: 0, action: function () { } },
        { collections: { }, waitForSync: "foo", action: function () { } },
        { collections: { }, waitForSync: [ ], action: function () { } },
        { collections: { }, waitForSync: { }, action: function () { } }
      ];

      tests.forEach(function (test) {
        try {
          TRANSACTION(test);
          fail();
        }
        catch (err) {
          assertEqual(internal.errors.ERROR_BAD_PARAMETER.code, err.errorNum);
        }
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: valid invocations of TRANSACTION() function
////////////////////////////////////////////////////////////////////////////////

    testValidEmptyInvocations : function () {
      var result;

      var tests = [
        { collections: { }, action: function () { result = 1; return true; } },
        { collections: { read: [ ] }, action: function () { result = 1; return true; } },
        { collections: { write: [ ] }, action: function () { result = 1; return true; } },
        { collections: { read: [ ], write: [ ] }, action: function () { result = 1; return true; } },
        { collections: { read: [ ], write: [ ] }, lockTimeout: 5.0, action: function () { result = 1; return true; } },
        { collections: { read: [ ], write: [ ] }, lockTimeout: 0.0, action: function () { result = 1; return true; } },
        { collections: { read: [ ], write: [ ] }, waitForSync: true, action: function () { result = 1; return true; } },
        { collections: { read: [ ], write: [ ] }, waitForSync: false, action: function () { result = 1; return true; } }
      ];

      tests.forEach(function (test) {
        result = 0;
          
        TRANSACTION(test);
        assertEqual(1, result);
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: return values
////////////////////////////////////////////////////////////////////////////////

    testReturnValues : function () {
      var result;

      var tests = [
        { expected: 1, trx: { collections: { }, action: function () { return 1; } } },
        { expected: undefined, trx: { collections: { }, action: function () { } } },
        { expected: [ ], trx: { collections: { read: [ ] }, action: function () { return [ ] } } },
        { expected: [ null, true, false ], trx: { collections: { write: [ ] }, action: function () { return [ null, true, false ] } } },
        { expected: "foo", trx: { collections: { read: [ ], write: [ ] }, action: function () { return "foo" } } },
        { expected: { "a" : 1, "b" : 2 }, trx: { collections: { read: [ ], write: [ ] }, action: function () { return { "a" : 1, "b" : 2 } } } }
      ];

      tests.forEach(function (test) {
        assertEqual(test.expected, TRANSACTION(test.trx));
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: action
////////////////////////////////////////////////////////////////////////////////

    testActionFunction : function () {
      var obj = {
        collections : {
        },
        action : function () {
          return 42;
        }
      };

      assertEqual(42, TRANSACTION(obj));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: action
////////////////////////////////////////////////////////////////////////////////

    testActionInvalidString : function () {
      try {
        TRANSACTION({
          collections : {
          },
          action : "return 42;"
        });
        fail();
      }
      catch (err) {
        assertEqual(arangodb.errors.ERROR_BAD_PARAMETER.code, err.errorNum);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: action
////////////////////////////////////////////////////////////////////////////////

    testActionString : function () {
      var obj = {
        collections : {
        },
        action : "function () { return 42; }"
      };

      assertEqual(42, TRANSACTION(obj));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: nesting
////////////////////////////////////////////////////////////////////////////////

    testNesting : function () {
      var obj = {
        collections : {
        },
        action : function () {
          TRANSACTION({
            collections: {
            },
            action: "function () { return 1; }"
          });
        }
      };

      try {
        TRANSACTION(obj);
        fail();
      }
      catch (err) {
        assertEqual(arangodb.errors.ERROR_TRANSACTION_NESTED.code, err.errorNum);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: params
////////////////////////////////////////////////////////////////////////////////

    testParamsFunction : function () {
      var obj = {
        collections : {
        },
        action : function (params) {
          return [ params[1], params[4] ];
        },
        params: [ 1, 2, 3, 4, 5 ]
      };

      assertEqual([ 2, 5 ], TRANSACTION(obj));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: params
////////////////////////////////////////////////////////////////////////////////

    testParamsString : function () {
      var obj = {
        collections : {
        },
        action : "function (params) { return [ params[1], params[4] ]; }",
        params: [ 1, 2, 3, 4, 5 ]
      };

      assertEqual([ 2, 5 ], TRANSACTION(obj));
    }

  };
}

// -----------------------------------------------------------------------------
// --SECTION--                                                        test suite
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////

function transactionCollectionsSuite () {
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
      c1 = db._create(cn1);
      
      db._drop(cn2);
      c2 = db._create(cn2);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief tear down
////////////////////////////////////////////////////////////////////////////////

    tearDown : function () {
      if (c1 !== null) {
        c1.drop();
      }

      c1 = null;
      
      if (c2 !== null) {
        c2.drop();
      }

      c2 = null;
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: trx using non-existing collections
////////////////////////////////////////////////////////////////////////////////

    testNonExistingCollectionsArray : function () {
      var obj = {
        collections : {
          read : [ "UnitTestsTransactionNonExisting" ]
        },
        action : function () {
          return true;
        }
      };

      try {
        TRANSACTION(obj);
        fail();
      }
      catch (err) {
        assertEqual(arangodb.errors.ERROR_ARANGO_COLLECTION_NOT_FOUND.code, err.errorNum);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: trx using non-existing collections
////////////////////////////////////////////////////////////////////////////////

    testNonExistingCollectionsString : function () {
      var obj = {
        collections : {
          read : "UnitTestsTransactionNonExisting"
        },
        action : function () {
          return true;
        }
      };

      try {
        TRANSACTION(obj);
        fail();
      }
      catch (err) {
        assertEqual(arangodb.errors.ERROR_ARANGO_COLLECTION_NOT_FOUND.code, err.errorNum);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: trx using non-declared collections
////////////////////////////////////////////////////////////////////////////////

    testNonDeclaredCollections1 : function () {
      var obj = {
        collections : {
        },
        action : function () {
          c1.save({ _key : "foo" });
          return true;
        }
      };

      try {
        TRANSACTION(obj);
        fail();
      }
      catch (err) {
        assertEqual(arangodb.errors.ERROR_TRANSACTION_UNREGISTERED_COLLECTION.code, err.errorNum);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: trx using non-declared collections
////////////////////////////////////////////////////////////////////////////////

    testNonDeclaredCollections2 : function () {
      var obj = {
        collections : { 
          write : [ cn2 ]
        },
        action : function () {
          c1.save({ _key : "foo" });
          return true;
        }
      };

      try {
        TRANSACTION(obj);
        fail();
      }
      catch (err) {
        assertEqual(arangodb.errors.ERROR_TRANSACTION_UNREGISTERED_COLLECTION.code, err.errorNum);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: trx using wrong mode
////////////////////////////////////////////////////////////////////////////////

    testNonDeclaredCollections3 : function () {
      var obj = {
        collections : { 
          read : [ cn1 ]
        },
        action : function () {
          c1.save({ _key : "foo" });
          return true;
        }
      };

      try {
        TRANSACTION(obj);
        fail();
      }
      catch (err) {
        assertEqual(arangodb.errors.ERROR_TRANSACTION_UNREGISTERED_COLLECTION.code, err.errorNum);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: trx using no collections
////////////////////////////////////////////////////////////////////////////////

    testNoCollections : function () {
      var obj = {
        collections : { 
        },
        action : function () {
          return true;
        }
      };

      assertTrue(TRANSACTION(obj));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: trx using no collections
////////////////////////////////////////////////////////////////////////////////

    testNoCollectionsAql : function () {
      var result;
      
      var obj = {
        collections : { 
        },
        action : function () {
          result = internal.AQL_QUERY("FOR i IN [ 1, 2, 3 ] RETURN i").getRows();
          return true;
        }
      };

      assertTrue(TRANSACTION(obj));
      assertEqual([ 1, 2, 3 ], result);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: trx using valid collections
////////////////////////////////////////////////////////////////////////////////

    testValidCollectionsArray : function () {
      var obj = {
        collections : { 
          write : [ cn1 ]
        },
        action : function () {
          c1.save({ _key : "foo" });
          return true;
        }
      };

      assertTrue(TRANSACTION(obj));
    },
////////////////////////////////////////////////////////////////////////////////
/// @brief test: trx using valid collections
////////////////////////////////////////////////////////////////////////////////

    testValidCollectionsString : function () {
      var obj = {
        collections : { 
          write : cn1 
        },
        action : function () {
          c1.save({ _key : "foo" });
          return true;
        }
      };

      assertTrue(TRANSACTION(obj));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: trx using valid collections
////////////////////////////////////////////////////////////////////////////////

    testValidMultipleCollectionsArray : function () {
      var obj = {
        collections : { 
          write : [ cn1, cn2 ]
        },
        action : function () {
          c1.save({ _key : "foo" });
          c2.save({ _key : "foo" });
          return true;
        }
      };

      assertTrue(TRANSACTION(obj));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: trx using valid collections
////////////////////////////////////////////////////////////////////////////////

    testValidMultipleCollectionsString : function () {
      c2.save({ _key : "foo" });

      var obj = {
        collections : { 
          write : cn1, 
          read: cn2 
        },
        action : function () {
          c1.save({ _key : "foo" });
          c2.document("foo"); 
          return true;
        }
      };

      assertTrue(TRANSACTION(obj));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: trx using valid collections
////////////////////////////////////////////////////////////////////////////////

    testRedeclareCollectionArray : function () {
      var obj = {
        collections : { 
          read : [ cn1 ],
          write : [ cn1 ]
        },
        action : function () {
          c1.save({ _key : "foo" });
          return true;
        }
      };

      assertTrue(TRANSACTION(obj));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: trx using valid collections
////////////////////////////////////////////////////////////////////////////////

    testRedeclareCollectionString : function () {
      var obj = {
        collections : { 
          read : cn1,
          write : cn1 
        },
        action : function () {
          c1.save({ _key : "foo" });
          return true;
        }
      };

      assertTrue(TRANSACTION(obj));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: trx using valid collections
////////////////////////////////////////////////////////////////////////////////

    testReadWriteCollections : function () {
      var obj = {
        collections : { 
          read : [ cn1 ],
          write : [ cn2 ]
        },
        action : function () {
          c2.save({ _key : "foo" });
          return true;
        }
      };

      assertTrue(TRANSACTION(obj));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: trx using a reserved name
////////////////////////////////////////////////////////////////////////////////

    testTrxCollection : function () {
      var obj = {
        collections : {
          write: "_trx"
        },
        action : function () {
          return true;
        }
      };

      try {
        TRANSACTION(obj);
        fail();
      }
      catch (err) {
        assertEqual(arangodb.errors.ERROR_TRANSACTION_DISALLOWED_OPERATION.code, err.errorNum);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: trx using waitForSync
////////////////////////////////////////////////////////////////////////////////

    testWaitForSyncTrue : function () {
      var obj = {
        collections : { 
          write : [ cn1 ]
        },
        waitForSync: true,
        action : function () {
          c1.save({ _key : "foo" });
          return true;
        }
      };

      assertTrue(TRANSACTION(obj));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: trx using waitForSync
////////////////////////////////////////////////////////////////////////////////

    testWaitForSyncFalse : function () {
      var obj = {
        collections : { 
          write : [ cn1 ]
        },
        waitForSync: false,
        action : function () {
          c1.save({ _key : "foo" });
          return true;
        }
      };

      assertTrue(TRANSACTION(obj));
    }

  };
}

// -----------------------------------------------------------------------------
// --SECTION--                                                        test suite
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////

function transactionOperationsSuite () {
  var cn1 = "UnitTestsTransaction1";
  var cn2 = "UnitTestsTransaction2";

  var c1 = null;
  var c2 = null;
  
  var sortedKeys = function (col) {
    var keys = [ ];

    col.toArray().forEach(function (d) { 
      keys.push(d._key);
    });

    keys.sort();
    return keys;
  };

  return {

////////////////////////////////////////////////////////////////////////////////
/// @brief set up
////////////////////////////////////////////////////////////////////////////////

    setUp : function () {
      db._drop(cn1);
      db._drop(cn2);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief tear down
////////////////////////////////////////////////////////////////////////////////

    tearDown : function () {
      if (c1 !== null) {
        c1.drop();
      }

      c1 = null;
      
      if (c2 !== null) {
        c2.drop();
      }

      c2 = null;
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: trx with create operation
////////////////////////////////////////////////////////////////////////////////

    testCreate : function () {
      var obj = {
        collections : {
        },
        action : function () {
          db._create(cn1);
          return true;
        }
      };

      try {
        TRANSACTION(obj);
        fail();
      }
      catch (err) {
        assertEqual(arangodb.errors.ERROR_TRANSACTION_DISALLOWED_OPERATION.code, err.errorNum);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: trx with drop operation
////////////////////////////////////////////////////////////////////////////////

    testDrop : function () {
      c1 = db._create(cn1);

      var obj = {
        collections : {
        },
        action : function () {
          c1.drop();
          return true;
        }
      };

      try {
        TRANSACTION(obj);
        fail();
      }
      catch (err) {
        assertEqual(arangodb.errors.ERROR_TRANSACTION_DISALLOWED_OPERATION.code, err.errorNum);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: trx with rename operation
////////////////////////////////////////////////////////////////////////////////

    testRename : function () {
      c1 = db._create(cn1);

      var obj = {
        collections : {
        },
        action : function () {
          c1.rename(cn2);
          return true;
        }
      };

      try {
        TRANSACTION(obj);
        fail();
      }
      catch (err) {
        assertEqual(arangodb.errors.ERROR_TRANSACTION_DISALLOWED_OPERATION.code, err.errorNum);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: trx with create index operation
////////////////////////////////////////////////////////////////////////////////

    testCreatePqIndex : function () {
      c1 = db._create(cn1);

      var obj = {
        collections : {
        },
        action : function () {
          c1.ensurePQIndex("foo");
          return true;
        }
      };

      try {
        TRANSACTION(obj);
        fail();
      }
      catch (err) {
        assertEqual(arangodb.errors.ERROR_TRANSACTION_DISALLOWED_OPERATION.code, err.errorNum);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: trx with create index operation
////////////////////////////////////////////////////////////////////////////////

    testCreateHashConstraint : function () {
      c1 = db._create(cn1);

      var obj = {
        collections : {
        },
        action : function () {
          c1.ensureUniqueConstraint("foo");
          return true;
        }
      };

      try {
        TRANSACTION(obj);
        fail();
      }
      catch (err) {
        assertEqual(arangodb.errors.ERROR_TRANSACTION_DISALLOWED_OPERATION.code, err.errorNum);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: trx with create index operation
////////////////////////////////////////////////////////////////////////////////

    testCreateHashIndex : function () {
      c1 = db._create(cn1);

      var obj = {
        collections : {
        },
        action : function () {
          c1.ensureHashIndex("foo");
          return true;
        }
      };

      try {
        TRANSACTION(obj);
        fail();
      }
      catch (err) {
        assertEqual(arangodb.errors.ERROR_TRANSACTION_DISALLOWED_OPERATION.code, err.errorNum);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: trx with create index operation
////////////////////////////////////////////////////////////////////////////////

    testCreateSkiplistIndex : function () {
      c1 = db._create(cn1);

      var obj = {
        collections : {
        },
        action : function () {
          c1.ensureSkiplist("foo");
          return true;
        }
      };

      try {
        TRANSACTION(obj);
        fail();
      }
      catch (err) {
        assertEqual(arangodb.errors.ERROR_TRANSACTION_DISALLOWED_OPERATION.code, err.errorNum);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: trx with create index operation
////////////////////////////////////////////////////////////////////////////////

    testCreateSkiplistConstraint : function () {
      c1 = db._create(cn1);

      var obj = {
        collections : {
        },
        action : function () {
          c1.ensureUniqueSkiplist("foo");
          return true;
        }
      };

      try {
        TRANSACTION(obj);
        fail();
      }
      catch (err) {
        assertEqual(arangodb.errors.ERROR_TRANSACTION_DISALLOWED_OPERATION.code, err.errorNum);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: trx with create index operation
////////////////////////////////////////////////////////////////////////////////

    testCreateFulltextIndex : function () {
      c1 = db._create(cn1);

      var obj = {
        collections : {
        },
        action : function () {
          c1.ensureFulltextIndex("foo");
          return true;
        }
      };

      try {
        TRANSACTION(obj);
        fail();
      }
      catch (err) {
        assertEqual(arangodb.errors.ERROR_TRANSACTION_DISALLOWED_OPERATION.code, err.errorNum);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: trx with create index operation
////////////////////////////////////////////////////////////////////////////////

    testCreateGeoIndex : function () {
      c1 = db._create(cn1);

      var obj = {
        collections : {
        },
        action : function () {
          c1.ensureGeoIndex("foo");
          return true;
        }
      };

      try {
        TRANSACTION(obj);
        fail();
      }
      catch (err) {
        assertEqual(arangodb.errors.ERROR_TRANSACTION_DISALLOWED_OPERATION.code, err.errorNum);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: trx with create index operation
////////////////////////////////////////////////////////////////////////////////

    testCreateGeoConstraint : function () {
      c1 = db._create(cn1);

      var obj = {
        collections : {
        },
        action : function () {
          c1.ensureGeoConstraint("foo");
          return true;
        }
      };

      try {
        TRANSACTION(obj);
        fail();
      }
      catch (err) {
        assertEqual(arangodb.errors.ERROR_TRANSACTION_DISALLOWED_OPERATION.code, err.errorNum);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: trx with drop index operation
////////////////////////////////////////////////////////////////////////////////

    testDropIndex : function () {
      c1 = db._create(cn1);
      var idx = c1.ensureUniqueConstraint("foo");

      var obj = {
        collections : {
        },
        action : function () {
          c1.dropIndex(idx.id);
          return true;
        }
      };

      try {
        TRANSACTION(obj);
        fail();
      }
      catch (err) {
        assertEqual(arangodb.errors.ERROR_TRANSACTION_DISALLOWED_OPERATION.code, err.errorNum);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: trx with read operation
////////////////////////////////////////////////////////////////////////////////

    testSingleRead1 : function () {
      c1 = db._create(cn1);
      c1.save({ _key: "foo", a: 1 });

      var obj = {
        collections : {
          read: [ cn1 ]
        },
        action : function () {
          assertEqual(1, c1.document("foo").a);
          return true;
        }
      };

      TRANSACTION(obj);
      assertEqual(1, c1.count());
      assertEqual([ "foo" ], sortedKeys(c1));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: trx with read operation
////////////////////////////////////////////////////////////////////////////////

    testSingleRead2 : function () {
      c1 = db._create(cn1);
      c1.save({ _key: "foo", a: 1 });

      var obj = {
        collections : {
          write: [ cn1 ]
        },
        action : function () {
          assertEqual(1, c1.document("foo").a);
          return true;
        }
      };

      TRANSACTION(obj);
      assertEqual(1, c1.count());
      assertEqual([ "foo" ], sortedKeys(c1));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: trx with read operation
////////////////////////////////////////////////////////////////////////////////

    testScan1 : function () {
      c1 = db._create(cn1);
      for (var i = 0; i < 100; ++i) {
        c1.save({ _key: "foo" + i, a: i });
      }

      var obj = {
        collections : {
          read: [ cn1 ]
        },
        action : function () {
          assertEqual(100, c1.toArray().length);
          assertEqual(100, c1.count());

          for (var i = 0; i < 100; ++i) {
            assertEqual(i, c1.document("foo" + i).a);
          }
          return true;
        }
      };

      TRANSACTION(obj);
      assertEqual(100, c1.count());
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: trx with read operation
////////////////////////////////////////////////////////////////////////////////

    testScan2 : function () {
      c1 = db._create(cn1);
      for (var i = 0; i < 100; ++i) {
        c1.save({ _key: "foo" + i, a: i });
      }

      var obj = {
        collections : {
          write: [ cn1 ]
        },
        action : function () {
          assertEqual(100, c1.toArray().length);
          assertEqual(100, c1.count());

          for (var i = 0; i < 100; ++i) {
            assertEqual(i, c1.document("foo" + i).a);
          }
          return true;
        }
      };

      TRANSACTION(obj);
      assertEqual(100, c1.count());
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: trx with insert operation
////////////////////////////////////////////////////////////////////////////////

    testSingleInsert : function () {
      c1 = db._create(cn1);

      var obj = {
        collections : {
          write: [ cn1 ]
        },
        action : function () {
          c1.save({ _key: "foo" });
          return true;
        }
      };

      TRANSACTION(obj);
      assertEqual(1, c1.count());
      assertEqual([ "foo" ], sortedKeys(c1));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: trx with insert operation
////////////////////////////////////////////////////////////////////////////////

    testMultiInsert : function () {
      c1 = db._create(cn1);

      var obj = {
        collections : {
          write: [ cn1 ]
        },
        action : function () {
          c1.save({ _key: "foo" });
          c1.save({ _key: "bar" });
          return true;
        }
      };

      TRANSACTION(obj);
      assertEqual(2, c1.count());
      assertEqual([ "bar", "foo" ], sortedKeys(c1));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: trx with insert operation
////////////////////////////////////////////////////////////////////////////////

    testInsertWithExisting : function () {
      c1 = db._create(cn1);
      c1.save({ _key: "foo" });
      c1.save({ _key: "bar" });

      var obj = {
        collections : {
          write: [ cn1 ]
        },
        action : function () {
          c1.save({ _key: "baz" });
          c1.save({ _key: "bam" });
          return true;
        }
      };

      TRANSACTION(obj);
      assertEqual(4, c1.count());
      assertEqual([ "bam", "bar", "baz", "foo" ], sortedKeys(c1));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: trx with insert operation
////////////////////////////////////////////////////////////////////////////////

    testInsertWithCap : function () {
      c1 = db._create(cn1);
      c1.ensureCapConstraint(3);
      c1.save({ _key: "foo" });
      c1.save({ _key: "bar" });

      var obj = {
        collections : {
          write: [ cn1 ]
        },
        action : function () {
          c1.save({ _key: "baz" });
          c1.save({ _key: "bam" });
      
          assertEqual(3, c1.count());
          assertEqual([ "bam", "bar", "baz" ], sortedKeys(c1));
          
          c1.save({ _key: "zap" });
          assertEqual(3, c1.count());
          assertEqual([ "bam", "baz", "zap" ], sortedKeys(c1));

          c1.save({ _key: "abc" });
          assertEqual(3, c1.count());
          assertEqual([ "abc", "bam", "zap" ], sortedKeys(c1));

          return true;
        }
      };

      TRANSACTION(obj);
      assertEqual(3, c1.count());
      assertEqual([ "abc", "bam", "zap" ], sortedKeys(c1));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: trx with replace operation
////////////////////////////////////////////////////////////////////////////////

    testReplace : function () {
      c1 = db._create(cn1);
      c1.save({ _key: "foo", a: 1 });
      c1.save({ _key: "bar", b: 2, c: 3 });

      var obj = {
        collections : {
          write: [ cn1 ]
        },
        action : function () {
          assertEqual(1, c1.document("foo").a);
          c1.replace("foo", { a: 3 });

          assertEqual(2, c1.document("bar").b);
          assertEqual(3, c1.document("bar").c);
          c1.replace("bar", { b: 9 });
          return true;
        }
      };

      TRANSACTION(obj);
      assertEqual(2, c1.count());
      assertEqual([ "bar", "foo" ], sortedKeys(c1));
      assertEqual(3, c1.document("foo").a);
      assertEqual(9, c1.document("bar").b);
      assertEqual(undefined, c1.document("bar").c);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: trx with replace operation
////////////////////////////////////////////////////////////////////////////////

    testReplaceReplace : function () {
      c1 = db._create(cn1);
      c1.save({ _key: "foo", a: 1 });

      var obj = {
        collections : {
          write: [ cn1 ]
        },
        action : function () {
          assertEqual(1, c1.document("foo").a);
          c1.replace("foo", { a: 3 });
          assertEqual(3, c1.document("foo").a);
          c1.replace("foo", { a: 4 });
          assertEqual(4, c1.document("foo").a);
          c1.replace("foo", { a: 5 });
          assertEqual(5, c1.document("foo").a);
          c1.replace("foo", { a: 6, b: 99 });
          assertEqual(6, c1.document("foo").a);
          assertEqual(99, c1.document("foo").b);
          return true;
        }
      };

      TRANSACTION(obj);
      assertEqual(1, c1.count());
      assertEqual(6, c1.document("foo").a);
      assertEqual(99, c1.document("foo").b);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: trx with update operation
////////////////////////////////////////////////////////////////////////////////

    testUpdate : function () {
      c1 = db._create(cn1);
      c1.save({ _key: "foo", a: 1 });
      c1.save({ _key: "bar", b: 2 });
      c1.save({ _key: "baz", c: 3 });
      c1.save({ _key: "bam", d: 4 });

      var obj = {
        collections : {
          write: [ cn1 ]
        },
        action : function () {
          assertEqual(1, c1.document("foo").a);
          c1.update("foo", { a: 3 });

          assertEqual(2, c1.document("bar").b);
          c1.update("bar", { b: 9 });

          assertEqual(3, c1.document("baz").c);
          c1.update("baz", { b: 9, c: 12 });
          
          assertEqual(4, c1.document("bam").d);
          return true;
        }
      };

      TRANSACTION(obj);
      assertEqual(4, c1.count());
      assertEqual([ "bam", "bar", "baz", "foo" ], sortedKeys(c1));
      assertEqual(3, c1.document("foo").a);
      assertEqual(9, c1.document("bar").b);
      assertEqual(9, c1.document("baz").b);
      assertEqual(12, c1.document("baz").c);
      assertEqual(4, c1.document("bam").d);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: trx with remove operation
////////////////////////////////////////////////////////////////////////////////

    testRemove : function () {
      c1 = db._create(cn1);
      c1.save({ _key: "foo", a: 1 });
      c1.save({ _key: "bar", b: 2 });
      c1.save({ _key: "baz", c: 3 });

      var obj = {
        collections : {
          write: [ cn1 ]
        },
        action : function () {
          c1.remove("foo");
          c1.remove("baz");
          return true;
        }
      };

      TRANSACTION(obj);
      assertEqual(1, c1.count());
      assertEqual([ "bar" ], sortedKeys(c1));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: trx with truncate operation
////////////////////////////////////////////////////////////////////////////////

    testTruncateEmpty : function () {
      c1 = db._create(cn1);

      var obj = {
        collections : {
          write: [ cn1 ]
        },
        action : function () {
          c1.truncate();
          return true;
        }
      };

      TRANSACTION(obj);
      assertEqual(0, c1.count());
      assertEqual([ ], sortedKeys(c1));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: trx with truncate operation
////////////////////////////////////////////////////////////////////////////////

    testTruncateNonEmpty : function () {
      c1 = db._create(cn1);

      for (var i = 0; i < 100; ++i) {
        c1.save({ a: i });
      }

      var obj = {
        collections : {
          write: [ cn1 ]
        },
        action : function () {
          c1.truncate();
          return true;
        }
      };

      TRANSACTION(obj);
      assertEqual(0, c1.count());
      assertEqual([ ], sortedKeys(c1));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: trx with truncate operation
////////////////////////////////////////////////////////////////////////////////

    testTruncateAndAdd : function () {
      c1 = db._create(cn1);

      for (var i = 0; i < 100; ++i) {
        c1.save({ a: i });
      }

      var obj = {
        collections : {
          write: [ cn1 ]
        },
        action : function () {
          c1.truncate();
          c1.save({ _key: "foo" });
          return true;
        }
      };

      TRANSACTION(obj);
      assertEqual(1, c1.count());
      assertEqual([ "foo" ], sortedKeys(c1));
    }

  };
}

// -----------------------------------------------------------------------------
// --SECTION--                                                        test suite
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////

function transactionBarriersSuite () {
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
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief tear down
////////////////////////////////////////////////////////////////////////////////

    tearDown : function () {
      if (c1 !== null) {
        c1.drop();
      }

      c1 = null;
      
      if (c2 !== null) {
        c2.drop();
      }

      c2 = null;
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: usage of barriers outside of transaction
////////////////////////////////////////////////////////////////////////////////

    testBarriersOutsideCommit : function () {
      c1 = db._create(cn1);

      var docs = [ ];
      var i;

      var obj = {
        collections : {
          write: [ cn1 ]
        },
        action : function () {
          for (i = 0; i < 100; ++i) {
            c1.save({ _key: "foo" + i, value1: i, value2: "foo" + i + "x" });
          }
          
          for (i = 0; i < 100; ++i) {
            docs.push(c1.document("foo" + i));
          }

          return c1.document("foo0");
        }
      };

      var result = TRANSACTION(obj);
     
      assertEqual(100, docs.length);
      assertEqual(100, c1.count());

      assertEqual("foo0", result._key);
      assertEqual(0, result.value1);
      assertEqual("foo0x", result.value2);
     
      for (i = 0; i < 100; ++i) {
        assertEqual("foo" + i, docs[i]._key);
        assertEqual(i, docs[i].value1);
        assertEqual("foo" + i + "x", docs[i].value2);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: usage of barriers outside of transaction
////////////////////////////////////////////////////////////////////////////////

    testBarriersOutsideRollback : function () {
      c1 = db._create(cn1);

      var docs = [ ];
      var i;

      var obj = {
        collections : {
          write: [ cn1 ]
        },
        action : function () {
          for (i = 0; i < 100; ++i) {
            c1.save({ _key: "foo" + i, value1: i, value2: "foo" + i + "x" });
          }
          
          for (i = 0; i < 100; ++i) {
            docs.push(c1.document("foo" + i));
          }

          throw "doh!";
        }
      };

      try {
        TRANSACTION(obj);
        fail();
      }
      catch (err) {
      }
     
      assertEqual(100, docs.length);

      for (i = 0; i < 100; ++i) {
        assertEqual("foo" + i, docs[i]._key);
        assertEqual(i, docs[i].value1);
        assertEqual("foo" + i + "x", docs[i].value2);
      }
    }

  };
}

// -----------------------------------------------------------------------------
// --SECTION--                                                        test suite
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////

function transactionGraphSuite () {
  var cn1 = "UnitTestsVertices";
  var cn2 = "UnitTestsEdges";

  var g = require('org/arangodb/graph').Graph; 

  var c1 = null;
  var c2 = null;
  
  return {

////////////////////////////////////////////////////////////////////////////////
/// @brief set up
////////////////////////////////////////////////////////////////////////////////

    setUp : function () {
      db._drop(cn1);
      db._drop(cn2);

      db._create(cn1);
      db._createEdgeCollection(cn2);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief tear down
////////////////////////////////////////////////////////////////////////////////

    tearDown : function () {
      if (c1 !== null) {
        c1.drop();
      }

      c1 = null;
      
      if (c2 !== null) {
        c2.drop();
      }

      c2 = null;
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: rollback updates in a graph transaction
////////////////////////////////////////////////////////////////////////////////

    testRollbackGraphUpdates : function () {
      var graph;

      try {
        graph = new g('UnitTestsGraph'); 
        graph.drop();
      }
      catch (err) {
      }

      graph = new g('UnitTestsGraph', cn1, cn2);
      var gotHere = 0;

      var obj = {
        collections: { 
          write: [ cn1, cn2 ]
        }, 
        action : function () {
          var result = { };
          result.enxirvp = graph.addVertex(null, { _rev : null })._properties;
          result.biitqtk = graph.addVertex(null, { _rev : null })._properties;
          result.oboyuhh = graph.addEdge(graph.getVertex(result.enxirvp._id), graph.getVertex(result.biitqtk._id), null, { name: "john smith" })._properties;
          result.cvwmkym = db[cn1].replace(result.enxirvp._id, { _rev : null });
          result.gsalfxu = db[cn1].replace(result.biitqtk._id, { _rev : null });
          result.xsjzbst = function (){
            graph.removeEdge(graph.getEdge(result.oboyuhh._id)); 
            return true;
          }(); 

          result.thizhdd = graph.addEdge(graph.getVertex(result.cvwmkym._id), graph.getVertex(result.gsalfxu._id), result.oboyuhh._key, { name: "david smith" });
          gotHere = 1;

          result.rldfnre = graph.addEdge(graph.getVertex(result.cvwmkym._id), graph.getVertex(result.gsalfxu._id), result.oboyuhh._key, { name : "david smith" })._properties;
          gotHere = 2;

          return result;
        } 
      };

      try {
        TRANSACTION(obj);
        fail();
      }
      catch (err) {
      }

      assertEqual(0, db[cn1].count());
      assertEqual(0, db[cn2].count());
      assertEqual(1, gotHere);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: usage of barriers outside of graph transaction
////////////////////////////////////////////////////////////////////////////////

    testUseBarriersOutsideGraphTransaction : function () {
      var graph;

      try {
        graph = new g('UnitTestsGraph'); 
        graph.drop();
      }
      catch (err) {
      }

      graph = new g('UnitTestsGraph', cn1, cn2);

      var obj = {
        collections: { 
          write: [ cn1, cn2 ]
        }, 
        action : function () {
          var result = { };

          result.enxirvp = graph.addVertex(null, { _rev : null })._properties;
          result.biitqtk = graph.addVertex(null, { _rev : null })._properties;
          result.oboyuhh = graph.addEdge(graph.getVertex(result.enxirvp._id), graph.getVertex(result.biitqtk._id), null, { name : "john smith" })._properties;
          result.cvwmkym = db[cn1].replace(result.enxirvp._id, { _rev : null });
          result.gsalfxu = db[cn1].replace(result.biitqtk._id, { _rev : null });
          result.xsjzbst = function (){
            graph.removeEdge(graph.getEdge(result.oboyuhh._id)); 
            return true;
          }();

          result.rldfnre = graph.addEdge(graph.getVertex(result.cvwmkym._id), graph.getVertex(result.gsalfxu._id), result.oboyuhh._key, { name : "david smith" })._properties;

          return result;
        } 
      };

      var result = TRANSACTION(obj);
      assertTrue(result.enxirvp._key.length > 0);
      assertEqual(undefined, result.enxirvp.name);

      assertTrue(result.biitqtk._key.length > 0);
      assertEqual(undefined, result.biitqtk.name);
      
      assertTrue(result.oboyuhh._key.length > 0);
      assertEqual("john smith", result.oboyuhh.name);
      assertEqual(null, result.oboyuhh.$label);
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
      assertEqual("david smith", result.rldfnre.name);
      assertEqual(null, result.rldfnre.$label);
    }

  };
}

// -----------------------------------------------------------------------------
// --SECTION--                                                        test suite
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////

function transactionRollbackSuite () {
  var cn1 = "UnitTestsTransaction1";

  var c1 = null;
      
  var sortedKeys = function (col) {
    var keys = [ ];

    col.toArray().forEach(function (d) { 
      keys.push(d._key);
    });

    keys.sort();
    return keys;
  };

  return {

////////////////////////////////////////////////////////////////////////////////
/// @brief set up
////////////////////////////////////////////////////////////////////////////////

    setUp : function () {
      db._drop(cn1);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief tear down
////////////////////////////////////////////////////////////////////////////////

    tearDown : function () {
      if (c1 !== null) {
        c1.drop();
      }

      c1 = null;
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: the collection revision id
////////////////////////////////////////////////////////////////////////////////

    testRollbackRevision : function () {
      c1 = db._create(cn1);
      c1.save({ _key: "foo" });

      var r = c1.revision();

      var obj = {
        collections : {
          write: [ cn1 ]
        },
        action : function () {
          c1.save({ _key: "tom" });
          assertTrue(c1.revision() > r);

          c1.save({ _key: "tim" });
          assertTrue(c1.revision() > r);

          c1.save({ _key: "tam" });
          assertTrue(c1.revision() > r);
        
          throw "rollback";
        }
      };

      try {
        TRANSACTION(obj);
        fail();
      }
      catch (err) {
      }

      assertEqual(1, c1.count());
      assertEqual(r, c1.revision());
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: rollback inserts
////////////////////////////////////////////////////////////////////////////////

    testRollbackInsert : function () {
      c1 = db._create(cn1);
      c1.save({ _key: "foo" });
      c1.save({ _key: "bar" });
      c1.save({ _key: "meow" });

      var obj = {
        collections : {
          write: [ cn1 ]
        },
        action : function () {
          c1.save({ _key: "tom" });
          c1.save({ _key: "tim" });
          c1.save({ _key: "tam" });

          assertEqual(6, c1.count());
          throw "rollback";
        }
      };

      try {
        TRANSACTION(obj);
        fail();
      }
      catch (err) {
      }

      assertEqual(3, c1.count());
      assertEqual([ "bar", "foo", "meow" ], sortedKeys(c1));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: rollback update
////////////////////////////////////////////////////////////////////////////////

    testRollbackUpdate : function () {
      var d1, d2, d3;

      c1 = db._create(cn1);
      d1 = c1.save({ _key: "foo", a: 1 });
      d2 = c1.save({ _key: "bar", a: 2 });
      d3 = c1.save({ _key: "meow", a: 3 });

      var obj = {
        collections : {
          write: [ cn1 ]
        },
        action : function () {
          c1.update(d1, { a: 4 });
          c1.update(d2, { a: 5 });
          c1.update(d3, { a: 6 });

          assertEqual(3, c1.count());
          throw "rollback";
        }
      };

      try {
        TRANSACTION(obj);
        fail();
      }
      catch (err) {
      }

      assertEqual(3, c1.count());
      assertEqual([ "bar", "foo", "meow" ], sortedKeys(c1));
      assertEqual(d1._rev, c1.document("foo")._rev);
      assertEqual(d2._rev, c1.document("bar")._rev);
      assertEqual(d3._rev, c1.document("meow")._rev);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: rollback update
////////////////////////////////////////////////////////////////////////////////

    testRollbackUpdateUpdate : function () {
      var d1, d2, d3;

      c1 = db._create(cn1);
      d1 = c1.save({ _key: "foo", a: 1 });
      d2 = c1.save({ _key: "bar", a: 2 });
      d3 = c1.save({ _key: "meow", a: 3 });

      var obj = {
        collections : {
          write: [ cn1 ]
        },
        action : function () {
          for (var i = 0; i < 100; ++i) {
            c1.replace("foo", { a: i });
            c1.replace("bar", { a: i + 42 });
            c1.replace("meow", { a: i + 23 });

            assertEqual(i, c1.document("foo").a);
            assertEqual(i + 42, c1.document("bar").a);
            assertEqual(i + 23, c1.document("meow").a);
          }

          assertEqual(3, c1.count());

          throw "rollback";
        }
      };

      try {
        TRANSACTION(obj);
        fail();
      }
      catch (err) {
      }

      assertEqual(3, c1.count());
      assertEqual([ "bar", "foo", "meow" ], sortedKeys(c1));
      assertEqual(1, c1.document("foo").a);
      assertEqual(2, c1.document("bar").a);
      assertEqual(3, c1.document("meow").a);
      assertEqual(d1._rev, c1.document("foo")._rev);
      assertEqual(d2._rev, c1.document("bar")._rev);
      assertEqual(d3._rev, c1.document("meow")._rev);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: rollback remove
////////////////////////////////////////////////////////////////////////////////

    testRollbackRemove : function () {
      c1 = db._create(cn1);
      c1.save({ _key: "foo" });
      c1.save({ _key: "bar" });
      c1.save({ _key: "meow" });

      var obj = {
        collections : {
          write: [ cn1 ]
        },
        action : function () {
          c1.remove("meow");
          c1.remove("foo");

          assertEqual(1, c1.count());
          assertEqual([ "bar" ], sortedKeys(c1));

          throw "rollback";
        }
      };

      try {
        TRANSACTION(obj);
        fail();
      }
      catch (err) {
      }

      assertEqual(3, c1.count());
      assertEqual([ "bar", "foo", "meow" ], sortedKeys(c1));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: rollback remove
////////////////////////////////////////////////////////////////////////////////

    testRollbackRemoveMulti : function () {
      c1 = db._create(cn1);
      c1.save({ _key: "foo" });

      var obj = {
        collections : {
          write: [ cn1 ]
        },
        action : function () {
          for (var i = 0; i < 100; ++i) {
            c1.save({ _key: "foo" + i });
          } 

          assertEqual(101, c1.count());

          throw "rollback";
        }
      };

      try {
        TRANSACTION(obj);
        fail();
      }
      catch (err) {
      }

      assertEqual(1, c1.count());
      assertEqual([ "foo" ], sortedKeys(c1));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: rollback truncate
////////////////////////////////////////////////////////////////////////////////

    testRollbackTruncateEmpty : function () {
      c1 = db._create(cn1);

      var obj = {
        collections : {
          write: [ cn1 ]
        },
        action : function () {
          // truncate often...
          for (var i = 0; i < 100; ++i) {
            c1.truncate();
          } 

          throw "rollback";
        }
      };

      try {
        TRANSACTION(obj);
        fail();
      }
      catch (err) {
      }

      assertEqual(0, c1.count());
      assertEqual([ ], sortedKeys(c1));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: rollback truncate
////////////////////////////////////////////////////////////////////////////////

    testRollbackTruncateNonEmpty : function () {
      c1 = db._create(cn1);
      for (var i = 0; i < 100; ++i) {
        c1.save({ _key: "foo" + i });
      }
      assertEqual(100, c1.count());

      var obj = {
        collections : {
          write: [ cn1 ]
        },
        action : function () {
          // truncate often...
          for (var i = 0; i < 100; ++i) {
            c1.truncate();
          } 
          c1.save({ _key: "bar" });

          throw "rollback";
        }
      };

      try {
        TRANSACTION(obj);
        fail();
      }
      catch (err) {
      }

      assertEqual(100, c1.count());
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: trx rollback with unique constraint violation 
////////////////////////////////////////////////////////////////////////////////

    testRollbackUniquePrimary : function () {
      c1 = db._create(cn1);
      var d1 = c1.save({ _key: "foo" });

      var obj = {
        collections : {
          write: [ cn1 ]
        },
        action : function () {
          c1.save({ _key: "bar" });

          c1.save({ _key: "foo" });

          throw "rollback";
        }
      };

      try {
        TRANSACTION(obj);
        fail();
      }
      catch (err) {
      }

      assertEqual(1, c1.count());
      assertEqual("foo", c1.toArray()[0]._key);
      assertEqual(d1._rev, c1.toArray()[0]._rev);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: trx rollback with unique constraint violation 
////////////////////////////////////////////////////////////////////////////////

    testRollbackUniqueSecondary : function () {
      c1 = db._create(cn1);
      c1.ensureUniqueConstraint("name");
      var d1 = c1.save({ name: "foo" });

      var obj = {
        collections : {
          write: [ cn1 ]
        },
        action : function () {
          c1.save( { name: "bar" });
          c1.save( { name: "baz" });
          c1.save( { name: "foo" });

          throw "rollback";
        }
      };

      try {
        TRANSACTION(obj);
        fail();
      }
      catch (err) {
      }
      assertEqual(1, c1.count());
      assertEqual("foo", c1.toArray()[0].name);
      assertEqual(d1._rev, c1.toArray()[0]._rev);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: rollback inserts with cap constraint
////////////////////////////////////////////////////////////////////////////////

    testRollbackInsertCapConstraint1 : function () {
      c1 = db._create(cn1);
      c1.ensureCapConstraint(2);
      c1.save({ _key: "foo" });
      c1.save({ _key: "bar" });

      var obj = {
        collections : {
          write: [ cn1 ]
        },
        action : function () {
          c1.save({ _key: "tom" });

          assertEqual(2, c1.count()); // bar, tom
          assertEqual([ "bar", "tom" ], sortedKeys(c1));

          throw "rollback";
        }
      };

      try {
        TRANSACTION(obj);
        fail();
      }
      catch (err) {
      }

      assertEqual(2, c1.count());
      assertEqual([ "bar", "foo" ], sortedKeys(c1));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: rollback inserts with cap constraint
////////////////////////////////////////////////////////////////////////////////

    testRollbackInsertCapConstraint2 : function () {
      c1 = db._create(cn1);
      c1.ensureCapConstraint(2);
      c1.save({ _key: "foo" });
      c1.save({ _key: "bar" });
      c1.replace("bar", { a : 1 });

      var obj = {
        collections : {
          write: [ cn1 ]
        },
        action : function () {
          c1.save({ _key: "tom" });
          c1.save({ _key: "tim" });

          assertEqual(2, c1.count()); // tom, tim
          assertEqual([ "tim", "tom" ], sortedKeys(c1));

          throw "rollback";
        }
      };

      try {
        TRANSACTION(obj);
        fail();
      }
      catch (err) {
      }

      assertEqual(2, c1.count());
      assertEqual([ "bar", "foo" ], sortedKeys(c1));
      
      c1.save({ _key: "baz" });
      assertEqual(2, c1.count());
      assertEqual([ "bar", "baz" ], sortedKeys(c1));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: rollback inserts with cap constraint
////////////////////////////////////////////////////////////////////////////////

    testRollbackInsertCapConstraint3 : function () {
      c1 = db._create(cn1);
      c1.ensureCapConstraint(2);
      c1.save({ _key: "foo" });
      c1.save({ _key: "bar" });

      var obj = {
        collections : {
          write: [ cn1 ]
        },
        action : function () {
          c1.save({ _key: "tom" });
          c1.save({ _key: "tim" });
          c1.save({ _key: "tum" });
          assertEqual(2, c1.count()); // tim, tum
          assertEqual([ "tim", "tum" ], sortedKeys(c1));

          throw "rollback";
        }
      };

      try {
        TRANSACTION(obj);
        fail();
      }
      catch (err) {
      }

      assertEqual(2, c1.count());
      assertEqual([ "bar", "foo" ], sortedKeys(c1));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: rollback inserts with cap constraint
////////////////////////////////////////////////////////////////////////////////

    testRollbackInsertCapConstraint4 : function () {
      c1 = db._create(cn1);
      c1.ensureCapConstraint(2);
      c1.save({ _key: "foo" });
      c1.save({ _key: "bar" });

      var obj = {
        collections : {
          write: [ cn1 ]
        },
        action : function () {
          c1.save({ _key: "tom" });
          c1.save({ _key: "tim" });
          c1.save({ _key: "tum" });
          c1.save({ _key: "tam" });
          c1.save({ _key: "tem" });
          assertEqual(2, c1.count()); // tim, tum
          assertEqual([ "tam", "tem" ], sortedKeys(c1));

          c1.remove("tem");
          assertEqual(1, c1.count()); // tim, tum
          assertEqual([ "tam" ], sortedKeys(c1));

          throw "rollback";
        }
      };

      try {
        TRANSACTION(obj);
        fail();
      }
      catch (err) {
      }

      assertEqual(2, c1.count());
      assertEqual([ "bar", "foo" ], sortedKeys(c1));
      
      c1.save({ _key: "baz" });
      assertEqual(2, c1.count());
      assertEqual([ "bar", "baz" ], sortedKeys(c1));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: rollback inserts with cap constraint
////////////////////////////////////////////////////////////////////////////////

    testRollbackInsertCapConstraint5 : function () {
      c1 = db._create(cn1);
      c1.ensureCapConstraint(3);
      c1.save({ _key: "1" });
      c1.save({ _key: "2" });
      c1.save({ _key: "3" });

      var obj = {
        collections : {
          write: [ cn1 ]
        },
        action : function () {
          c1.save({ _key: "4" });
          c1.save({ _key: "5" });
          assertEqual(3, c1.count()); 
          assertEqual([ "3", "4", "5" ], sortedKeys(c1));

          throw "rollback";
        }
      };

      try {
        TRANSACTION(obj);
        fail();
      }
      catch (err) {
      }

      c1.save({ _key: "4" });
      assertEqual(3, c1.count());
      assertEqual([ "2", "3", "4" ], sortedKeys(c1));
      
      c1.save({ _key: "5" });
      assertEqual(3, c1.count()); 
      assertEqual([ "3", "4", "5" ], sortedKeys(c1));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: rollback inserts with cap constraint
////////////////////////////////////////////////////////////////////////////////

    testRollbackInsertCapConstraint6 : function () {
      c1 = db._create(cn1);
      c1.ensureCapConstraint(3);
      c1.save({ _key: "foo" });
      c1.save({ _key: "bar" });

      var obj = {
        collections : {
          write: [ cn1 ]
        },
        action : function () {
          c1.save({ _key: "tom" });
          c1.save({ _key: "tim" });
          c1.save({ _key: "tum" });
          assertEqual(3, c1.count()); // tom, tim, tum
          assertEqual([ "tim", "tom", "tum" ], sortedKeys(c1));
          c1.replace("tum", { a : 1 });

          throw "rollback";
        }
      };

      try {
        TRANSACTION(obj);
        fail();
      }
      catch (err) {
      }

      assertEqual(2, c1.count());
      assertEqual([ "bar", "foo" ], sortedKeys(c1));

      c1.save({ _key: "test" });
      assertEqual(3, c1.count());
      assertEqual([ "bar", "foo", "test" ], sortedKeys(c1));
      
      c1.save({ _key: "abc" });
      assertEqual(3, c1.count());
      assertEqual([ "abc", "bar", "test" ], sortedKeys(c1));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: rollback inserts with cap constraint
////////////////////////////////////////////////////////////////////////////////

    testRollbackUpdateCapConstraint6 : function () {
      c1 = db._create(cn1);
      c1.ensureCapConstraint(3);
      c1.save({ _key: "foo" });
      c1.save({ _key: "bar" });
      c1.save({ _key: "baz" });

      var obj = {
        collections : {
          write: [ cn1 ]
        },
        action : function () {
          c1.replace("baz", { a : 1 });
          c1.replace("bar", { a : 1 });
          c1.replace("foo", { a : 1 });
          
          assertEqual(3, c1.count());
          c1.save({ _key: "tim" });
          assertEqual([ "bar", "foo", "tim" ], sortedKeys(c1));

          throw "rollback";
        }
      };

      try {
        TRANSACTION(obj);
        fail();
      }
      catch (err) {
      }

      assertEqual(3, c1.count());
      assertEqual([ "bar", "baz", "foo" ], sortedKeys(c1));

      c1.save({ _key: "test" });
      assertEqual(3, c1.count());
      assertEqual([ "bar", "baz", "test" ], sortedKeys(c1));
    }

  };
}

// -----------------------------------------------------------------------------
// --SECTION--                                                        test suite
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////

function transactionCountSuite () {
  var cn1 = "UnitTestsTransaction1";

  var c1 = null;

  return {

////////////////////////////////////////////////////////////////////////////////
/// @brief set up
////////////////////////////////////////////////////////////////////////////////

    setUp : function () {
      db._drop(cn1);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief tear down
////////////////////////////////////////////////////////////////////////////////

    tearDown : function () {
      if (c1 !== null) {
        c1.drop();
      }

      c1 = null;
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: test counts during trx 
////////////////////////////////////////////////////////////////////////////////

    testCountDuring : function () {
      c1 = db._create(cn1);
      assertEqual(0, c1.count());

      var obj = {
        collections : {
          write: [ cn1 ]
        },
        action : function () {
          var d1, d2;

          c1.save({ a : 1 });
          assertEqual(1, c1.count());

          d1 = c1.save({ a : 2 });
          assertEqual(2, c1.count());

          d2 = c1.update(d1, { a : 3 });
          assertEqual(2, c1.count());

          assertEqual(3, c1.document(d2).a);

          c1.remove(d2);
          assertEqual(1, c1.count());

          c1.truncate();
          assertEqual(0, c1.count());
          
          c1.truncate();
          assertEqual(0, c1.count());

          return true;
        }
      };

      TRANSACTION(obj);
      assertEqual(0, c1.count());
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: test counts during and after trx 
////////////////////////////////////////////////////////////////////////////////

    testCountCommit : function () {
      c1 = db._create(cn1);
      c1.save({ _key: "foo" });
      c1.save({ _key: "bar" });
      assertEqual(2, c1.count());

      var obj = {
        collections : {
          write: [ cn1 ]
        },
        action : function () {
          var d1;

          c1.save({ _key: "baz" });
          assertEqual(3, c1.count());

          c1.save({ _key: "meow" });
          assertEqual(4, c1.count());

          c1.remove("foo");
          assertEqual(3, c1.count());
          return true;
        }
      };

      TRANSACTION(obj);
      assertEqual(3, c1.count());
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: test counts during and after trx 
////////////////////////////////////////////////////////////////////////////////

    testCountRollback : function () {
      c1 = db._create(cn1);
      c1.save({ _key: "foo" });
      c1.save({ _key: "bar" });
      assertEqual(2, c1.count());

      var obj = {
        collections : {
          write: [ cn1 ]
        },
        action : function () {
          var d1;

          c1.save({ _key: "baz" });
          assertEqual(3, c1.count());

          c1.save({ _key: "meow" });
          assertEqual(4, c1.count());

          c1.remove("foo");
          assertEqual(3, c1.count());

          throw "rollback";
        }
      };

      try {
        TRANSACTION(obj);
        fail();
      }
      catch (err) {
      }
      assertEqual(2, c1.count());

      var keys = [ ];
      c1.toArray().forEach(function (d) { 
        keys.push(d._key);
      });
      keys.sort();
      assertEqual([ "bar", "foo" ], keys);
    }

  };
}

// -----------------------------------------------------------------------------
// --SECTION--                                                        test suite
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////

function transactionCrossCollectionSuite () {
  var cn1 = "UnitTestsTransaction1";
  var cn2 = "UnitTestsTransaction2";

  var c1 = null;
  var c2 = null;
  
  var sortedKeys = function (col) {
    var keys = [ ];

    col.toArray().forEach(function (d) { 
      keys.push(d._key);
    });

    keys.sort();
    return keys;
  };

  return {

////////////////////////////////////////////////////////////////////////////////
/// @brief set up
////////////////////////////////////////////////////////////////////////////////

    setUp : function () {
      db._drop(cn1);
      db._drop(cn2);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief tear down
////////////////////////////////////////////////////////////////////////////////

    tearDown : function () {
      if (c1 !== null) {
        c1.drop();
      }

      c1 = null;
      
      if (c2 !== null) {
        c2.drop();
      }

      c2 = null;
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: test cross collection commit
////////////////////////////////////////////////////////////////////////////////

    testInserts : function () {
      c1 = db._create(cn1);
      c2 = db._create(cn2);

      var obj = {
        collections : {
          write: [ cn1, cn2 ]
        },
        action : function () {
          var i;

          for (i = 0; i < 10; ++i) {
            c1.save({ _key: "a" + i });
            c2.save({ _key: "b" + i });
          }

          return true;
        }
      };

      TRANSACTION(obj);
      assertEqual(10, c1.count());
      assertEqual(10, c2.count());
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: test cross collection commit
////////////////////////////////////////////////////////////////////////////////

    testUpdates : function () {
      c1 = db._create(cn1);
      c2 = db._create(cn2);

      var i;
      for (i = 0; i < 10; ++i) {
        c1.save({ _key: "a" + i, a: i });
        c2.save({ _key: "b" + i, b: i });
      }

      var obj = {
        collections : {
          write: [ cn1, cn2 ]
        },
        action : function () {
          for (i = 0; i < 10; ++i) {
            c1.update("a" + i, { a: i + 20 });

            c2.update("b" + i, { b: i + 20 });
            c2.remove("b" + i);
          }

          return true;
        }
      };

      TRANSACTION(obj);
      assertEqual(10, c1.count());
      assertEqual(0, c2.count());
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: test cross collection commit
////////////////////////////////////////////////////////////////////////////////

    testDeletes : function () {
      c1 = db._create(cn1);
      c2 = db._create(cn2);

      var i;
      for (i = 0; i < 10; ++i) {
        c1.save({ _key: "a" + i, a: i });
        c2.save({ _key: "b" + i, b: i });
      }

      var obj = {
        collections : {
          write: [ cn1, cn2 ]
        },
        action : function () {
          for (i = 0; i < 10; ++i) {
            c1.remove("a" + i);
            c2.remove("b" + i);
          }

          return true;
        }
      };

      TRANSACTION(obj);
      assertEqual(0, c1.count());
      assertEqual(0, c2.count());
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: test cross collection commit
////////////////////////////////////////////////////////////////////////////////

    testDeleteReload : function () {
      c1 = db._create(cn1);
      c2 = db._create(cn2);

      var i;
      for (i = 0; i < 10; ++i) {
        c1.save({ _key: "a" + i, a: i });
        c2.save({ _key: "b" + i, b: i });
      }

      var obj = {
        collections : {
          write: [ cn1, cn2 ]
        },
        action : function () {
          for (i = 0; i < 10; ++i) {
            c1.remove("a" + i);
            c2.remove("b" + i);
          }

          return true;
        }
      };

      TRANSACTION(obj);
      assertEqual(0, c1.count());
      assertEqual(0, c2.count());

      c1.unload();
      c2.unload();
      internal.wait(4);
      
      assertEqual(0, c1.count());
      assertEqual(0, c2.count());
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: test cross collection commit
////////////////////////////////////////////////////////////////////////////////

    testCommitReload : function () {
      c1 = db._create(cn1);
      c2 = db._create(cn2);

      var obj = {
        collections : {
          write: [ cn1, cn2 ]
        },
        action : function () {
          var i;

          for (i = 0; i < 10; ++i) {
            c1.save({ _key: "a" + i });
          }
          
          for (i = 0; i < 10; ++i) {
            c2.save({ _key: "b" + i });
          }
          
          assertEqual(10, c1.count());
          assertEqual(10, c2.count());

          c1.remove("a4");
          c2.remove("b6");
          
          return true;
        }
      };

      TRANSACTION(obj);
      assertEqual(9, c1.count());
      assertEqual(9, c2.count());

      c1.unload();
      c2.unload();

      internal.wait(4);

      assertEqual(9, c1.count());
      assertEqual(9, c2.count());
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: test cross collection rollback
////////////////////////////////////////////////////////////////////////////////

    testRollbackReload : function () {
      c1 = db._create(cn1);
      c2 = db._create(cn2);

      c1.save({ _key: "a1" });
      c2.save({ _key: "b1", a: 1 });

      var obj = {
        collections : {
          write: [ cn1, cn2 ]
        },
        action : function () {
          c1.save({ _key: "a2" });
          c1.save({ _key: "a3" });

          c2.save({ _key: "b2" });
          c2.update("b1", { a: 2 });
          assertEqual(2, c2.document("b1").a);
        
          throw "rollback";  
        }
      };

      try {
        TRANSACTION(obj);
        fail();
      }
      catch (err) {
      }

      assertEqual(1, c1.count());
      assertEqual(1, c2.count());

      assertEqual([ "a1" ], sortedKeys(c1));
      assertEqual([ "b1" ], sortedKeys(c2));
      assertEqual(1, c2.document("b1").a);

      c1.unload();
      c2.unload();

      internal.wait(4);

      assertEqual(1, c1.count());
      assertEqual(1, c2.count());
      
      assertEqual([ "a1" ], sortedKeys(c1));
      assertEqual([ "b1" ], sortedKeys(c2));
      assertEqual(1, c2.document("b1").a);
    }

  };
}

// -----------------------------------------------------------------------------
// --SECTION--                                                        test suite
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////

function transactionServerFailuresSuite () {
  var cn = "UnitTestsTransaction";

  var c = null;
  
  return {

////////////////////////////////////////////////////////////////////////////////
/// @brief set up
////////////////////////////////////////////////////////////////////////////////

    setUp : function () {
      internal.debugClearFailAt();
      db._drop(cn);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief tear down
////////////////////////////////////////////////////////////////////////////////

    tearDown : function () {
      internal.debugClearFailAt();

      if (c !== null) {
        c.drop();
      }

      c = null;
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: rollback in case of a server-side fail
////////////////////////////////////////////////////////////////////////////////

    testRollbackInsertSingle1 : function () {
      c = db._create(cn);

      internal.debugSetFailAt("TRI_WriteOperationDocumentCollection");
      try {
        c.save({ _key: "foo" });
        fail();
      }
      catch (err) {
        assertEqual(internal.errors.ERROR_INTERNAL.code, err.errorNum);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: rollback in case of a server-side fail
////////////////////////////////////////////////////////////////////////////////

    testRollbackInsertSingle2 : function () {
      c = db._create(cn);

      c.save({ _key: "foo" });
      internal.debugSetFailAt("TRI_WriteOperationDocumentCollection");
      try {
        c.save({ _key: "foo2" });
        fail();
      }
      catch (err) {
        assertEqual(internal.errors.ERROR_INTERNAL.code, err.errorNum);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: rollback in case of a server-side fail
////////////////////////////////////////////////////////////////////////////////

    testRollbackInsertMulti1 : function () {
      c = db._create(cn);
      c.save({ _key: "baz" });

      var obj = {
        collections : {
          write: [ cn ]
        },
        action : function () {
          c.save({ _key: "foo" });
          internal.debugSetFailAt("AddCollectionOperation-OOM");
          c.save({ _key: "bar" });
          fail(); 
        }
      };

      try {
        TRANSACTION(obj);
        fail();
      }
      catch (err) {
        assertEqual(internal.errors.ERROR_OUT_OF_MEMORY.code, err.errorNum);
      }
      
      assertEqual(1, c.count());
      assertEqual("baz", c.document("baz")._key);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: rollback in case of a server-side fail
////////////////////////////////////////////////////////////////////////////////

    testRollbackInsertMulti2 : function () {
      c = db._create(cn);

      var i;
      for (i = 0; i < 100; ++i) {
        c.save({ _key: "key" + i });
      }

      var obj = {
        collections : {
          write: [ cn ]
        },
        action : function () {
          for (i = 0; i < 100; ++i) {
            c.save({ _key: "foo" + i });
          }
          internal.debugSetFailAt("AddCollectionOperation-OOM");
          c.save({ _key: "bar" });
          fail(); 
        }
      };

      try {
        TRANSACTION(obj);
        fail();
      }
      catch (err) {
        assertEqual(internal.errors.ERROR_OUT_OF_MEMORY.code, err.errorNum);
      }
      
      assertEqual(100, c.count());
      assertEqual("key0", c.document("key0")._key);
      assertEqual("key99", c.document("key99")._key);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: rollback in case of a server-side fail
////////////////////////////////////////////////////////////////////////////////
    
    testRollbackUpdateSingle1 : function () {
      c = db._create(cn);

      c.save({ _key: "foo", value: 1 });

      internal.debugSetFailAt("TRI_WriteOperationDocumentCollection");
      try {
        c.update("foo", { value: 2 });
        fail();
      }
      catch (err) {
        assertEqual(internal.errors.ERROR_INTERNAL.code, err.errorNum);
      }

      assertEqual(1, c.count());
      assertEqual("foo", c.document("foo")._key);
      assertEqual(1, c.document("foo").value);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: rollback in case of a server-side fail
////////////////////////////////////////////////////////////////////////////////
    
    testRollbackUpdateSingle2 : function () {
      c = db._create(cn);

      c.save({ _key: "foo", value: 1 });
      c.save({ _key: "bar", value: "a" });

      c.update("foo", { value: 2 });
      internal.debugSetFailAt("TRI_WriteOperationDocumentCollection");
      try {
        c.update("bar", { value: "b" });
        fail();
      }
      catch (err) {
        assertEqual(internal.errors.ERROR_INTERNAL.code, err.errorNum);
      }

      assertEqual(2, c.count());
      assertEqual("foo", c.document("foo")._key);
      assertEqual(2, c.document("foo").value);
      assertEqual("bar", c.document("bar")._key);
      assertEqual("a", c.document("bar").value);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: rollback in case of a server-side fail
////////////////////////////////////////////////////////////////////////////////

    testRollbackUpdateMulti1 : function () {
      c = db._create(cn);
      
      c.save({ _key: "foo", value: 1 });
      c.save({ _key: "bar", value: "a" });

      var obj = {
        collections : {
          write: [ cn ]
        },
        action : function () {
          c.update("foo", { value: 2 });
          internal.debugSetFailAt("AddCollectionOperation-OOM");
          c.update("bar", { value: "b" });
          fail(); 
        }
      };

      try {
        TRANSACTION(obj);
        fail();
      }
      catch (err) {
        assertEqual(internal.errors.ERROR_OUT_OF_MEMORY.code, err.errorNum);
      }
      
      assertEqual(2, c.count());
      assertEqual("foo", c.document("foo")._key);
      assertEqual(1, c.document("foo").value);
      assertEqual("bar", c.document("bar")._key);
      assertEqual("a", c.document("bar").value);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: rollback in case of a server-side fail
////////////////////////////////////////////////////////////////////////////////

    testRollbackUpdateMulti2 : function () {
      c = db._create(cn);
      
      c.save({ _key: "foo", value: 1 });
      c.save({ _key: "bar", value: "a" });

      var obj = {
        collections : {
          write: [ cn ]
        },
        action : function () {
          internal.debugSetFailAt("AddCollectionOperation-OOM");
          c.update("foo", { value: 2 });
          fail(); 
        }
      };

      try {
        TRANSACTION(obj);
        fail();
      }
      catch (err) {
        assertEqual(internal.errors.ERROR_OUT_OF_MEMORY.code, err.errorNum);
      }
      
      assertEqual(2, c.count());
      assertEqual("foo", c.document("foo")._key);
      assertEqual(1, c.document("foo").value);
      assertEqual("bar", c.document("bar")._key);
      assertEqual("a", c.document("bar").value);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: rollback in case of a server-side fail
////////////////////////////////////////////////////////////////////////////////
    
    testRollbackRemoveSingle1 : function () {
      c = db._create(cn);

      c.save({ _key: "foo" });

      internal.debugSetFailAt("TRI_WriteOperationDocumentCollection");
      try {
        c.remove("foo");
        fail();
      }
      catch (err) {
        assertEqual(internal.errors.ERROR_INTERNAL.code, err.errorNum);
      }

      assertEqual(1, c.count());
      assertEqual("foo", c.document("foo")._key);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: rollback in case of a server-side fail
////////////////////////////////////////////////////////////////////////////////
    
    testRollbackRemoveSingle2 : function () {
      c = db._create(cn);

      c.save({ _key: "foo" });
      c.save({ _key: "bar" });

      internal.debugSetFailAt("TRI_WriteOperationDocumentCollection");
      try {
        c.remove("foo");
        fail();
      }
      catch (err) {
        assertEqual(internal.errors.ERROR_INTERNAL.code, err.errorNum);
      }

      assertEqual(2, c.count());
      assertEqual("foo", c.document("foo")._key);
      assertEqual("bar", c.document("bar")._key);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: rollback in case of a server-side fail
////////////////////////////////////////////////////////////////////////////////

    testRollbackRemoveMulti1 : function () {
      c = db._create(cn);
      
      c.save({ _key: "foo" });
      c.save({ _key: "bar" });

      var obj = {
        collections : {
          write: [ cn ]
        },
        action : function () {
          c.remove("foo");
          internal.debugSetFailAt("AddCollectionOperation-OOM");
          c.remove("bar");
          fail(); 
        }
      };

      try {
        TRANSACTION(obj);
        fail();
      }
      catch (err) {
        assertEqual(internal.errors.ERROR_OUT_OF_MEMORY.code, err.errorNum);
      }
      
      assertEqual(2, c.count());
      assertEqual("foo", c.document("foo")._key);
      assertEqual("bar", c.document("bar")._key);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: rollback in case of a server-side fail
////////////////////////////////////////////////////////////////////////////////

    testRollbackRemoveMulti2 : function () {
      c = db._create(cn);
      
      c.save({ _key: "foo" });
      c.save({ _key: "bar" });

      var obj = {
        collections : {
          write: [ cn ]
        },
        action : function () {
          internal.debugSetFailAt("AddCollectionOperation-OOM");
          c.remove("foo");
          fail(); 
        }
      };

      try {
        TRANSACTION(obj);
        fail();
      }
      catch (err) {
        assertEqual(internal.errors.ERROR_OUT_OF_MEMORY.code, err.errorNum);
      }
      
      assertEqual(2, c.count());
      assertEqual("foo", c.document("foo")._key);
      assertEqual("bar", c.document("bar")._key);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: rollback in case of a server-side fail
////////////////////////////////////////////////////////////////////////////////

    testRollbackRemoveMixed1 : function () {
      c = db._create(cn);
      
      var i;

      for (i = 0; i < 100; ++i) {
        c.save({ _key: "key" + i, value: i });
      }

      var obj = {
        collections : {
          write: [ cn ]
        },
        action : function () {

          for (i = 0; i < 50; ++i) {
            c.remove("key" + i);
          }

          for (i = 50; i < 100; ++i) {
            c.update("key" + i, { value: i - 50 });
          }

          internal.debugSetFailAt("AddCollectionOperation-OOM");
          c.remove("key50");
          fail(); 
        }
      };

      try {
        TRANSACTION(obj);
        fail();
      }
      catch (err) {
        assertEqual(internal.errors.ERROR_OUT_OF_MEMORY.code, err.errorNum);
      }
      
      assertEqual(100, c.count());
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: rollback in case of a server-side fail
////////////////////////////////////////////////////////////////////////////////

    testRollbackRemoveMixed2 : function () {
      c = db._create(cn);
      
      c.save({ _key: "foo" });
      c.save({ _key: "bar" });

      var obj = {
        collections : {
          write: [ cn ]
        },
        action : function () {
          var i;

          for (i = 0; i < 10; ++i) {
            c.save({ _key: "key" + i, value: i });
          }

          for (i = 0; i < 5; ++i) {
            c.remove("key" + i);
          }

          for (i = 5; i < 10; ++i) {
            c.update("key" + i, { value: i - 5 });
          }

          internal.debugSetFailAt("AddCollectionOperation-OOM");
          c.remove("key5");
          fail(); 
        }
      };

      try {
        TRANSACTION(obj);
        fail();
      }
      catch (err) {
        assertEqual(internal.errors.ERROR_OUT_OF_MEMORY.code, err.errorNum);
      }
      
      assertEqual(2, c.count());
      assertEqual("foo", c.document("foo")._key);
      assertEqual("bar", c.document("bar")._key);
    }

  };
}

// -----------------------------------------------------------------------------
// --SECTION--                                                              main
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suites
////////////////////////////////////////////////////////////////////////////////

jsunity.run(transactionInvocationSuite);
jsunity.run(transactionCollectionsSuite);
jsunity.run(transactionOperationsSuite);
jsunity.run(transactionBarriersSuite);
jsunity.run(transactionGraphSuite);
jsunity.run(transactionRollbackSuite);
jsunity.run(transactionCountSuite);
jsunity.run(transactionCrossCollectionSuite);

// only run this test suite if server-side failures are enabled
if (internal.debugCanUseFailAt()) {
  jsunity.run(transactionServerFailuresSuite);
}

return jsunity.done();

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "\\(/// @brief\\|/// @addtogroup\\|// --SECTION--\\|/// @page\\|/// @\\}\\)"
// End:
