/* jshint globalstrict:false, strict:false, unused:false */
/* global runSetup assertEqual, assertNotNull, assertTrue, print */

'use strict';

const jsunity = require('jsunity');
const internal = require('internal');
const db = require('@arangodb').db;
const fs = require('fs');

const pu = require('@arangodb/testutils/process-utils');
const { executeExternalAndWaitWithSanitizer } = require('@arangodb/test-helper');

const fixtureDir = internal.pathForTesting('fixtures/legacy-geo/dump', '');

// #region agent log
function _debugLog(location, message, data, hypothesisId) {
  const payload = { location, message, data: data || {}, timestamp: Date.now(), hypothesisId };
  print('[legacy-geo-restore] ' + JSON.stringify(payload));
}
// #endregion

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
  // #region agent log
  const fixtureExists = fs.isDirectory(fixtureDir);
  let fixtureFiles = [];
  try {
    if (fixtureExists) {
      fixtureFiles = fs.list(fixtureDir).slice(0, 20);
    }
  } catch (e) {
    fixtureFiles = ['list_error: ' + e.message];
  }
  print('[legacy-geo-restore] fixtureDir=' + fixtureDir + ' exists=' + fixtureExists + ' fileCount=' + fixtureFiles.length);
  if (fixtureFiles.length > 0) {
    print('[legacy-geo-restore] fixtureFiles: ' + fixtureFiles.join(', '));
  }
  _debugLog('legacy-geo-restore-rewrite.js:restoreLegacyGeoFixture:entry', 'fixture path and existence', { fixtureDir, fixtureExists, fixtureFilesCount: fixtureFiles.length, fixtureFiles }, 'H1');
  // #endregion

  // Find arangorestore binary used by the test framework
  const restoreBin = pu.ARANGORESTORE_BIN || pu.arangorestoreBin || 'arangorestore';
  const args = ['--create-database', 'true', '--input-directory', fixtureDir];
  addConnectionArgs(args);

  // #region agent log
  const endpoint = internal.arango.getEndpoint();
  print('[legacy-geo-restore] restoreBin=' + restoreBin + ' endpoint=' + endpoint);
  print('[legacy-geo-restore] args: ' + JSON.stringify(args));
  _debugLog('legacy-geo-restore-rewrite.js:restoreLegacyGeoFixture:before_restore', 'binary and connection', { restoreBin, endpoint, argsLength: args.length, args }, 'H2-H3');
  // #endregion

  // Capture stderr so we can print the real error when restore fails (CI only gets exit code otherwise)
  const stderrFile = fs.join(fs.getTempPath(), 'legacy-geo-restore-rewrite.stderr');
  const quote = function (s) { return "'" + String(s).replace(/'/g, "'\"'\"'") + "'"; };
  const shellCmd = restoreBin + ' ' + args.map(quote).join(' ') + ' 2>"' + stderrFile + '"';
  const res = executeExternalAndWaitWithSanitizer('sh', ['-c', shellCmd], 'legacy-geo-restore-rewrite');

  // #region agent log
  const resKeys = res ? Object.keys(res) : [];
  const resSafe = res ? {} : {};
  if (res) {
    resKeys.forEach(function (k) { resSafe[k] = res[k]; });
  }
  print('[legacy-geo-restore] arangorestore result: exit=' + (res && res.exit) + ' status=' + (res && res.status) + ' keys=' + resKeys.join(','));
  print('[legacy-geo-restore] full result: ' + JSON.stringify(resSafe));
  if (res && res.exit !== 0 && fs.exists(stderrFile)) {
    try {
      const stderrContent = fs.read(stderrFile);
      print('[legacy-geo-restore] arangorestore stderr (this is the real error):');
      print(stderrContent);
    } catch (e) {
      print('[legacy-geo-restore] could not read stderr file: ' + e.message);
    }
  }
  _debugLog('legacy-geo-restore-rewrite.js:restoreLegacyGeoFixture:after_restore', 'arangorestore result', { resKeys, resSafe, exit: res && res.exit, status: res && res.status }, 'H4-H5');
  // #endregion

  if (!res.hasOwnProperty('exit')) {
    print('[legacy-geo-restore] FAIL: unexpected result (no exit property)');
    throw new Error('arangorestore returned unexpected result: ' + JSON.stringify(res));
  }
  if (res.exit !== 0) {
    print('[legacy-geo-restore] FAIL: arangorestore exited with ' + res.exit + '. See stderr above. To re-run: ' + restoreBin + ' ' + args.join(' '));
    throw new Error('arangorestore failed: ' + JSON.stringify(res));
  }
  print('[legacy-geo-restore] restore completed successfully');
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

      // Functional check via AQL (avoid SimpleQueryNear / c.near(), which may 404)
      const res = db._query(`
        FOR d IN NEAR(${cn}, 0, 0, 10)
          RETURN d
      `).toArray();

      assertEqual(10, res.length, 'NEAR() should return 10 docs');
    }
  };
}

jsunity.run(testSuite);
return jsunity.done();