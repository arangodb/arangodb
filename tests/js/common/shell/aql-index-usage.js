/*jshint globalstrict:false, strict:false */
/*global assertEqual, assertNotEqual, assertTrue */

////////////////////////////////////////////////////////////////////////////////
/// @brief test index usage
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2010-2012 triagens GmbH, Cologne, Germany
///
/// Licensed under the Apache License, Version 2.0 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     http://www.apache.org/licenses/LICENSE-2.0
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is triAGENS GmbH, Cologne, Germany
///
/// @author Jan Steemann
/// @author Copyright 2018, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

let jsunity = require("jsunity");

let arangodb = require("@arangodb");
let db = arangodb.db;
let tasks = require("@arangodb/tasks");

function IndexUsageSuite () {
  const cnData = "UnitTestsCollection"; // used for test data
  const cnComm = "UnitTestsCommunication"; // used for communication

  return {

    setUp : function () {
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
      db._drop(cnData);
      db._drop(cnComm);
    },

    testIndexUsage : function () {
      let task = tasks.register({
        command: function(params) {
          require('jsunity').jsUnity.attachAssertions();
          let db = require("internal").db;
          let comm = db[params.cnComm];
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
              db._query("FOR doc IN " + params.cnData + " FILTER doc.value > 10 LIMIT 10 RETURN doc");
              comm.update("runner1", { value: ++success });
            } catch (err) {
              // if the index that was picked for the query is dropped in the meantime,
              // we will get the following error back
              assertEqual(err.errorNum, errors.ERROR_QUERY_BAD_JSON_PLAN.code);
            }
          } while (time() - start < 10.0);
        },
        params: { cnComm, cnData }
      });

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

      while (true) {
        try {
          tasks.get(task);
          require("internal").wait(0.25, false);
        } catch (err) {
          // "task not found" means the task is finished
          break;
        }
      }

      assertEqual(2, comm.count());
      let doc = comm.document("runner1");
      assertTrue(doc.value > 0, doc);
      assertTrue(success > 0, success);
    },

  };
}

jsunity.run(IndexUsageSuite);

return jsunity.done();
