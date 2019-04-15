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
    'javascript.harden' : 'true',
    'javascript.files-black-list': [
      '^/etc/',
    ],
    'javascript.endpoints-white-list' : [
      'ssl://arangodb.com:443'
    ],
  };
}

const jsunity = require('jsunity');

const internal = require('internal');
const db = internal.db;
const sleep = internal.sleep;
const tasks = require('@arangodb/tasks');


//TODO - delete when done 
const print = internal.print;

// HELPER FUNCTIONS
function waitForState(state, coll, time = 10, explain = false) {
  if (!Array.isArray(state)) {
    state = [ state ];
  }

  while(time > 0) {
    const query = "FOR x IN @@name FILTER x.state IN @state RETURN x";
    let bind = {"@name": coll, "state" : state };

    let rv = db._query(query, bind).toArray();
    if (explain) {
      print("#########################################################################");
      db._explain(query, bind);
      print("RESULT as Array:");
      print(rv);
      explain = false;
      print("#########################################################################");
    }

    let found = ( rv.length > 0 );
    if (found) {
      return true;
    }
    time = time - 1;
    sleep(1);
  }
  return false;
}

//get first document in collection that has one of the given states
function getFirstOfState(state, coll) {
  if (!Array.isArray(state)) {
    state = [ state ];
  }

  const query = "FOR x IN @@name FILTER x.state IN @state RETURN x";
  let bind = {"@name": coll, "state" : state };
  return db._query(query, bind).toArray()[0];
}

//check if document has "content" attribute containing "ArangoError `num` ....."
function contentHasArangoError(doc, num) {
  let content = doc['content'];
  if(typeof content === 'string') {
    if (content.startsWith("ArangoError " + String(num))) {
      return true;
    }
  }
  return false;
}

function debug(coll) {
  internal.print("UUUUUUUUUUUUUUUUUUUUULLLLLLLLLLLLLLLFFFFFFFFFFFFFFFFFFF");
  if (waitForState(["done", "failed"], coll)) {
    let first = getFirstOfState(["done", "failed"], coll);
    internal.print(first);
  }
}

// HELPER FUNCTIONS - END

const collName = "testTasks";
let counter = 0;
let currentCollection;
let currentCollectionName;
let currentTask;

function testSuite() {
  return {
    setUp: function() {
      counter = counter + 1;
      currentCollectionName = collName + String(counter);
      currentTask = collName + String(counter);
      db._drop(currentCollectionName);
      currentCollection = db._create(currentCollectionName);
    },

    tearDown: function() {
      tasks.unregister(currentTask);
      db._drop(currentCollectionName);
    },


    testFramework : function() {
      tasks.register({
        id: currentTask,
        name: "test task-test-framework",
        period: 30,
        command: function(params) {
          const internal = require("internal");
          const db = internal.db;
          db._collection(params.coll).save({state : "started"});
          db._collection(params.coll).save({state : "done"});
        },
        params : { coll : currentCollectionName }
      });

      assertTrue(waitForState("started", currentCollectionName));
      assertTrue(waitForState(["done" , "failed"], currentCollectionName));
    },

    testPasswd : function() {
      tasks.register({
        id: currentTask,
        name: "try to read passwd",
        period: 30,
        command: function(params) {
          const internal = require("internal");
          const db = internal.db;
          db._collection(params.coll).save({state : "started"});

          let state = "done";
          let content;
          try {
              content = require("fs").read('/etc/passwd');
          } catch (ex) {
              content = String(ex);
              state = "failed";
          }
          db._collection(params.coll).save({state, content});
        },
        params : { coll : currentCollectionName }
      });

      assertTrue(waitForState("started", currentCollectionName));
      //debug(currentCollectionName)
      assertTrue(waitForState("failed", currentCollectionName));
      let first = getFirstOfState("failed", currentCollectionName);
      assertTrue(contentHasArangoError(first, 11));
    },

    testGetPid : function() {
      tasks.register({
        id: currentTask,
        name: "get pid",
        period: 30,
        command: function(params) {
          const internal = require("internal");
          const db = internal.db;
          db._collection(params.coll).save({state : "started"});

          let state = "done";
          let content;
          try {
              content = internal.getPid();
          } catch (ex) {
              content = String(ex);
              state = "failed";
          }
          db._collection(params.coll).save({state, content});
        },
        params : { coll : currentCollectionName }
      });

      assertTrue(waitForState("started", currentCollectionName), "task not started");
      //debug(currentCollectionName)
      assertTrue(waitForState("failed", currentCollectionName));
      let first = getFirstOfState("failed", currentCollectionName);
      assertTrue(contentHasArangoError(first, 11));
    },


    testDownload : function() {
      tasks.register({
        id: currentTask,
        name: "download",
        period: 30,
        command: function(params) {
          const internal = require("internal");
          const db = internal.db;
          db._collection(params.coll).save({state : "started"});

          let state = "done";
          let content;
          try {
              content = internal.download("https://heise:443/foo/bar");
          } catch (ex) {
              content = String(ex);
              state = "failed";
          }
          db._collection(params.coll).save({state, content});
        },
        params : { coll : currentCollectionName }
      });

      assertTrue(waitForState("started", currentCollectionName), "task not started");
      //debug(currentCollectionName)
      assertTrue(waitForState("failed", currentCollectionName));
      let first = getFirstOfState("failed", currentCollectionName);
      assertTrue(contentHasArangoError(first, 11));
    },
  };
}

jsunity.run(testSuite);
return jsunity.done();
