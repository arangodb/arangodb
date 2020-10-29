/*jshint globalstrict:false, strict:false */
/*global assertEqual, assertTrue, fail */

////////////////////////////////////////////////////////////////////////////////
/// @brief tests range deletion
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
/// Copyright holder is triAGENS GmbH, Cologne, Germany
///
/// @author Dr. Frank Celler
/// @author Copyright 2012, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

const jsunity = require("jsunity");

const arangodb = require("@arangodb");
const internal = require("internal");
const db = arangodb.db;
const ERRORS = arangodb.errors;

function CollectionRangeDeleteSuite () {
  const cn = "UnitTestsRangeDelete";
  let c;

  return {
    setUp : function () {
      internal.debugClearFailAt();
      db._drop(cn);
      c = db._create(cn);
    },

    tearDown : function () {
      internal.debugClearFailAt();
      db._drop(cn);
    },
    
    testRangeDeleteTriggerInSingleServer : function () {
      if (require("@arangodb/cluster").isCluster()) {
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

      internal.debugSetFailAt("RocksDBRemoveLargeRangeOn");
      try {
        // should fire
        c.truncate();
        fail();
      } catch (err) {
        assertEqual(ERRORS.ERROR_DEBUG.code, err.errorNum);
      }
    },
    
    testRangeDeleteDontTriggerInCluster : function () {
      if (!require("@arangodb/cluster").isCluster()) {
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

      internal.debugSetFailAt("RocksDBRemoveLargeRangeOn");
      // should not fire
      c.truncate({ compact: false });
      
      assertEqual(0, c.count());
    },

    testRangeDeleteNotTriggered : function () {
      // number of docs too small for RangeDelete
      for (let i = 0; i < 100; ++i) {
        c.insert({});
      }

      assertEqual(100, c.count());
      internal.debugSetFailAt("RocksDBRemoveLargeRangeOn");
      // should not fire
      c.truncate({ compact: false });
      assertEqual(0, c.count());
      
      for (let i = 0; i < 100; ++i) {
        c.insert({});
      }
      
      internal.debugClearFailAt();
      internal.debugSetFailAt("RocksDBRemoveLargeRangeOff");

      try {
        // should fire
        c.truncate();
        fail();
      } catch (err) {
        assertTrue(err.errorNum === ERRORS.ERROR_DEBUG.code ||
                   err.errorNum === ERRORS.ERROR_CLUSTER_COULD_NOT_TRUNCATE_COLLECTION.code);
      }
    },
  };
}

if (internal.debugCanUseFailAt()) {
  jsunity.run(CollectionRangeDeleteSuite);
}

return jsunity.done();
