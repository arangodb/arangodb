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
    'foxx.enable': 'false',
  };
}
const jsunity = require('jsunity');

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
        [ "/this/foxx/does-not-exist", 403 ],
        [ "/this/foxx/does-not-exist/", 403 ],
        [ "/foxx/does-not-exist", 403 ],
        [ "/foxx/does-not-exist/", 403 ],
        [ "/does-not-exist", 403 ],
        [ "/does-not-exist/", 403 ],
      ];
      urls.forEach((data) => {
        const [url, code] = data;
        let res = arango.GET_RAW(url);
        assertEqual(code, res.code, { res, url });
        assertTrue(res.error, { res, url });
      });
    },

    testInstallAndAccessFoxxApp: function() {
      const url = "https://github.com/arangodb-foxx/demo-itzpapalotl/archive/refs/heads/master.zip";

      // try installing the app, but it should fail
      let res = arango.PUT_RAW(`/_admin/aardvark/foxxes/url?mount=${mount}`, { url });
      assertTrue(res.error, { res, url });
      assertEqual(403, res.code, { res, url });

      // access Foxx app
      res = arango.GET_RAW(mount);
      assertEqual(403, res.code, { res, url });
      assertTrue(res.error, { res, url });
    },

  };
}

jsunity.run(testSuite);
return jsunity.done();
