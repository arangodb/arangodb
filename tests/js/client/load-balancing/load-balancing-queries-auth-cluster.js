/*jshint globalstrict:false, strict:false */
/* global assertTrue, assertFalse, assertEqual */

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
// //////////////////////////////////////////////////////////////////////////////

// Per-user scoping of GET /_api/query/current, GET /_api/query/slow,
// DELETE /_api/query/{id} and DELETE /_api/query/slow across coordinators
// (COR-1023). alice's queries run on one coordinator; every request of the
// tests goes to the other one. The listings fan out from the asked
// coordinator to the owning one and the kill is forwarded to it, so this
// checks that the caller's identity survives the hop: another read-only user
// sees, kills and clears nothing of alice's, while alice and an admin (root,
// read-write access to _system) get through.

'use strict';
const jsunity = require("jsunity");
const { assertEqual, assertTrue } = jsunity.jsUnity.assertions;
const internal = require('internal');
const base64Encode = internal.base64Encode;
const request = require("@arangodb/request");
const userModule = require("@arangodb/users");
let { instanceRole } = require('@arangodb/testutils/instance');
const IM = global.instanceManager;

function QueriesAuthSuite () {
  'use strict';
  let coordinators = [];
  const users = {
    root: { username: 'root', password: '' },
    alice: { username: 'alice', password: 'pass1' },
    bob: { username: 'bob', password: 'pass2' }
  };
  const secret = 'alice-secret-4711';
  // bind variables must be referenced in the query, otherwise AQL rejects them
  const sleepQuery = 'RETURN { s: SLEEP(@secs), secret: @secret }';
  // alice's queries run on this coordinator ...
  const owning = 0;
  // ... and the requests of the tests go to this one
  const other = 1;

  // sends a request as `user` to the given coordinator
  function sendRequest(user, method, path, body, coordinator, headers) {
    const envelope = {
      json: true,
      method,
      url: `${coordinators[coordinator].url}${path}`,
      headers: Object.assign({
        authorization: `Basic ${base64Encode(user.username + ':' + user.password)}`
      }, headers || {})
    };
    if (method !== 'GET') {
      envelope.body = body;
    }
    const res = request(envelope);
    if (typeof res.body === "string") {
      res.body = res.body === "" ? {} : JSON.parse(res.body);
    }
    return res;
  }

  const sleepQueries = (list) => list.filter((query) => /SLEEP/.test(query.query));

  // polls `condition` until it returns something truthy; returns the last value
  function waitFor(condition) {
    let value;
    for (let tries = 0; tries < 120; ++tries) {
      value = condition();
      if (value) {
        return value;
      }
      internal.sleep(0.25);
    }
    return value;
  }

  // the running sleep queries root (an admin) sees via the owning coordinator
  function runningSleepQueriesAsRoot() {
    const res = sendRequest(users.root, 'GET', '/_api/query/current', null, owning);
    assertEqual(200, res.status, JSON.stringify(res.body));
    return sleepQueries(res.body);
  }

  // starts a long running query as alice on the owning coordinator and
  // returns it once it shows up in the running query list
  function startSleepQueryAsAlice() {
    const res = sendRequest(users.alice, 'POST', '/_api/cursor', {
      query: sleepQuery,
      bindVars: { secs: 300, secret }
    }, owning, { 'x-arango-async': 'store' });
    assertEqual(202, res.status, JSON.stringify(res.body));

    const query = waitFor(() => runningSleepQueriesAsRoot()[0]);
    assertTrue(query !== undefined, 'sleep query did not show up in the running query list');
    return query;
  }

  // runs a query as alice on the owning coordinator that ends up in its
  // slow query list
  function runSlowQueryAsAlice() {
    const res = sendRequest(users.alice, 'POST', '/_api/cursor', {
      query: sleepQuery,
      bindVars: { secs: 0.3, secret }
    }, owning);
    assertEqual(201, res.status, JSON.stringify(res.body));
  }

  function killAllSleepQueriesAsRoot() {
    runningSleepQueriesAsRoot().forEach((query) => {
      sendRequest(users.root, 'DELETE', `/_api/query/${query.id}`, null, owning);
    });
    assertTrue(waitFor(() => runningSleepQueriesAsRoot().length === 0), 'leftover sleep queries');
  }

  return {
    setUpAll: function() {
      coordinators = IM.getInstancesRole(instanceRole.coordinator);
      if (coordinators.length < 2) {
        throw new Error('Expecting at least two coordinators');
      }

      // alice and bob are plain read-only users of _system; root is an admin
      [users.alice, users.bob].forEach((user) => {
        try {
          userModule.remove(user.username);
        } catch (err) {
        }
        userModule.save(user.username, user.password);
        userModule.grantDatabase(user.username, '_system', 'ro');
      });
      // the new users have to become known to the other coordinator as well
      internal.wait(2);

      // make alice's short queries count as slow on the owning coordinator
      const res = sendRequest(users.root, 'PUT', '/_api/query/properties',
                              { slowQueryThreshold: 0.1 }, owning);
      assertEqual(200, res.status, JSON.stringify(res.body));
    },

    tearDownAll: function() {
      killAllSleepQueriesAsRoot();
      sendRequest(users.root, 'PUT', '/_api/query/properties', { slowQueryThreshold: 10 }, owning);
      [users.alice, users.bob].forEach((user) => {
        try {
          userModule.remove(user.username);
        } catch (err) {
        }
      });
      coordinators = [];
    },

    setUp: function() {
      // start every test with empty slow query lists on both coordinators
      [owning, other].forEach((coordinator) => {
        const res = sendRequest(users.root, 'DELETE', '/_api/query/slow', null, coordinator);
        assertEqual(200, res.status, JSON.stringify(res.body));
      });
    },

    tearDown: function() {
      killAllSleepQueriesAsRoot();
    },

    // ── running queries, asked on the other coordinator ──────────────────

    testListCurrentAsOtherUser: function() {
      startSleepQueryAsAlice();

      const res = sendRequest(users.bob, 'GET', '/_api/query/current', null, other);
      assertEqual(200, res.status, JSON.stringify(res.body));
      const body = JSON.stringify(res.body);
      assertEqual(0, sleepQueries(res.body).length, body);
      assertEqual(-1, body.indexOf(secret), body);
    },

    testListCurrentAsOwner: function() {
      startSleepQueryAsAlice();

      const res = sendRequest(users.alice, 'GET', '/_api/query/current', null, other);
      assertEqual(200, res.status, JSON.stringify(res.body));
      const found = sleepQueries(res.body);
      assertEqual(1, found.length, JSON.stringify(res.body));
      assertEqual('alice', found[0].user);
      assertEqual(secret, found[0].bindVars.secret);
    },

    testListCurrentAsAdmin: function() {
      startSleepQueryAsAlice();

      const res = sendRequest(users.root, 'GET', '/_api/query/current', null, other);
      assertEqual(200, res.status, JSON.stringify(res.body));
      const found = sleepQueries(res.body);
      assertEqual(1, found.length, JSON.stringify(res.body));
      assertEqual('alice', found[0].user);
    },

    // ── killing, asked on the other coordinator ──────────────────────────

    testKillAsOtherUser: function() {
      const query = startSleepQueryAsAlice();

      const res = sendRequest(users.bob, 'DELETE', `/_api/query/${query.id}`, null, other);
      assertEqual(403, res.status, JSON.stringify(res.body));
      // still running
      assertEqual(1, runningSleepQueriesAsRoot().length);
    },

    testKillAsOwner: function() {
      const query = startSleepQueryAsAlice();

      const res = sendRequest(users.alice, 'DELETE', `/_api/query/${query.id}`, null, other);
      assertEqual(200, res.status, JSON.stringify(res.body));
      assertTrue(waitFor(() => runningSleepQueriesAsRoot().length === 0), 'query was not killed');
    },

    testKillAsAdmin: function() {
      const query = startSleepQueryAsAlice();

      const res = sendRequest(users.root, 'DELETE', `/_api/query/${query.id}`, null, other);
      assertEqual(200, res.status, JSON.stringify(res.body));
      assertTrue(waitFor(() => runningSleepQueriesAsRoot().length === 0), 'query was not killed');
    },

    // ── slow queries, asked on the other coordinator ─────────────────────

    testListSlowAsOtherUser: function() {
      runSlowQueryAsAlice();

      const res = sendRequest(users.bob, 'GET', '/_api/query/slow', null, other);
      assertEqual(200, res.status, JSON.stringify(res.body));
      const body = JSON.stringify(res.body);
      assertEqual(0, sleepQueries(res.body).length, body);
      assertEqual(-1, body.indexOf(secret), body);
    },

    testListSlowAsAdmin: function() {
      runSlowQueryAsAlice();

      const res = sendRequest(users.root, 'GET', '/_api/query/slow', null, other);
      assertEqual(200, res.status, JSON.stringify(res.body));
      const found = sleepQueries(res.body);
      assertEqual(1, found.length, JSON.stringify(res.body));
      assertEqual('alice', found[0].user);
    },

    testClearSlowAsOtherUser: function() {
      runSlowQueryAsAlice();

      const res = sendRequest(users.bob, 'DELETE', '/_api/query/slow', null, other);
      assertEqual(200, res.status, JSON.stringify(res.body));
      // alice's entry on the owning coordinator survived
      const list = sendRequest(users.root, 'GET', '/_api/query/slow', null, owning);
      assertEqual(1, sleepQueries(list.body).length, JSON.stringify(list.body));
    },

    testClearSlowAsAdmin: function() {
      runSlowQueryAsAlice();

      const res = sendRequest(users.root, 'DELETE', '/_api/query/slow', null, other);
      assertEqual(200, res.status, JSON.stringify(res.body));
      const list = sendRequest(users.root, 'GET', '/_api/query/slow', null, owning);
      assertEqual(0, sleepQueries(list.body).length, JSON.stringify(list.body));
    }
  };
}

jsunity.run(QueriesAuthSuite);
return jsunity.done();
