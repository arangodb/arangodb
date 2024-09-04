/*jshint globalstrict:false, strict:false */
/*global assertEqual, assertNotEqual */

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
// //////////////////////////////////////////////////////////////////////////////

let jsunity = require("jsunity");
let arangodb = require("@arangodb");
let internal = require("internal");
let db = arangodb.db;
let { getEndpointById,
      getEndpointsByType,
      reconnectRetry
    } = require('@arangodb/test-helper');
let { instanceRole } = require('@arangodb/testutils/instance');
let IM = global.instanceManager;

function RecalculateCountSuite() {
  const cn = "UnitTestsCollection";

  return {

    setUp : function () {
      IM.debugClearFailAt('', instanceRole.dbServer);
      db._drop(cn);
    },

    tearDown : function () {
      IM.debugClearFailAt('', instanceRole.dbServer);
      db._drop(cn);
    },
    
    testFixBrokenCounts : function () {
      let c = db._create(cn, { numberOfShards: 5 });

      IM.debugSetFailAt("DisableCommitCounts", instanceRole.dbServer);

      for (let i = 0; i < 1000; ++i) {
        c.insert({});
      }

      assertNotEqual(1000, c.count());
      assertEqual(1000, c.toArray().length);

      IM.debugClearFailAt('', instanceRole.dbServer);
 
      c.recalculateCount();
      
      assertEqual(1000, c.count());
      assertEqual(1000, c.toArray().length);
    },
    
  };
}

jsunity.run(RecalculateCountSuite);
return jsunity.done();
