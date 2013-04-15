/*jslint indent: 2, nomen: true, maxlen: 80 */
/*global require, assertEqual, assertTrue */

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
var arangodb = require("org/arangodb");
var ERRORS = arangodb.errors;
var db = arangodb.db;

// -----------------------------------------------------------------------------
// --SECTION--                                                transactions tests
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////

function TransactionsInvocationsSuite () {

  var unregister = function (name) {
    try {
      aqlfunctions.unregister(name);
    }
    catch (err) {
    }
  };

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
/// @brief execute a transaction with an invalid action
////////////////////////////////////////////////////////////////////////////////

    testInvokeActionInvalid2 : function () {
      try {
        db._executeTransaction({ 
          collections: { }, 
          action: [ ],
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

// -----------------------------------------------------------------------------
// --SECTION--                                                              main
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suite
////////////////////////////////////////////////////////////////////////////////

jsunity.run(TransactionsInvocationsSuite);

return jsunity.done();

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// @addtogroup\\|// --SECTION--\\|/// @page\\|/// @}\\)"
// End:
