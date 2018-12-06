/*jshint globalstrict:false, strict:false */
/*global fail, assertEqual, assertNotEqual, assertTrue, assertFalse */

////////////////////////////////////////////////////////////////////////////////
/// @brief test the index
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
/// Copyright holder is triAGENS GmbH, Cologne, Germany
///
/// @author Dr. Frank Celler, Lucas Dohmen
/// @author Copyright 2012, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

var jsunity = require("jsunity");
var internal = require("internal");
var errors = internal.errors;
var testHelper = require("@arangodb/test-helper").Helper;

function backgroundIndexSuite() {
  'use strict';
  let cn = "UnitTestsCollectionIdx";
  let tasks = require("@arangodb/tasks");

  return {

    setUp : function () {
      internal.db._drop(cn);
      internal.db._create(cn);
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
      internal.db._drop(cn);
    },

    testInsertInParallel: function () {
      let n = 10;
      for (let i = 0; i < n; ++i) {
        let command = `let c = require("internal").db._collection("${cn}"); 
                       let x = 25; while(x-- > 0) {
                         let docs = []; 
                         for(let i = 0; i < 1000; i++) {
                           docs.push({value:i})
                         } 
                         c.save(docs);
                       }`;
        tasks.register({ name: "UnitTestsIndexInsert" + i, command: command });
      }

      let time = require("internal").time;
      let start = time();
      while (true) {
        let indexes = require("internal").db._collection(cn).getIndexes();
        if (indexes.length === n + 1) {
          // primary index + user-defined indexes
          break;
        }
        if (time() - start > 180) {
          // wait for 3 minutes maximum
          fail("Timeout creating 80 indices after 3 minutes: " + JSON.stringify(indexes));
        }
        require("internal").wait(0.5, false);
      }
        
      let indexes = require("internal").db._collection(cn).getIndexes();
      assertEqual(n + 1, indexes.length);
    },

    testCreateInParallelDuplicate: function () {
      let n = 100;
      for (let i = 0; i < n; ++i) {
        let command = 'require("internal").db._collection("' + cn + '").ensureIndex({ type: "hash", fields: ["value' + (i % 4) + '"] });';
        tasks.register({ name: "UnitTestsIndexCreate" + i, command: command });
      }

      let time = require("internal").time;
      let start = time();
      while (true) {
        let indexes = require("internal").db._collection(cn).getIndexes();
        if (indexes.length === 4 + 1) {
          // primary index + user-defined indexes
          break;
        }
        if (time() - start > 180) {
          // wait for 3 minutes maximum
          fail("Timeout creating indices after 3 minutes: " + JSON.stringify(indexes));
        }
        require("internal").wait(0.5, false);
      }
      
      // wait some extra time because we just have 4 distinct indexes
      // these will be created relatively quickly. by waiting here a bit
      // we give the other pending tasks a chance to execute too (but they
      // will not do anything because the target indexes already exist)
      require("internal").wait(5, false);
        
      let indexes = require("internal").db._collection(cn).getIndexes();
      assertEqual(4 + 1, indexes.length);
    }

  };
}

jsunity.run(backgroundIndexSuite);

return jsunity.done();
