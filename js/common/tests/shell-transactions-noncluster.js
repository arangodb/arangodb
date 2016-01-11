/*jshint globalstrict:false, strict:false */
/*global assertEqual, fail */

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
var ERRORS = arangodb.errors;
var db = arangodb.db;


////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
/// these tests won't work in the cluster since no cluster transactions are
/// available
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
      c2 = db._create(cn2);
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
/// @brief uses an implicitly declared collection for writing
////////////////////////////////////////////////////////////////////////////////

    testUseForWriting : function () {
      try {
        db._executeTransaction({
          collections: { },
          action: "function (params) { var db = require('internal').db; db._collection(params.cn1).truncate(); }",
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

    testUseReadForWriting : function () {
      try {
        db._executeTransaction({
          collections: { read: cn1 },
          action: "function (params) { var db = require('internal').db; db._collection(params.cn1).truncate(); }",
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

    testUseOtherForWriting : function () {
      try {
        db._executeTransaction({
          collections: { write: cn2 },
          action: "function (params) { var db = require('internal').db; db._collection(params.cn1).truncate(); }",
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

    testUseOtherForWriteAllowImplicit : function () {
      try {
        db._executeTransaction({
          collections: { read: cn1, allowImplicit : true },
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

jsunity.run(TransactionsImplicitCollectionsSuite);

return jsunity.done();

