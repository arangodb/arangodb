/*jshint globalstrict:false, strict:false */
/* global getOptions, assertEqual, assertFalse, arango */

////////////////////////////////////////////////////////////////////////////////
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
/// Copyright holder is ArangoDB Inc, Cologne, Germany
///
/// @author Jan Steemann
/// @author Copyright 2021, ArangoDB Inc, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

if (getOptions === true) {
  return {
    'foxx.allow-install-from-remote': 'true',
  };
}
const jsunity = require('jsunity');
const errors = require('@arangodb').errors;
const db = require('internal').db;
const FoxxManager = require('@arangodb/foxx/manager');

function testSuite() {
  const mount = "/test123";

  return {
    testInstallViaAardvark: function() {
      const urls = [
        "https://github.com/arangodb-foxx/demo-itzpapalotl/archive/refs/heads/master.zip",
      ];
      urls.forEach((url) => {
        try {
          let res = arango.PUT(`/_admin/aardvark/foxxes/url?mount=${mount}`, { url });
          assertFalse(res.error);
          assertEqual("itzpapalotl", res.name);
        } finally {
          try {
            FoxxManager.uninstall(mount);
          } catch (err) {}
        }
      });
    },
    
    testInstallViaFoxxAPIOld: function() {
      const urls = [
        "https://github.com/arangodb-foxx/demo-itzpapalotl/archive/refs/heads/master.zip",
      ];
      urls.forEach((url) => {
        try {
          let res = arango.POST("/_admin/foxx/install", { appInfo: url, mount });
          assertFalse(res.error);
          assertEqual("itzpapalotl", res.name);
        } finally {
          try {
            FoxxManager.uninstall(mount);
          } catch (err) {}
        }
      });
    },
    
    testInstallViaFoxxAPINew: function() {
      const urls = [
        "https://github.com/arangodb-foxx/demo-itzpapalotl/archive/refs/heads/master.zip",
      ];
      urls.forEach((url) => {
        try {
          let res = arango.POST(`/_api/foxx?mount=${mount}`, { source: url });
          assertFalse(res.error);
          assertEqual("itzpapalotl", res.name);
        } finally {
          try {
            FoxxManager.uninstall(mount);
          } catch (err) {}
        }
      });
    },
    
  };
}

jsunity.run(testSuite);
return jsunity.done();
