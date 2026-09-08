/* jshint globalstrict:false, strict:false, maxlen: 200 */
/* global db, fail, arango, assertTrue, assertFalse, assertEqual, assertNotUndefined */

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
// //////////////////////////////////////////////////////////////////////////////

'use strict';
const arangodb = require("@arangodb");
const internal = require('internal');
const isCluster = internal.isCluster();
let db = arangodb.db;
const jsunity = require("jsunity");

////////////////////////////////////////////////////////////////////////////////;
// error handling;
////////////////////////////////////////////////////////////////////////////////;
function creationSuite () {
  let cn = "test";
  let vn = "vtest";
  let systemDb = '_system';
  let testDb = 'test';

  function getArangoSearchStats (dbName) {

    if (dbName == null || dbName === "") {
      dbName = systemDb;
    }
    let api = `/_arango/experimental/_db/${dbName}/_admin/arangosearch/stats`;
    return arango.GET(api);
  }

  return {
    setUpAll: function() {

      if (isCluster) {
        return;
      }

      //  Set up testArangosearchViewStats
      {
        let coll = db._create(cn);
        db._createView(vn, "arangosearch", {
          consolidationIntervalMsec: 5000,
          consolidationPolicy: {
              type: "tier",
              maxSkewThreshold: 0.2,
          },
          links: {
            [cn]: {includeAllFields: true}
          }
        });
      }

      //  Set up testDifferentDbAndInvertedIndexStats
      {
        db._createDatabase(testDb);
        db._useDatabase(testDb);
        let coll = db._create(cn);
        coll.ensureIndex({"type": "inverted", "name": "inverted", "fields": ["name", "age"]});
      }
    },

    tearDownAll: function() {

      if (isCluster) {
        return;
      }

      //  Cleanup testArangosearchViewStats
      db._useDatabase(systemDb);
      db._dropView(vn);
      db._drop(cn);

      //  Cleanup testDifferentDbAndInvertedIndexStats
      db._useDatabase(testDb);
      db._drop(cn);
      db._useDatabase(systemDb);
      db._dropDatabase(testDb);
    },

    testArangosearchViewStats: function() {

      if (isCluster) {
        return;
      }

      //  Add a few docs to the collection and get ArangoSearch stats.
      //  Then add another doc which will create a new segment.
      //  Run a search query with waitForSync: true to ensure the view is synced.
      //  Then delete a doc and check that the stats are updated accordingly.
      db._useDatabase(systemDb);
      let coll = db._collection(cn);
      coll.insert({ name: "Gustavo", age: 21 });
      coll.insert({ name: "Tyrus", age: 29 });
      let jimmy = coll.insert({ name: "Jimmy", age: 31 });
      coll.insert({ name: "Kim", age: 15 });
      coll.insert({ name: "Nacho", age: 18 });

      //  wait till view is updated
      db._query("FOR d IN " + vn + " OPTIONS { waitForSync: true } RETURN d");
      let result = getArangoSearchStats();

      //  general stats
      assertEqual(result.numDocs, 5);
      assertEqual(result.numFiles, 6);
      assertEqual(result.numSegments, 1);
      assertEqual(result.deletionRatio, 0.0);

      //  segment info
      let segment = result.segments[0];
      assertEqual(segment.name, "_1");
      assertEqual(segment.deletionRatio, 0.0);
      assertEqual(segment.numDocs, 5);
      assertEqual(segment.numLiveDocs, 5);

      //  Add one more doc to create a new segment.
      coll.insert({ name: "Francesca", age: 35 });

      db._query("FOR d IN " + vn + " OPTIONS { waitForSync: true } RETURN d");
      result = getArangoSearchStats("_system");
      assertEqual(result.numDocs, 6);
      assertEqual(result.numSegments, 2);

      let segment1 = result.segments[0];
      let segment2 = result.segments[1];
      assertEqual(segment1.name, "_1");
      assertEqual(segment2.name, "_2");

      assertEqual(segment1.numDocs, 5);
      assertEqual(segment2.numLiveDocs, 1);

      //  Delete a doc.
      db._remove(jimmy._id);

      db._query("FOR d IN " + vn + " OPTIONS { waitForSync: true } RETURN d");
      result = getArangoSearchStats("_system");

      //  general stats
      assertEqual(result.numDocs, 6);
      assertEqual(result.numLiveDocs, 5);
      assertEqual(result.deletionRatio, 0.17);

      //  segment stats
      let segments = result.segments;
      assertEqual(segments[0].deletionRatio, 0.2);
      assertEqual(segments[1].deletionRatio, 0);
    },

    testDifferentDbAndInvertedIndexStats : function() {

      if (isCluster) {
        return;
      }

      db._useDatabase(testDb);
      let result = getArangoSearchStats(testDb);

      //  general stats
      assertEqual(result.numDocs, 0);
      assertEqual(result.numFiles, 1);
      assertEqual(result.numSegments, 0);

      let coll = db._collection(cn);
      coll.insert({ name: "Lalo", age: 39 });
      coll.insert({ name: "Bolsa", age: 40 });
      db._query("for d in " + cn + " OPTIONS { indexHint: 'inverted', forceIndexHint: true," +
          "waitForSync: true } filter d.name == 'Bolsa' return d");

      result = getArangoSearchStats(testDb);
      assertEqual(result.numDocs, 2);
      assertEqual(result.numFiles, 6);
      assertEqual(result.numSegments, 1);
    }
  };
}

jsunity.run(creationSuite);
return jsunity.done();
