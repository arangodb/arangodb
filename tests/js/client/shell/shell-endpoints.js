/*jshint globalstrict:false, strict:false */
/*global fail, assertEqual */

// //////////////////////////////////////////////////////////////////////////////
// / DISCLAIMER
// /
// / Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
// / Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
// /
// / Licensed under the Business Source License 1.1 (the "License");
// / you may not use this file except in compliance with the License.
// / You may obtain a copy of the License at
// /
// /     https://github.com/arangodb/arangodb/blob/devel/LICENSE
// /
// / Unless required by applicable law or agreed to in writing, software
// / distributed under the License is distributed on an "AS IS" BASIS,
// / WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// / See the License for the specific language governing permissions and
// / limitations under the License.
// /
// / Copyright holder is ArangoDB GmbH, Cologne, Germany
// /
/// @author Jan Steemann
/// @author Copyright 2013, triAGENS GmbH, Cologne, Germany
// //////////////////////////////////////////////////////////////////////////////

const jsunity = require("jsunity");
const {assertEqual, assertTrue, assertFalse, assertNotEqual} = jsunity.jsUnity.assertions;
const arangodb = require("@arangodb");
const arango = arangodb.arango;
const db = arangodb.db;
const ERRORS = arangodb.errors;
let IM = global.instanceManager;


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
      IM.rememberConnection();
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

      IM.reconnectMe();
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

