/*jshint globalstrict:false, strict:false */
/*global fail, assertEqual, assertNotEqual, assertTrue, assertFalse, assertNotUndefined */

////////////////////////////////////////////////////////////////////////////////
/// @brief test indexes
///
/// DISCLAIMER
///
/// Copyright 2018-2019 ArangoDB GmbH, Cologne, Germany
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
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

const jsunity = require("jsunity");
const internal = require("internal");
const errors = internal.errors;
const db = internal.db;

function backgroundIndexSuite() {
  'use strict';
  const cn = "UnitTestsCollectionIdx";
  const tasks = require("@arangodb/tasks");
  const tasksCompleted = () => {
    return tasks.get().filter((task) => {
      return (task.id.match(/^UnitTest/) || task.name.match(/^UnitTest/));
    }).length;
  };
  const waitForTasks = () => {
    const time = internal.time;
    const start = time();
    let c = db[cn];
    let oldCount = c.count();
    let deadline = 300;
    let count = 0;
    // wait for 5 minutes + progress maximum
    let noTasksCompleted = tasksCompleted();
    while (true) {
      let newTasksCompleted = tasksCompleted();
      if (newTasksCompleted === 0) {
        break;
      }
      if (time() - start > deadline) { 
        fail(`Timeout after ${deadline / 60} minutes`);
      }
      internal.wait(0.5, false);
      count += 1;
      if (newTasksCompleted !== noTasksCompleted) {
        // one more minute per completed task!
        noTasksCompleted = newTasksCompleted;
        deadline += 60;
      }
      if (count % 2 === 0) {
        let newCount = c.count();
        if (newCount > oldCount) {
          // 10 more seconds for added documents
          oldCount = newCount;
          deadline += 10;
        }
      }
    }
    internal.wal.flush(true, true);
    // wait an extra second for good measure
    internal.wait(1.0, false);
  };

  return {

    setUp : function () {
      db._drop(cn);
      db._create(cn);
    },

    tearDown : function () {
      tasks.get().forEach(function(task) {
        if (task.id.match(/^UnitTest/) || task.name.match(/^UnitTest/)) {
          try {
            tasks.unregister(task);
          }
          catch (err) {
          }
        }
      });
      db._drop(cn);
    },

    testCreateIndexTransactionInsertRemove: function () {
      let c = require("internal").db._collection(cn);
      // first lets add some initial documents
      const numDocuments = 100000;
      let x = 0;
      while (x < numDocuments) {
        let docs = []; 
        for(let i = 0; i < 1000; i++) {
          docs.push({_key: "test_" + x, value: x});
          ++x;
        } 
        c.save(docs);
      }
      
      assertEqual(c.count(), numDocuments);

      {
        // build the index in background
        let command = `const c = require("internal").db._collection("${cn}"); 
          c.ensureIndex({type: 'persistent', fields: ['value'], unique: false, inBackground: true});`;
        tasks.register({ name: "UnitTestsIndexCreateIDX", command: command });
      }

      while (tasksCompleted() > 0) {
        const trx = db._createTransaction({collections: {write: [cn]}});
        const c = trx.collection(cn);
        c.insert({_key: "x", value: 1});
        c.remove({_key: "x", value: 1});
        trx.commit();
      }

      // wait for insertion tasks to complete
      waitForTasks();
      
      // basic checks
      assertEqual(c.count(), numDocuments);
      // check for new entries via index
      const cursor = db._query("FOR doc IN @@coll FILTER doc.value >= 0 RETURN 1",
                                  {'@coll': cn}, {count:true});
      assertEqual(cursor.count(), numDocuments);
    },
  };
  
}

jsunity.run(backgroundIndexSuite);

return jsunity.done();
