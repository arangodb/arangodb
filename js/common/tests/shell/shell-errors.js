/*jshint globalstrict:false, strict:false */
/*global fail, assertFalse, assertTrue, assertEqual */

////////////////////////////////////////////////////////////////////////////////
/// @brief test the error codes
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
var ArangoError = require("@arangodb").ArangoError;
var ERRORS = arangodb.errors;


////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////

function ErrorsSuite () {
  'use strict';
  var throwError = function (code, message) {
    var err = new ArangoError();
    err.errorNum = code; 
    err.errorMessage = message;
    throw err;
  };

  return {

    setUp : function () {
    },
    
    tearDown : function () {
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test built-in error
////////////////////////////////////////////////////////////////////////////////

    testBuiltIn : function () {
      var testthrow = false;
      try {
        throw "foo";
      }
      catch (err) {
        assertFalse(err instanceof ArangoError);
        assertEqual("string", typeof err);
        assertEqual("foo", err);
        testthrow = true;  
      }
      assertTrue(testthrow);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test built-in error
////////////////////////////////////////////////////////////////////////////////

    testTypeErrorBuiltIn : function () {
      var testthrow = false;
      try {
        throw new TypeError("invalid type!");
      }
      catch (err) {
        assertFalse(err instanceof ArangoError);
        assertEqual("invalid type!", err.message);
        testthrow = true;  
      }
      assertTrue(testthrow);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test struct
////////////////////////////////////////////////////////////////////////////////

    testArangoErrorStruct : function () {
      try {
        throwError(1, "the foxx");
        fail();
      }
      catch (err) {
        assertTrue(err.hasOwnProperty("errorNum"));
        assertTrue(err.hasOwnProperty("errorMessage"));
        assertTrue(err.hasOwnProperty("error"));
        
        assertEqual("number", typeof err.errorNum);
        assertEqual("string", typeof err.errorMessage);
        assertEqual("boolean", typeof err.error);

        assertEqual(1, err.errorNum);
        assertEqual("the foxx", err.errorMessage);
        assertTrue(err.error);

        assertTrue(err instanceof ArangoError);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test a custom code
////////////////////////////////////////////////////////////////////////////////

    testArangoErrorCustom : function () {
      try {
        throwError(12345, "myerrormessage");
        fail();
      }
      catch (err) {
        assertEqual(12345, err.errorNum);
        assertEqual("myerrormessage", err.errorMessage);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test a custom message
////////////////////////////////////////////////////////////////////////////////

    testArangoErrorMessage : function () {
      try {
        throwError(ERRORS.ERROR_ARANGO_DOCUMENT_TYPE_INVALID.code, ERRORS.ERROR_ARANGO_DOCUMENT_TYPE_INVALID.message);
        fail();
      }
      catch (err) {
        assertEqual(ERRORS.ERROR_ARANGO_DOCUMENT_TYPE_INVALID.code, err.errorNum);
        assertEqual(ERRORS.ERROR_ARANGO_DOCUMENT_TYPE_INVALID.message, err.errorMessage);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test to string
////////////////////////////////////////////////////////////////////////////////

    testArangoToString1 : function () {
      var e = ERRORS.ERROR_ARANGO_DOCUMENT_TYPE_INVALID;
      
      try {
        throwError(e.code, e.message);
        fail();
      }
      catch (err) {
        assertEqual("ArangoError " + e.code + ": " + e.message, err.toString());
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test to string
////////////////////////////////////////////////////////////////////////////////

    testArangoToString2 : function () {
      var e = ERRORS.ERROR_ARANGO_DOCUMENT_NOT_FOUND;
      
      try {
        throwError(e.code, e.message + ": did not find document");
        fail();
      }
      catch (err) {
        assertEqual("ArangoError " + e.code + ": " + e.message + ": did not find document", err.toString());
      }
    }

  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suite
////////////////////////////////////////////////////////////////////////////////

jsunity.run(ErrorsSuite);

return jsunity.done();

