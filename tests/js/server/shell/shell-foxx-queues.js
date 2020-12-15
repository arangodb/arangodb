/*global assertEqual, assertNotEqual, assertNotUndefined, assertFalse, fail */

////////////////////////////////////////////////////////////////////////////////
/// @brief test foxx queues
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
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Jan Steemann
/// @author Copyright 2015, ArangoDB GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

"use strict";

var jsunity = require("jsunity");
var internal = require("internal");
var db = require("internal").db;
var queues = require('@arangodb/foxx/queues');
var FoxxManager = require('org/arangodb/foxx/manager');
var fs = require('fs');
var basePath = fs.makeAbsolute(fs.join(internal.pathForTesting('common'), 'test-data', 'apps'));

require("@arangodb/test-helper").waitForFoxxInitialized();

function foxxQueuesSuite () {
  var cn = "UnitTestsFoxx";
  var qn = "FoxxCircus";

  return {

    setUp : function () {
      db._drop(cn);
      db._create(cn);
      queues.delete(qn);
    },

    tearDown : function () {
      db._drop(cn);
      queues.delete(qn);
    },
    
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
      var queue = queues.create(qn);
      assertEqual([], queue.all());
    },
    
    testCreateQueueAndFetch : function () {
      var queue = queues.create(qn);
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
      var delay = { delayUntil: Date.now() + 1000 * 86400 };
      var queue = queues.create(qn);
      var id = queue.push({
        name: 'testi',
        mount: '/test'
      }, {}, delay);
      assertEqual(1, queue.all().length);
      var job = db._jobs.document(id);
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
      var queue = queues.create(qn);
      var id = queue.push({
        name: 'testi',
        mount: '/test-this-does-not-exist-please-ignore'
      }, {});

      var tries = 0;
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
      var queue = queues.create(qn);
      var id = queue.push({
        name: 'peng',
        mount: '/_admin/aardvark',
        preprocess: function(data) { data.hund = 'hans'; delete data.meow; return data; }
      }, { a: 1, b: 2, meow: true });

      var job = db._jobs.document(id);
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
        var queue = queues.create(qn);
        var id = queue.push({
          name: 'job',
          mount: '/unittest/queues-test'
        }, {});

        var tries = 0;
        while (++tries < 240) {
          var result = db._jobs.document(id);
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
      var queue = queues.create(qn);
      var id = queue.push({
        name: 'peng',
        mount: '/_admin/aardvark'
      }, {}, { failure: function() { 
        require("internal").db.UnitTestsFoxx.insert({ failed: true }); 
      }});

      var tries = 0;
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
      var queue = queues.create(qn);
      var id = queue.push({
        name: 'peng',
        mount: '/_admin/aardvark',
        repeatDelay: 1, // milliseconds
        maxFailures: 3
      }, {}, { failure: function() { 
        require("internal").db.UnitTestsFoxx.insert({ failed: true }); 
      }});

      var tries = 0;
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
      var queue = queues.create(qn);
      var id = queue.push({
        name: 'peng',
        mount: '/_admin/aardvark',
        maxFailures: -1
      }, {}, { failure: function() {
        require("internal").db.UnitTestsFoxx.insert({ failed: true }); 
      }});

      for (let tries = 0; tries < 60; tries++) {
        var result = db.UnitTestsFoxx.count();
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

jsunity.run(foxxQueuesSuite);

return jsunity.done();
