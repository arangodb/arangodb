/*jshint globalstrict:false, strict:false */
/*global fail, assertEqual, assertTrue */

////////////////////////////////////////////////////////////////////////////////
/// @brief test parallel index drop/create
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
///
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

const jsunity = require("jsunity");
const internal = require("internal");
const errors = internal.errors;
const db = internal.db;

function ParallelIndexCreateDropSuite() {
  'use strict';
  const cn = "UnitTestsCollectionIdx";
  const tasks = require("@arangodb/tasks");
  const tasksCompleted = () => {
    return 0 === tasks.get().filter((task) => {
      return (task.id.match(/^UnitTest/) || task.name.match(/^UnitTest/));
    }).length;
  };
  const waitForTasks = () => {
    const time = internal.time;
    const start = time();
    while (!tasksCompleted()) {
      if (time() - start > 300) { // wait for 5 minutes maximum
        fail("Timeout after 5 minutes");
      }
      internal.sleep(0.5);
    }
  };
      
  const threads = 6;
  const attrs = 5;

  return {

    setUpAll : function () {
      db._drop(cn);
      let c = db._create(cn);

      // fill with some initial data
      let docs = [];
      for (let i = 0; i < threads; ++i) {
        // only populate some of the attributes, to have more variance in 
        // index creation/drop speed
        for (let j = i; j < attrs; ++j) {
          docs.push({ ["thread-" + i - "-attribute-" + j] : i * j });
        }
      }
      c.insert(docs);
    },

    tearDownAll : function () {
      tasks.get().forEach(function(task) {
        if (task.id.match(/^UnitTest/) || task.name.match(/^UnitTest/)) {
          try {
            tasks.unregister(task);
          } catch (err) {
          }
        }
      });
      db._drop(cn);
    },
    
    testCreateDropInParallel: function () {
      let c = require("internal").db._collection(cn);
      // lets insert the rest via tasks
      for (let i = 0; i < threads; ++i) {
        let command = `
let db = require("internal").db;
let c = db._collection("${cn}");
for (let iteration = 0; iteration < 15; ++iteration) {
  require("console").log("thread ${i}, iteration " + iteration);
  let indexes = [];
  for (let i = 0; i < ${attrs}; ++i) {
    indexes.push(c.ensureIndex({ type: "persistent", sparse: ${i} % 2 == 0, fields: ["thread-${i}-attribute-" + i] }));
  }
  indexes.forEach(function(index) {
    c.dropIndex(index);
  });
}
c.insert({ _key: "done${i}", value: true });
`;
        tasks.register({ name: "UnitTestsIndexCreateDrop" + i, command: command });
      }

      // wait for insertion tasks to complete
      waitForTasks();
      
      // check that all indexes except primary are gone
      assertEqual(1, c.indexes().length);
      let count = threads;
      for (let i = 0; i < threads; ++i) {
        count += attrs <= i ? 0 : attrs - i;
      }
      assertEqual(count, c.count());
      for (let i = 0; i < threads; ++i) {
        assertTrue(c.document("done" + i).value);
      }
    },
  };
}

jsunity.run(ParallelIndexCreateDropSuite);

return jsunity.done();
