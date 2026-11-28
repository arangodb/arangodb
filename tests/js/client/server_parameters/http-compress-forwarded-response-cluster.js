/*jshint globalstrict:false, strict:false */
/* global getOptions, assertEqual, assertTrue, assertNotEqual */

// //////////////////////////////////////////////////////////////////////////////
// / DISCLAIMER
// /
// / Copyright 2014-2026 ArangoDB GmbH, Cologne, Germany
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
    'http.compress-response-threshold': '1024',
    'opts': {
      coordinators: 2
    }
  };
}

const jsunity = require('jsunity');
const request = require('@arangodb/request');
const { instanceRole } = require('@arangodb/testutils/instance');
const IM = global.instanceManager;

function CompressForwardedResponseSuite() {
  const largeBatchQuery = 'FOR i IN 1..10000 RETURN CONCAT("value-", i)';
  let coordinators = [];

  const send = (coordinator, method, path, body, headers = {}) => {
    return request({
      method,
      url: coordinator.url + path,
      body: JSON.stringify(body),
      auth: { username: 'root', password: '' },
      headers: Object.assign({ 'accept-encoding': 'gzip' }, headers),
    });
  };

  const assertCompressedJson = (res) => {
    assertEqual('gzip', res.headers['content-encoding'], res.headers);
    assertEqual('application/json; charset=utf-8', res.headers['content-type'],
                res.headers);
  };

  return {
    setUpAll: function () {
      coordinators = IM.getInstancesRole(instanceRole.coordinator);
      assertTrue(coordinators.length >= 2, coordinators.length);
    },

    testCursorContinuationOnOtherCoordinatorIsCompressed: function () {
      const create = send(coordinators[0], 'POST', '/_api/cursor',
                          { query: largeBatchQuery, batchSize: 1000 },
                          { 'accept-encoding': 'identity' });
      assertEqual(201, create.status);
      const cursorId = JSON.parse(create.body).id;
      try {
        const next = send(coordinators[1], 'POST', `/_api/cursor/${cursorId}`, {});
        assertEqual(200, next.status);
        assertNotEqual(undefined, next.headers['x-arango-request-forwarded-to']);
        assertCompressedJson(next);
      } finally {
        send(coordinators[0], 'DELETE', `/_api/cursor/${cursorId}`, {});
      }
    },

    testStreamTransactionQueryOnOtherCoordinatorIsCompressed: function () {
      const begin = send(coordinators[0], 'POST', '/_api/transaction/begin',
                         { collections: {} }, { 'accept-encoding': 'identity' });
      assertEqual(201, begin.status);
      const trxId = JSON.parse(begin.body).result.id;
      try {
        const query = send(coordinators[1], 'POST', '/_api/cursor',
                           { query: largeBatchQuery, batchSize: 10000 },
                           { 'x-arango-trx-id': trxId });
        assertEqual(201, query.status);
        assertNotEqual(undefined, query.headers['x-arango-request-forwarded-to']);
        assertCompressedJson(query);
      } finally {
        send(coordinators[0], 'DELETE', `/_api/transaction/${trxId}`, {});
      }
    },
  };
}

jsunity.run(CompressForwardedResponseSuite);
return jsunity.done();
