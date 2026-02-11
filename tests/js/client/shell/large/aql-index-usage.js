/*jshint globalstrict:false, strict:false */
/*global assertEqual, assertNotEqual, assertTrue */

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
/// @author Jan Steemann
/// @author Copyright 2018, triAGENS GmbH, Cologne, Germany
// //////////////////////////////////////////////////////////////////////////////

let jsunity = require("jsunity");

let arangodb = require("@arangodb");
let db = arangodb.db;
const {
  launchPlainSnippetInBG,
  joinBGShells,
  cleanupBGShells
} = require('@arangodb/testutils/client-tools').run;

let IM = global.instanceManager;
const waitFor = IM.options.isInstrumented ? 80 * 7 : 80;

function IndexUsageSuite () {
  const cnData = "UnitTestsCollection"; // used for test data
  const cnComm = "UnitTestsCommunication"; // used for communication
  let clients = [];

  return {

    setUp : function () {
      clients = [];
      db._drop(cnData);
      db._drop(cnComm);
      db._create(cnData);
      db._create(cnComm);

      let docs = [];
      for (let i = 0; i < 5000; ++i) {
        docs.push({ value: "test" + i });
      }
      db[cnData].insert(docs);
    },

    tearDown : function () {
      joinBGShells(IM.options, clients, waitFor, cnData);
      db._drop(cnData);
      db._drop(cnComm);
      if (IM.debugCanUseFailAt()) {
        IM.debugClearFailAt();
      }
    },

    testIndexUsage : function () {
      clients.push({client: launchPlainSnippetInBG(`
          require('jsunity').jsUnity.attachAssertions();
          let db = require("internal").db;
          let comm = db["${cnComm}"];
          let errors = require("@arangodb").errors;
          comm.insert({ _key: "runner1", value: 0 });

          while (!comm.exists("runner2")) {
            require("internal").sleep(0.02);
          }

          let success = 0;
          let time = require("internal").time;
          let start = time();
          do {
            try {
              db._query("FOR doc IN ${cnData} FILTER doc.value > 10 LIMIT 10 RETURN doc");
              comm.update("runner1", { value: ++success });
            } catch (err) {
              // if the index that was picked for the query is dropped in the meantime,
              // we will get one of the following errors back
              assertTrue(err.errorNum === errors.ERROR_QUERY_BAD_JSON_PLAN.code || 
                         err.errorNum === errors.ERROR_ARANGO_INDEX_NOT_FOUND.code);
            }
          } while (time() - start < 10.0);
        `, 'testIndexUsage')});

      let comm = db[cnComm];
      comm.insert({ _key: "runner2" });
      while (!comm.exists("runner1")) {
        require("internal").sleep(0.02);
      }

      let time = require("internal").time;
      let start = time();
      let success = 0;
      do {
        let indexes = db[cnData].indexes();
        if (indexes.length > 1) {
          db[cnData].dropIndex(indexes[1]);
        }
        db[cnData].ensureIndex({ type: "hash", fields: ["value"], inBackground: true });
        ++success;
      } while (time() - start < 10.0);


      assertEqual(2, comm.count());
      let doc = comm.document("runner1");
      assertTrue(doc.value > 0, doc);
      assertTrue(success > 0, success);
    },

    testPrimaryIndexLookupConsistency : function () {
      if (IM.debugCanUseFailAt()) {
        IM.debugSetFailAt("RocksDBCollection::read-delay");

      clients.push({client: launchPlainSnippetInBG(`
            require('jsunity').jsUnity.attachAssertions();
            const {db, time} = require("internal");
            const comm = db["${cnComm}"];
            comm.insert({ _key: "runner1", value: 0 });

            while (!comm.exists("runner2")) {
              require("internal").sleep(0.02);
            }

            const start = time();
            // this tasks keeps updating document "runner1"
            let cnt = 0;
            do {
              comm.update("runner1", { value: ++cnt });
            } while (time() - start < 10.0);
        `, 'testPrimaryIndexLookupConsistency')});

        const comm = db[cnComm];
        comm.insert({ _key: "runner2" });
        while (!comm.exists("runner1")) {
          require("internal").sleep(0.02);
        }

        let time = require("internal").time;
        let start = time();
        // this task keeps reading document "runner1"
        do {
          assertTrue(comm.exists("runner1"));
        } while (time() - start < 10.0);
      }
    },
  };
}

jsunity.run(IndexUsageSuite);

return jsunity.done();
