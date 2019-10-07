/*jshint globalstrict:false, strict:false */
/*global assertEqual */

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
/// @author Jan Steemann
/// @author Copyright 2012, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

var jsunity = require("jsunity");

var arangodb = require("@arangodb");
var db = arangodb.db;
var tasks = require("@arangodb/tasks");

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////

function DeadlockSuite () {
  var cn1 = "UnitTestsDeadlock1";
  var cn2 = "UnitTestsDeadlock2";

  return {

////////////////////////////////////////////////////////////////////////////////
/// @brief set up
////////////////////////////////////////////////////////////////////////////////

    setUp : function () {
      db._drop(cn1);
      db._drop(cn2);
      db._create(cn1);
      db._create(cn2);
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

    testCreateTaskInvalidPeriod1 : function () {
      // start a task that will write-lock collection2 and then read-lock collection1
      tasks.register({ 
        command: function() {
          var db = require("internal").db;
          db._executeTransaction({ 
            collections: { write: [ "UnitTestsDeadlock2" ] }, 
            action: function () {
              require("internal").wait(7, false);
              var db = require("internal").db;
              db.UnitTestsDeadlock1.any();
              db.UnitTestsDeadlock2.insert({ done: true });
            }
          });
        }
      });
       
      try {   
        // and start a transaction that does the opposite
        db._executeTransaction({ 
          collections: { write: [ "UnitTestsDeadlock1" ] }, 
          action: function () {
            require("internal").wait(7, false);
            var db = require("internal").db;
            db.UnitTestsDeadlock2.any();
            db.UnitTestsDeadlock1.insert({ done: true });
          }
        });
      } catch (err) {
        // nothing to do here
      }

      // only one transaction should have succeeded
      assertEqual(1, db.UnitTestsDeadlock1.count() + db.UnitTestsDeadlock2.count());
    }

  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suite
////////////////////////////////////////////////////////////////////////////////

jsunity.run(DeadlockSuite);

return jsunity.done();
