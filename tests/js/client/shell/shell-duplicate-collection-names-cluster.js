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
      let c = db._create(cn);
      while (true) {
        let s = arango.PUT(`/_api/job/${r.headers["x-arango-async-id"]}`,{});
        if (s.status !== 204) {
          break;
        }
      }
      c.drop();
      c = db._collection(cn);
      assertTrue(c === null, "Duplicate collection detected!");
    },
    
  };
}

jsunity.run(createDuplicateCollectionNameSuite);
return jsunity.done();
