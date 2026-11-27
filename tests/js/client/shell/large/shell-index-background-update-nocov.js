/*jshint globalstrict:false, strict:false */
/*global fail, assertEqual, assertNotEqual, assertTrue, assertFalse, assertNotUndefined, arango */

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

const jsunity = require("jsunity");
const internal = require("internal");
const db = internal.db;
const { waitForEstimatorSync } = require('@arangodb/test-helper');
const {
  launchPlainSnippetInBG,
  joinBGShells
} = require('@arangodb/testutils/client-tools').run;
let IM = global.instanceManager;
const waitFor = IM.options.isInstrumented ? 150 * 7 : 150;


function backgroundIndexUpdateSuite() {
  'use strict';
  const cn = "UnitTestsCollectionIdx";
  let clients = [];

  return {

    setUp : function () {
      db._drop(cn);
      db._create(cn);
    },

    tearDown : function () {
      db._drop(cn);
    },

    testUpdateParallel: function () {
      let c = require("internal").db._collection(cn);
      // first lets add some initial documents
      let x = 0;
      while (x < 100000) {
        let docs = []; 
        for(let i = 0; i < 1000; i++) {
          docs.push({_key: "test_" + x, value: x});
          ++x;
        } 
        c.save(docs);
      }
      
      assertEqual(c.count(), 100000);

      // lets update all via tasks
      for (let i = 0; i < 10; ++i) {
        if (i === 5) { // create the index in a task
          let command = `const c = require("internal").db._collection("${cn}"); 
          c.ensureIndex({type: 'persistent', fields: ['value'], unique: false, inBackground: true});`;
          clients.push(launchPlainSnippetInBG(command, "UnitTestsIndexCreateIDX" + i));
        }
        let command = `const c = require("internal").db._collection("${cn}"); 
                       if (!c) {
                         throw new Error('could not find collection');
                       }
                       let x = ${i * 10000}; 
                       while (x < ${(i + 1) * 10000}) {
                         let updated = false;
                         const current = x++;
                         const key = "test_" + current
                         const doc = {value: current + 100000};
                         while (!updated) {
                           try {
                             const res = c.update(key, doc);
                             updated = true;
                           } catch (err) {}
                         }
                       }`;
        clients.push(launchPlainSnippetInBG(command, "UnitTestsIndexUpdate" + i));
      }

      // wait for insertion tasks to complete
      joinBGShells(IM.options, clients, waitFor, cn);
      clients = [];
      
      // basic checks
      assertEqual(c.count(), 100000);
      // check for new entries via index
      const newCursor = db._query("FOR doc IN @@coll FILTER doc.value >= @val RETURN 1", 
                                  {'@coll': cn, 'val': 100000}, {count:true});
      assertEqual(newCursor.count(), 100000);

      // check for old entries via index
      const oldCursor = db._query("FOR doc IN @@coll FILTER doc.value < @val RETURN 1", 
                                  {'@coll': cn, 'val': 100000}, {count:true});
      assertEqual(oldCursor.count(), 0);

      waitForEstimatorSync(); // make sure estimates are consistent
      let indexes = c.indexes(true);
      for (let i of indexes) {
        switch (i.type) {
          case 'primary':
            break;
          case 'persistent':
            assertTrue(Math.abs(i.selectivityEstimate - 1.0) < 0.005, i);
          break;
          default:
            fail();
        }
      }
    },
  };
}

jsunity.run(backgroundIndexUpdateSuite);

return jsunity.done();
