/*jshint globalstrict:false, strict:false */
/*global fail, assertEqual, assertTrue */

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
// //////////////////////////////////////////////////////////////////////////////

const jsunity = require("jsunity");
const internal = require("internal");
const errors = internal.errors;
const db = internal.db;
const {
  launchPlainSnippetInBG,
  joinBGShells,
  cleanupBGShells
} = require('@arangodb/testutils/client-tools').run;
let IM = global.instanceManager;
const waitFor = IM.options.isInstrumented ? 80 * 7 : 80;

function ParallelIndexCreateDropSuite() {
  'use strict';
  const cn = "UnitTestsCollectionIdx";
  const threads = 6;
  const attrs = 5;
  let clients = [];

  return {

    setUpAll : function () {
      clients = [];
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
      joinBGShells(IM.options, clients, waitFor, cn);
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
        clients.push({client: launchPlainSnippetInBG(command, "UnitTestsIndexCreateDrop" + i)});
      }

      // wait for insertion tasks to complete
      joinBGShells(IM.options, clients, waitFor, cn);
      clients = [];
      
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
