/*jshint globalstrict:false, strict:false */
/*global assertTrue */

////////////////////////////////////////////////////////////////////////////////
/// @brief tests for client-specific functionality
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
/// @author Esteban Lombeyda
/// @author Copyright 2014, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

var jsunity = require("jsunity");
var db = require("@arangodb").db;

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////

function changeOperationModePositiveCaseTestSuite () {
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
      // reset server-mode after executing this test
      db._executeTransaction({
        collections: { },
        action: function () {
          var db = require('internal').db;
          try {
            db._changeMode('Normal');
          }
          catch (err) {
          }
        }
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief tests if the change of the operation mode of the arango server
///        can be done.
///        Note: this test needs an arango server with endpoint unix:...
///        See target unittests-shell-client-readonly
////////////////////////////////////////////////////////////////////////////////

    testChangeMode : function () {
      // we need to use a transaction (the transaction is shipped to the
      // server and executed there) to execute the changeMode function call on
      // the server...
      var result = db._executeTransaction({
        collections: { },
        action: function () {
          var db = require('internal').db;
          var result;
          try {
            result = db._changeMode('NoCreate');
          }
          catch (err) {
            result = false;
          }
          return result;
        }
      });
      assertTrue(result);
    }

  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suite
////////////////////////////////////////////////////////////////////////////////

jsunity.run(changeOperationModePositiveCaseTestSuite);

return jsunity.done();

