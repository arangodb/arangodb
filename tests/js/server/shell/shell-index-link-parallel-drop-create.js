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

function ParallelIndexLinkCreateDropSuite() {
  'use strict';
  const cn = "UnitTestsCollectionIdx";
  const vn = "UnitTestsViewIdx";
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
      
  return {
    setUpAll : function () {
      db._drop(cn);
      db._dropView(vn);
      db._createView(vn, "arangosearch", {});
      let c = db._create(cn);

      // fill with some initial data
      let docs = [];
      for (let i = 0; i < 100 * 1000; ++i) {
        docs.push({ value1: i, value2: "test" + i });
        if (docs.length === 5000) {
          c.insert(docs);
          docs = [];
        }
      }
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
      db._dropView(vn);
    },
    
    testCreateDropInParallel: function () {
      const threads = 4;
      const iterations = 15;

      let c = require("internal").db._collection(cn);

      for (let i = 0; i < threads; ++i) {
        let command = `
let db = require("internal").db;
let c = db._collection("${cn}");
`;

        if (i === 0) {
          command += `
let idx = db["${cn}"].ensureIndex({ type: "persistent", fields: ["value2"] });   
db["${cn}"].dropIndex(idx); 
`;
        } else {
          command += `
for (let iteration = 0; iteration < ${iterations}; ++iteration) {
  require("console").log("thread ${i}, iteration " + iteration);

  try {
    db._view("${vn}").properties({ links : { ${cn} : { includeAllFields: true} } }, true);
    db._view("${vn}").properties({ links : { ${cn} : null }}, true);   
  } catch (err) {
    // concurrent modification of links can fail in the cluster
    console.warn("caught exception: " + String(err));
  }
  db._view("${vn}");
  c.indexes();
}
`;
        }

        command += `
c.insert({ _key: "done${i}", value: true });
`;

        tasks.register({ name: "UnitTestsIndexCreateDrop" + i, command });
      }

      // wait for tasks to complete
      waitForTasks();
      
      // check that all indexes except primary are gone
      assertEqual(1, c.indexes().length);

      let properties = db._view(vn).properties();
      assertEqual({}, properties.links);
    },
  };
}

jsunity.run(ParallelIndexLinkCreateDropSuite);

return jsunity.done();
