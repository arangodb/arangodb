/*jshint globalstrict:false, strict:false */
/*global assertEqual,print,fail */

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

////////////////////////////////////////////////////////////////////////////////
/// @brief create a task with an invalid period
////////////////////////////////////////////////////////////////////////////////

    testExclusiveExpectConflict : function () {
      c1.insert({ "_key" : "XXX" , "name" : "initial" });
      let task = tasks.register({
        command: function() {
          var db = require("internal").db;
          db._executeTransaction({
            collections: { write: [ "UnitTestsExclusiveCollection1", "UnitTestsExclusiveCollection2" ], exclusive: [ ] },
            action: function () {
              var db = require("internal").db;
              for(var i = 0; i <= 100000; i++) {
                db.UnitTestsExclusiveCollection1.update("XXX", { name : "runner1" });
              }
              db.UnitTestsExclusiveCollection2.insert({ runner1: true });
            }
          });
        }
      });

      var error_in_runner2 = false;
      try {
        db._executeTransaction({
          collections: { write: [ "UnitTestsExclusiveCollection1", "UnitTestsExclusiveCollection2" ], exclusive: [ ] },
          action: function () {
            require("internal").wait(7, false);
            var db = require("internal").db;
            for(var i = 0; i <= 100000; i++) {
              db.UnitTestsExclusiveCollection1.update("XXX", { name : "runner2" });
            }
            db.UnitTestsExclusiveCollection2.insert({ runner2: true });
          }
        });
        //fail("no conflict error");
      } catch (err) {
        assertEqual(ERRORS.ERROR_ARANGO_CONFLICT.code, err.errorNum);
        error_in_runner2 = true;
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
      assertEqual(1, c2.count());
      if (error_in_runner2){
        assertEqual(true, c2.toArray()[0].runner1);
        assertEqual(undefined, c2.toArray()[0].runner2);
      } else {
        assertEqual(undefined, c2.toArray()[0].runner1);
        assertEqual(true, c2.toArray()[0].runner2);
      }
    }, // testExclusiveExpectConflict

    testExclusiveExpectNoConflict : function () {
      c1.insert({ "_key" : "XXX" , "name" : "initial" });
      let task = tasks.register({
        command: function() {
          var db = require("internal").db;
          db._executeTransaction({
            collections: { exclusive: [ "UnitTestsExclusiveCollection1", "UnitTestsExclusiveCollection2" ] },
            action: function () {
              var db = require("internal").db;
              for(var i = 0; i <= 100000; i++) {
                db.UnitTestsExclusiveCollection1.update("XXX", { name : "runner1" });
              }
              db.UnitTestsExclusiveCollection2.insert({ runner1: true });
            }
          });
        }
      });

      db._executeTransaction({
        collections: { exclusive: [ "UnitTestsExclusiveCollection1", "UnitTestsExclusiveCollection2" ] },
        action: function () {
          require("internal").wait(7, false);
          var db = require("internal").db;
          for(var i = 0; i <= 100000; i++) {
            db.UnitTestsExclusiveCollection1.update("XXX", { name : "runner2" });
          }
          db.UnitTestsExclusiveCollection2.insert({ runner2: true });
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
      
      // both transaction should have succeeded
      assertEqual(2, c2.count());
    }
  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suite
////////////////////////////////////////////////////////////////////////////////

jsunity.run(ExclusiveSuite);

return jsunity.done();
