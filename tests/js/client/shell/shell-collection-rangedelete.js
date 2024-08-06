/*jshint globalstrict:false, strict:false */
/*global assertEqual, assertTrue, fail */

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
/// @author Dr. Frank Celler
/// @author Copyright 2012, triAGENS GmbH, Cologne, Germany
// //////////////////////////////////////////////////////////////////////////////

const jsunity = require("jsunity");

const arangodb = require("@arangodb");
const internal = require("internal");
const db = arangodb.db;
const ERRORS = arangodb.errors;
let IM = global.instanceManager;

function CollectionRangeDeleteSuite () {
  const cn = "UnitTestsRangeDelete";
  let c;

  return {
    setUp : function () {
      IM.debugClearFailAt();
      db._drop(cn);
      c = db._create(cn);
    },

    tearDown : function () {
      IM.debugClearFailAt();
      db._drop(cn);
    },
    
    testRangeDeleteTriggerInSingleServer : function () {
      if (require("internal").isCluster()) {
        return;
      }

      let docs = [];
      for (let i = 0; i < 100000; ++i) {
        docs.push({});
        if (docs.length === 5000) {
          c.insert(docs);
          docs = [];
        }
      }
      
      assertEqual(100000, c.count());

      IM.debugSetFailAt("RocksDBRemoveLargeRangeOn");
      try {
        // should fire
        c.truncate();
        fail();
      } catch (err) {
        assertEqual(ERRORS.ERROR_DEBUG.code, err.errorNum);
      }
    },
    
    testRangeDeleteTriggersInCluster : function () {
      if (!require("internal").isCluster()) {
        return;
      }

      let docs = [];
      for (let i = 0; i < 100000; ++i) {
        docs.push({});
        if (docs.length === 5000) {
          c.insert(docs);
          docs = [];
        }
      }
      
      assertEqual(100000, c.count());

      IM.debugSetFailAt("RocksDBRemoveLargeRangeOn");
      // should fire!
      try {
        c.truncate({ compact: false });
        fail();
      } catch (err) {
        assertEqual(err.errorNum, ERRORS.ERROR_DEBUG.code);
      }
      
      assertEqual(100000, c.count());
      IM.debugClearFailAt();
      
      c.truncate({ compact: false });
      assertEqual(0, c.count());
    },

    testRangeDeleteNotTriggered : function () {
      // number of docs too small for RangeDelete
      for (let i = 0; i < 100; ++i) {
        c.insert({});
      }

      assertEqual(100, c.count());
      IM.debugSetFailAt("RocksDBRemoveLargeRangeOn");
      // should not fire
      c.truncate({ compact: false });
      assertEqual(0, c.count());
      
      for (let i = 0; i < 100; ++i) {
        c.insert({});
      }
      
      IM.debugClearFailAt();
      IM.debugSetFailAt("RocksDBRemoveLargeRangeOff");

      try {
        // should fire
        c.truncate();
        fail();
      } catch (err) {
        assertEqual(err.errorNum, ERRORS.ERROR_DEBUG.code);
      }
    },
  };
}

if (IM.debugCanUseFailAt()) {
  jsunity.run(CollectionRangeDeleteSuite);
}

return jsunity.done();
