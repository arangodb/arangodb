/*jshint globalstrict:false, strict:false */
/* global getOptions */

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

let db = require('internal').db;
const protocols = ["tcp", "h2"];

if (getOptions === true) {
  return {
    'server.authentication': 'false'
  };
}
const jsunity = require('jsunity');
const {assertEqual, assertTrue, assertFalse, assertNotEqual} = jsunity.jsUnity.assertions;
const arango = require('@arangodb').arango;
let IM = global.instanceManager;

function testSuite() {

  return {
    setUp: function() {
      IM.rememberConnection();
    },

    tearDown: function() {
      IM.reconnectMe();
    },

    testHeader: function() {
      protocols.forEach((protocol) => {
        arango.reconnect(IM.url.replace(/^[a-zA-Z0-9\+]+:/, protocol + ':'), db._name(), "root", "");
        let result = arango.GET_RAW("/_api/version");
        assertEqual(200, result.code);
        assertFalse(result.headers.hasOwnProperty('www-authenticate'));
      });
    },
  };
}

jsunity.run(testSuite);
return jsunity.done();
