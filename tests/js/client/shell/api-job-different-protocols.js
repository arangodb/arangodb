/*jshint globalstrict:false, strict:false */
/*global arango, assertEqual, assertNotEqual, assertTrue, assertFalse, assertUndefined, assertNull, fail */

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

const jsunity = require("jsunity");
const arangodb = require("@arangodb");
const internal = require('internal');
const db = arangodb.db;

const originalEndpoint = arango.getEndpoint();
const createConnection = (protocol) => {
  let endpoint = protocol + originalEndpoint.replace(/^[a-z0-9]+:/, ':');
  arango.reconnect(endpoint, db._name(), "root", "");
};

function JobsApiWithDifferentProtocols() {
  'use strict';
  
  return {
    tearDown: function () {
      arango.reconnect(originalEndpoint, db._name(), "root", "");
    },

    testStartJobWithHttp1AndFetchWithHttp2: function () {
      createConnection("http");
      let res = arango.POST_RAW("/_api/cursor", {query: "RETURN 'foobar'"}, {"x-arango-async": "store"});
      const id = res.headers["x-arango-async-id"];

      createConnection("h2");
      let tries = 60;
      while (--tries > 0) {
        res = arango.PUT_RAW("/_api/job/" + id, {});
        if (res.code === 201) {
          assertEqual(["foobar"], res.parsedBody.result);
          break;
        }
        internal.sleep(0.5);
      }
      assertTrue(tries > 0, "timed out");
    },
    
    testStartJobWithHttp2AndFetchWithHttp1: function () {
      createConnection("h2");
      let res = arango.POST_RAW("/_api/cursor", {query: "RETURN 'foobar'"}, {"x-arango-async": "store"});
      const id = res.headers["x-arango-async-id"];

      createConnection("http");
      let tries = 60;
      while (--tries > 0) {
        res = arango.PUT_RAW("/_api/job/" + id, {});
        if (res.code === 201) {
          assertEqual(["foobar"], res.parsedBody.result);
          break;
        }
        internal.sleep(0.5);
      }
      assertTrue(tries > 0, "timed out");
    },

  };
}

jsunity.run(JobsApiWithDifferentProtocols);
return jsunity.done();
