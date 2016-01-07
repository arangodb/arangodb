/*jshint globalstrict:false, strict:false */
/*global fail, assertTrue, assertFalse, assertEqual, assertMatch, assertNotNull */

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
var fm = require("@arangodb/foxx/manager");
var arango = require("@arangodb").arango;
var db = require("internal").db;


////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////

function FoxxManagerSuite () {
  'use strict';

  var mountPoint = "/itz";

  function inList (list, name) {
    var i;

    for (i = 0; i < list.length; ++i) {
      var entry = list[i];

      assertTrue(entry.hasOwnProperty("name"));

      if (entry.name === name && entry.mount === mountPoint) {
        return true;
      }
    }

    return false;
  }

  return {

    setUp: function () {
      db._useDatabase("_system");
      fm.update();
    },

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

    testInstall : function () {
      try {
        fm.uninstall(mountPoint);
      }
      catch (err) {
      }

      try {
        fm.install("git:jsteemann/itzpapalotl", mountPoint);
      } catch(err) {
        require("console").log(err);
        fail(err.errorMessage);
      }
      var result = fm.listJson(), i;

      for (i = 0; i < result.length; ++i) {
        var entry = result[i];

        if (entry.mount === mountPoint) {
          assertEqual("jsteemann", entry.author);
          assertEqual("itzpapalotl", entry.name);
          assertTrue(entry.hasOwnProperty("description"));
          assertTrue(entry.hasOwnProperty("version"));
          fm.uninstall(mountPoint);
          return;
        }
      }

      fail();
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test search
////////////////////////////////////////////////////////////////////////////////

    testSearchEmpty : function () {
      var result = fm.searchJson();

      var i, n;

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

        var version;

        for (version in entry.versions) {
          if (entry.versions.hasOwnProperty(version)) {
            assertMatch(/^\d+(\.\d+)*(-.*)?$/, version);
            assertTrue(entry.versions[version].hasOwnProperty("type"));
            assertTrue(entry.versions[version].hasOwnProperty("location"));
          }
        }
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test search existing app
////////////////////////////////////////////////////////////////////////////////

    testSearchAztec : function () {
      var result = fm.searchJson("itzpapalotl");

      var n;

      n = result.length;
      assertTrue(n > 0);

      // validate the results structure, simply take the first match
      var entry = result.pop();

      assertEqual("Jan Steemann", entry.author);
      assertEqual("itzpapalotl", entry.name);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test install
////////////////////////////////////////////////////////////////////////////////

    testInstallSingle : function () {
      fm.update();

      try {
        fm.uninstall(mountPoint);
      }
      catch (err) {
      }

      fm.install("itzpapalotl", mountPoint);
      assertTrue(inList(fm.listJson(), "itzpapalotl"));

      var url = mountPoint + '/random';
      var fetched = arango.GET(url);

      assertTrue(fetched.hasOwnProperty("name"));

      fm.uninstall(mountPoint);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test install
////////////////////////////////////////////////////////////////////////////////

    testInstallMulti : function () {
      fm.update();

      try {
        fm.uninstall(mountPoint + "1");
      }
      catch (err) {
      }
      try {
        fm.uninstall(mountPoint + "2");
      }
      catch (err) {
      }

      fm.install("itzpapalotl", mountPoint + "1");
      fm.install("itzpapalotl", mountPoint + "2");

      var url, fetched;

      url  = mountPoint + '1/random';
      fetched = arango.GET(url);
      assertTrue(fetched.hasOwnProperty("name"));

      url  = mountPoint + '2/random';
      fetched = arango.GET(url);

      assertTrue(fetched.hasOwnProperty("name"));

      fm.uninstall(mountPoint + "1");
      fm.uninstall(mountPoint + "2");
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test uninstall
////////////////////////////////////////////////////////////////////////////////

    testUninstallInstalled : function () {
      fm.update();

      try {
        fm.uninstall(mountPoint);
      }
      catch (err) {
      }

      fm.install("itzpapalotl", mountPoint);
      assertTrue(inList(fm.listJson(), "itzpapalotl"));

      fm.uninstall(mountPoint);
      assertFalse(inList(fm.listJson(), "itzpapalotl"));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test uninstall
////////////////////////////////////////////////////////////////////////////////

    testUninstallUninstalled : function () {
      fm.update();

      try {
        fm.uninstall(mountPoint);
      }
      catch (err1) {
      }

      try {
        // already uninstalled
        fm.uninstall(mountPoint);
        fail();
      }
      catch (err2) {
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test uninstall
////////////////////////////////////////////////////////////////////////////////

    testUninstallSystem : function () {
      fm.update();

      var doc = db._collection('_apps').firstExample({ mount: "/_admin/aardvark" });

      assertNotNull(doc);

      try {
        // cannout uninstall a system app
        fm.uninstall(doc.mount);
        fail();
      }
      catch (err) {
      }
    }

  };
}


////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suite
////////////////////////////////////////////////////////////////////////////////

jsunity.run(FoxxManagerSuite);

return jsunity.done();

