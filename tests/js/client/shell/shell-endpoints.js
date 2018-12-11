/*jshint globalstrict:false, strict:false */
/*global fail, assertEqual */

////////////////////////////////////////////////////////////////////////////////
/// @brief test the endpoints
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
var arangodb = require("@arangodb");
var arango = arangodb.arango;
var db = arangodb.db;
var ERRORS = arangodb.errors;
var originalEndpoint = arango.getEndpoint().replace(/localhost/, '127.0.0.1');


////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////

function EndpointsSuite () {
  'use strict';

  return {

////////////////////////////////////////////////////////////////////////////////
/// @brief set up
////////////////////////////////////////////////////////////////////////////////

    setUp : function () {
      db._useDatabase("_system");
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief tear down
////////////////////////////////////////////////////////////////////////////////

    tearDown : function () {
      db._useDatabase("_system");

      try {
        db._dropDatabase("UnitTestsDatabase0");
      }
      catch (err) {
      }

      arango.reconnect(originalEndpoint, "_system", "root", "");
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test non-system
////////////////////////////////////////////////////////////////////////////////

    testEndpointsNonSystem : function () {
      try {
        db._dropDatabase("UnitTestsDatabase0");
      }
      catch (err1) {
      }

      db._createDatabase("UnitTestsDatabase0");
      db._useDatabase("UnitTestsDatabase0");

      // _endpoints is forbidden
      try {
        db._endpoints();
        fail();
      }
      catch (err2) {
        assertEqual(ERRORS.ERROR_ARANGO_USE_SYSTEM_DATABASE.code, err2.errorNum);
      }
    }

  };
}


////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suite
////////////////////////////////////////////////////////////////////////////////

jsunity.run(EndpointsSuite);

return jsunity.done();

