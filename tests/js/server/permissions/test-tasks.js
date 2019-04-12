/*jshint globalstrict:false, strict:false */
/* global getOptions, assertTrue, assertFalse, assertEqual, assertMatch, fail, arango */

////////////////////////////////////////////////////////////////////////////////
/// @brief test for security-related server options
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
/// Copyright holder is ArangoDB Inc, Cologne, Germany
///
/// @author Wilfried Goesgens
/// @author Copyright 2019, ArangoDB Inc, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////


if (getOptions === true) {
  let users = require("@arangodb/users");

  //users.save("test_rw", "testi");
  //users.grantDatabase("test_rw", "_system", "rw");
  //users.save("test_ro", "testi");
  //users.grantDatabase("test_ro", "_system", "ro");


  return {
    'server.harden': 'true',
    'server.authentication': 'true',
  };
}


const internal = require('internal');
const db = internal.db;
const sleep = internal.sleep;

const jsunity = require('jsunity');
const tasks = require('@arangodb/tasks');

const collName = "testTasks";

const print = internal.print;

function waitForState(state, coll, time = 10, explain = true) {
  if (!Array.isArray(state)) {
    state = [ state ];
  }

  while(time > 0) {
      const query = "FOR x IN @@name FILTER x.state IN @state RETURN x";
      let bind = {"@name": coll, "state" : state };

      if (explain) {
        print(state);
        db._explain(query, bind);
        explain = false;
      }

      let rv = db._query(query, bind).toArray();
      let found = ( rv.length > 0 );
      if (found) {
        return true;
      }
      time = time - 1;
      sleep(1);
  }
  return false;
}


print("#########################################################################");
print("#########################################################################");
print("#########################################################################");

function testSuite() {
  return {
    setUp: function() {
      db._drop(collName);
      db._create(collName);
    },
    tearDown: function() {},
    testFrist : function() {

      tasks.register({
        id: "task1",
        name: "this just tests task ex<cution",
        period: 1,
        command: function() {
          const collName = "testTasks";
          const db = require("internal").db;
          db._collection(collName).save({state : "started"});
          db._collection(collName).save({state : "done"});
        }
      });

      waitForState("started", collName);
      waitForState(["done" , "failed"], collName);

    },


  };
}
jsunity.run(testSuite);
return jsunity.done();
