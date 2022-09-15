/*jshint globalstrict:false, strict:false */
/* global getOptions, assertEqual, assertFalse, assertTrue, arango */

////////////////////////////////////////////////////////////////////////////////
/// @brief test for security-related server options
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
/// Copyright holder is ArangoDB Inc, Cologne, Germany
///
/// @author Jan Steemann
/// @author Copyright 2019, ArangoDB Inc, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

if (getOptions === true) {
  return {
    'server.authentication': 'true',
    'server.jwt-secret': 'haxxmann',
  };
}
const jsunity = require('jsunity');
const crypto = require('@arangodb/crypto');
const protocols = ["tcp", "h2"];
const users = require("@arangodb/users");
let db = require('internal').db;
const user = "root";

let connectWith = function(protocol, user, password) {
  let endpoint = arango.getEndpoint().replace(/^[a-zA-Z0-9\+]+:/, protocol + ':');
  arango.reconnect(endpoint, db._name(), user, password);
};

function HttpAuthenticateSuite() {

  return {

    testUnauthorized: function() {
      protocols.forEach((protocol) => {
        connectWith(protocol, user, "");
        // need to change password, otherwise, the request will return 200
        const updateRes = users.update(user, "abc");
        if (updateRes.code === 200) {
          let result = arango.GET_RAW("/_api/version");
          assertEqual(401, result.code);
          assertTrue(result.headers.hasOwnProperty('www-authenticate'));
        }
        try {
          // to change password back to the original one, we must reconnect with the current one, then change it back to the original one, then connect again
          arango.reconnectWithNewPassword("abc");
          users.update(user, "");
          arango.reconnectWithNewPassword("");
        } catch (err) {
          throw err;
        }
      });
    },

    testUnauthorizedOmit: function() {
      protocols.forEach(protocol => {
        connectWith(protocol, user, "");
        // need to change password, otherwise, the request will return 200
        const updateRes = users.update(user, "abc");
        if (updateRes.code === 200) {
          let result = arango.GET_RAW("/_api/version", {"x-omit-www-authenticate": "abc"});
          assertEqual(401, result.code);
          assertFalse(result.headers.hasOwnProperty('www-authenticate'));
        }
        try {
          // to change password back to the original one, we must reconnect with the current one, then change it back to the original one, then connect again
          arango.reconnectWithNewPassword("abc");

          users.update(user, "");

          arango.reconnectWithNewPassword("");
        } catch (err2) {
          throw(err2);
        }
      });
    },

    testAuthorized: function() {
      const jwtSecret = 'haxxmann';
      const jwtRoot = crypto.jwtEncode(jwtSecret, {
        "preferred_username": "root",
        "iss": "arangodb",
        "exp": Math.floor(Date.now() / 1000) + 3600
      }, 'HS256');
      protocols.forEach(protocol => {
        connectWith(protocol, user, "");
        let result = arango.GET_RAW("/_api/version", {"bearer": jwtRoot});
        assertEqual(200, result.code);
        assertFalse(result.headers.hasOwnProperty('www-authenticate'));
      });
    },

  };
}

jsunity.run(HttpAuthenticateSuite);
return jsunity.done();
