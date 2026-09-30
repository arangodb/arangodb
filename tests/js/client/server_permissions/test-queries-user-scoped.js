/*jshint globalstrict:false, strict:false */
/* global getOptions, runSetup, arango */

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

// Regression test for COR-1023: the running and slow query lists, killing a
// query and clearing the slow query list are scoped to the calling user's own
// queries. Other users' queries (including their query strings and bind
// variables) are only accessible to admins, i.e. in classic authentication
// mode to users with read-write access to the _system database.

const jsunity = require("jsunity");
const {assertEqual, assertTrue, assertFalse} = jsunity.jsUnity.assertions;
const db = require('@arangodb').db;
const arango = require('@arangodb').arango;
const internal = require('internal');
const queries = require('@arangodb/aql/queries');
let IM = global.instanceManager;

const dbName = "UnitTestsQueryScope";
const password = "testi";
const secret = "alice-secret-4711";
// bind variables must be referenced in the query, otherwise AQL rejects them
const sleepQuery = "RETURN { s: SLEEP(@secs), secret: @secret }";
const queryApi = "/_db/" + dbName + "/_api/query";
const cursorApi = "/_db/" + dbName + "/_api/cursor";

if (getOptions === true) {
  return {
    'server.authentication': 'true',
    'runSetup': true
  };
}

if (runSetup === true) {
  const users = require("@arangodb/users");

  db._createDatabase(dbName);

  // alice and bob are plain read-only users of the test database.
  // root keeps read-write access to _system and is therefore an admin.
  users.save("alice", password);
  users.grantDatabase("alice", dbName, "ro");
  users.save("bob", password);
  users.grantDatabase("bob", dbName, "ro");

  return true;
}

const connectAs = (user) => {
  arango.reconnect(IM.endpoint, dbName, user, user === "root" ? "" : password);
};

const sleepQueries = (list) => list.filter((query) => /SLEEP/.test(query.query));

// polls `condition` until it returns something truthy; returns the last value
const waitFor = (condition) => {
  let value;
  for (let tries = 0; tries < 120; ++tries) {
    value = condition();
    if (value) {
      return value;
    }
    internal.sleep(0.25);
  }
  return value;
};

// starts a long-running query as the currently connected user
const startSleepQuery = () => {
  const result = arango.POST_RAW(cursorApi, {
    query: sleepQuery,
    bindVars: { secs: 300, secret }
  }, { "x-arango-async": "store" });
  assertEqual(202, result.code, JSON.stringify(result));
};

// returns the first running sleep query visible to the connected user
const waitForRunningSleepQuery = () => {
  const query = waitFor(() => sleepQueries(queries.current())[0]);
  assertTrue(query !== undefined, "sleep query did not show up in the running query list");
  return query;
};

const waitForNoRunningSleepQuery = () => {
  return waitFor(() => sleepQueries(queries.current()).length === 0);
};

// runs a query as `user` that ends up in the slow query list
const runSlowQueryAs = (user) => {
  connectAs(user);
  const result = arango.POST_RAW(cursorApi, {
    query: sleepQuery,
    bindVars: { secs: 0.5, secret }
  });
  assertEqual(201, result.code, JSON.stringify(result));
};

function testSuite() {
  return {
    setUpAll: function () {
      connectAs("root");
      const result = arango.PUT(queryApi + "/properties", { slowQueryThreshold: 0.1 });
      assertEqual(0.1, result.slowQueryThreshold, JSON.stringify(result));
    },

    setUp: function () {
      connectAs("root");
      const result = arango.DELETE(queryApi + "/slow");
      assertFalse(result.error, JSON.stringify(result));
    },

    tearDown: function () {
      // root is an admin and may kill every user's queries
      connectAs("root");
      sleepQueries(queries.current()).forEach((query) => {
        try {
          queries.kill(query.id);
        } catch (err) {
        }
      });
      assertTrue(waitForNoRunningSleepQuery(), "leftover sleep queries");
    },

    // ── running queries ──────────────────────────────────────────────────

    testOwnerSeesOwnRunningQuery: function () {
      connectAs("alice");
      startSleepQuery();

      const query = waitForRunningSleepQuery();
      assertEqual("alice", query.user, JSON.stringify(query));
      assertEqual(secret, query.bindVars.secret, JSON.stringify(query));
      assertEqual(1, sleepQueries(queries.current()).length);
    },

    testOtherUserCannotSeeForeignRunningQuery: function () {
      connectAs("alice");
      startSleepQuery();
      waitForRunningSleepQuery();

      connectAs("bob");
      const result = arango.GET_RAW(queryApi + "/current");
      assertEqual(200, result.code, JSON.stringify(result));
      const body = JSON.stringify(result.parsedBody);
      assertEqual(0, sleepQueries(result.parsedBody).length, body);
      assertEqual(-1, body.indexOf(secret), body);
    },

    testAdminSeesForeignRunningQuery: function () {
      connectAs("alice");
      startSleepQuery();
      waitForRunningSleepQuery();

      connectAs("root");
      const query = waitForRunningSleepQuery();
      assertEqual("alice", query.user, JSON.stringify(query));
      assertEqual(secret, query.bindVars.secret, JSON.stringify(query));
    },

    // ── killing queries ──────────────────────────────────────────────────

    testOtherUserCannotKillForeignQuery: function () {
      connectAs("alice");
      startSleepQuery();
      const query = waitForRunningSleepQuery();

      connectAs("bob");
      const result = arango.DELETE_RAW(queryApi + "/" + query.id);
      assertEqual(403, result.code, JSON.stringify(result));
      assertTrue(result.parsedBody.error, JSON.stringify(result));

      // the query is still running
      connectAs("alice");
      assertEqual(1, sleepQueries(queries.current()).length);
    },

    testOtherUserKillOfUnknownIdIsNotFound: function () {
      connectAs("bob");
      const result = arango.DELETE_RAW(queryApi + "/123456789");
      assertEqual(404, result.code, JSON.stringify(result));
    },

    testOwnerCanKillOwnQuery: function () {
      connectAs("alice");
      startSleepQuery();
      const query = waitForRunningSleepQuery();

      const result = queries.kill(query.id);
      assertEqual(200, result.code, JSON.stringify(result));
      assertTrue(waitForNoRunningSleepQuery(), "query was not killed");
    },

    testAdminCanKillForeignQuery: function () {
      connectAs("alice");
      startSleepQuery();
      waitForRunningSleepQuery();

      connectAs("root");
      const query = waitForRunningSleepQuery();
      const result = queries.kill(query.id);
      assertEqual(200, result.code, JSON.stringify(result));
      assertTrue(waitForNoRunningSleepQuery(), "query was not killed");
    },

    // ── slow queries ─────────────────────────────────────────────────────

    testOwnerSeesOwnSlowQuery: function () {
      runSlowQueryAs("alice");

      const found = sleepQueries(queries.slow());
      assertEqual(1, found.length, JSON.stringify(found));
      assertEqual("alice", found[0].user, JSON.stringify(found));
      assertEqual(secret, found[0].bindVars.secret, JSON.stringify(found));
    },

    testOtherUserCannotSeeForeignSlowQuery: function () {
      runSlowQueryAs("alice");

      connectAs("bob");
      const result = arango.GET_RAW(queryApi + "/slow");
      assertEqual(200, result.code, JSON.stringify(result));
      const body = JSON.stringify(result.parsedBody);
      assertEqual(0, sleepQueries(result.parsedBody).length, body);
      assertEqual(-1, body.indexOf(secret), body);
    },

    testAdminSeesForeignSlowQuery: function () {
      runSlowQueryAs("alice");

      connectAs("root");
      const found = sleepQueries(queries.slow());
      assertEqual(1, found.length, JSON.stringify(found));
      assertEqual("alice", found[0].user, JSON.stringify(found));
      assertEqual(secret, found[0].bindVars.secret, JSON.stringify(found));
    },

    // ── clearing the slow query list ─────────────────────────────────────

    testOtherUserCannotClearForeignSlowQuery: function () {
      runSlowQueryAs("alice");

      connectAs("bob");
      const result = arango.DELETE_RAW(queryApi + "/slow");
      assertEqual(200, result.code, JSON.stringify(result));

      // alice's entry survived
      connectAs("alice");
      assertEqual(1, sleepQueries(queries.slow()).length);
      connectAs("root");
      assertEqual(1, sleepQueries(queries.slow()).length);
    },

    testOwnerCanClearOwnSlowQuery: function () {
      runSlowQueryAs("alice");

      const result = queries.clearSlow();
      assertEqual(200, result.code, JSON.stringify(result));
      assertEqual(0, sleepQueries(queries.slow()).length);
      connectAs("root");
      assertEqual(0, sleepQueries(queries.slow()).length);
    },

    testAdminCanClearForeignSlowQuery: function () {
      runSlowQueryAs("alice");

      connectAs("root");
      const result = queries.clearSlow();
      assertEqual(200, result.code, JSON.stringify(result));
      connectAs("alice");
      assertEqual(0, sleepQueries(queries.slow()).length);
    }
  };
}

jsunity.run(testSuite);
return jsunity.done();
