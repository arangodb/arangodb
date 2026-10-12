/* jshint strict: false, sub: true */
/* global arango, getOptions */
'use strict';

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

// Authorization questions asked by GET /_api/query/current, GET /_api/query/slow,
// DELETE /_api/query/{id} and DELETE /_api/query/slow when a query of another
// user is involved (COR-1023).
//
// Handlers: arangod/RestHandler/RestQueryHandler.cpp,
//           arangod/VocBase/Methods/Queries.cpp (ExecContext::canAccessQuery)
//
// A user's own queries are always accessible; listing, killing or clearing
// them asks nothing beyond the base checks. As soon as a query of another
// user is encountered, the server asks `AdminAqlQueries` (classic auth mode:
// read-write access to _system) exactly once per request. A read-only user
// therefore neither sees nor kills nor clears the other user's query, an
// admin does.
//
// The requests of the read-only users are sent through the request module
// with basic auth, so the arangosh connection stays root for the observer.

if (getOptions === true) {
  return {
    'server.authentication': 'true',
    'log.force-direct': 'true',
    // keep background threads from asking questions of their own
    'foxx.queues': 'false',
    // disable so it doesn't spoil the test output:
    'server.statistics': 'false'
  };
}

const jsunity = require('jsunity');
const { assertEqual, assertTrue } = jsunity.jsUnity.assertions;
const internal = require('internal');
const request = require('@arangodb/request');
const users = require('@arangodb/users');
const {
  beginObserve,
  endObserve,
  disableObserve,
  assertPermissions
} = require('@arangodb/testutils/permissions-observer');
const {
  setUpApiTestData,
  tearDownApiTestData,
  DB
} = require('@arangodb/testutils/apitest-fixtures');

const baseQuestions = [
  'UseApiVersion version=0',
  `UseDatabase name=${DB} level=read`
];
const foreignQueryQuestions = baseQuestions.concat(['AdminAqlQueries']);

const password = 'testi';
const secret = 'alice-secret-4711';
// bind variables must be referenced in the query, otherwise AQL rejects them
const sleepQuery = 'RETURN { s: SLEEP(@secs), secret: @secret }';
const queryApi = `/_db/${DB}/_api/query`;
const cursorApi = `/_db/${DB}/_api/cursor`;

// sends a request as `user` to the connected server without touching the
// arangosh connection, which has to stay root for the observer
function sendAs (user, method, path, body, extraHeaders) {
  const url = arango.getEndpoint().replace(/^tcp:/, 'http:').replace(/^ssl:/, 'https:');
  return request({
    method,
    url: url + path,
    body,
    json: true,
    headers: Object.assign({
      authorization: `Basic ${internal.base64Encode(user + ':' + password)}`
    }, extraHeaders || {})
  });
}

function sleepQueries (list) {
  return list.filter((query) => /SLEEP/.test(query.query));
}

// root is an admin and sees every user's queries
function runningQueriesAsRoot () {
  return arango.GET(`${queryApi}/current`);
}

// starts a long running query as `user` and returns its id once it shows up
// in the list of running queries; this happens before an observation starts,
// so the questions of the query itself are not observed
function startQueryAs (user) {
  const res = sendAs(user, 'POST', cursorApi, {
    query: sleepQuery,
    bindVars: { secs: 300, secret }
  }, { 'x-arango-async': 'store' });
  assertEqual(202, res.status, JSON.stringify(res.json));

  for (let tries = 0; tries < 120; ++tries) {
    const found = sleepQueries(runningQueriesAsRoot())
      .filter((query) => query.user === user);
    if (found.length > 0) {
      return found[0].id;
    }
    internal.sleep(0.25);
  }
  throw new Error(`query of ${user} did not show up in the running query list`);
}

// runs a query as `user` that ends up in the slow query list
function runSlowQueryAs (user) {
  const res = sendAs(user, 'POST', cursorApi, {
    query: sleepQuery,
    bindVars: { secs: 0.3, secret }
  });
  assertEqual(201, res.status, JSON.stringify(res.json));
}

function killAllSleepQueriesAsRoot () {
  sleepQueries(runningQueriesAsRoot()).forEach((query) => {
    arango.DELETE_RAW(`${queryApi}/${query.id}`);
  });
  for (let tries = 0; tries < 120; ++tries) {
    if (sleepQueries(runningQueriesAsRoot()).length === 0) {
      return;
    }
    internal.sleep(0.25);
  }
  throw new Error('leftover sleep queries');
}

function queryApiAuthzSuite () {

  return {
    setUpAll: function () {
      setUpApiTestData();
      // alice and bob are plain read-only users of the test database;
      // root keeps read-write access to _system and is therefore an admin
      ['alice', 'bob'].forEach((user) => {
        users.save(user, password);
        users.grantDatabase(user, DB, 'ro');
      });
      arango.PUT(`${queryApi}/properties`, { slowQueryThreshold: 0.1 });
    },

    tearDownAll: function () {
      killAllSleepQueriesAsRoot();
      ['alice', 'bob'].forEach((user) => {
        try {
          users.remove(user);
        } catch (err) {
        }
      });
      tearDownApiTestData();
    },

    setUp: function () {
      arango.DELETE(`${queryApi}/slow`);
    },

    tearDown: function () {
      disableObserve();
      killAllSleepQueriesAsRoot();
    },

    // ── GET /_api/query/current ──────────────────────────────────────────

    // own queries never trigger the admin question
    testListCurrentOwnQuery: function () {
      startQueryAs('alice');

      beginObserve();
      const res = sendAs('alice', 'GET', `${queryApi}/current`);
      assertPermissions(baseQuestions, endObserve());

      assertEqual(200, res.status, JSON.stringify(res.json));
      const found = sleepQueries(res.json);
      assertEqual(1, found.length, JSON.stringify(res.json));
      assertEqual('alice', found[0].user);
    },

    // a read-only user is asked for AdminAqlQueries and does not get to see
    // the other user's query, its query string or its bind variables
    testListCurrentForeignQueryAsUser: function () {
      startQueryAs('alice');

      beginObserve();
      const res = sendAs('bob', 'GET', `${queryApi}/current`);
      assertPermissions(foreignQueryQuestions, endObserve());

      assertEqual(200, res.status, JSON.stringify(res.json));
      const body = JSON.stringify(res.json);
      assertEqual(0, sleepQueries(res.json).length, body);
      assertEqual(-1, body.indexOf(secret), body);
    },

    // an admin is asked the same question and gets to see the query
    testListCurrentForeignQueryAsAdmin: function () {
      startQueryAs('alice');

      beginObserve();
      const res = arango.GET_RAW(`${queryApi}/current`);
      assertPermissions(foreignQueryQuestions, endObserve());

      assertEqual(200, res.code, JSON.stringify(res.parsedBody));
      const found = sleepQueries(res.parsedBody);
      assertEqual(1, found.length, JSON.stringify(res.parsedBody));
      assertEqual('alice', found[0].user);
      assertEqual(secret, found[0].bindVars.secret);
    },

    // ── DELETE /_api/query/{id} ──────────────────────────────────────────

    testKillOwnQuery: function () {
      const id = startQueryAs('alice');

      beginObserve();
      const res = sendAs('alice', 'DELETE', `${queryApi}/${id}`);
      assertPermissions(baseQuestions, endObserve());

      assertEqual(200, res.status, JSON.stringify(res.json));
    },

    // a read-only user is asked for AdminAqlQueries, gets 403 and the query
    // keeps running
    testKillForeignQueryAsUser: function () {
      const id = startQueryAs('alice');

      beginObserve();
      const res = sendAs('bob', 'DELETE', `${queryApi}/${id}`);
      assertPermissions(foreignQueryQuestions, endObserve());

      assertEqual(403, res.status, JSON.stringify(res.json));
      assertTrue(res.json.error, JSON.stringify(res.json));
      assertEqual(1, sleepQueries(runningQueriesAsRoot()).length);
    },

    testKillForeignQueryAsAdmin: function () {
      const id = startQueryAs('alice');

      beginObserve();
      const res = arango.DELETE_RAW(`${queryApi}/${id}`);
      assertPermissions(foreignQueryQuestions, endObserve());

      assertEqual(200, res.code, JSON.stringify(res.parsedBody));
    },

    // an unknown query id is "not found" for everybody; there is no query
    // whose owner could be checked, so no admin question is asked
    testKillUnknownQueryAsUser: function () {
      beginObserve();
      const res = sendAs('bob', 'DELETE', `${queryApi}/123456789`);
      assertPermissions(baseQuestions, endObserve());

      assertEqual(404, res.status, JSON.stringify(res.json));
    },

    // ── GET /_api/query/slow ─────────────────────────────────────────────

    testListSlowOwnQuery: function () {
      runSlowQueryAs('alice');

      beginObserve();
      const res = sendAs('alice', 'GET', `${queryApi}/slow`);
      assertPermissions(baseQuestions, endObserve());

      assertEqual(200, res.status, JSON.stringify(res.json));
      const found = sleepQueries(res.json);
      assertEqual(1, found.length, JSON.stringify(res.json));
      assertEqual('alice', found[0].user);
      assertEqual(secret, found[0].bindVars.secret);
    },

    testListSlowForeignQueryAsUser: function () {
      runSlowQueryAs('alice');

      beginObserve();
      const res = sendAs('bob', 'GET', `${queryApi}/slow`);
      assertPermissions(foreignQueryQuestions, endObserve());

      assertEqual(200, res.status, JSON.stringify(res.json));
      const body = JSON.stringify(res.json);
      assertEqual(0, sleepQueries(res.json).length, body);
      assertEqual(-1, body.indexOf(secret), body);
    },

    testListSlowForeignQueryAsAdmin: function () {
      runSlowQueryAs('alice');

      beginObserve();
      const res = arango.GET_RAW(`${queryApi}/slow`);
      assertPermissions(foreignQueryQuestions, endObserve());

      assertEqual(200, res.code, JSON.stringify(res.parsedBody));
      const found = sleepQueries(res.parsedBody);
      assertEqual(1, found.length, JSON.stringify(res.parsedBody));
      assertEqual('alice', found[0].user);
    },

    // ── DELETE /_api/query/slow ──────────────────────────────────────────

    testClearSlowOwnQuery: function () {
      runSlowQueryAs('alice');

      beginObserve();
      const res = sendAs('alice', 'DELETE', `${queryApi}/slow`);
      assertPermissions(baseQuestions, endObserve());

      assertEqual(200, res.status, JSON.stringify(res.json));
      assertEqual(0, sleepQueries(arango.GET(`${queryApi}/slow`)).length);
    },

    // a read-only user is asked for AdminAqlQueries and the other user's
    // entry survives the clear
    testClearSlowForeignQueryAsUser: function () {
      runSlowQueryAs('alice');

      beginObserve();
      const res = sendAs('bob', 'DELETE', `${queryApi}/slow`);
      assertPermissions(foreignQueryQuestions, endObserve());

      assertEqual(200, res.status, JSON.stringify(res.json));
      assertEqual(1, sleepQueries(arango.GET(`${queryApi}/slow`)).length);
    },

    testClearSlowForeignQueryAsAdmin: function () {
      runSlowQueryAs('alice');

      beginObserve();
      const res = arango.DELETE_RAW(`${queryApi}/slow`);
      assertPermissions(foreignQueryQuestions, endObserve());

      assertEqual(200, res.code, JSON.stringify(res.parsedBody));
      assertEqual(0, sleepQueries(arango.GET(`${queryApi}/slow`)).length);
    }
  };
}

jsunity.run(queryApiAuthzSuite);
return jsunity.done();
