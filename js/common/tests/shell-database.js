////////////////////////////////////////////////////////////////////////////////
/// @brief test the database interface
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
var internal = require("internal");

// -----------------------------------------------------------------------------
// --SECTION--                                                  database methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite: error handling
////////////////////////////////////////////////////////////////////////////////

function DatabaseSuite () {
  return {

////////////////////////////////////////////////////////////////////////////////
/// @brief test the version information
////////////////////////////////////////////////////////////////////////////////

    testVersion : function () {
      assertMatch(/^1\.4/, internal.db._version());
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test _name function
////////////////////////////////////////////////////////////////////////////////

    testName : function () {
      assertEqual("_system", internal.db._name());
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test _path function
////////////////////////////////////////////////////////////////////////////////

    testPath : function () {
      assertTrue(typeof internal.db._path() === "string");
      assertTrue(internal.db._path() !== "");
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test name function
////////////////////////////////////////////////////////////////////////////////

    testQuery : function () {
      assertEqual([ 1 ], internal.db._query("return 1").toArray());
      assertEqual([ [ 1, 2, 9, "foo" ] ], internal.db._query("return [ 1, 2, 9, \"foo\" ]").toArray());
    }

  };
}

// -----------------------------------------------------------------------------
// --SECTION--                                                              main
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suite
////////////////////////////////////////////////////////////////////////////////

jsunity.run(DatabaseSuite);

return jsunity.done();

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// @addtogroup\\|// --SECTION--\\|/// @page\\|/// @}\\)"
// End:

