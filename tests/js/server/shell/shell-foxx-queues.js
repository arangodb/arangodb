/*global assertEqual, assertNotEqual, assertNotUndefined, assertFalse, fail */

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
/// @author Copyright 2015, ArangoDB GmbH, Cologne, Germany
// //////////////////////////////////////////////////////////////////////////////

"use strict";

const jsunity = require("jsunity");
const internal = require("internal");
const db = require("internal").db;
const queues = require('@arangodb/foxx/queues');
const FoxxManager = require('org/arangodb/foxx/manager');
const fs = require('fs');
const { deriveTestSuite } = require('@arangodb/test-helper');
const basePath = fs.makeAbsolute(fs.join(internal.pathForTesting('common'), 'test-data', 'apps'));
  
const cn = "UnitTestsFoxx";
const qn = "UnitTestFoxxQueue";

function BaseTestConfig () {
  return {
    testCreateQueueInsideTransactionNoCollectionDeclaration : function () {
      try {
        db._executeTransaction({
          collections: {},
          action: function() {
            queues.create(qn);
          }
        });
        fail();
      } catch (err) {
        assertEqual(internal.errors.ERROR_TRANSACTION_UNREGISTERED_COLLECTION.code, err.errorNum);
      }
    },
    
    testCreateQueueInsideTransactionWithCollectionDeclaration : function () {
      db._executeTransaction({
        collections: { write: ["_queues"] },
        action: function() {
          queues.create(qn);
        }
      });
      assertEqual([], queues.get(qn).all());
    },

    testCreateEmptyQueue : function () {
      let queue = queues.create(qn);
      assertEqual([], queue.all());
    },
    
    testCreateQueueAndFetch : function () {
      let queue = queues.create(qn);
      assertEqual(qn, queues.get(qn).name);
    },

    testCheckJobIndexes : function () {
      let indexes = db._jobs.getIndexes();
      assertEqual(indexes.length, 3);
      indexes.forEach(idx => {
        switch(idx.type) {
          case "primary":
            break;
          case "skiplist":
            assertNotUndefined(idx.fields);
            assertFalse(idx.unique, idx);
            assertFalse(idx.sparse);
            assertEqual(idx.fields.length, 3);
            if (idx.fields[0] === "queue") {
              assertEqual(idx.fields, ["queue", "status", "delayUntil"]);
            } else {
              assertEqual(idx.fields, ["status", "queue", "delayUntil"]);
            }
            break;
          default:
            fail();
        }
      });
    },
    
    testCreateDelayedJob : function () {
      const delay = { delayUntil: Date.now() + 1000 * 86400 };
      let queue = queues.create(qn);
      let id = queue.push({
        name: 'testi',
        mount: '/test'
      }, {}, delay);
      assertEqual(1, queue.all().length);
      let job = db._jobs.document(id);
      assertEqual(id, job._id);
      assertEqual("pending", job.status);
      assertEqual(qn, job.queue);
      assertEqual("testi", job.type.name);
      assertEqual("/test", job.type.mount);
      assertEqual(0, job.runs);
      assertEqual([], job.failures);
      assertEqual({}, job.data);
      assertEqual(0, job.maxFailures);
      assertEqual(0, job.repeatTimes);
      assertEqual(0, job.repeatDelay);
      assertEqual(-1, job.repeatUntil);
    },
    
    testCreateAndExecuteJobThatWillFail : function () {
      let queue = queues.create(qn);
      let id = queue.push({
        name: 'testi',
        mount: '/test-this-does-not-exist-please-ignore'
      }, {});

      let tries = 0;
      while (++tries < 240) {
        var result = db._jobs.document(id);
        if (result.status === 'pending' || result.status === 'progress') {
          internal.wait(0.5, false);
        } else {
          break;
        }
      }
      assertEqual('failed', db._jobs.document(id).status);
    },
    
    testPreprocessJobData : function () {
      let queue = queues.create(qn);
      let id = queue.push({
        name: 'peng',
        mount: '/_admin/aardvark',
        preprocess: function(data) { data.hund = 'hans'; delete data.meow; return data; }
      }, { a: 1, b: 2, meow: true });

      let job = db._jobs.document(id);
      assertEqual(1, job.data.a);
      assertEqual(2, job.data.b);
      assertEqual('hans', job.data.hund);
      assertFalse(job.data.hasOwnProperty('meow'));
    },
    
    testExecuteJobThatWillTakeLong : function () {
      try {
        FoxxManager.uninstall('/unittest/queues-test', {force: true});
      } catch (err) {}

      FoxxManager.install(fs.join(basePath, 'queues-test'), '/unittest/queues-test');
      try {
        let queue = queues.create(qn);
        let id = queue.push({
          name: 'job',
          mount: '/unittest/queues-test'
        }, {});

        let tries = 0;
        while (++tries < 240) {
          let result = db._jobs.document(id);
          if (result.status === 'pending') {
            internal.wait(0.5, false);
          } else {
            break;
          }
        }
        // should still be still in progress
        assertEqual('progress', db._jobs.document(id).status);
      } finally {
        FoxxManager.uninstall('/unittest/queues-test', {force: true});
      }
    },
    
    testExecuteJob : function () {
      assertEqual(0, db.UnitTestsFoxx.count());
      let queue = queues.create(qn);
      let id = queue.push({
        name: 'peng',
        mount: '/_admin/aardvark'
      }, {}, { failure: function() { 
        require("internal").db.UnitTestsFoxx.insert({ failed: true }); 
      }});

      let tries = 0;
      while (++tries < 240) {
        let result = db._jobs.document(id);
        if (result.status === 'pending' || result.status === 'progress') {
          internal.wait(0.5, false);
        } else {
          break;
        }
      }
      // should have failed
      assertEqual('failed', db._jobs.document(id).status);
      // failure callback is executed after job status is updated
      // so we need to wait a few a extra seconds here
      tries = 0;
      while (++tries < 20) {
        if (db.UnitTestsFoxx.count() === 0) {
          internal.wait(0.5, false);
        } else {
          break;
        }
      }
      assertNotEqual(0, db.UnitTestsFoxx.count());
    },
    
    testExecuteRepeatedJob : function () {
      assertEqual(0, db.UnitTestsFoxx.count());
      let queue = queues.create(qn);
      let id = queue.push({
        name: 'peng',
        mount: '/_admin/aardvark',
        repeatDelay: 1, // milliseconds
        maxFailures: 3
      }, {}, { failure: function() { 
        require("internal").db.UnitTestsFoxx.insert({ failed: true }); 
      }});

      let tries = 0;
      while (++tries < 240) {
        var result = db._jobs.document(id);
        if (result.status === 'pending' || result.status === 'progress') {
          internal.wait(0.5, false);
        } else {
          break;
        }
      }
      // should have failed
      assertEqual('failed', db._jobs.document(id).status);
      // failure callback is executed after job status is updated
      // so we need to wait a few a extra seconds here
      tries = 0;
      while (++tries < 20) {
        if (db.UnitTestsFoxx.count() < 3) {
          internal.wait(0.5, false);
        } else {
          break;
        }
      }
      assertNotEqual(3, db.UnitTestsFoxx.count());
    },

    testExecuteInfinitelyFailingJob : function () {
      assertEqual(0, db.UnitTestsFoxx.count());
      let queue = queues.create(qn);
      let id = queue.push({
        name: 'peng',
        mount: '/_admin/aardvark',
        maxFailures: -1
      }, {}, { failure: function() {
        require("internal").db.UnitTestsFoxx.insert({ failed: true }); 
      }});

      for (let tries = 0; tries < 60; tries++) {
        let result = db.UnitTestsFoxx.count();
        if (result < 2) {
          internal.wait(0.5, false);
          continue;
        }
        queue.delete(id);
        break;
      }
      assertNotEqual(0, db.UnitTestsFoxx.count());
    }

  };
}

function foxxQueuesSuite () {
  'use strict';

  let suite = {
    setUp : function () {
      db._useDatabase('_system');
      db._drop(cn);
      db._create(cn);
      queues.delete(qn);
    },

    tearDown : function () {
      db._drop(cn);
      queues.delete(qn);
    },
  };

  deriveTestSuite(BaseTestConfig(), suite, '_SystemDatabase');
  return suite;
}
    
function foxxQueuesOtherDatabaseSuite () {
  'use strict';

  let suite = {
    setUp : function () {
      db._useDatabase('_system');
      db._createDatabase(cn);
      db._useDatabase(cn);
      db._create(cn);
    },

    tearDown : function () {
      db._useDatabase('_system');
      db._dropDatabase(cn);
    },
  };

  deriveTestSuite(BaseTestConfig(), suite, '_OtherDatabase');
  return suite;
}

function foxxQueuesMultipleDatabasesSuite () {
  'use strict';

  let suite = {
    setUp : function () {
      db._useDatabase('_system');
      db._create(cn);
      // create a queue in _system as well
      let queue = queues.create(qn);
      queue.push({
        name: 'peng',
        mount: '/_admin/aardvark',
        repeatDelay: 1, // milliseconds
      }, {}, { failure: function() { 
        require("internal").db.UnitTestsFoxx.insert({ failed: true }); 
      }});

      db._createDatabase(cn);
      db._useDatabase(cn);
      db._create(cn);
    },

    tearDown : function () {
      db._useDatabase('_system');
      db._dropDatabase(cn);
      try {
        queues.delete(qn);
      } catch (err) {}
      db._drop(cn);
    },
  };

  deriveTestSuite(BaseTestConfig(), suite, '_MultipleDatabases');
  return suite;
}

jsunity.run(foxxQueuesSuite);
jsunity.run(foxxQueuesOtherDatabaseSuite);
jsunity.run(foxxQueuesMultipleDatabasesSuite);

return jsunity.done();
