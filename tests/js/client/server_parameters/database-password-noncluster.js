/*jshint globalstrict:false, strict:false */
/*global assertEqual, assertTrue, getOptions, fail, arango*/

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
/// @author Copyright 2021, triAGENS GmbH, Cologne, Germany
// //////////////////////////////////////////////////////////////////////////////

if (getOptions === true) {
  return {
    'database.password': 'testi1234',
    'server.authentication': 'true',
    'server.jwt-secret': 'haxxmann',
  };
}

const jsunity = require("jsunity");
const arangodb = require("@arangodb");
const db = arangodb.db;
const errors = require('@arangodb').errors;

function OptionsTestSuite () {
  return {

    testConnectInvalidPassword: function () {
      try {
        arango.reconnect(arango.getEndpoint(), db._name(), arango.connectedUser(), "testmann");
        fail();
      } catch (err) {
        assertEqual(errors.ERROR_BAD_PARAMETER.code, err.errorNum);
      }
    },
    
    testConnectValidPassword: function () {
      arango.reconnect(arango.getEndpoint(), db._name(), arango.connectedUser(), "testi1234");
      assertTrue(arango.isConnected());
    },
    
    testConnectNoUserNoPassword: function () {
      arango.setJwtSecret("haxxmann");
      try {
        arango.reconnect(arango.getEndpoint(), db._name());
        assertTrue(arango.isConnected());
      } finally {
        arango.setJwtSecret("");
      }
    },
    
    testConnectNoPassword: function () {
      arango.setJwtSecret("haxxmann");
      try {
        arango.reconnect(arango.getEndpoint(), db._name(), arango.connectedUser());
        assertTrue(arango.isConnected());
      } finally {
        arango.setJwtSecret("");
      }
    },
  
  };
}

jsunity.run(OptionsTestSuite);

return jsunity.done();
