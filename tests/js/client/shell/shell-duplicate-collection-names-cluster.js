/* jshint globalstrict:false, strict:false, maxlen: 200 */
/* global fail, assertEqual, assertTrue */

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

let jsunity = require('jsunity');
let internal = require('internal');
let arangodb = require('@arangodb');
let db = arangodb.db;
let IM = global.instanceManager;

function createDuplicateCollectionNameSuite() {
  'use strict';
  const cn = 'UnitTestsDuplicateCollectionName';

  return {
    tearDown: function() {
      let c = db._collection(cn);
      if (c !== null) {
        c.drop();
      }
    },

    testCreateTwoCollectionsWithTheSameName: function () {
      // This should not work!
      let r = arango.POST_RAW("/_api/collection",{name:cn},{"x-arango-async":"store"});
      // We use a direct POST request here, since one of the two will fail,
      // with a normal `db._create(cn);` we would run the risk of an exception.
      let rr = arango.POST("/_api/collection", {name:cn});
      let count = 0;
      while (true) {
        let s = arango.PUT(`/_api/job/${r.headers["x-arango-async-id"]}`,{});
        if (s.status !== 204) {
          break;
        }
        internal.wait(1);
        if (++count > 10) {
          assertTrue(false, "Async job did not finish quickly enough!");
        }
      }
      let c;
      count = 0;
      while (true) {
        // Reconnect to clear collection cache in arangosh:
        arango.reconnect(arango.getEndpoint(), db._name(), arango.connectedUser(), "");
        c = db._collection(cn);
        if (c !== null) {
          break;
        }
        if (++count > 10) {
          assertTrue(false, "Collection did not appear quickly enough!");
        }
      }
      c.drop();  // this will drop one copy (should be the only one)
      // Give `arangosh` some tries to see the duplicate collection:
      for (let i = 0; i < 10; ++i) {
        // Reconnect to clear collection cache in arangosh:
        arango.reconnect(arango.getEndpoint(), db._name(), arango.connectedUser(), "");
        c = db._collection(cn);
        if (c !== null) {
          break;   // This is going badly!
        }
        internal.wait(1);
      }
      assertTrue(c === null, "Duplicate collection detected!");
    },
    
  };
}

jsunity.run(createDuplicateCollectionNameSuite);
return jsunity.done();
