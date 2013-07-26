////////////////////////////////////////////////////////////////////////////////
/// @brief test the foxx manager
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
var fm = require("org/arangodb/foxx-manager");
var arango = require("org/arangodb").arango;

// -----------------------------------------------------------------------------
// --SECTION--                                                foxx manager tests
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////

function FoxxManagerSuite () {

  return {

////////////////////////////////////////////////////////////////////////////////
/// @brief test search
////////////////////////////////////////////////////////////////////////////////

    testSearchEmpty : function () {
      var result = fm.searchJson();

      var i, j, n;

      n = result.length;
      assertTrue(n > 0);

      // validate the results structure
      for (i = 0; i < n; ++i) {
        var entry = result[i];

        assertTrue(entry.hasOwnProperty("_key"));
        assertTrue(entry.hasOwnProperty("_rev"));
        assertTrue(entry.hasOwnProperty("description"));
        assertTrue(entry.hasOwnProperty("name"));
        assertTrue(entry.hasOwnProperty("author"));
        assertTrue(entry.hasOwnProperty("versions"));

        versions = entry.versions;

        for (version in entry.versions) {
          if (entry.versions.hasOwnProperty(version)) {
            assertMatch(/^\d+(\.\d+)*$/, version);
            assertTrue(entry.versions[version].hasOwnProperty("type"));
            assertTrue(entry.versions[version].hasOwnProperty("location"));
          }
        }
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test search existings
////////////////////////////////////////////////////////////////////////////////

    testSearchAztec : function () {
      var result = fm.searchJson("itzpapalotl");

      var i, j, n;

      n = result.length;
      assertTrue(n > 0);

      // validate the results structure, simply take the first match
      var entry = result.pop();

      assertEqual("jsteemann", entry.author);
      assertEqual("itzpapalotl", entry.name);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test install
////////////////////////////////////////////////////////////////////////////////

    testInstall : function () {
      try {
        fm.uninstall("/itz");
      }
      catch (err) {
      }

      fm.install("itzpapalotl", "/itz");
  
      var endpoint = arango.getEndpoint();
      endpoint = endpoint.replace(/^tcp:/g, 'http:').replace(/^ssl:/g, 'https:');
      
      var url = endpoint + '/itz';

      require("internal").print(arango.GET(url));

      fm.uninstall("/itz");
    }

  };
}

// -----------------------------------------------------------------------------
// --SECTION--                                                              main
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suite
////////////////////////////////////////////////////////////////////////////////

jsunity.run(FoxxManagerSuite);

return jsunity.done();

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// @addtogroup\\|// --SECTION--\\|/// @page\\|/// @}\\)"
// End:

