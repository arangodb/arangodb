/*jshint globalstrict:false, strict:false */
/*global assertEqual, assertTrue, getOptions, fail, assertFalse, assertMatch */

////////////////////////////////////////////////////////////////////////////////
/// @brief test the deadlock detection
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
/// @author Jan Christoph Uhde
/// @author Copyright 2019, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

if (getOptions === true) {
  return {
    'rocksdb.exclusive-writes': 'false',
  };
}

const jsunity = require("jsunity");
const tasks = require("@arangodb/tasks");
const arangodb = require("@arangodb");
const db = arangodb.db;
const ERRORS = arangodb.errors;

function OptionsTestSuite () {
  const cn1 = "UnitTestsExclusiveCollection1"; // used for test data
  const cn2 = "UnitTestsExclusiveCollection2"; // used for communication
  let c1, c2;

  return {

    setUp : function () {
      db._drop(cn1);
      db._drop(cn2);
      c1 = db._create(cn1);
      c2 = db._create(cn2);
    },

    tearDown : function () {
      db._drop(cn1);
      db._drop(cn2);
    },

    testExclusiveExpectConflictWithoutOption : function () {
      c1.insert({ "_key" : "XXX" , "name" : "initial" });
      let task = tasks.register({
        command: function() {
          let db = require("internal").db;
          db.UnitTestsExclusiveCollection2.insert({ _key: "runner1", value: false });

          while (!db.UnitTestsExclusiveCollection2.exists("runner2")) {
            require("internal").sleep(0.02);
          }

          try {
            for (let i = 0; i < 50; ++i) {
              db._executeTransaction({
                collections: { write: [ "UnitTestsExclusiveCollection1" ] },
                action: function () {
                  let db = require("internal").db;
                  for (let i = 0; i < 10000; ++i) {
                    db.UnitTestsExclusiveCollection1.update("XXX", { name : "runner1" });
                  }
                }
              });

              if (db.UnitTestsExclusiveCollection2.document("runner2").value) {
                break;
              }
            }
          } catch (err) {
            db.UnitTestsExclusiveCollection2.insert({ _key: "other-failed", errorNum: err.errorNum, errorMessage: err.errorMessage, code: err.code });
          }
          db.UnitTestsExclusiveCollection2.update("runner1", { value: true });
        }
      });

      db.UnitTestsExclusiveCollection2.insert({ _key: "runner2", value: false });
      while (!db.UnitTestsExclusiveCollection2.exists("runner1")) {
        require("internal").sleep(0.02);
      }

      try {
        for (let i = 0; i < 50; ++i) {
          db._executeTransaction({
            collections: { write: [ "UnitTestsExclusiveCollection1" ] },
            action: function () {
              let db = require("internal").db;
              for (let i = 0; i < 10000; ++i) {
                db.UnitTestsExclusiveCollection1.update("XXX", { name : "runner2" });
              }
            }
          });

          if (db.UnitTestsExclusiveCollection2.document("runner1").value) {
            break;
          }
        }
      } catch (err) {
        db.UnitTestsExclusiveCollection2.insert({ _key: "we-failed", errorNum: err.errorNum, errorMessage: err.errorMessage, code: err.code });
      }
      db.UnitTestsExclusiveCollection2.update("runner2", { value: true });

      while (true) {
        try {
          tasks.get(task);
          require("internal").wait(0.25, false);
        } catch (err) {
          // "task not found" means the task is finished
          break;
        }
      }
      
      let found = 0;
      let keys = ["other-failed", "we-failed"];
      keys.forEach((k) => {
        if (db.UnitTestsExclusiveCollection2.exists(k)) {
          let doc = db.UnitTestsExclusiveCollection2.document(k);
          assertEqual(ERRORS.ERROR_ARANGO_CONFLICT.code, doc.errorNum);
          assertEqual(409, doc.code); // conflict
          assertMatch( // it is possible to get two different errors messages here (two different internal states can appear)
            /(precondition failed|timeout waiting to lock key.*Operation timed out)/, doc.errorMessage);
          assertMatch(/XXX/, doc.errorMessage);
          ++found;
          db.UnitTestsExclusiveCollection2.remove(k);
        }
      });

      assertEqual(1, found);

      // only one transaction should have succeeded
      assertEqual(2, c2.count());
    }

  };
}

jsunity.run(OptionsTestSuite);

return jsunity.done();
