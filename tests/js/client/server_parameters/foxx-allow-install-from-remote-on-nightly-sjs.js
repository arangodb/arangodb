/*jshint globalstrict:false, strict:false */
/* global getOptions, assertEqual, assertTrue, assertFalse, arango */

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
// //////////////////////////////////////////////////////////////////////////////

if (getOptions === true) {
  return {
    'foxx.allow-install-from-remote': 'true',
  };
}
const jsunity = require('jsunity');
const errors = require('@arangodb').errors;
const db = require('internal').db;
const FoxxManager = require('@arangodb/foxx/manager');

// installing from github relies on resources outside of the SUT host.
// retry if the source could not be fetched; give up silently (with a log
// message) instead of failing the test if github remains unreachable.
const maxTries = 5;
function installFromRemote(url, installFn) {
  let res;
  for (let i = 1; i <= maxTries; ++i) {
    res = installFn();
    if (!res.error || res.errorNum !== errors.ERROR_SERVICE_SOURCE_ERROR.code) {
      return res;
    }
    console.warn(`attempt ${i}/${maxTries} to fetch ${url} failed: ${JSON.stringify(res)}`);
    if (i < maxTries) {
      require('internal').sleep(i);
    }
  }
  console.warn(`giving up fetching ${url} after ${maxTries} attempts, not considering this a test failure`);
  return null;
}

function testSuite() {
  const mount = "/test123";

  return {
    testInstallViaAardvark: function() {
      const urls = [
        "https://github.com/arangodb-foxx/demo-itzpapalotl/archive/refs/heads/master.zip",
      ];
      urls.forEach((url) => {
        try {
          let res = installFromRemote(url, () => arango.PUT(`/_admin/aardvark/foxxes/url?mount=${mount}`, { url }));
          if (res === null) {
            return;
          }
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
          let res = installFromRemote(url, () => arango.POST("/_admin/foxx/install", { appInfo: url, mount }));
          if (res === null) {
            return;
          }
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
          let res = installFromRemote(url, () => arango.POST(`/_api/foxx?mount=${mount}`, { source: url }));
          if (res === null) {
            return;
          }
          assertFalse(res.error);
          assertEqual("itzpapalotl", res.name);
        } finally {
          try {
            FoxxManager.uninstall(mount);
          } catch (err) {}
        }
      });
    },

    testInstallEmptyBody: function() {
      let res = arango.POST("/_admin/foxx/install", {}, {"content-type": "application/x-velocypack"});
      assertTrue(res.error);
      assertEqual(res.errorNum, 400);
      assertEqual(res.errorMessage, "query parameter \"mount\" is required");
    },


  };
}

jsunity.run(testSuite);
return jsunity.done();
