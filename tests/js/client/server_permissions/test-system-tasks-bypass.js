/*jshint globalstrict:false, strict:false */
/* global getOptions, assertTrue, assertFalse, assertEqual, assertMatch, fail, arango */

// //////////////////////////////////////////////////////////////////////////////
// / DISCLAIMER
// /
// / Copyright 2014-2026 ArangoDB GmbH, Cologne, Germany
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
    'server.authentication': 'true',
    'server.jwt-secret': 'abc123'
  };
}

const jsunity = require("jsunity");
const {db} = require("@arangodb");
const users = require("@arangodb/users");


let IM = require('@arangodb/test-helper').getInstanceInfo();

const USER = "hackerman";
const PASSWORD = "sehrgeheimespasswort";

function testSuite() {
  const username = "hackerman";
  return {
    setUpAll: function() {
      IM.rememberConnection();
    },
    setUp: function() {
      IM.reconnectMe();
      db._useDatabase("_system");
      users.save(USER, PASSWORD);
      users.grantDatabase(USER, "_system");
      users.reload();
    },

    tearDown: function() {
      IM.reconnectMe();
      db._useDatabase("_system");
      users.remove(USER);
    },

    tearDownAll: function() {
      IM.reconnectMe();
    },

    testSystemTask : function() {
      arango.reconnect(arango.getEndpoint(), "_system", USER, PASSWORD);
     
      const res = arango.POST_RAW("/_api/tasks",
                      {  "name": "EvilSystemTask",
                         "command": `(function() { require('@arangodb').print("Hello, world"); })();`,
                         "params": {},
                         "period": 2,
                         "isSystem": true
                      });
      assertEqual(res.code, 403, "A normal user should not be allowed to register a system task");
    },
  };
}

jsunity.run(testSuite);
return jsunity.done();
