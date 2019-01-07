/*jshint globalstrict:false, strict:false */
/*global fail, assertEqual, assertNotEqual, assertTrue, assertFalse */

////////////////////////////////////////////////////////////////////////////////
/// @brief test the index
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2018 ArangoDB GmbH, Cologne, Germany
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
/// @author 2018 Simon Grätzer, Dan Larkin-York
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
    return 0 === tasks.get().filter((task) => {
      return (task.id.match(/^UnitTest/) || task.name.match(/^UnitTest/));
    }).length;
  };
  const waitForTasks = () => {
    const time = require("internal").time;
    const start = time();
    while (!tasksCompleted()) {
      if (time() - start > 300) { // wait for 5 minutes maximum
        fail("Timeout after 5 minutes");
      }
      require("internal").wait(0.5, false);
    }
    require('internal').wal.flush(true, true);
    // wait an extra second for good measure
    require("internal").wait(1.0, false);
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

    testInsertParallelNonUnique: function () {
      let c = require("internal").db._collection(cn);
      // first lets add some initial documents
      let x = 10; 
      while(x-- > 0) {
        let docs = []; 
        for(let i = 0; i < 1000; i++) {
          docs.push({value:i});
        } 
        c.save(docs);
      }

      // lets insert the rest via tasks
      let n = 9;
      for (let i = 0; i < n; ++i) {
        let command = `const c = require("internal").db._collection("${cn}"); 
                       let x = 10;
                       while(x-- > 0) {
                         let docs = []; 
                         for(let i = 0; i < 1000; i++) {
                           docs.push({value:i})
                         } 
                         c.save(docs);
                       }`;
        tasks.register({ name: "UnitTestsIndexInsert" + i, command: command });
      }

      // create the index on the main thread
      c.ensureIndex({type: 'hash', fields: ['value'], unique: false, inBackground: true});

      // wait for insertion tasks to complete
      waitForTasks();
      
      // sanity checks
      assertEqual(c.count(), 100000);
      for (let i = 0; i < 1000; i++) { // 100 entries of each value [0,999]
        let cursor = db._query("FOR doc IN @@coll FILTER doc.value == @val RETURN 1", 
                               {'@coll': cn, 'val': i}, {count:true});
        assertEqual(cursor.count(), 100);
      }

      internal.waitForEstimatorSync(); // make sure estimates are consistent
      let indexes = c.getIndexes(true);
      for (let i of indexes) {
        switch (i.type) {
          case 'primary':
            break;
          case 'hash':
            assertEqual(i.selectivityEstimate, 0.01);
            break;
          default:
            fail();
        }
      }
    },

    testInsertParallelUnique: function () {
      let c = require("internal").db._collection(cn);
      // first lets add some initial documents
      let x = 0;
      while(x < 10000) {
        let docs = []; 
        for(let i = 0; i < 1000; i++) {
          docs.push({value: x++});
        } 
        c.save(docs);
      }

      // lets insert the rest via tasks
      for (let i = 1; i < 5; ++i) {
        let command = `const c = require("internal").db._collection("${cn}"); 
                       let x = ${i} * 10000; 
                       while(x < ${i + 1} * 10000) {
                         let docs = []; 
                         for(let i = 0; i < 1000; i++) {
                           docs.push({value: x++})
                         } 
                         c.save(docs);
                       }`;
        tasks.register({ name: "UnitTestsIndexInsert" + i, command: command });
      }

      // create the index on the main thread
      c.ensureIndex({type: 'hash', fields: ['value'], unique: true, inBackground: true });

      // wait for insertion tasks to complete
      waitForTasks();
      
      // sanity checks
      assertEqual(c.count(), 50000);
      for (let i = 0; i < 50000; i++) {
        const cursor = db._query("FOR doc IN @@coll FILTER doc.value == @val RETURN 1", 
                               {'@coll': cn, 'val': i}, {count:true});
        assertEqual(cursor.count(), 1);
      }

      let indexes = c.getIndexes(true);
      for (let i of indexes) {
        switch (i.type) {
          case 'primary':
            break;
          case 'hash':
            assertEqual(i.selectivityEstimate, 1.0);
            break;
          default:
            fail();
        }
      }
    },

    testInsertParallelUniqueConstraintViolation: function () {
      let c = require("internal").db._collection(cn);
      // first lets add some initial documents
      let x = 0;
      while(x < 10000) {
        let docs = []; 
        for(let i = 0; i < 1000; i++) {
          docs.push({value: x++});
        } 
        c.save(docs);
      }

      // lets insert the rest via tasks
      for (let i = 1; i < 5; ++i) {
        let command = `const c = require("internal").db._collection("${cn}"); 
                       let x = ${i} * 10000; 
                       while(x < ${i + 1} * 10000) {
                         let docs = []; 
                         for(let i = 0; i < 1000; i++) {
                           docs.push({value: x++})
                         } 
                         c.save(docs);
                       }`;
        tasks.register({ name: "UnitTestsIndexInsert" + i, command: command });
      }

      // now insert a document that will cause a conflict while indexing
      c.save({value: 1 });

      try {
        // create the index on the main thread
        c.ensureIndex({type: 'hash', fields: ['value'], unique: true, inBackground: true});
        fail();
      } catch(err) {
        assertEqual(errors.ERROR_ARANGO_UNIQUE_CONSTRAINT_VIOLATED.code, err.errorNum, err);
      }

      // wait for insertion tasks to complete
      waitForTasks();
      
      // sanity checks
      assertEqual(c.count(), 50001);

      let indexes = c.getIndexes();
      for (let i of indexes) {
        switch (i.type) {
          case 'primary':
            break;
          case 'hash':
          default:
            fail();
        }
      }
    },

    testRemoveParallel: function () {
      let c = require("internal").db._collection(cn);
      // first lets add some initial documents
      let x = 0;
      while(x < 100000) {
        let docs = []; 
        for(let i = 0; i < 1000; i++) {
          docs.push({_key: "test_" + x, value: x++});
        } 
        c.save(docs);
      }
      
      assertEqual(c.count(), 100000);

      // lets remove half via tasks
      for (let i = 0; i < 10; ++i) {
        let command = `const c = require("internal").db._collection("${cn}"); 
                       if (!c) {
                         throw new Error('could not find collection');
                       }
                       let x = ${i} * 10000; 
                       while(x < ${i} * 10000 + 5000) {
                         let docs = [];
                         for(let i = 0; i < 1000; i++) {
                           docs.push("test_" + x++);
                         }
                         let removed = false;
                         while (!removed) {
                           const res = c.remove(docs);
                           removed = (res.filter(r => !r.error).length === 0);
                         }
                       }`;
        tasks.register({ name: "UnitTestsIndexRemove" + i, command: command });
      }

      // create the index on the main thread
      c.ensureIndex({type: 'hash', fields: ['value'], inBackground: true });

      // wait for insertion tasks to complete
      waitForTasks();
      
      // sanity checks
      assertEqual(c.count(), 50000);
      for (let i = 0; i < 10; i++) { // check for remaining docs via index
        for (let x = i * 10000 + 5000; x < (i+1) * 10000; x++) {
          const cursor = db._query("FOR doc IN @@coll FILTER doc.value == @val RETURN 1", 
                                {'@coll': cn, 'val': x}, {count:true});
          assertEqual(cursor.count(), 1);
        }
      }
      for (let i = 0; i < 10; i++) { // check for removed docs via index
        for (let x = i * 10000; x < i * 10000 + 5000; x++) {
          const cursor = db._query("FOR doc IN @@coll FILTER doc.value == @val RETURN 1", 
                                {'@coll': cn, 'val': x}, {count:true});
          assertEqual(cursor.count(), 0);
        }
      }

      let indexes = c.getIndexes(true);
      for (let i of indexes) {
        switch (i.type) {
          case 'primary':
            break;
          case 'hash':
            break;
          default:
            fail();
        }
      }
    },

    testUpdateParallel: function () {
      let c = require("internal").db._collection(cn);
      // first lets add some initial documents
      let x = 0;
      while(x < 100000) {
        let docs = []; 
        for(let i = 0; i < 1000; i++) {
          docs.push({_key: "test_" + x, value: x++});
        } 
        c.save(docs);
      }
      
      assertEqual(c.count(), 100000);

      // lets update all via tasks
      for (let i = 0; i < 10; ++i) {
        let command = `const c = require("internal").db._collection("${cn}"); 
                       if (!c) {
                         throw new Error('could not find collection');
                       }
                       let x = ${i * 10000}; 
                       while(x < ${(i+1) * 10000}) {
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
        tasks.register({ name: "UnitTestsIndexUpdate" + i, command: command });
      }

      // wait for insertion tasks to complete
      waitForTasks();
      
      // create the index on the main thread
      c.ensureIndex({type: 'skiplist', fields: ['value'], inBackground: true });

      // sanity checks
      assertEqual(c.count(), 100000);
      // check for new entries via index
      const newCursor = db._query("FOR doc IN @@coll FILTER doc.value >= @val RETURN 1", 
                                  {'@coll': cn, 'val': 100000}, {count:true});
      assertEqual(newCursor.count(), 100000);
      // check for old entries via index
      const oldCursor = db._query("FOR doc IN @@coll FILTER doc.value < @val RETURN 1", 
                                  {'@coll': cn, 'val': 100000}, {count:true});
      assertEqual(oldCursor.count(), 0);

      let indexes = c.getIndexes(true);
      for (let i of indexes) {
        switch (i.type) {
          case 'primary':
            break;
          case 'skiplist':
            break;
          default:
            fail();
        }
      }
    },

  };
}

jsunity.run(backgroundIndexSuite);

return jsunity.done();
