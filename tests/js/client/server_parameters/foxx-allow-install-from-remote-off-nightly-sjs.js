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
    'foxx.allow-install-from-remote': 'false',
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
    testInstallViaAardvarkOk: function() {
      const urls = [
        "http://github.com/arangodb-foxx/demo-itzpapalotl/archive/refs/heads/master.zip",
        "https://github.com/arangodb-foxx/demo-itzpapalotl/archive/refs/heads/master.zip",
        "http://www.github.com/arangodb-foxx/demo-itzpapalotl/archive/refs/heads/master.zip",
        "https://www.github.com/arangodb-foxx/demo-itzpapalotl/archive/refs/heads/master.zip",
      ];
      urls.forEach((url) => {
        try {
          let res = installFromRemote(url, () => arango.PUT(`/_admin/aardvark/foxxes/url?mount=${mount}`, { url }));
          if (res === null) {
            return;
          }
          assertFalse(res.error, { res, url });
        } finally {
          try {
            FoxxManager.uninstall(mount);
          } catch (err) {}
        }
      });
    },

    testInstallViaAardvarkFail: function() {
      const urls = [
        "http://some.other.domain/foo/bar",
        "https://some.other.domain/foo/bar",
        "https://github.com.some.deceptive.site/foo/bar",
        "https://some.deceptive.github.com.site/foo/bar",
        "https://github.com.evil/foo/bar",
      ];
      urls.forEach((url) => {
        try {
          let res = arango.PUT(`/_admin/aardvark/foxxes/url?mount=${mount}`, { url });
          assertTrue(res.error, url);
          assertEqual(403, res.code);
        } finally {
          try {
            FoxxManager.uninstall(mount);
          } catch (err) {}
        }
      });
    },
    
    testInstallViaFoxxAPIOld: function() {
      // note: installing from Github is still allowed here
      const urls = [
        "http://some.other.domain/foo/bar",
        "https://some.other.domain/foo/bar",
        "https://github.com.some.deceptive.site/foo/bar",
        "https://some.deceptive.github.com.site/foo/bar",
        "https://github.com.evil/foo/bar",
      ];
      urls.forEach((url) => {
        try {
          let res = arango.POST("/_admin/foxx/install", { appInfo: url, mount });
          assertTrue(res.error);
          assertEqual(403, res.code);
          assertEqual(11, res.errorNum);
        } finally {
          try {
            FoxxManager.uninstall(mount);
          } catch (err) {}
        }
      });
    },
    
    testInstallViaFoxxAPINew: function() {
      // note: installing from Github is still allowed here
      const urls = [
        "http://some.other.domain/foo/bar",
        "https://some.other.domain/foo/bar",
        "https://github.com.some.deceptive.site/foo/bar",
        "https://some.deceptive.github.com.site/foo/bar",
        "https://github.com.evil/foo/bar",
      ];
      urls.forEach((url) => {
        try {
          let res = arango.POST(`/_api/foxx?mount=${mount}`, { source: url });
          assertTrue(res.error);
          assertEqual(403, res.code);
          assertEqual(11, res.errorNum);
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
