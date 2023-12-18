/*jshint globalstrict:false, strict:false */
/*global assertEqual, assertNotEqual, assertTrue */

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
/// @author Copyright 2018, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

var jsunity = require("jsunity");

var arangodb = require("@arangodb");
var db = arangodb.db;
var tasks = require("@arangodb/tasks");

var ERRORS = arangodb.errors;

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////

function ExclusiveSuite () {
  var cn1 = "UnitTestsExclusiveCollection1"; // used for test data
  var cn2 = "UnitTestsExclusiveCollection2"; // used for communication
  var c1, c2;

  return {

////////////////////////////////////////////////////////////////////////////////
/// @brief set up
////////////////////////////////////////////////////////////////////////////////

    setUp : function () {
      db._drop(cn1);
      db._drop(cn2);
      c1 = db._create(cn1);
      c2 = db._create(cn2);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief tear down
////////////////////////////////////////////////////////////////////////////////

    tearDown : function () {
      db._drop(cn1);
      db._drop(cn2);
    },

    testExclusiveExpectConflict : function () {
      c1.insert({ "_key" : "XXX" , "name" : "initial" });
      let task = tasks.register({
        command: function() {
          let db = require("internal").db;
          db.UnitTestsExclusiveCollection2.insert({ _key: "runner1", value: false });
              
          while (!db.UnitTestsExclusiveCollection2.exists("runner2")) {
            require("internal").sleep(0.02);
          }

          db._executeTransaction({
            collections: { write: [ "UnitTestsExclusiveCollection1", "UnitTestsExclusiveCollection2" ], exclusive: [ ] },
            action: function () {
              let db = require("internal").db;
              for (let i = 0; i < 100000; ++i) {
                db.UnitTestsExclusiveCollection1.update("XXX", { name : "runner1" });
              }
              db.UnitTestsExclusiveCollection2.update("runner1", { value: true });
            }
          });
        }
      });

      db.UnitTestsExclusiveCollection2.insert({ _key: "runner2", value: false });
      while (!db.UnitTestsExclusiveCollection2.exists("runner1")) {
        require("internal").sleep(0.02);
      }

      try {
        db._executeTransaction({
          collections: { write: [ "UnitTestsExclusiveCollection1", "UnitTestsExclusiveCollection2" ], exclusive: [ ] },
          action: function () {
            let db = require("internal").db;
            for (let i = 0; i < 100000; ++i) {
              db.UnitTestsExclusiveCollection1.update("XXX", { name : "runner2" });
            }
            db.UnitTestsExclusiveCollection2.update("runner2", { value: true });
          }
        });
      } catch (err) {
        assertEqual(ERRORS.ERROR_ARANGO_CONFLICT.code, err.errorNum);
      }

      while (true) {
        try {
          tasks.get(task);
          require("internal").wait(0.25, false);
        } catch (err) {
          // "task not found" means the task is finished
          break;
        }
      }

      // only one transaction should have succeeded
      assertEqual(2, c2.count());
      let docs = c2.toArray().sort(function(l, r) { return l._key < r._key; });
      assertNotEqual(docs[0].value, docs[1].value);
    },

    testExclusiveExpectNoConflict : function () {
      assertEqual(0, c2.count());
      c1.insert({ "_key" : "XXX" , "name" : "initial" });
      let task = tasks.register({
        command: function() {
          let db = require("internal").db;
          db.UnitTestsExclusiveCollection2.insert({ _key: "runner1", value: false });
          while (!db.UnitTestsExclusiveCollection2.exists("runner2")) {
            require("internal").sleep(0.02);
          }

          db._executeTransaction({
            collections: { exclusive: [ "UnitTestsExclusiveCollection1", "UnitTestsExclusiveCollection2" ] },
            action: function () {
              let db = require("internal").db;
              for (let i = 0; i < 100000; ++i) {
                db.UnitTestsExclusiveCollection1.update("XXX", { name : "runner1" });
              }
              db.UnitTestsExclusiveCollection2.update("runner1", { value: true });
            }
          });
        }
      });

      db.UnitTestsExclusiveCollection2.insert({ _key: "runner2", value: false });
      while (!db.UnitTestsExclusiveCollection2.exists("runner1")) {
        require("internal").sleep(0.02);
      }

      db._executeTransaction({
        collections: { exclusive: [ "UnitTestsExclusiveCollection1", "UnitTestsExclusiveCollection2" ] },
        action: function () {
          let db = require("internal").db;
          for (let i = 0; i < 100000; ++i) {
            db.UnitTestsExclusiveCollection1.update("XXX", { name : "runner2" });
          }
          db.UnitTestsExclusiveCollection2.update("runner2", { value: true });
        }
      });

      while (true) {
        try {
          tasks.get(task);
          require("internal").wait(0.25, false);
        } catch (err) {
          // "task not found" means the task is finished
          break;
        }
      }
      
      // both transactions should have succeeded
      assertEqual(2, c2.count());
      let docs = c2.toArray().sort(function(l, r) { return l._key < r._key; });
      assertTrue(docs[0].value);
      assertTrue(docs[1].value);
    },

    testExclusiveExpectConflictAQL : function () {
      c1.insert({ "_key" : "XXX" , "name" : "initial" });
      let task = tasks.register({
        command: function() {
          let db = require("internal").db;
          db.UnitTestsExclusiveCollection2.insert({ _key: "runner1", value: false });
          while (!db.UnitTestsExclusiveCollection2.exists("runner2")) {
            require("internal").sleep(0.02);
          }
          for (let i = 0; i < 10000; ++i) {
            db._query("UPSERT { _key: 'XXX' } INSERT { name: 'runner1' } UPDATE { name: 'runner1' } IN UnitTestsExclusiveCollection1");
          }
          db.UnitTestsExclusiveCollection2.update("runner1", { value: true });
        }
      });

      try {
        db.UnitTestsExclusiveCollection2.insert({ _key: "runner2", value: false });
        while (!db.UnitTestsExclusiveCollection2.exists("runner1")) {
          require("internal").sleep(0.02);
        }
        for (let i = 0; i < 10000; i++) {
          db._query("UPSERT { _key: 'XXX' } INSERT { name: 'runner2' } UPDATE { name: 'runner2' } IN UnitTestsExclusiveCollection1");
        }
        db.UnitTestsExclusiveCollection2.update("runner2", { value: true });
      } catch (err) {
        assertEqual(ERRORS.ERROR_ARANGO_CONFLICT.code, err.errorNum);
      }

      while (true) {
        try {
          tasks.get(task);
          require("internal").wait(0.25, false);
        } catch (err) {
          // "task not found" means the task is finished
          break;
        }
      }

      // only one transaction should have succeeded
      assertEqual(2, c2.count());
      let docs = c2.toArray().sort(function(l, r) { return l._key < r._key; });
      assertNotEqual(docs[0].value, docs[1].value);
    },
    
    testExclusiveExpectNoConflictAQL : function () {
      c1.insert({ "_key" : "XXX" , "name" : "initial" });
      let task = tasks.register({
        command: function() {
          let db = require("internal").db;
          db.UnitTestsExclusiveCollection2.insert({ _key: "runner1", value: false });
          while (!db.UnitTestsExclusiveCollection2.exists("runner2")) {
            require("internal").sleep(0.02);
          }
          for (let i = 0; i < 10000; ++i) {
            db._query("UPSERT { _key: 'XXX' } INSERT { name: 'runner1' } UPDATE { name: 'runner1' } IN UnitTestsExclusiveCollection1 OPTIONS { exclusive: true }");
          }
          db.UnitTestsExclusiveCollection2.update("runner1", { value: true });
        }
      });

      db.UnitTestsExclusiveCollection2.insert({ _key: "runner2", value: false });
      while (!db.UnitTestsExclusiveCollection2.exists("runner1")) {
        require("internal").sleep(0.02);
      }
      for (let i = 0; i < 10000; i++) {
        db._query("UPSERT { _key: 'XXX' } INSERT { name: 'runner2' } UPDATE { name: 'runner2' } IN UnitTestsExclusiveCollection1 OPTIONS { exclusive: true }");
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

      // both transactions should have succeeded
      assertEqual(2, c2.count());
      let docs = c2.toArray().sort(function(l, r) { return l._key < r._key; });
      assertTrue(docs[0].value);
      assertTrue(docs[1].value);
    }
  
  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suite
////////////////////////////////////////////////////////////////////////////////

jsunity.run(ExclusiveSuite);

return jsunity.done();
