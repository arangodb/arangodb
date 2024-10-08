/*jshint globalstrict:false, strict:false */
/*global assertEqual, fail */

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

const jsunity = require("jsunity");
const arangodb = require("@arangodb");
const db = arangodb.db;
const tasks = require("@arangodb/tasks");
const internal = require("internal");
const { deriveTestSuite } = require('@arangodb/test-helper');
const ERRORS = arangodb.errors;
let IM = global.instanceManager;
const cn = "UnitTestsCollection";
const taskName = "UnitTestsTask";
  
let setupCollection = (type) => {
  let c;
  if (type === 'edge') {
    c = db._createEdgeCollection(cn);
  } else {
    c = db._create(cn);
  }
  let docs = [];
  for (let i = 0; i < 5000; ++i) {
    docs.push({ value1: i, value2: "test" + i, _from: "v/test" + i, _to: "v/test" + i });
  }
  for (let i = 0; i < 350000; i += docs.length) {
    c.insert(docs);
  }
  return c;
};
  
let shutdownTask = (task) => {
  db._useDatabase("_system");   // We need to switch to the _system database
                                // such that we are not tripped off by a
                                // "database not found" exception. This is
                                // only necessary for the drop database test.
  try {
    if (task === undefined) {
      tasks.unregister(taskName);
    } else {
      while (true) {
        // will be left by exception here
        tasks.get(task);
        require("internal").wait(0.25, false);
      }
    }
  } catch (err) {
    // "task not found" means the task is finished
  }
};

function BaseTestConfig (dropCb, expectedError) {
  return {
    testIndexCreationAborts : function () {
      let c = setupCollection('document');
      let task = dropCb();

      try {
        c.ensureIndex({ type: "persistent", fields: ["value1", "value2"] });
        fail();
      } catch (err) {
        // unfortunately it is possible that this fails because of unexpected
        // scheduling. the whole test is time-based and assumes reasonable
        // scheduling, which probably isn't guaranteed on some CI servers.
        assertEqual(expectedError, err.errorNum);
      } finally {
        shutdownTask(task);
      }
    },
    
    testIndexCreationInBackgroundAborts : function () {
      let c = setupCollection('document');
      let task = dropCb();

      try {
        c.ensureIndex({ type: "persistent", fields: ["value1", "value2"], inBackground: true });
        fail();
      } catch (err) {
        // unfortunately it is possible that this fails because of unexpected
        // scheduling. the whole test is time-based and assumes reasonable
        // scheduling, which probably isn't guaranteed on some CI servers.
        assertEqual(expectedError, err.errorNum);
      } finally {
        shutdownTask(task);
      }
    },
    
    testWarmupAborts : function () {
      if (!IM.debugCanUseFailAt()) {
        return;
      }

      IM.debugSetFailAt("warmup::executeDirectly");
      
      let c = setupCollection('edge');
      let task = dropCb();

      try {
        c.loadIndexesIntoMemory();
        fail();
      } catch (err) {
        // unfortunately it is possible that this fails because of unexpected
        // scheduling. the whole test is time-based and assumes reasonable
        // scheduling, which probably isn't guaranteed on some CI servers.
        assertEqual(expectedError, err.errorNum);
      } finally {
        shutdownTask(task);
      }
    },

  };
}

function AbortLongRunningOperationsWhenCollectionIsDroppedSuite() {
  'use strict';

  let dropCb = () => {
    let task = tasks.register({
      name: taskName,
      command: function() {
        let db = require("internal").db;
        let cn = "UnitTestsCollection";
        db[cn].insert({ _key: "runner1", _from: "v/test1", _to: "v/test2" });

        while (!db[cn].exists("runner2")) {
          require("internal").sleep(0.02);
        }

        require("internal").sleep(0.02);
        db._drop(cn);
      },
    });

    try {
      while (!db[cn].exists("runner1")) {
        require("internal").sleep(0.02);
      }
      db[cn].insert({ _key: "runner2", _from: "v/test1", _to: "v/test2" });
      return task;
    } catch (err) {
      shutdownTask(task);
      throw err;
    }
  };

  let suite = {
    tearDown: function () {
      shutdownTask();
      IM.debugClearFailAt();
      db._drop(cn);
    }
  };

  deriveTestSuite(BaseTestConfig(dropCb, ERRORS.ERROR_ARANGO_DATA_SOURCE_NOT_FOUND.code), suite, '_collection');
  return suite;
}

function AbortLongRunningOperationsWhenDatabaseIsDroppedSuite() {
  'use strict';

  let dropCb = () => {
    let old = db._name();
    db._useDatabase('_system');
    try {
      let task = tasks.register({
        name: taskName,
        command: function(params) {
          let db = require("internal").db;
          db._useDatabase(params.old);
          let cn = "UnitTestsCollection";
          db[cn].insert({ _key: "runner1", _from: "v/test1", _to: "v/test2" });

          while (!db[cn].exists("runner2")) {
            require("internal").sleep(0.02);
          }

          require("internal").sleep(0.02);
          db._useDatabase('_system');
          db._dropDatabase(cn);
        },
        params: { old },
        isSystem: true,
      });

      try {
        db._useDatabase(old);
        while (!db[cn].exists("runner1")) {
          require("internal").sleep(0.02);
        }
        db[cn].insert({ _key: "runner2", _from: "v/test1", _to: "v/test2" });
        return task;
      } catch (err) {
        db._useDatabase(old);
        shutdownTask(task);
        throw err;
      }
    } finally {
      db._useDatabase(old);
    }
  };

  let suite = {
    setUp: function () {
      db._createDatabase(cn);
      db._useDatabase(cn);
    },
    tearDown: function () {
      shutdownTask();
      IM.debugClearFailAt();
      db._useDatabase('_system');
      try {
        db._dropDatabase(cn);
      } catch (err) {
        // in most cases the DB will already have been deleted
      }
    }
  };

  deriveTestSuite(BaseTestConfig(dropCb, ERRORS.ERROR_ARANGO_DATABASE_NOT_FOUND.code), suite, '_database');
  return suite;
}

jsunity.run(AbortLongRunningOperationsWhenCollectionIsDroppedSuite);
jsunity.run(AbortLongRunningOperationsWhenDatabaseIsDroppedSuite);
return jsunity.done();
