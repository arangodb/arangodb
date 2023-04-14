/*jshint globalstrict:false, strict:false */
/* global getOptions, assertEqual, assertFalse, assertTrue, arango, print */

////////////////////////////////////////////////////////////////////////////////
/// @brief test for server startup options
/// DISCLAIMER
///
/// Copyright 2014-2022 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
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
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Julia Puget
////////////////////////////////////////////////////////////////////////////////

if (getOptions === true) {
  return {
    'server.authentication': 'true',
    'server.jwt-secret': 'haxxmann',
  };
}
const jsunity = require('jsunity');
const internal = require('internal');
const crypto = require('@arangodb/crypto');
const protocols = ["tcp", "h2"];
const users = require("@arangodb/users");
const db = require('internal').db;
let maintainerMode = require('internal').db._version(true)['details']['maintainer-mode'];
const user = "testUser";

let connectWith = function(protocol, user, password) {
  let endpoint = arango.getEndpoint().replace(/^[a-zA-Z0-9\+]+:/, protocol + ':');
  arango.reconnect(endpoint, db._name(), user, password);
};

function HttpAuthenticateSuite() {

  return {

    setUp: function() {
      connectWith("tcp", "root", "");
      users.save(user, "");
      users.grantDatabase(user, '_system', 'rw');
    },

    tearDown: function() {
      connectWith("tcp", "root", "");
      users.remove(user);
    },

    testUnauthorized: function() {
      protocols.forEach((protocol) => {
        connectWith(protocol, user, "");
        let result = "";
        // we put this in a while because the request might be made when the password has not
        // changed yet so it wouldn't get a 401
        let numSecs = 0.5;
        while (true) {
          // need to change password, otherwise, the request will return 200
          result = users.update(user, "abc");
          if (result.code === 200 || numSecs >= 16) {
            break;
          }
          internal.sleep(numSecs);
          numSecs *= 2;
        }
        assertEqual(result.code, 200, "update password returned with code " + result.code);
        result = arango.GET_RAW("/_api/version");
        assertEqual(401, result.code, JSON.stringify(result));
        assertTrue(result.headers.hasOwnProperty('www-authenticate'), JSON.stringify(result));
        // to change password back to the original one, we must reconnect with the current one, then change it back to the original one, then connect again
        arango.reconnectWithNewPassword("abc");
        users.update(user, "");
        arango.reconnectWithNewPassword("");
      });
    },

    testUnauthorizedOmit: function () {
      protocols.forEach(protocol => {
        print(`connecting with ${protocol}`);
        connectWith(protocol, user, "");
        let result = "";
        // we put this in a while because the request might be made when the password has not
        // changed yet so it wouldn't get a 401
        let numSecs = 0.5;
        while (true) {
          // need to change password, otherwise, the request will return 200
          result = users.update(user, "abc");
          if (result.code === 200 || numSecs >= 16) {
            break;
          }
          internal.sleep(numSecs);
          numSecs *= 2;
        }
        assertEqual(result.code, 200, "update password returned with code " + result.code);
        result = arango.GET_RAW("/_api/version", {"x-omit-www-authenticate": "abc"});
        assertEqual(401, result.code, JSON.stringify(result));
        assertFalse(result.headers.hasOwnProperty('www-authenticate'), JSON.stringify(result));
        // to change password back to the original one, we must reconnect with the current one, then change it back to the original one, then connect again
        arango.reconnectWithNewPassword("abc");
        users.update(user, "");
        arango.reconnectWithNewPassword("");
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
        print(`connecting with ${protocol}`);
        connectWith(protocol, user, "");
        let result = arango.GET_RAW("/_api/version", {"bearer": jwtRoot});
        assertEqual(200, result.code, JSON.stringify(result));
        assertFalse(result.headers.hasOwnProperty('www-authenticate'), JSON.stringify(result));
      });
    },
  };
}

if (maintainerMode) {
  jsunity.run(HttpAuthenticateSuite);
}
return jsunity.done();
