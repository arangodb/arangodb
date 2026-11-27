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
const {
  launchPlainSnippetInBG,
  joinBGShells
} = require('@arangodb/testutils/client-tools').run;
let IM = global.instanceManager;
const waitFor = IM.options.isInstrumented ? 150 * 7 : 150;


function backgroundIndexUniqueSuite() {
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

    testInsertParallelUnique: function () {
      let c = require("internal").db._collection(cn);
      // first lets add some initial documents
      let x = 0;
      while (x < 5000) {
        let docs = []; 
        for (let i = 0; i < 1000; i++) {
          docs.push({value: x++});
        } 
        c.save(docs);
      }

      const idxDef = {type: 'persistent', fields: ['value'], unique: true, inBackground: true};
      // lets insert the rest via tasks
      for (let i = 1; i < 5; ++i) {
        if (i === 2) { // create the index in a task
          let command = `const c = require("internal").db._collection("${cn}"); 
          let idx = c.ensureIndex(${JSON.stringify(idxDef)});
          c.save({_key: 'myindex', index: idx});`;
          clients.push(launchPlainSnippetInBG(command,"UnitTestsIndexCreateIDX" + i));
        }
        let command = `const c = require("internal").db._collection("${cn}"); 
                       let x = ${i} * 5000; 
                       while (x < ${i + 1} * 5000) {
                         let docs = []; 
                         for (let i = 0; i < 1000; i++) {
                           docs.push({value: x++})
                         } 
                         c.save(docs);
                       }`;
        clients.push(launchPlainSnippetInBG(command, "UnitTestsIndexInsert" + i));
      }

      // wait for insertion tasks to complete
      joinBGShells(IM.options, clients, waitFor, cn);
      clients = [];
      
      // basic checks
      assertEqual(c.count(), 25001);

      // verify that the index was created
      let idx = c.document('myindex').index;
      assertNotUndefined(idx);
      idxDef.inBackground = false;
      let cmp = c.ensureIndex(idxDef);
      assertEqual(cmp.id, idx.id);

      for (let i = 0; i < 25000; i++) {
        const cursor = db._query("FOR doc IN @@coll FILTER doc.value == @val RETURN 1", 
                               {'@coll': cn, 'val': i}, {count:true});
        assertEqual(cursor.count(), 1);
      }

      let indexes = c.indexes(true);
      for (let i of indexes) {
        switch (i.type) {
          case 'primary':
            break;
          case 'persistent':
            assertEqual(i.selectivityEstimate, 1.0);
            break;
          default:
            fail();
        }
      }
    },
  };
}

jsunity.run(backgroundIndexUniqueSuite);

return jsunity.done();
