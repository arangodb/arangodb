/*jshint globalstrict:false, strict:false */
/* global getOptions */

// //////////////////////////////////////////////////////////////////////////////
// / DISCLAIMER
// /
// / Copyright 2014-2026 ArangoDB GmbH, Cologne, Germany
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

if (getOptions === true) {
  return {
    'server.session-timeout': '10',
    'server.authentication': 'true',
    'server.jwt-secret': 'haxxmann',
  };
}

const jsunity = require('jsunity');
const { assertEqual, assertNotEqual, assertTrue } = jsunity.jsUnity.assertions;
const db = require('@arangodb').db;
const fs = require('fs');
const internal = require('internal');
const request = require('@arangodb/request');
const crypto = require('@arangodb/crypto');
const pu = require('@arangodb/testutils/process-utils');
const { executeExternalAndWaitWithSanitizer } = require('@arangodb/test-helper');
const IM = global.instanceManager;

// must match 'server.session-timeout' above
const tokenLifetimeSeconds = 10;
// makes every /_api/dump/next request sleep 200 ms on the server
const slowFetchFailurePoint = 'RestDumpHandler::slow-next';

function dumpJwtTokenRenewalSuite() {
  'use strict';
  const cn = 'UnitTestsDumpJwtRenewal';
  // arangodump never fetches fewer than 100 documents per batch, so this gives
  // 100 sequential fetches of 200 ms each: the dump outlives the 10 s token
  const documentCount = 10000;

  const fetchUserToken = () => {
    const result = request.post({
      url: IM.url + '/_open/auth',
      body: { username: 'root', password: '' },
      json: true,
    });
    assertEqual(200, result.statusCode, JSON.stringify(result.json));
    return result.json.jwt;
  };

  const runDump = (outputDirectory, token, renewalThresholdSeconds) => executeExternalAndWaitWithSanitizer(
    pu.ARANGODUMP_BIN, [
      '--collection', cn,
      '--output-directory', outputDirectory,
      '--overwrite', 'true',
      '--compress-output', 'false',
      '--docs-per-batch', '100',
      '--threads', '1',
      '--local-network-threads', '1',
      '--server.endpoint', IM.endpoint,
      '--server.database', db._name(),
      '--server.jwt-token', token,
      '--server.jwt-renewal-threshold', String(renewalThresholdSeconds),
    ], 'dump-jwt-token-renewal');

  return {
    setUpAll: function () {
      const collection = db._create(cn, { numberOfShards: 3 });
      let docs = [];
      for (let i = 0; i < documentCount; ++i) {
        docs.push({ value: i });
        if (docs.length === 1000) {
          collection.insert(docs);
          docs = [];
        }
      }
      if (IM.debugCanUseFailAt()) {
        IM.debugSetFailAt(slowFetchFailurePoint);
      }
    },

    // the collection is deliberately not dropped: the aborted dump leaves a
    // server-side dump context behind that pins the collection until its
    // time-to-live expires, and the instance is discarded after this file anyway
    tearDownAll: function () {
      if (IM.debugCanUseFailAt()) {
        IM.debugClearFailAt(slowFetchFailurePoint);
      }
    },

    testDumpSurvivesTokenExpiry: function () {
      if (!IM.debugCanUseFailAt()) {
        return;
      }
      const outputDirectory = fs.getTempFile();
      fs.makeDirectory(outputDirectory);
      try {
        const start = internal.time();
        // renew 4 s before expiry: enough slack for slow CI machines
        const rc = runDump(outputDirectory, fetchUserToken(), 4);
        const durationSeconds = internal.time() - start;
        assertEqual(0, rc.exit, `arangodump aborted: ${JSON.stringify(rc)}`);
        assertTrue(durationSeconds > tokenLifetimeSeconds,
          `dump took only ${durationSeconds}s and cannot have outlived the token; increase documentCount`);
        const dataFile = fs.join(outputDirectory, cn + '_' + crypto.md5(cn) + '.data.json');
        assertTrue(fs.isFile(dataFile), `no data file in ${JSON.stringify(fs.list(outputDirectory))}`);
        assertEqual(documentCount + 1, fs.readFileSync(dataFile).toString().split('\n').length);
      } finally {
        fs.removeDirectoryRecursive(outputDirectory, true);
      }
    },

    // with a threshold of 0 the token is only renewed once it has expired,
    // which the server refuses; this proves the dump really outlives the token
    testDumpAbortsWithoutRenewal: function () {
      if (!IM.debugCanUseFailAt()) {
        return;
      }
      const outputDirectory = fs.getTempFile();
      fs.makeDirectory(outputDirectory);
      try {
        const start = internal.time();
        const rc = runDump(outputDirectory, fetchUserToken(), 0);
        const durationSeconds = internal.time() - start;
        assertNotEqual(0, rc.exit, 'arangodump finished although its token expired');
        // the server truncates the token's issue time to whole seconds, so the
        // token may expire up to one second earlier than its nominal lifetime
        assertTrue(durationSeconds >= tokenLifetimeSeconds - 1,
          `arangodump aborted after ${durationSeconds}s, before its token could expire: ${JSON.stringify(rc)}`);
      } finally {
        fs.removeDirectoryRecursive(outputDirectory, true);
      }
    },
  };
}

jsunity.run(dumpJwtTokenRenewalSuite);
return jsunity.done();
