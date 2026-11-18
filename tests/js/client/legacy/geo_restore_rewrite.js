/* jshint globalstrict:false, strict:false, unused:false */

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

const jsunity = require('jsunity');
const {assertTrue, assertFalse, assertEqual} = jsunity.jsUnity.assertions;
const internal = require('internal');
const db = require('@arangodb').db;
const fs = require('fs');
const ct = require('@arangodb/testutils/client-tools');
const IM = global.instanceManager;

const fixtureDir = fs.join(internal.pathForTesting('common'), 'test-data/legacy-geo/dump');
const dbName = 'LegacyGeoFixtureDB';
const collectionName = 'places';
let previousDbName = '_system';
let options = IM.options;

function restoreLegacyGeoFixture() {
  let restoreConfig = ct.createBaseConfig('restore', options, IM, dbName);
  restoreConfig.setRootDir('');
  restoreConfig.setInputDirectory(fs.normalize(fs.makeAbsolute(fixtureDir)), true);
  restoreConfig.setIncludeSystem(false);
  restoreConfig.setEndpoint(IM.endpoint);

  if (options.hasOwnProperty("threads")) {
    restoreConfig.setThreads(options.threads);
  }
  return ct.run.arangoDumpRestoreWithConfig(restoreConfig, options, IM.rootDir, options.coreCheck);
}


function testSuite() {
  jsunity.jsUnity.attachAssertions();
  return {
    setUpAll: function() {
      previousDbName = db._name();
      restoreLegacyGeoFixture();
      db._useDatabase(dbName);
    },

    tearDownAll: function() {
      db._useDatabase(previousDbName);
      db._dropDatabase(dbName);
    },

    testRestoreRewritesLegacyGeoTypesToGeo: function() {
      const collection = db._collection(collectionName);
      assertTrue(collection !== null, 'collection places should exist after restore');

      assertEqual(441, collection.count(), 'expected deterministic doc count');

      const indexes = collection.indexes();

      let geoLoc = null;
      let geoLatLon = null;

      for (const i of indexes) {
        if (i.fields && i.fields.length === 1 && i.fields[0] === 'loc') {
          geoLoc = i;
        } else if (i.fields && i.fields.length === 2 && i.fields[0] === 'lat' && i.fields[1] === 'lon') {
          geoLatLon = i;
        }
      }

      assertTrue(geoLoc !== null, 'geo index on loc should exist');
      assertTrue(geoLatLon !== null, 'geo index on lat/lon should exist');

      // Key check: legacy types are rewritten to canonical "geo" on restore
      assertEqual('geo', geoLoc.type);
      assertEqual('geo', geoLatLon.type);

      const result = db._query(`
        FOR d IN ${collectionName}
          LET point = HAS(d, "loc") ? d.loc : GEO_POINT(d.lon, d.lat)
          SORT GEO_DISTANCE(point, [0, 0]) ASC
          LIMIT 10
          RETURN d
      `).toArray();

      assertEqual(10, result.length, 'GEO_DISTANCE query should return 10 docs');
    }
  };
}

jsunity.run(testSuite);
return jsunity.done();
