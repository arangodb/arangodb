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
const { assertTrue, assertEqual, assertNotEqual } = jsunity.jsUnity.assertions;
const arango = require("@arangodb").arango;
const db = require("internal").db;
const users = require("@arangodb/users");
const KILLED = require("internal").errors.ERROR_QUERY_KILLED.code;
const IM = require("@arangodb/test-helper").getInstanceInfo();

function suite() {
  const dbA = "AbortA";
  const dbB = "AbortB";
  const coll = "victim";

  const query = `FOR i IN 1..3 INSERT {i} INTO ${coll} RETURN NEW.i`;
  const login = (d, u) => arango.reconnect(arango.getEndpoint(), d, u, "pw");

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
      // bob opens a streaming modification query
      login(dbB, "bob");
      let res = arango.POST_RAW("/_api/cursor",
                                { query, batchSize: 1, options: { stream: true } });
      assertEqual(201, res.code, JSON.stringify(res));
      assertTrue(res.parsedBody.hasMore, JSON.stringify(res));
      const cursorId = res.parsedBody.id;
      let results = res.parsedBody.result;

      // alice, read-only on dbA and no access to dbB, fires the bulk cancel
      login(dbA, "alice");
      const abort = arango.DELETE_RAW(`/_db/${dbA}/_api/transaction/write`);
      assertEqual(200, abort.code, JSON.stringify(abort));

      // bob's query must survive
      login(dbB, "bob");
      while (res.parsedBody.hasMore) {
        res = arango.POST_RAW(`/_api/cursor/${cursorId}`, {});
        assertNotEqual(410, res.code, "alice killed bob's query: " + JSON.stringify(res));
        assertNotEqual(KILLED, res.parsedBody.errorNum, JSON.stringify(res));
        assertEqual(200, res.code, JSON.stringify(res));
        results = results.concat(res.parsedBody.result);
      }
      assertEqual([1, 2, 3], results);
      assertEqual(3, db._collection(coll).count());
    },
  };
}

jsunity.run(suite);
return jsunity.done();
