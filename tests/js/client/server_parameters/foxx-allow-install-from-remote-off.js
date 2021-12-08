/*jshint globalstrict:false, strict:false */
/* global getOptions, assertEqual, assertTrue, arango */

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
    'foxx.allow-install-from-remote': 'false',
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
        "http://github.com/arangodb-foxx/itzpapalotl",
        "https://github.com/arangodb-foxx/itzpapalotl",
        "http://www.github.com/arangodb-foxx/itzpapalotl",
        "https://www.github.com/arangodb-foxx/itzpapalotl",
        "http://foo:bar@github.com/arangodb-foxx/itzpapalotl",
        "https://foo:bar@www.github.com/arangodb-foxx/itzpapalotl",
        "https://foo:@github.com/arangodb-foxx/itzpapalotl",
        "http://some.other.domain/foo/bar",
        "https://some.other.domain/foo/bar",
        "https://github.com.some.deceptive.site/foo/bar",
        "https://some.deceptive.github.com.site/foo/bar",
        "https://github.com.evil/foo/bar",
      ];
      urls.forEach((url) => {
        try {
          let res = arango.PUT(`/_admin/aardvark/foxxes/url?mount=${mount}`, { url });
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
