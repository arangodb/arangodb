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
var fm = require("org/arangodb/foxx/manager");
var arango = require("org/arangodb").arango;
var db = require("internal").db;

// -----------------------------------------------------------------------------
// --SECTION--                                                foxx manager tests
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////

function FoxxManagerSuite () {

  function inList (list, name) {
    var i;

    for (i = 0; i < list.length; ++i) {
      var entry = list[i];

      assertTrue(entry.hasOwnProperty("name"));

      if (entry.name === name) {
        return true;
      }
    }
    
    return false;
  }

  return {

////////////////////////////////////////////////////////////////////////////////
/// @brief test update
////////////////////////////////////////////////////////////////////////////////

    testUpdate : function () {
      fm.update();

      var result = fm.availableJson();
      var i, n;

      n = result.length;
      assertTrue(n > 0);

      // validate the results structure
      for (i = 0; i < n; ++i) {
        var entry = result[i];

        assertTrue(entry.hasOwnProperty("description"));
        assertTrue(entry.hasOwnProperty("name"));
        assertTrue(entry.hasOwnProperty("author"));
        assertTrue(entry.hasOwnProperty("latestVersion"));
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test available
////////////////////////////////////////////////////////////////////////////////

    testFetch : function () {
      try {
        fm.purge("itzpapalotl");
      }
      catch (err) {
      }

      fm.update();
      fm.fetch("github", "jsteemann/itzpapalotl");

      var result = fm.fetchedJson(), i;

      for (i = 0; i < result.length; ++i) {
        var entry = result[i];

        if (entry.name === 'itzpapalotl') {
          assertEqual("jsteemann", entry.author);
          assertEqual("itzpapalotl", entry.name);
          assertTrue(entry.hasOwnProperty("description"));
          assertTrue(entry.hasOwnProperty("version"));
          assertTrue(entry.hasOwnProperty("path"));
        }
        return;
       }

      fail();
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test purge
////////////////////////////////////////////////////////////////////////////////

    testPurge : function () {
      try {
        fm.purge("itzpapalotl");
      }
      catch (err) {
      }

      fm.update();
      fm.install("itzpapalotl", "/itz1");
      fm.install("itzpapalotl", "/itz2");
      fm.install("itzpapalotl", "/itz3");

      fm.purge("itzpapalotl");

      assertFalse(inList(fm.listJson(), "itzpapalotl"));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test purge
////////////////////////////////////////////////////////////////////////////////

    testPurgeSystem : function () {
      fm.update();

      try {
        fm.purge("aardvark");
        fail();
      }
      catch (err) {
      }
    },

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

    testInstallSingle : function () {
      fm.update();

      try {
        fm.purge("/itzpapalotl");
      }
      catch (err) {
      }

      fm.install("itzpapalotl", "/itz");
      assertTrue(inList(fm.listJson(), "itzpapalotl"));
 
      var url = '/itz/random';
      var fetched = arango.GET(url);

      assertTrue(fetched.hasOwnProperty("name"));
      
      fm.uninstall("/itz");
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test install
////////////////////////////////////////////////////////////////////////////////

    testInstallMulti : function () {
      fm.update();

      try {
        fm.uninstall("/itz1");
        fm.uninstall("/itz2");
      }
      catch (err) {
      }

      fm.install("itzpapalotl", "/itz1");
      fm.install("itzpapalotl", "/itz2");
 
      var url, fetched;

      url  = '/itz1/random';
      fetched = arango.GET(url);
      assertTrue(fetched.hasOwnProperty("name"));
      
      url  = '/itz2/random';
      fetched = arango.GET(url);

      assertTrue(fetched.hasOwnProperty("name"));

      fm.uninstall("/itz1");
      fm.uninstall("/itz2");
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test uninstall
////////////////////////////////////////////////////////////////////////////////

    testUninstallInstalled : function () {
      fm.update();

      try {
        fm.purge("itzpapalotl");
      }
      catch (err) {
      }

      fm.install("itzpapalotl", "/itz");
      assertTrue(inList(fm.listJson(), "itzpapalotl"));

      fm.uninstall("/itz");
      assertFalse(inList(fm.listJson(), "itzpapalotl"));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test uninstall
////////////////////////////////////////////////////////////////////////////////

    testUninstallUninstalled : function () {
      fm.update();

      try {
        fm.uninstall("/itz");
      }
      catch (err) {
      }

      try {
        // already uninstalled
        fm.uninstall("/itz");
        fail();
      }
      catch (err) {
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test uninstall
////////////////////////////////////////////////////////////////////////////////

    testUninstallSystem : function () {
      fm.update();

      var doc = db._collection('_aal').firstExample({ type: "mount", name: "aardvark" });
      
      assertNotNull(doc);

      try {
        // cannout uninstall a system app
        fm.uninstall(doc.mount);
        fail();
      }
      catch (err) {
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test unmount
////////////////////////////////////////////////////////////////////////////////

    testUnmountInstalled : function () {
      fm.update();

      try {
        fm.uninstall("/itz");
      }
      catch (err) {
      }

      fm.install("itzpapalotl", "/itz");
      fm.unmount("/itz");
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test unmount
////////////////////////////////////////////////////////////////////////////////

    testUnmountUninstalled : function () {
      fm.update();

      try {
        fm.uninstall("/itz");
      }
      catch (err) {
      }

      try {
        // already unmounted
        fm.unmount("/itz");
        fail();
      }
      catch (err) {
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test unmount
////////////////////////////////////////////////////////////////////////////////

    testUnmountSystem : function () {
      fm.update();

      var doc = db._collection('_aal').firstExample({ type: "mount", name: "aardvark" });
      
      assertNotNull(doc);

      try {
        // cannout unmount a system app
        fm.unmount(doc.mount);
        fail();
      }
      catch (err) {
      }
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

