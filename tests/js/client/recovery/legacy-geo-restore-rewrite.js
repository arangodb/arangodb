/* jshint globalstrict:false, strict:false, unused:false */
/* global runSetup assertEqual, assertNotNull, assertTrue */

'use strict';

const jsunity = require('jsunity');
const internal = require('internal');
const db = require('@arangodb').db;
const fs = require('fs');

const pu = require('@arangodb/testutils/process-utils');
const { executeExternalAndWaitWithSanitizer } = require('@arangodb/test-helper');

const fixtureDir = internal.pathForTesting('fixtures/legacy-geo/dump', '');

const dbName = 'LegacyGeoFixtureDB';
const cn = 'places';

function addConnectionArgs(args) {
  let endpoint = internal.arango.getEndpoint()
    .replace(/\+vpp/, '')
    .replace(/^http:/, 'tcp:')
    .replace(/^https:/, 'ssl:')
    .replace(/^h2:/, 'tcp:');

  args.push('--server.endpoint');
  args.push(endpoint);
  args.push('--server.database');
  args.push(dbName);
  args.push('--server.username');
  args.push(internal.arango.connectedUser());
}

function restoreLegacyGeoFixture() {
  const inputDir = fs.normalize(fs.makeAbsolute(fixtureDir));

  const restoreBin = pu.ARANGORESTORE_BIN;
  if (!restoreBin || !fs.isFile(restoreBin)) {
    throw new Error('arangorestore not found! ARANGORESTORE_BIN=' + restoreBin);
  }

  const args = ['--create-database', 'true', '--input-directory', inputDir];
  addConnectionArgs(args);

  const res = executeExternalAndWaitWithSanitizer(
    restoreBin, args, 'legacy-geo-restore-rewrite'
  );

  if (!res.hasOwnProperty('exit')) {
    throw new Error('arangorestore returned unexpected result: ' + JSON.stringify(res));
  }
  if (res.exit !== 0) {
    throw new Error('arangorestore failed: ' + JSON.stringify(res));
  }
}

if (runSetup === true) {
  'use strict';
  db._useDatabase('_system');
  try { db._dropDatabase(dbName); } catch (e) {}
  restoreLegacyGeoFixture();
  return 0;
}

function testSuite() {
  jsunity.jsUnity.attachAssertions();
  return {
    setUpAll: function () {
      db._useDatabase(dbName);
    },

    tearDownAll: function () {
      db._useDatabase('_system');
    },

    testRestoreRewritesLegacyGeoTypesToGeo: function () {
      const c = db._collection(cn);
      assertTrue(c !== null, 'collection places should exist after restore');

      assertEqual(441, c.count(), 'expected deterministic doc count');

      const idx = c.indexes();

      let geoLoc = null;
      let geoLatLon = null;

      for (const i of idx) {
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

      const res = db._query(`
        FOR d IN ${cn}
          LET point = HAS(d, "loc") ? d.loc : GEO_POINT(d.lon, d.lat)
          SORT GEO_DISTANCE(point, [0, 0]) ASC
          LIMIT 10
          RETURN d
      `).toArray();

      assertEqual(10, res.length, 'GEO_DISTANCE query should return 10 docs');
    }
  };
}

jsunity.run(testSuite);
return jsunity.done();