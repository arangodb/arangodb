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
    'server.session-timeout': '5',
    'server.authentication': 'true',
    'server.jwt-secret': 'haxxmann',
  };
}

const jsunity = require('jsunity');
const { assertEqual, assertTrue } = jsunity.jsUnity.assertions;
const db = require('@arangodb').db;
const fs = require('fs');
const internal = require('internal');
const request = require('@arangodb/request');
const crypto = require('@arangodb/crypto');
const pu = require('@arangodb/testutils/process-utils');
const { executeExternalAndWaitWithSanitizer } = require('@arangodb/test-helper');
const IM = global.instanceManager;

// must match 'server.session-timeout' above
const tokenLifetimeSeconds = 5;
// makes every /_api/dump/next request sleep 200 ms on the server
const slowFetchFailurePoint = 'RestDumpHandler::slow-next';

function dumpJwtTokenRenewalSuite() {
  'use strict';
  const cn = 'UnitTestsDumpJwtRenewal';
  // arangodump never fetches fewer than 100 documents per batch, so this gives
  // 50 sequential fetches of 200 ms each: the dump outlives the 5 s token
  const documentCount = 5000;

  const fetchUserToken = () => {
    const result = request.post({
      url: IM.url + '/_open/auth',
      body: { username: 'root', password: '' },
      json: true,
    });
    assertEqual(200, result.statusCode, JSON.stringify(result.json));
    return result.json.jwt;
  };

  const runDump = (outputDirectory, token) => executeExternalAndWaitWithSanitizer(
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
      '--server.jwt-renewal-threshold', '1',
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

    tearDownAll: function () {
      if (IM.debugCanUseFailAt()) {
        IM.debugClearFailAt(slowFetchFailurePoint);
      }
      db._drop(cn);
    },

    testDumpSurvivesTokenExpiry: function () {
      if (!IM.debugCanUseFailAt()) {
        return;
      }
      const outputDirectory = fs.getTempFile();
      fs.makeDirectory(outputDirectory);
      try {
        const start = internal.time();
        const rc = runDump(outputDirectory, fetchUserToken());
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
  };
}

jsunity.run(dumpJwtTokenRenewalSuite);
return jsunity.done();
