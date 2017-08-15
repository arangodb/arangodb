/*jshint globalstrict:false, strict:false, maxlen : 200 */
/*global fail, assertTrue, assertEqual, TRANSACTION */

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

var jsunity = require("jsunity");
var internal = require("internal");
var arangodb = require("@arangodb");
var helper = require("@arangodb/aql-helper");
var db = arangodb.db;
var testHelper = require("@arangodb/test-helper").Helper;

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
  var cn = "UnitTestsTransaction";
  var c = null;
  
  return {

    setUp : function () {
      internal.debugClearFailAt();
      db._drop(cn);
      c = db._create(cn);
    },

    tearDown : function () {
      internal.debugClearFailAt();

      if (c !== null) {
        c.drop();
      }

      c = null;
      internal.wait(0);
    },
    
    testInsertUniqueFailing : function () {
      var doc = c.insert({ _key: "test", value: 1 });
      try {
        db._executeTransaction({ 
          collections: { write: c.name() }, 
          action: function() {
            c.insert({ _key: "test", value: 2 }); 
          }
        });
        fail();
      } catch (err) {
      }

      if (db._engine().name === "mmfiles") {
        assertEqual(1, c.figures().revisions.count);
      }
      assertEqual(1, c.count());
      assertEqual(1, c.toArray().length);
      assertEqual(1, c.document("test").value);
    },


    
    testInsertUniqueSingleFailing : function () {
      var doc = c.insert({ _key: "test", value: 1 });
      try {
        c.insert({ _key: "test", value: 2 }); 
        fail();
      } catch (err) {
      }

      if (db._engine().name === "mmfiles") {
        assertEqual(1, c.figures().revisions.count);
      }
      assertEqual(1, c.count());
      assertEqual(1, c.toArray().length);
      assertEqual(1, c.document("test").value);
    },
    
    testInsertTransactionFailing : function () {
      var doc = c.insert({ _key: "test", value: 1 });
      try {
        db._executeTransaction({ 
          collections: { write: c.name() }, 
          action: function() {
            c.insert({ _key: "test2", value: 2 }); 
            throw "foo";
          }
        });
        fail();
      } catch (err) {
      }

      assertEqual(1, c.toArray().length);
      if (db._engine().name === "mmfiles") {
        assertEqual(1, c.figures().revisions.count);
      }
      assertEqual(1, c.document("test").value);
    },
    
    testRemoveTransactionFailing : function () {
      var doc = c.insert({ _key: "test", value: 1 });
      try {
        db._executeTransaction({ 
          collections: { write: c.name() }, 
          action: function() {
            c.remove("test");
            throw "foo";
          }
        });
        fail();
      } catch (err) {
      }

      assertEqual(1, c.toArray().length);
      if (db._engine().name === "mmfiles") {
        assertEqual(1, c.figures().revisions.count);
      }
      assertEqual(1, c.document("test").value);
    },
 
    testRemoveInsertWithSameRev : function () {
      var doc = c.insert({ _key: "test", value: 1 });
      db._executeTransaction({ 
        collections: { write: c.name() }, 
        action: function() {
          c.remove("test"); 
          c.insert({ _key: "test", _rev: doc._rev, value: 2 }, { isRestore: true }); 
        }
      });

      assertEqual(1, c.toArray().length);
      if (db._engine().name === "mmfiles") {
        assertEqual(1, c.figures().revisions.count);
      }
      assertEqual(2, c.document("test").value);
    },
    
    testUpdateWithSameRev : function () {
      var doc = c.insert({ _key: "test", value: 1 });
      c.update("test", { _key: "test", _rev: doc._rev, value: 2 }, { isRestore: true }); 

      assertEqual(1, c.toArray().length);
      if (db._engine().name === "mmfiles") {
        assertEqual(1, c.figures().revisions.count);
      }
      assertEqual(2, c.document("test").value);
    },

    testUpdateWithSameRevTransaction : function () {
      var doc = c.insert({ _key: "test", value: 1 });
      db._executeTransaction({ 
        collections: { write: c.name() }, 
        action: function() {
          c.update("test", { _key: "test", _rev: doc._rev, value: 2 }, { isRestore: true }); 
        }
      });

      assertEqual(1, c.toArray().length);
      if (db._engine().name === "mmfiles") {
        assertEqual(1, c.figures().revisions.count);
      }
      assertEqual(2, c.document("test").value);
    },

    testUpdateFailingWithSameRev : function () {
      var doc = c.insert({ _key: "test", value: 1 });
      try {
        db._executeTransaction({ 
          collections: { write: c.name() }, 
          action: function() {
            c.update("test", { _key: "test", _rev: doc._rev, value: 2 }, { isRestore: true });
            throw "foo"; 
          }
        });
        fail();
      } catch (err) {
      }

      assertEqual(1, c.toArray().length);
      if (db._engine().name === "mmfiles") {
        assertEqual(1, c.figures().revisions.count);
      }
      assertEqual(1, c.document("test").value);
    },
    
    testUpdateFailing : function () {
      var doc = c.insert({ _key: "test", value: 1 });
      try {
        db._executeTransaction({ 
          collections: { write: c.name() }, 
          action: function() {
            c.update({ _key: "test", value: 2 });
            throw "foo"; 
          }
        });
        fail();
      } catch (err) {
      }

      assertEqual(1, c.toArray().length);
      if (db._engine().name === "mmfiles") {
        assertEqual(1, c.figures().revisions.count);
      }
      assertEqual(1, c.document("test").value);
    },

    testUpdateAndInsertFailing : function () {
      var doc = c.insert({ _key: "test", value: 1 });
      try {
        db._executeTransaction({ 
          collections: { write: c.name() }, 
          action: function() {
            c.update({ _key: "test", value: 2 });
            c.insert({ _key: "test", value: 3 });
            throw "foo"; 
          }
        });
        fail();
      } catch (err) {
      }

      assertEqual(1, c.toArray().length);
      if (db._engine().name === "mmfiles") {
        assertEqual(1, c.figures().revisions.count);
      }
      assertEqual(1, c.document("test").value);
    },
    
    testRemoveAndInsert : function () {
      var doc = c.insert({ _key: "test", value: 1 });
      db._executeTransaction({ 
        collections: { write: c.name() }, 
        action: function() {
          c.remove("test");
          c.insert({ _key: "test", value: 2 });
        }
      });

      assertEqual(1, c.toArray().length);
      if (db._engine().name === "mmfiles") {
        assertEqual(1, c.figures().revisions.count);
      }
      assertEqual(2, c.document("test").value);
    },

    testRemoveAndInsertFailing : function () {
      var doc = c.insert({ _key: "test", value: 1 });
      try {
        db._executeTransaction({ 
          collections: { write: c.name() }, 
          action: function() {
            c.remove("test");
            c.insert({ _key: "test", value: 3 });
            throw "foo"; 
          }
        });
        fail();
      } catch (err) {
      }

      assertEqual(1, c.toArray().length);
      if (db._engine().name === "mmfiles") {
        assertEqual(1, c.figures().revisions.count);
      }
      assertEqual(1, c.document("test").value);
    }
  
  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////

function transactionInvocationSuite () {
  'use strict';
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
        { collections: { read: true }, action: function () { }, },
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

      let localDebug=false;
      tests.forEach(function (test) {
        if(localDebug) {
          require("internal").print(test);
        }
        try {
          TRANSACTION(test);
          if(localDebug) {
            require("internal").print("no exception failing");
          }
          fail();
        }
        catch (err) {
          var expected = internal.errors.ERROR_BAD_PARAMETER.code;
          if(test && test.hasOwnProperty("exErr")) {
              expected = test.exErr;
          }
          if(localDebug) {
            require("internal").print("exp: " + expected + " real: " + err.errorNum);
          }
          assertEqual(expected, err.errorNum);
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
      var tests = [
        { expected: 1, trx: { collections: { }, action: function () { return 1; } } },
        { expected: undefined, trx: { collections: { }, action: function () { } } },
        { expected: [ ], trx: { collections: { read: [ ] }, action: function () { return [ ]; } } },
        { expected: [ null, true, false ], trx: { collections: { write: [ ] }, action: function () { return [ null, true, false ]; } } },
        { expected: "foo", trx: { collections: { read: [ ], write: [ ] }, action: function () { return "foo"; } } },
        { expected: { "a" : 1, "b" : 2 }, trx: { collections: { read: [ ], write: [ ] }, action: function () { return { "a" : 1, "b" : 2 }; } } }
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
/// @brief test: nesting
////////////////////////////////////////////////////////////////////////////////

    testNestingEmbedFlag : function () {
      var obj = {
        collections : {
        },
        action : function () {
          return 19 + TRANSACTION({
            collections: {
            },
            embed: true,
            action: "function () { return 23; }"
          });
        }
      };

      assertEqual(42, TRANSACTION(obj));
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


////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////

function transactionCollectionsSuite () {
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
      internal.wait(0);
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
      var getQueryResults = helper.getQueryResults;
      
      var obj = {
        collections : { 
        },
        action : function () {
          result = getQueryResults("FOR i IN [ 1, 2, 3 ] RETURN i");
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
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: trx with embedded AQL
////////////////////////////////////////////////////////////////////////////////

    testAqlRead : function () {
      var i = 0;
      for (i = 0; i < 10; ++i) {
        c1.save({ _key : "test" + i });
      }

      var obj = {
        collections : { 
          read : [ cn1 ]
        },
        action : function () {
          var docs = db._query("FOR i IN @@cn1 RETURN i", { "@cn1" : cn1 }).toArray();
          assertEqual(10, docs.length);
          return true;
        }
      };

      assertTrue(TRANSACTION(obj));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: trx with embedded AQL
////////////////////////////////////////////////////////////////////////////////

    testAqlReadMulti : function () {
      var i = 0;
      for (i = 0; i < 10; ++i) {
        c1.save({ _key : "test" + i });
        c2.save({ _key : "test" + i });
      }

      var obj = {
        collections : { 
          read : [ cn1, cn2 ]
        },
        action : function () {
          var docs = db._query("FOR i IN @@cn1 FOR j IN @@cn2 RETURN i", { "@cn1" : cn1, "@cn2" : cn2 }).toArray();
          assertEqual(100, docs.length);
          return true;
        }
      };

      assertTrue(TRANSACTION(obj));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: trx with embedded AQL
////////////////////////////////////////////////////////////////////////////////

    testAqlReadMultiUndeclared : function () {
      var i = 0;
      for (i = 0; i < 10; ++i) {
        c1.save({ _key : "test" + i });
        c2.save({ _key : "test" + i });
      }

      var obj = {
        collections : {
          // intentionally empty 
        },
        action : function () {
          var docs = db._query("FOR i IN @@cn1 FOR j IN @@cn2 RETURN i", { "@cn1" : cn1, "@cn2" : cn2 }).toArray();
          assertEqual(100, docs.length);
          return true;
        }
      };

      assertTrue(TRANSACTION(obj));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: trx with embedded AQL
////////////////////////////////////////////////////////////////////////////////

    testAqlWrite : function () {
      var i = 0;
      for (i = 0; i < 10; ++i) {
        c1.save({ _key : "test" + i });
      }

      var obj = {
        collections : {
          write: [ cn1 ]
        },
        action : function () {
          var ops = db._query("FOR i IN @@cn1 REMOVE i._key IN @@cn1", { "@cn1" : cn1 }).getExtra().stats;
          assertEqual(10, ops.writesExecuted);
          assertEqual(0, c1.count());
          return true;
        }
      };

      assertTrue(TRANSACTION(obj));
      assertEqual(0, c1.count());
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: trx with embedded AQL
////////////////////////////////////////////////////////////////////////////////

    testAqlReadWrite : function () {
      var i = 0;
      for (i = 0; i < 10; ++i) {
        c1.save({ _key : "test" + i });
        c2.save({ _key : "test" + i });
      }

      var obj = {
        collections : {
          read: [ cn1 ],
          write: [ cn2 ]
        },
        action : function () {
          var ops = db._query("FOR i IN @@cn1 REMOVE i._key IN @@cn2", { "@cn1" : cn1, "@cn2" : cn2 }).getExtra().stats;
          assertEqual(10, ops.writesExecuted);
          return true;
        }
      };

      assertTrue(TRANSACTION(obj));
      
      assertEqual(10, c1.count());
      assertEqual(0, c2.count());
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: trx with embedded AQL
////////////////////////////////////////////////////////////////////////////////

    testAqlWriteUndeclared : function () {
      var i = 0;
      for (i = 0; i < 10; ++i) {
        c1.save({ _key : "test" + i });
        c2.save({ _key : "test" + i });
      }

      var obj = {
        collections : {
          read: [ cn1 ]
        },
        action : function () {
          try{
            db._query("FOR i IN @@cn1 REMOVE i._key IN @@cn2", { "@cn1" : cn1, "@cn2" : cn2 });
            fail();
          }
          catch (err) {
            assertEqual(arangodb.errors.ERROR_TRANSACTION_UNREGISTERED_COLLECTION.code, err.errorNum);
          }

          assertEqual(10, c1.count());
          return true;
        }
      };

      assertTrue(TRANSACTION(obj));
      
      assertEqual(10, c1.count());
      assertEqual(10, c2.count());
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: trx with embedded AQL
////////////////////////////////////////////////////////////////////////////////

    testAqlMultiWrite : function () {
      var i = 0;
      for (i = 0; i < 10; ++i) {
        c1.save({ _key : "test" + i });
        c2.save({ _key : "test" + i });
      }

      var obj = {
        collections : {
          write: [ cn1, cn2 ]
        },
        action : function () {
          var ops;
          ops = db._query("FOR i IN @@cn1 REMOVE i._key IN @@cn1", { "@cn1" : cn1 }).getExtra().stats;
          assertEqual(10, ops.writesExecuted);
          assertEqual(0, c1.count());

          ops = db._query("FOR i IN @@cn2 REMOVE i._key IN @@cn2", { "@cn2" : cn2 }).getExtra().stats;
          assertEqual(10, ops.writesExecuted);
          assertEqual(0, c2.count());
          return true;
        }
      };
          
      assertTrue(TRANSACTION(obj));
      
      assertEqual(0, c1.count());
      assertEqual(0, c2.count());
    }

  };
}


////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////

function transactionOperationsSuite () {
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
      internal.wait(0);
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
          fail();
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
          fail();
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
          fail();
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
          fail();
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
          fail();
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
          fail();
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
          fail();
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
          fail();
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
      if (db._engine().name === "rocksdb") {
        return;
      }
      
      c1 = db._create(cn1);

      var obj = {
        collections : {
        },
        action : function () {
          c1.ensureGeoIndex("foo", "bar");
          fail();
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
      if (db._engine().name === "rocksdb") {
        return;
      }
      
      c1 = db._create(cn1);

      var obj = {
        collections : {
        },
        action : function () {
          c1.ensureGeoConstraint("foo", "bar", true);
          fail();
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
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: trx with byExample operation
////////////////////////////////////////////////////////////////////////////////

    testByExample : function () {
      c1 = db._create(cn1);
      c1.ensureUniqueConstraint("name");

      for (var i = 0; i < 100; ++i) {
        c1.save({ name: "test" + i });
      }

      var obj = {
        collections : {
          write: [ cn1 ]
        },
        action : function () {
          var r = c1.byExample({ name: "test99" }).toArray();
          assertEqual(r.length, 1);
          assertEqual("test99", r[0].name);
        }
      };

      TRANSACTION(obj);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: trx with firstExample operation
////////////////////////////////////////////////////////////////////////////////

    testFirstExample1 : function () {
      c1 = db._create(cn1);
      c1.ensureUniqueConstraint("name");

      for (var i = 0; i < 100; ++i) {
        c1.save({ name: "test" + i });
      }

      var obj = {
        collections : {
          write: [ cn1 ]
        },
        action : function () {
          var r = c1.firstExample({ name: "test99" });
          assertEqual("test99", r.name);
        }
      };

      TRANSACTION(obj);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: trx with firstExample operation
////////////////////////////////////////////////////////////////////////////////

    testFirstExample2 : function () {
      c1 = db._create(cn1);
      c1.ensureHashIndex("name");

      for (var i = 0; i < 100; ++i) {
        c1.save({ name: "test" + i });
      }

      var obj = {
        collections : {
          write: [ cn1 ]
        },
        action : function () {
          var r = c1.firstExample({ name: "test99" });
          assertEqual("test99", r.name);
        }
      };

      TRANSACTION(obj);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: trx with firstExample operation
////////////////////////////////////////////////////////////////////////////////

    testFirstExample3 : function () {
      c1 = db._create(cn1);
      c1.ensureUniqueSkiplist("name");

      for (var i = 0; i < 100; ++i) {
        c1.save({ name: "test" + i });
      }

      var obj = {
        collections : {
          write: [ cn1 ]
        },
        action : function () {
          var r = c1.firstExample({ name: "test99" });
          assertEqual("test99", r.name);
        }
      };

      TRANSACTION(obj);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: trx with firstExample operation
////////////////////////////////////////////////////////////////////////////////

    testFirstExample4 : function () {
      c1 = db._create(cn1);
      c1.ensureSkiplist("name");

      for (var i = 0; i < 100; ++i) {
        c1.save({ name: "test" + i });
      }

      var obj = {
        collections : {
          write: [ cn1 ]
        },
        action : function () {
          var r = c1.firstExample({ name: "test99" });
          assertEqual("test99", r.name);
        }
      };

      TRANSACTION(obj);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: trx with fulltext operation
////////////////////////////////////////////////////////////////////////////////

    testFulltext : function () {
      
      c1 = db._create(cn1);
      var idx = c1.ensureFulltextIndex("text");

      c1.save({ text: "steam", other: 1 });
      c1.save({ text: "steamboot", other: 2 });

      var obj = {
        collections : {
          write: [ cn1 ]
        },
        action : function () {
          var r = c1.fulltext("text", "prefix:steam", idx).toArray();
          assertEqual(2, r.length);
          
          r = c1.fulltext("text", "steam", idx).toArray();
          assertEqual(1, r.length);
        }
      };

      TRANSACTION(obj);
    }

  };
}


////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////

function transactionBarriersSuite () {
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
      internal.wait(0);
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


////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////

function transactionGraphSuite () {
  'use strict';
  var cn1 = "UnitTestsVertices";
  var cn2 = "UnitTestsEdges";

  var G = require('@arangodb/general-graph');

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

////////////////////////////////////////////////////////////////////////////////
/// @brief test: rollback updates in a graph transaction
////////////////////////////////////////////////////////////////////////////////

    testRollbackGraphUpdates : function () {
      try {
        G._drop('UnitTestsGraph'); 
      }
      catch (err) {
      }

      var graph = G._create('UnitTestsGraph', [G._relation(cn2, cn1, cn1)]); 
      var gotHere = 0;
      
      assertEqual(0, db[cn1].count());

      var obj = {
        collections: { 
          write: [ cn1, cn2 ]
        }, 
        action : function () {
          var result = { };
          result.enxirvp = graph[cn1].save({});
          result.biitqtk = graph[cn1].save({});
          result.oboyuhh = graph[cn2].save({_from: result.enxirvp._id, _to: result.biitqtk._id, name: "john smith" });
          result.cvwmkym = db[cn1].replace(result.enxirvp._id, { _rev : null });
          result.gsalfxu = db[cn1].replace(result.biitqtk._id, { _rev : null });
          result.xsjzbst = (function (){
            graph[cn2].remove(result.oboyuhh._id); 
            return true;
          }());

          result.thizhdd = graph[cn2].save({_from: result.cvwmkym._id, _to: result.gsalfxu._id, _key: result.oboyuhh._key, name: "david smith" });
          gotHere = 1;

          result.rldfnre = graph[cn2].save({_from: result.cvwmkym._id, _to: result.gsalfxu._id, _key: result.oboyuhh._key, name: "david smith" });
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
      try {
        G._drop('UnitTestsGraph'); 
      }
      catch (err) {
      }

      var graph = G._create('UnitTestsGraph', [G._relation(cn2, cn1, cn1)]); 
 
      var obj = {
        collections: { 
          write: [ cn1, cn2 ]
        }, 
        action : function () {
          var result = { };

          result.enxirvp = graph[cn1].save({});
          result.biitqtk = graph[cn1].save({});
          result.oboyuhh = graph[cn2].save({_from: result.enxirvp._id, _to: result.biitqtk._id, name: "john smith" });
          result.oboyuhh = graph[cn2].document(result.oboyuhh);
          result.cvwmkym = db[cn1].replace(result.enxirvp._id, { _rev : null });
          result.gsalfxu = db[cn1].replace(result.biitqtk._id, { _rev : null });
          result.xsjzbst = (function (){
            graph[cn2].remove(result.oboyuhh._id); 
            return true;
          }());

          graph[cn2].save({_from: result.cvwmkym._id, _to: result.gsalfxu._id, _key: result.oboyuhh._key, name: "david smith" });
          result.rldfnre = graph[cn2].document(result.oboyuhh._key);

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
      assertEqual("david smith", result.rldfnre.name);
      assertEqual(undefined, result.rldfnre.$label);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: usage of read-copied documents inside write-transaction
////////////////////////////////////////////////////////////////////////////////

    testReadWriteDocumentsList : function () {
      c1.save({ _key: "bar" });
      c1.save({ _key: "baz" });
      c2.save(cn1 + "/bar", cn1 + "/baz", { type: "one" });
      c2.save(cn1 + "/baz", cn1 + "/bar", { type: "two" });

      var obj = {
        collections: { 
          write: [ cn2 ]
        }, 
        action : function () {
          var result = [ ];

          result.push(c2.inEdges(cn1 + "/baz"));
          result.push(c2.inEdges(cn1 + "/bar"));

          c2.save(cn1 + "/foo", cn1 + "/baz", { type: "three" });
          c2.save(cn1 + "/foo", cn1 + "/bar", { type: "four" });

          result.push(c2.inEdges(cn1 + "/baz"));
          result.push(c2.inEdges(cn1 + "/bar"));
          
          return result;
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

      var result = TRANSACTION(obj);

      assertEqual(4, result.length);
      var r = result[0];
      assertEqual(1, r.length);
      assertEqual(cn1 + "/bar", r[0]._from);
      assertEqual(cn1 + "/baz", r[0]._to);
      assertEqual("one", r[0].type);
      
      r = result[1];
      assertEqual(1, r.length);
      assertEqual(cn1 + "/baz", r[0]._from);
      assertEqual(cn1 + "/bar", r[0]._to);
      assertEqual("two", r[0].type);
      
      r = result[2];
      r.sort(sorter);

      assertEqual(2, r.length);
      assertEqual(cn1 + "/bar", r[0]._from);
      assertEqual(cn1 + "/baz", r[0]._to);
      assertEqual("one", r[0].type);
      
      assertEqual(cn1 + "/foo", r[1]._from);
      assertEqual(cn1 + "/baz", r[1]._to);
      assertEqual("three", r[1].type);

      r = result[3];
      r.sort(sorter);
      
      assertEqual(cn1 + "/foo", r[0]._from);
      assertEqual(cn1 + "/bar", r[0]._to);
      assertEqual("four", r[0].type);

      assertEqual(2, r.length);
      assertEqual(cn1 + "/baz", r[1]._from);
      assertEqual(cn1 + "/bar", r[1]._to);
      assertEqual("two", r[1].type);
    }

  };
}


////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////

function transactionRollbackSuite () {
  'use strict';
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
      internal.wait(0);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: rollback after flush
////////////////////////////////////////////////////////////////////////////////

    testRollbackAfterFlush : function () {
      c1 = db._create(cn1);

      var obj = {
        collections : {
          write: [ cn1 ]
        },
        action : function () {
          c1.save({ _key: "tom" });
          c1.save({ _key: "tim" });
          c1.save({ _key: "tam" });
      
          internal.wal.flush(true);
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

      assertEqual(0, c1.count());
      
      testHelper.waitUnload(c1);
      
      assertEqual(0, c1.count());
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
          var _r = r;

          c1.save({ _key: "tom" });
          assertEqual(1, compareStringIds(c1.revision(), _r));
          _r = c1.revision();

          c1.save({ _key: "tim" });
          assertEqual(1, compareStringIds(c1.revision(), _r));
          _r = c1.revision();

          c1.save({ _key: "tam" });
          assertEqual(1, compareStringIds(c1.revision(), _r));
          _r = c1.revision();
          
          c1.remove("tam");
          assertEqual(1, compareStringIds(c1.revision(), _r));
          _r = c1.revision();
          
          c1.update("tom", { "bom" : true });
          assertEqual(1, compareStringIds(c1.revision(), _r));
          _r = c1.revision();
          
          c1.remove("tom");
          assertEqual(1, compareStringIds(c1.revision(), _r));
          //_r = c1.revision();
        
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
/// @brief test: rollback inserts w/ secondary indexes
////////////////////////////////////////////////////////////////////////////////

    testRollbackInsertSecondaryIndexes : function () {
      c1 = db._create(cn1);
      c1.save({ _key: "foo", value: "foo", a: 1 });
      c1.save({ _key: "bar", value: "bar", a: 1 });
      c1.save({ _key: "meow", value: "meow" });

      c1.ensureHashIndex("value");
      c1.ensureSkiplist("value");
      var good = false;
     
      var obj = {
        collections : {
          write: [ cn1 ]
        },
        action : function () {
          c1.save({ _key: "tom", value: "tom" });
          c1.save({ _key: "tim", value: "tim" });
          c1.save({ _key: "tam", value: "tam" });
          c1.save({ _key: "troet", value: "foo", a: 2 });
          c1.save({ _key: "floxx", value: "bar", a: 2 });
      
          assertEqual(8, c1.count());

          good = true;
          throw "rollback";
        }
      };

      try {
        TRANSACTION(obj);
        fail();
      }
      catch (err) {
      }
      assertEqual(true, good);

      assertEqual(3, c1.count());
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: rollback inserts
////////////////////////////////////////////////////////////////////////////////

    testRollbackInsertUpdate : function () {
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

          c1.update("tom", { });
          c1.update("tim", { });
          c1.update("tam", { });
          c1.update("bar", { });
          c1.remove("foo");
          c1.remove("bar");
          c1.remove("meow");
          c1.remove("tom");

          assertEqual(2, c1.count());
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
/// @brief test: rollback remove w/ secondary indexes
////////////////////////////////////////////////////////////////////////////////

    testRollbackUpdateSecondaryIndexes : function () {
      c1 = db._create(cn1);
      c1.save({ _key: "foo", value: "foo", a: 1 });
      c1.save({ _key: "bar", value: "bar", a: 1 });
      c1.save({ _key: "meow", value: "meow" });

      c1.ensureHashIndex("value");
      c1.ensureSkiplist("value");
      var good = false;
     
      var obj = {
        collections : {
          write: [ cn1 ]
        },
        action : function () {
          c1.update("foo", { value: "foo", a: 2 });
          c1.update("bar", { value: "bar", a: 2 });
          c1.update("meow", { value: "troet" });

          assertEqual(3, c1.count());

          good = true;
          throw "rollback";
        }
      };

      try {
        TRANSACTION(obj);
        fail();
      }
      catch (err) {
      }

      assertEqual(true, good);
      assertEqual(3, c1.count());
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
/// @brief test: rollback remove w/ secondary indexes
////////////////////////////////////////////////////////////////////////////////

    testRollbackRemoveSecondaryIndexes : function () {
      c1 = db._create(cn1);
      c1.save({ _key: "foo", value: "foo", a: 1 });
      c1.save({ _key: "bar", value: "bar", a: 1 });
      c1.save({ _key: "meow", value: "meow" });

      c1.ensureHashIndex("value");
      c1.ensureSkiplist("value");
      var good = false;
     
      var obj = {
        collections : {
          write: [ cn1 ]
        },
        action : function () {
          c1.remove("meow");
          c1.remove("bar");
          c1.remove("foo");
      
          assertEqual(0, c1.count());

          good = true;

          throw "rollback";
        }
      };

      try {
        TRANSACTION(obj);
        fail();
      }
      catch (err) {
      }

      assertEqual(true, good);
      assertEqual(3, c1.count());
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: rollback insert/remove w/ secondary indexes
////////////////////////////////////////////////////////////////////////////////

    testRollbackRemoveInsertSecondaryIndexes : function () {
      c1 = db._create(cn1);
      c1.save({ _key: "foo", value: "foo", a: 1 });
      c1.save({ _key: "bar", value: "bar", a: 1 });
      c1.save({ _key: "meow", value: "meow" });

      c1.ensureHashIndex("value");
      c1.ensureSkiplist("value");
      var good = false;
     
      var obj = {
        collections : {
          write: [ cn1 ]
        },
        action : function () {
          c1.remove("meow");
          c1.remove("bar");
          c1.remove("foo");
          assertEqual(0, c1.count());

          c1.save({ _key: "foo2", value: "foo", a: 2 });
          c1.save({ _key: "bar2", value: "bar", a: 2 });
          assertEqual(2, c1.count());

          good = true;

          throw "rollback";
        }
      };

      try {
        TRANSACTION(obj);
        fail();
      }
      catch (err) {
      }

      assertEqual(true, good);
      assertEqual(3, c1.count());
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
          fail();
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
          c1.save({ name: "bar" });
          c1.save({ name: "baz" });
          c1.save({ name: "foo" });
          fail();
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
/// @brief test: rollback a mixed workload
////////////////////////////////////////////////////////////////////////////////

    testRollbackMixed1 : function () {
      c1 = db._create(cn1);
      
      var i;

      for (i = 0; i < 100; ++i) {
        c1.save({ _key: "key" + i, value: i });
      }

      var obj = {
        collections : {
          write: [ cn1 ]
        },
        action : function () {

          for (i = 0; i < 50; ++i) {
            c1.remove("key" + i);
          }

          for (i = 50; i < 100; ++i) {
            c1.update("key" + i, { value: i - 50 });
          }

          c1.remove("key50");
          throw "doh!";
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
/// @brief test: rollback a mixed workload
////////////////////////////////////////////////////////////////////////////////

    testRollbackMixed2 : function () {
      c1 = db._create(cn1);
      
      c1.save({ _key: "foo" });
      c1.save({ _key: "bar" });

      var obj = {
        collections : {
          write: [ cn1 ]
        },
        action : function () {
          var i;

          for (i = 0; i < 10; ++i) {
            c1.save({ _key: "key" + i, value: i });
          }

          for (i = 0; i < 5; ++i) {
            c1.remove("key" + i);
          }

          for (i = 5; i < 10; ++i) {
            c1.update("key" + i, { value: i - 5 });
          }

          c1.remove("key5");
          throw "doh!";
        }
      };

      try {
        TRANSACTION(obj);
        fail();
      }
      catch (err) {
      }
      
      assertEqual(2, c1.count());
      assertEqual("foo", c1.document("foo")._key);
      assertEqual("bar", c1.document("bar")._key);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: rollback a mixed workload
////////////////////////////////////////////////////////////////////////////////

    testRollbackMixed3 : function () {
      c1 = db._create(cn1);
      
      c1.save({ _key: "foo" });
      c1.save({ _key: "bar" });

      var obj = {
        collections : {
          write: [ cn1 ]
        },
        action : function () {
          var i;

          for (i = 0; i < 10; ++i) {
            c1.save({ _key: "key" + i, value: i });
          }

          for (i = 0; i < 10; ++i) {
            c1.remove("key" + i);
          }
          
          for (i = 0; i < 10; ++i) {
            c1.save({ _key: "key" + i, value: i });
          }

          for (i = 0; i < 10; ++i) {
            c1.update("key" + i, { value: i - 5 });
          }
          
          for (i = 0; i < 10; ++i) {
            c1.update("key" + i, { value: i + 5 });
          }
          
          for (i = 0; i < 10; ++i) {
            c1.remove("key" + i);
          }
          
          for (i = 0; i < 10; ++i) {
            c1.save({ _key: "key" + i, value: i });
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
      
      assertEqual(2, c1.count());
      assertEqual("foo", c1.document("foo")._key);
      assertEqual("bar", c1.document("bar")._key);
    }

  };
}


////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////

function transactionCountSuite () {
  'use strict';
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
      internal.wait(0);
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

    testCountCommitAfterFlush : function () {
      c1 = db._create(cn1);
      c1.save({ _key: "foo" });
      c1.save({ _key: "bar" });
      assertEqual(2, c1.count());

      var obj = {
        collections : {
          write: [ cn1 ]
        },
        action : function () {
          c1.save({ _key: "baz" });
          assertEqual(3, c1.count());
          
          internal.wal.flush(true, false);

          c1.save({ _key: "meow" });
          assertEqual(4, c1.count());
          
          internal.wal.flush(true, false);

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


////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////

function transactionCrossCollectionSuite () {
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
      internal.wait(0);
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
 
      testHelper.waitUnload(c1);
      testHelper.waitUnload(c2);
      
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

      testHelper.waitUnload(c1);
      testHelper.waitUnload(c2);

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
      testHelper.waitUnload(c1);
      testHelper.waitUnload(c2);

      assertEqual(1, c1.count());
      assertEqual(1, c2.count());
      
      assertEqual([ "a1" ], sortedKeys(c1));
      assertEqual([ "b1" ], sortedKeys(c2));
      assertEqual(1, c2.document("b1").a);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: unload / reload after failed transactions
////////////////////////////////////////////////////////////////////////////////

    testUnloadReloadFailedTrx : function () {
      c1 = db._create(cn1);

      var i;
      for (i = 0; i < 10; ++i) {
        c1.save({ _key: "a" + i, a: i });
      }
      
      var obj = {
        collections : {
          write: [ cn1 ],
        },
        action : function () {
          var i;
          for (i = 0; i < 100; ++i) {
            c1.save({ _key: "test" + i });
          }
        
          throw "rollback";  
        }
      };
      
      for (i = 0; i < 50; ++i) {
        try {
          TRANSACTION(obj);
          fail();
        }
        catch (err) {
        }
      }

      assertEqual(10, c1.count());
 
      testHelper.waitUnload(c1);
      
      assertEqual(10, c1.count());
    }

  };
}


////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////

function transactionConstraintsSuite () {
  'use strict';
  var cn = "UnitTestsTransaction";

  var c = null;
  
  return {

////////////////////////////////////////////////////////////////////////////////
/// @brief set up
////////////////////////////////////////////////////////////////////////////////

    setUp : function () {
      db._drop(cn);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief tear down
////////////////////////////////////////////////////////////////////////////////

    tearDown : function () {
      if (c !== null) {
        c.drop();
      }

      c = null;
      internal.wait(0);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: rollback in case of a server-side fail
////////////////////////////////////////////////////////////////////////////////

    testMultiHashConstraintInsert1 : function () {
      c = db._create(cn);
      c.ensureUniqueConstraint("value1");
      c.ensureUniqueConstraint("value2");

      var i;
      for (i = 0; i < 10; ++i) {
        c.save({ _key: "test" + i, value1: i, value2: i });
      }
      assertEqual(10, c.count());

      try {
        c.save({ value1: 9, value2: 17 });
        fail();
      }
      catch (err) {
        assertEqual(internal.errors.ERROR_ARANGO_UNIQUE_CONSTRAINT_VIOLATED.code, err.errorNum);
      }

      assertEqual(10, c.count());
      assertEqual(9, c.document("test9").value1);
      assertEqual(9, c.document("test9").value2);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: rollback in case of a server-side fail
////////////////////////////////////////////////////////////////////////////////

    testMultiHashConstraintInsert2 : function () {
      c = db._create(cn);
      c.ensureUniqueConstraint("value1");
      c.ensureUniqueConstraint("value2");

      var i;
      for (i = 0; i < 10; ++i) {
        c.save({ _key: "test" + i, value1: i, value2: i });
      }
      assertEqual(10, c.count());

      try {
        c.save({ value1: 17, value2: 9 });
        fail();
      }
      catch (err) {
        assertEqual(internal.errors.ERROR_ARANGO_UNIQUE_CONSTRAINT_VIOLATED.code, err.errorNum);
      }

      assertEqual(10, c.count());
      assertEqual(9, c.document("test9").value1);
      assertEqual(9, c.document("test9").value2);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: rollback in case of a server-side fail
////////////////////////////////////////////////////////////////////////////////

    testMultiSkipConstraintInsert1 : function () {
      c = db._create(cn);
      c.ensureUniqueSkiplist("value1");
      c.ensureUniqueSkiplist("value2");

      var i;
      for (i = 0; i < 10; ++i) {
        c.save({ _key: "test" + i, value1: i, value2: i });
      }
      assertEqual(10, c.count());

      try {
        c.save({ value1: 9, value2: 17 });
        fail();
      }
      catch (err) {
        assertEqual(internal.errors.ERROR_ARANGO_UNIQUE_CONSTRAINT_VIOLATED.code, err.errorNum);
      }

      assertEqual(10, c.count());
      assertEqual(9, c.document("test9").value1);
      assertEqual(9, c.document("test9").value2);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: rollback in case of a server-side fail
////////////////////////////////////////////////////////////////////////////////

    testMultiSkipConstraintInsert2 : function () {
      c = db._create(cn);
      c.ensureUniqueSkiplist("value1");
      c.ensureUniqueSkiplist("value2");

      var i;
      for (i = 0; i < 10; ++i) {
        c.save({ _key: "test" + i, value1: i, value2: i });
      }
      assertEqual(10, c.count());

      try {
        c.save({ value1: 17, value2: 9 });
        fail();
      }
      catch (err) {
        assertEqual(internal.errors.ERROR_ARANGO_UNIQUE_CONSTRAINT_VIOLATED.code, err.errorNum);
      }

      assertEqual(10, c.count());
      assertEqual(9, c.document("test9").value1);
      assertEqual(9, c.document("test9").value2);
    }

  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////

function transactionTraversalSuite () {
  'use strict';
  var cn = "UnitTestsTransaction";
  
  return {

////////////////////////////////////////////////////////////////////////////////
/// @brief set up
////////////////////////////////////////////////////////////////////////////////

    setUp : function () {
      db._drop(cn + "Vertex");
      db._drop(cn + "Edge");
      db._create(cn + "Vertex");
      db._createEdgeCollection(cn + "Edge");

      var i;
      for (i = 0; i < 100; ++i) {
        db.UnitTestsTransactionVertex.insert({ _key: String(i) });
      }

      for (i = 1; i < 100; ++i) {
        db.UnitTestsTransactionEdge.insert(cn + "Vertex/" + i, cn + "Vertex/" + (i + 1), { });
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief tear down
////////////////////////////////////////////////////////////////////////////////

    tearDown : function () {
      db._drop(cn + "Vertex");
      db._drop(cn + "Edge");
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: use of undeclared traversal collection in transaction
////////////////////////////////////////////////////////////////////////////////

    testUndeclaredTraversalCollection : function () {
      var result = db._executeTransaction({
        collections: {
          read: [ cn + "Edge" ],
          write: [ cn + "Edge" ]
        },
        action: function() {
          var db = require("internal").db;

          var results = db._query("FOR v, e IN ANY '" + cn + "Vertex/20' " + cn + "Edge FILTER v._id == '" + cn + "Vertex/21' LIMIT 1 RETURN e").toArray();

          if (results.length > 0) {
            var result = results[0];
            db[cn + "Edge"].remove(result);
            return 1;
          }
          return 0;
        }
      });

      assertEqual(1, result);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: use of undeclared traversal collection in transaction
////////////////////////////////////////////////////////////////////////////////

    testTestCount : function () {
      for (var i = 0; i < 100; ++i) {
        db[cn + "Edge"].insert(cn + "Edge/test" + (i % 21), cn + "Edge/test" + (i % 7), { });
      }

      var result = db._executeTransaction({
        collections: {
          read: [ cn + "Edge" ],
          write: [ cn + "Edge" ]
        },
        action: function() {
          var db = require("internal").db;
          var from = cn + "Edge/test1";
          var to = cn + "Edge/test8";
    
          var newDoc = db[cn + "Edge"].insert(from, to, { request: true });
          var fromCount1 = db[cn + "Edge"].byExample({ _from: from, request: false }).count();

          newDoc.request = false;
          db[cn + "Edge"].update({ _id: newDoc._id }, newDoc);

          var fromCount2 = db[cn + "Edge"].byExample({ _from: from, request: false }).count();
          return [ fromCount1, fromCount2 ];
        }
      });

      assertEqual(result[0] + 1, result[1]);
    }

  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suites
////////////////////////////////////////////////////////////////////////////////

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

