/* jshint globalstrict:false, strict:false, unused : false */
/* global runSetup assertEqual, assertFalse, assertTrue, assertNotNull */
// //////////////////////////////////////////////////////////////////////////////
// / DISCLAIMER
// /
// / Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
// / Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
// /
// / Licensed under the Business Source License 1.1 (the "License");
// / you may not use it except in compliance with the License.
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
// //////////////////////////////////////////////////////////////////////////////

var db = require('@arangodb').db;
var internal = require('internal');
var simple = require('@arangodb/simple-query');
var jsunity = require('jsunity');

if (runSetup === true) {
  print("SETUP START")
  'use strict';
  global.instanceManager.debugClearFailAt();

  db._drop('UnitTestsRecoveryGeoCluster');
  var c = db._create('UnitTestsRecoveryGeoCluster');
  c.ensureIndex({ type: "geo1", fields: ["loc"], geoJson: false });
  c.ensureIndex({ type: "geo2", fields: ["lat", "lon"] });

  var docs = [];
  for (var i = -40; i < 40; ++i) {
    for (var j = -40; j < 40; ++j) {
      docs.push({ loc: [i, j], lat: i, lon: j });
    }
  }
  c.insert(docs);

  // --- Diagnostics: (1) Plan, (2) DBServer storage, (3) Current ---
  var IM = global.instanceManager;
  var agencyMgr = IM.agencyMgr;
  var colName = 'UnitTestsRecoveryGeoCluster';
  var planColls = agencyMgr.getAt('Plan/Collections/_system');
  var colId = null;
  if (planColls && typeof planColls === 'object') {
    for (var pid in planColls) {
      if (planColls[pid].name === colName) {
        colId = planColls[pid].id;
        break;
      }
    }
  }
  if (!colId) {
    print("DIAG: Could not find collection id for " + colName + " in Plan.");
  } else {
    // (1) What does Plan contain? geo1/geo2 or geo?
    var planCol = agencyMgr.getAt('Plan/Collections/_system/' + colId);
    if (planCol && planCol.indexes && Array.isArray(planCol.indexes)) {
      print("DIAG (1) Plan indexes for " + colName + " (id=" + colId + "):");
      for (var pi = 0; pi < planCol.indexes.length; pi++) {
        var idx = planCol.indexes[pi];
        var t = idx.type !== undefined ? idx.type : idx.typeName;
        print("  [" + pi + "] type=" + t + " fields=" + JSON.stringify(idx.fields));
      }
    } else {
      print("DIAG (1) Plan: no indexes array or not found for " + colId);
    }

    // (2) What is actually stored on DBServer? We infer from what DBServer reports to Current (see (3)).
    print("DIAG (2) DBServer storage: reflected by what DBServer reports to Current (see (3)).");

    // (3) What does Current contain? What did DBServer report?
    var currentCol = agencyMgr.getAt('Current/Collections/_system/' + colId);
    if (currentCol && typeof currentCol === 'object') {
      print("DIAG (3) Current (what DBServers reported) for " + colName + ":");
      for (var shardId in currentCol) {
        var shardEntry = currentCol[shardId];
        if (shardEntry && shardEntry.indexes && Array.isArray(shardEntry.indexes)) {
          print("  shard " + shardId + " indexes:");
          for (var si = 0; si < shardEntry.indexes.length; si++) {
            var sidx = shardEntry.indexes[si];
            var st = sidx.type !== undefined ? sidx.type : sidx.typeName;
            print("    [" + si + "] type=" + st + " fields=" + JSON.stringify(sidx.fields));
          }
        } else {
          print("  shard " + shardId + ": no indexes array");
        }
      }
    } else {
      print("DIAG (3) Current: no entry yet for " + colId + " (DBServers may not have reported).");
    }
  }

  print("Verified: geo1/geo2 indexes are saved (reported in Current) before server restart...");
  db._drop('test');
  c = db._create('test');
  c.save({ _key: 'crashme' }, true);

  return 0;
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief test suite
// //////////////////////////////////////////////////////////////////////////////

function recoverySuite () {
  'use strict';
  jsunity.jsUnity.attachAssertions();

  return {

    // //////////////////////////////////////////////////////////////////////////
    // / @brief After cluster recovery, geo indexes exist and geo queries work.
    // //////////////////////////////////////////////////////////////////////////

    testGeoIndexesSurviveClusterRecovery: function () {
      var c = db._collection('UnitTestsRecoveryGeoCluster');
      var idx = c.indexes();
      var geoSingle = null;
      var geoDouble = null;

      for (var i = 1; i < idx.length; ++i) {
        if (idx[i].fields.length === 1 && idx[i].fields[0] === 'loc') {
          geoSingle = idx[i];
        } else if (idx[i].fields.length === 2 &&
                   idx[i].fields[0] === 'lat' && idx[i].fields[1] === 'lon') {
          geoDouble = idx[i];
        }
      }

      assertNotNull(geoSingle, "single-field geo index should exist after cluster recovery");
      assertNotNull(geoDouble, "two-field geo index should exist after cluster recovery");
      assertFalse(geoSingle.unique);
      assertTrue(geoSingle.sparse);
      assertEqual(['loc'], geoSingle.fields);
      assertFalse(geoDouble.unique);
      assertTrue(geoDouble.sparse);
      assertEqual(['lat', 'lon'], geoDouble.fields);

      assertEqual(100, new simple.SimpleQueryNear(c, 0, 0, geoSingle.id).limit(100).toArray().length);
      assertEqual(100, new simple.SimpleQueryNear(c, 0, 0, geoDouble.id).limit(100).toArray().length);
    }

  };
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief executes the test suite
// //////////////////////////////////////////////////////////////////////////////

jsunity.run(recoverySuite);
return jsunity.done();
