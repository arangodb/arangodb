/* jshint globalstrict:false, strict:false, maxlen: 200 */
/* global getOptions, fail, arango, assertEqual, assertTrue */

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
// / @author Jan Steemann
// //////////////////////////////////////////////////////////////////////////////

'use strict';
const jsunity = require('jsunity');
const arangodb = require('@arangodb');
const db = arangodb.db;
const crypto = require('@arangodb/crypto');
const request = require("@arangodb/request");

const jwtSecret = 'haxxmann';

if (getOptions === true) {
  return {
    'server.authentication': 'true',
    'server.jwt-secret': jwtSecret,
    'query.enable-debug-apis': 'false',
  };
}

const jwt = crypto.jwtEncode(jwtSecret, {
  "server_id": "ABCD",
  "iss": "arangodb", "exp": Math.floor(Date.now() / 1000) + 3600
}, 'HS256');

function RegistrySuite() { 
  const cn = "UnitTests";
  
  const baseUrl = function () {
    return arango.getEndpoint().replace(/^tcp:/, 'http:').replace(/^ssl:/, 'https:');
  };

  return {
    setUpAll: function () {
      let c = db._create(cn, {numberOfShards: 1, replicationFactor: 1});
      let docs = [];
      for (let i = 0; i < 5000; ++i) {
        docs.push({});
      }
      c.insert(docs);
    },

    tearDownAll: function () {
      db._drop(cn);
    },
    
    testRegistryContents: function () {
      const qs = `FOR doc IN ${cn} RETURN doc`;

      // note: async-prefetch disabled here so that we know that no engines
      // are opened when we read from the query registry
      let query = db._createStatement({ query: qs, options: { stream: true, optimizer: { rules: ["-async-prefetch"] } } }).execute();

      try {
        let result = request.get({
          url: baseUrl() + "/_api/query/registry",
          auth: {
            bearer: jwt,
          }
        }).json;
        
        assertTrue(result.error, result);
        assertEqual(403, result.code, result);
      } finally {
        query.dispose();
      }
    },

  };
}

jsunity.run(RegistrySuite);
return jsunity.done();
