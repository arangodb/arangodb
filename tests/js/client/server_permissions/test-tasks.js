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
  return {
    'server.harden': 'true',
    'server.authentication': 'true',
    'server.jwt-secret': 'abc123',
    'javascript.harden' : 'true',
    'javascript.files-whitelist' : [
      '^$'
    ],
    'javascript.endpoints-whitelist' : [
      'ssl://arangodb.com:443'
    ],
  };
}

const jsunity = require('jsunity');

const internal = require('internal');
const db = internal.db;

// HELPER FUNCTIONS
//get first document in collection that has one of the given states
function getFirstOfState(state, coll) {
  if (!Array.isArray(state)) {
    state = [ state ];
  }

  const query = "FOR x IN @@name FILTER x.state IN @state RETURN x";
  let bind = {"@name": coll, "state" : state };
  return db._query(query, bind).toArray()[0];
}

function waitForState(state, coll, time = 10) {
  while (time-- > 0) {
    if (getFirstOfState(state, coll) !== undefined) {
      return true;
    }
    internal.sleep(1);
  }
  return false;
}

//check if document has "content" attribute containing "ArangoError `num` ....."
function contentHasArangoError(doc, num) {
  let content = doc['content'];
  if (typeof content === 'string' &&
      content.startsWith("ArangoError " + String(num))) {
    return true;
  }
  return false;
}

// HELPER FUNCTIONS - END


function testSuite() {
  const collName = "testTasks";
  const taskName = "testTasks-permissions";
  const tasks = require('@arangodb/tasks');

  let assertFailing = function(func) {
    let command = function(params) {
      const db = require("internal").db;
      db._collection(params.coll).save({state : "started"});

      let state = "done";
      let content;
      try {
        eval(params.func);
      } catch (ex) {
        state = "failed";
        content = String(ex);
      }
      db._collection(params.coll).save({state, content});
    };

    tasks.register({
      id: taskName,
      offset: 0.001,
      command,
      params : { 
        coll : collName,
        func: String(func)
      }
    });

    assertTrue(waitForState("started", collName));
    assertTrue(waitForState("failed", collName));
    let first = getFirstOfState("failed", collName);
    assertTrue(contentHasArangoError(first, 11));
  };

  return {
    setUp: function() {
      db._drop(collName);
      db._create(collName);
      try {
        tasks.unregister(taskName);
      } catch (err) {}
    },

    tearDown: function() {
      try {
        tasks.unregister(taskName);
      } catch (err) {}
      db._drop(collName);
    },

    testFramework : function() {
      tasks.register({
        id: taskName,
        offset: 0.001,
        command: function(params) {
          const internal = require("internal");
          const db = internal.db;
          db._collection(params.coll).save({state : "started"});
          db._collection(params.coll).save({state : "done"});
        },
        params : { coll : collName }
      });

      assertTrue(waitForState("started", collName));
      assertTrue(waitForState("done", collName));
    },

    testPasswd : function() {
      assertFailing(`require("fs").read('/etc/passwd');`);
    },
    
    testGetPid : function() {
      assertFailing(`require("internal").getPid();`);
    },

    testDownload : function() {
      assertFailing(`require("internal").download("https://heise:443/foo/bar");`);
    },
  };
}

jsunity.run(testSuite);
return jsunity.done();
