/*jshint globalstrict:false, strict:false */
/* global getOptions, assertTrue, assertFalse, assertEqual, assertMatch, fail, arango */

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

const mountPoint = '/test-redirect';


if (getOptions === true) {
  return {
    'server.harden': 'true',
    'server.authentication': 'true',
    'server.jwt-secret': 'abc123',
    'javascript.harden' : 'true',
    'javascript.files-allowlist' : [
      '^$'
    ],
    'javascript.endpoints-allowlist' : [
      'ssl://arango.ai:443',
    ],
    'javascript.endpoints-denylist' : [
      '.*://.*:[0-9]+/test-redirect/redirectloop/3'
    ]
  };
}

const jsunity = require('jsunity');
const FoxxManager = require('@arangodb/foxx/manager');

const internal = require('internal');
const fs = require('fs');
const db = internal.db;
const basePath = fs.makeAbsolute(fs.join(internal.pathForTesting('common'), 'test-data', 'apps'));
const foxxApp = fs.join(basePath, 'redirect');
let IM = global.instanceManager;

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
      FoxxManager.uninstall(mountPoint, { force: true });
      FoxxManager.install(foxxApp, mountPoint);
    },

    tearDown: function() {
      FoxxManager.uninstall(mountPoint, { force: true });
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

    testDownloadRedirect : function() {
      assertFailing(`require("@arangodb/request").get({url: "${IM.url}${mountPoint}/redirectloop/0", maxRedirects: 5, followRedirects: true });`);
    },
  };
}

jsunity.run(testSuite);
return jsunity.done();
