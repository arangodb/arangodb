/*jshint globalstrict:false, strict:false */
/*global assertEqual, assertTrue, fail */

////////////////////////////////////////////////////////////////////////////////
/// @brief tests for client/server side transaction invocation, RocksDB version
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
////////////////////////////////////////////////////////////////////////////////

function RocksDBTransactionsInvocationsSuite () {
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
/// @brief max transaction size
////////////////////////////////////////////////////////////////////////////////
    
    testSmallMaxTransactionSize : function () {
      try {
        db._executeTransaction({
          collections: { write: cn },
          action: "function (params) { " +
            "var db = require('internal').db; " +
            "var c = db._collection(params.cn); " +
            "for (var i = 0; i < 10000; ++i) { c.insert({ someValue : i }); } " +
            "}",
          params: { cn },
          maxTransactionSize: 100 * 1000, // 100 KB => not enough!
        });
        fail();
      } catch (err) {
        assertEqual(ERRORS.ERROR_RESOURCE_LIMIT.code, err.errorNum);
      }
      
      assertEqual(0, db._collection(cn).count());
    },

    testBigMaxTransactionSize : function () {
      db._executeTransaction({
        collections: { write: cn },
        action: "function (params) { " +
          "var db = require('internal').db; " +
          "var c = db._collection(params.cn); " +
          "for (var i = 0; i < 10000; ++i) { c.insert({ someValue : i }); } " +
          "}",
        params: { cn },
        maxTransactionSize: 10 * 1000 * 1000, // 10 MB => enough!
      });

      assertEqual(10000, db._collection(cn).count());
    },
    
    testIntermediateCommitCountVerySmall : function () {
      db._executeTransaction({
        collections: { write: cn },
        action: "function (params) { " +
          "var db = require('internal').db; " +
          "var c = db._collection(params.cn); " +
          "for (var i = 0; i < 1000; ++i) { c.insert({ someValue : i }); } " +
          "}",
        params: { cn },
        intermediateCommitCount: 1 // this should produce 1000 intermediate commits
      });

      assertEqual(1000, db._collection(cn).count());
    },

    testIntermediateCommitCountBigger : function () {
      db._executeTransaction({
        collections: { write: cn },
        action: "function (params) { " +
          "var db = require('internal').db; " +
          "var c = db._collection(params.cn); " +
          "for (var i = 0; i < 10000; ++i) { c.insert({ someValue : i }); } " +
          "}",
        params: { cn },
        intermediateCommitCount: 1000 // this should produce 10 intermediate commits
      });

      assertEqual(10000, db._collection(cn).count());
    },
    
    testIntermediateCommitCountWithFail : function () {
      var failed = false;

      try {
        db._executeTransaction({
          collections: { write: cn },
          action: "function (params) { " +
            "var db = require('internal').db; " +
            "var c = db._collection(params.cn); " +
            "for (var i = 0; i < 10000; ++i) { c.insert({ someValue : i }); } " +
            "throw 'peng!'; " +
            "}",
          params: { cn },
          intermediateCommitCount: 1000 // this should produce 10 intermediate commits
        });
      } catch (err) {
        failed = true;
      }

      assertTrue(failed);
      assertEqual(10000, db._collection(cn).count());
    },

    testIntermediateCommitCountWithFailInTheMiddle : function () {
      var failed = false;

      try {
        db._executeTransaction({
          collections: { write: cn },
          action: "function (params) { " +
            "var db = require('internal').db; " +
            "var c = db._collection(params.cn); " +
            "for (var i = 0; i < 10000; ++i) { " + 
            "c.insert({ someValue : i }); " + 
            "if (i === 6532) { throw 'peng!'; } " +
            "} " +
            "}",
          params: { cn },
          intermediateCommitCount: 1000 // this should produce 6 intermediate commits
        });
      } catch (err) {
        failed = true;
      }

      assertTrue(failed);
      assertEqual(6000, db._collection(cn).count());
    },


    testIntermediateCommitSizeVerySmall : function () {
      db._executeTransaction({
        collections: { write: cn },
        action: "function (params) { " +
          "var db = require('internal').db; " +
          "var c = db._collection(params.cn); " +
          "for (var i = 0; i < 1000; ++i) { c.insert({ someValue : i }); } " +
          "}",
        params: { cn },
        intermediateCommitSize: 1000 // this should produce a lot of intermediate commits
      });

      assertEqual(1000, db._collection(cn).count());
    },
    
    testIntermediateCommitSizeBigger : function () {
      db._executeTransaction({
        collections: { write: cn },
        action: "function (params) { " +
          "var db = require('internal').db; " +
          "var c = db._collection(params.cn); " +
          "for (var i = 0; i < 10000; ++i) { c.insert({ someValue : i }); } " +
          "}",
        params: { cn },
        intermediateCommitSize: 10000 // this should produce a lot of intermediate commits
      });

      assertEqual(10000, db._collection(cn).count());
    },
    
    testIntermediateCommitSizeWithFail : function () {
      var failed = false;

      try {
        db._executeTransaction({
          collections: { write: cn },
          action: "function (params) { " +
            "var db = require('internal').db; " +
            "var c = db._collection(params.cn); " +
            "for (var i = 0; i < 10000; ++i) { c.insert({ someValue : i }); } " +
            "throw 'peng!'; " +
            "}",
          params: { cn },
          intermediateCommitSize: 10 // this should produce a lot of intermediate commits
        });
      } catch (err) {
        failed = true;
      }

      assertTrue(failed);
      assertEqual(10000, db._collection(cn).count());
    },

    testIntermediateCommitSizeWithFailInTheMiddle : function () {
      var failed = false;

      try {
        db._executeTransaction({
          collections: { write: cn },
          action: "function (params) { " +
            "var db = require('internal').db; " +
            "var c = db._collection(params.cn); " +
            "for (var i = 0; i < 10000; ++i) { " + 
            "c.insert({ someValue : i }); " + 
            "if (i === 6532) { throw 'peng!'; } " +
            "} " +
            "}",
          params: { cn },
          intermediateCommitSize: 10 // this should produce a lot of intermediate commits
        });
      } catch (err) {
        failed = true;
      }

      assertTrue(failed);
      assertEqual(6533, db._collection(cn).count());
    }
  };
}


////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suite
////////////////////////////////////////////////////////////////////////////////

jsunity.run(RocksDBTransactionsInvocationsSuite);

return jsunity.done();
