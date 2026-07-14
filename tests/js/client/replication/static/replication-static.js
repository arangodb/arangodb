/* jshint globalstrict:false, strict:false, unused: false */
/* global ARGUMENTS */

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
// / @author Jan Steemann
// / @author Copyright 2013, triAGENS GmbH, Cologne, Germany
// //////////////////////////////////////////////////////////////////////////////

const jsunity = require('jsunity');
const {
  assertEqual, assertFalse, assertInstanceOf, assertTrue
} = jsunity.jsUnity.assertions;
const arangodb = require('@arangodb');
const db = arangodb.db;
const _ = require('lodash');
const reconnectRetry = require('@arangodb/replication-common').reconnectRetry;
const internal = require('internal');
const arango = internal.arango;

// these must match the values in the Makefile!
const replicatorUser = 'replicator-user';
const replicatorPassword = 'replicator-password';
let IM = global.instanceManager;
const leaderEndpoint = IM.arangods[0].endpoint;

const connectToLeader = function() {
  reconnectRetry(leaderEndpoint, db._name(), replicatorUser, replicatorPassword);
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief test suite for WAL tailing / batch endpoints used by arangodump
// / (unrelated to the replication applier, kept independent of it)
// //////////////////////////////////////////////////////////////////////////////

function ReplicationWalTailingSuite() {
  return {
    testTailingWithTooHighSequenceNumber: function() {
      connectToLeader();

      const dbPrefix = db._name() === '_system' ? '' : '/_db/' + encodeURIComponent(db._name());

      const {
        lastTick: snapshotTick,
        id: replicationContextId
      } = arango.POST(`${dbPrefix}/_api/replication/batch?syncerId=123`, {ttl: 120});

      const callWailTail = (tick) => {
        const result = arango.GET_RAW(`${dbPrefix}/_api/wal/tail?from=${tick}&syncerId=123`);
        assertFalse(result.error, `Expected call to succeed, but got ${JSON.stringify(result)}`);
        assertEqual(204, result.code, `Unexpected response ${JSON.stringify(result)}`);
        return result;
      };

      try {
        // use a too high sequence number
        let result = callWailTail(snapshotTick * 100000);
        assertEqual("false", result.headers["x-arango-replication-checkmore"]);
        assertEqual("0", result.headers["x-arango-replication-lastincluded"]);
        let lastScanned = result.headers["x-arango-replication-lastscanned"];
        assertTrue(lastScanned === "0" || lastScanned === result.headers["x-arango-replication-lasttick"]);
      } finally {
        arango.DELETE(`${dbPrefix}/_api/replication/batch/${replicationContextId}`);
      }
    },

    // /////////////////////////////////////////////////////////////////////////////
    //  @brief Check that different syncer IDs and their WAL ticks are tracked
    //         separately
    // /////////////////////////////////////////////////////////////////////////////

    testWalRetain: function() {
      connectToLeader();

      const dbPrefix = db._name() === '_system' ? '' : '/_db/' + encodeURIComponent(db._name());
      const http = {
        GET: (route) => arango.GET(dbPrefix + route),
        POST: (route, body) => arango.POST(dbPrefix + route, body),
        DELETE: (route) => arango.DELETE(dbPrefix + route),
      };

      // The previous tests will have leftover entries in the
      // ReplicationClientsProgressTracker. So first, we look these up to not
      // choose a duplicate id, and be able to ignore them later.

      const existingClientSyncerIds = (() => {
        const {state: {running}, clients} = http.GET(`/_api/replication/logger-state`);
        assertTrue(running);
        assertInstanceOf(Array, clients);

        return new Set(clients.map(client => client.syncerId));
      })();
      const maxExistingSyncerId = Math.max(0, ...existingClientSyncerIds);

      const [syncer0, syncer1, syncer2] = _.range(maxExistingSyncerId + 1, maxExistingSyncerId + 4);

      // Get a snapshot
      const {lastTick: snapshotTick, id: replicationContextId}
        = http.POST(`/_api/replication/batch?syncerId=${syncer0}`, {ttl: 120});

      const callWailTail = (tick, syncerId) => {
        const result = http.GET(`/_api/wal/tail?from=${tick}&syncerId=${syncerId}`);
        assertFalse(result.error, `Expected call to succeed, but got ${JSON.stringify(result)}`);
        assertEqual(204, result.code, `Unexpected response ${JSON.stringify(result)}`);
      };

      callWailTail(snapshotTick, syncer1);
      callWailTail(snapshotTick, syncer2);

      // Now that the WAL should be held, release the snapshot.
      http.DELETE(`/_api/replication/batch/${replicationContextId}`);

      const getClients = () => {
        // e.g.
        // { "state": {"running": true, "lastLogTick": "71", "lastUncommittedLogTick": "71", "totalEvents": 71, "time": "2019-07-02T14:33:32Z"},
        //   "server": {"version": "3.5.0-devel", "serverId": "172021658338700", "engine": "rocksdb"},
        //   "clients": [
        //     {"syncerId": "102", "serverId": "", "time": "2019-07-02T14:33:32Z", "expires": "2019-07-02T16:33:32Z", "lastServedTick": "71"},
        //     {"syncerId": "101", "serverId": "", "time": "2019-07-02T14:33:32Z", "expires": "2019-07-02T16:33:32Z", "lastServedTick": "71"}
        //   ]}
        let {state: {running}, clients} = http.GET(`/_api/replication/logger-state`);
        assertTrue(running);
        assertInstanceOf(Array, clients);
        // remove clients that existed at the start of the test
        clients = clients.filter(client => !existingClientSyncerIds.has(client.syncerId));
        // sort ascending by syncerId
        clients.sort((a, b) => a.syncerId - b.syncerId);

        return clients;
      };

      let clients = getClients();
      assertEqual([syncer0, syncer1, syncer2], clients.map(client => client.syncerId));
      assertEqual(snapshotTick, clients[0].lastServedTick);
      assertEqual(snapshotTick, clients[1].lastServedTick);
      assertEqual(snapshotTick, clients[2].lastServedTick);

      // Update ticks
      callWailTail(parseInt(snapshotTick) + 1, syncer1);
      callWailTail(parseInt(snapshotTick) + 2, syncer2);

      clients = getClients();
      assertEqual([syncer0, syncer1, syncer2], clients.map(client => client.syncerId));
      assertEqual(snapshotTick, clients[0].lastServedTick);
      assertEqual((parseInt(snapshotTick) + 1).toString(), clients[1].lastServedTick);
      assertEqual((parseInt(snapshotTick) + 2).toString(), clients[2].lastServedTick);
    },
  };
}

jsunity.run(ReplicationWalTailingSuite);

return jsunity.done();
