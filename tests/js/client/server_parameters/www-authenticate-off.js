/*jshint globalstrict:false, strict:false */
/* global getOptions, assertEqual, assertFalse, arango */

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
/// @author Jan Steemann
/// @author Copyright 2019, ArangoDB Inc, Cologne, Germany
// //////////////////////////////////////////////////////////////////////////////

let db = require('internal').db;
const protocols = ["tcp", "h2"];
const user = "root";

if (getOptions === true) {
  return {
    'server.authentication': 'false'
  };
}
const jsunity = require('jsunity');
const request = require('@arangodb/request').request;

let connectWith = function(protocol, user, password) {
  let endpoint = arango.getEndpoint().replace(/^[a-zA-Z0-9\+]+:/, protocol + ':');
  arango.reconnect(endpoint, db._name(), user, password);
};

function testSuite() {

  return {

    tearDown: function() {
      connectWith(protocols[0], user, "");
    },

    testHeader: function() {
      protocols.forEach((protocol) => {
        connectWith(protocol, user, "");
        let result = arango.GET_RAW("/_api/version");
        assertEqual(200, result.code);
        assertFalse(result.headers.hasOwnProperty('www-authenticate'));
      });
    },
  };
}

jsunity.run(testSuite);
return jsunity.done();
