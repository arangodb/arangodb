/*jshint globalstrict:false, strict:false */
/* global getOptions, assertEqual, assertTrue, assertFalse, arango */

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
    'foxx.enable': 'true',
    'foxx.force-update-on-startup': 'true'
  };
}
const jsunity = require('jsunity');
const internal = require('internal');
const fs = require('fs');
const path = require('path');
const utils = require('@arangodb/foxx/manager-utils');
const FoxxManager = require('@arangodb/foxx/manager');
function loadFoxxIntoZip(path) {
  let zip = utils.zipDirectory(path);
  let content = fs.readFileSync(zip);
  fs.remove(zip);
  return {
    type: 'inlinezip',
    buffer: content
  };
}

function testSuite() {
  const mount = "/test123";

  return {
    testAccessAPIs: function() {
      const urls = [
        [ "/", 301 ],
        [ "/_admin/log", 200 ],
        [ "/_admin/log/level", 200 ],
        [ "/_admin/version", 200 ],
        [ "/_api/collection", 200 ],
        [ "/_api/version", 200 ],
      ];
      urls.forEach((data) => {
        const [url, code] = data;
        let res = arango.GET_RAW(url);
        assertEqual(code, res.code, { res, url });
        assertFalse(res.error, { res, url });
      });
    },

    testAccessWebInterface: function() {
      const urls = [
        [ "/", 301 ],
        [ "/_admin/aardvark", 307 ],
        [ "/_admin/aardvark/", 307 ],
        [ "/_admin/aardvark/index.html", 200 ],
      ];
      urls.forEach((data) => {
        const [url, code] = data;
        let res = arango.GET_RAW(url);
        assertEqual(code, res.code, { res, url });
        assertFalse(res.error, { res, url });
      });
    },
    
    testAccessNonExistingFoxxApps: function() {
      const urls = [
        [ "/this/foxx/does-not-exist", 404 ],
        [ "/this/foxx/does-not-exist/", 404 ],
        [ "/foxx/does-not-exist", 404 ],
        [ "/foxx/does-not-exist/", 404 ],
        [ "/does-not-exist", 404 ],
        [ "/does-not-exist/", 404 ],
      ];
      urls.forEach((data) => {
        const [url, code] = data;
        let res = arango.GET_RAW(url);
        assertEqual(code, res.code, { res, url });
        assertTrue(res.error, { res, url });
      });
    },

    testInstallAndAccessFoxxApp: function() {
      const itzpapalotlPath = path.resolve(internal.pathForTesting('common'), 'test-data', 'apps', 'itzpapalotl');
      const itzpapalotlZip = loadFoxxIntoZip(itzpapalotlPath);

      try {
        // install app first
        let res = arango.POST('/_api/foxx?mount=' + mount,
                              itzpapalotlZip.buffer,
                              { 'content-type': 'application/zip' });
        assertFalse(res.error, { res });

        // access Foxx app
        res = arango.GET_RAW(mount);
        assertEqual(307, res.code, { res });
        assertFalse(res.error, { res });
        
        res = arango.GET_RAW(mount + "/index");
        assertEqual(200, res.code, { res});
        assertFalse(res.error, { res });

      } finally {
        try {
          FoxxManager.uninstall(mount);
        } catch (err) {}
      }
    },

  };
}

jsunity.run(testSuite);
return jsunity.done();
