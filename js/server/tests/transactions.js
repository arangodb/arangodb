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
        { collections: { read: [ ], write: "foo" }, action: function () { } },
        { collections: { write: "foo" }, action: function () { } }
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
        { collections: { read: [ ], write: [ ] }, action: function () { result = 1; return true; } }
      ];

      tests.forEach(function (test) {
        result = 0;
          
        TRANSACTION(test);
        assertEqual(1, result);
      });
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

    testNonExistingCollections : function () {
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

    testValidCollections : function () {
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

    testValidMultipleCollections : function () {
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

    testRedeclareCollection : function () {
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

return jsunity.done();

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "\\(/// @brief\\|/// @addtogroup\\|// --SECTION--\\|/// @page\\|/// @\\}\\)"
// End:
