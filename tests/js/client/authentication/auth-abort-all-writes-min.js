/*jshint globalstrict:false, strict:false */

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

// Minimal repro: a user with read on one database can kill another user's
// modification query in a database they cannot access, via
// DELETE /_db/<readable>/_api/transaction/write.
// Run in the `authentication` suite (auth is on there).

const jsunity = require("jsunity");
const { assertTrue, assertNotEqual } = jsunity.jsUnity.assertions;
const arango = require("@arangodb").arango;
const db = require("internal").db;
const wait = require("internal").wait;
const users = require("@arangodb/users");
const KILLED = require("internal").errors.ERROR_QUERY_KILLED.code;
const IM = require("@arangodb/test-helper").getInstanceInfo();

function suite() {
  const dbA = "AbortA";
  const dbB = "AbortB";
  const coll = "victim";

  const query = `FOR i IN 1..200 INSERT {i, w: SLEEP(0.05)} INTO ${coll}`;
  const login = (d, u) => arango.reconnect(arango.getEndpoint(), d, u, "pw");
  const running = () =>
    require("@arangodb/aql/queries").current().some((x) => x.query === query);
  const poll = (fn) => { for (let i = 0; i < 3000; i++, wait(0.02)) { const r = fn(); if (r !== undefined) { return r; } } };

  return {
    setUpAll: function() {
      IM.rememberConnection();
      db._createDatabase(dbA);
      db._createDatabase(dbB);

      db._useDatabase(dbB);
      db._create(coll);
      db._useDatabase("_system");

      users.save("alice", "pw"); users.grantDatabase("alice", dbA, "ro");
      users.save("bob", "pw"); users.grantDatabase("bob", dbB, "rw");
    },

    tearDownAll: function() {
      IM.reconnectMe();
      db._useDatabase("_system");
      [dbA, dbB].forEach((d) => { try { db._dropDatabase(d); } catch (e) {} });
      ["alice", "bob"].forEach((u) => { try { users.remove(u); } catch (e) {} });
    },

    // COR-986
    testTransactionBetweenDatabaseIsolation: function() {
      // bob starts a tracked modification query and we wait until it runs
      login(dbB, "bob");
      const jobId = arango.POST_RAW("/_api/cursor", { query },
                                    { "x-arango-async": "store" })
                         .headers["x-arango-async-id"];
      assertTrue(poll(() => running() || undefined), "query never started");

      // alice, read-only on dbA and no access to dbB, fires the bulk cancel
      login(dbA, "alice");
      arango.DELETE_RAW(`/_db/${dbA}/_api/transaction/write`);

      // bob's query must survive (fails on current code: killed -> 410 / 1500)
      login(dbB, "bob");
      const res = poll(() => {
        const r = arango.PUT_RAW("/_api/job/" + jobId, {});
        return r.code === 204 ? undefined : r;
      });
      assertNotEqual(410, res.code, "alice killed bob's query: " + JSON.stringify(res));
      assertNotEqual(KILLED, res.parsedBody && res.parsedBody.errorNum,
                     JSON.stringify(res));
    },
  };
}

jsunity.run(suite);
return jsunity.done();
