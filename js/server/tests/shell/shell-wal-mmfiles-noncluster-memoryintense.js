/*jshint globalstrict:false, strict:false */
/*global fail, assertTrue, assertFalse, assertNotEqual, assertEqual */

////////////////////////////////////////////////////////////////////////////////
/// @brief test the collection interface
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
/// @author Dr. Frank Celler
/// @author Copyright 2012, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

var jsunity = require("jsunity");
var arangodb = require("@arangodb");
var testHelper = require("@arangodb/test-helper").Helper;
var ArangoCollection = require("@arangodb/arango-collection").ArangoCollection;
var db = arangodb.db;
var internal = require("internal");


////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////

function walFailureSuite () {
  'use strict';
  var cn = "UnitTestsWal";
  var c;
  var props;

  return {

    setUp: function () {
      props = internal.wal.properties();
      internal.debugClearFailAt();
      db._drop(cn);
      c = db._create(cn);
    },

    tearDown: function () {
      internal.wal.properties(props);
      internal.debugClearFailAt();
      db._drop(cn);
      c = null;
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test disabled collector
////////////////////////////////////////////////////////////////////////////////

    testCollectorDisabled : function () {
      internal.wal.flush(true, true);
      internal.debugSetFailAt("CollectorThreadCollect");

      var i = 0;
      for (i = 0; i < 1000; ++i) {
        c.save({ _key: "test" + i });
      }

      assertEqual(1000, c.count());
      internal.wal.flush(true, false);
      
      assertEqual(1000, c.count());
      internal.wait(6);
      internal.debugClearFailAt();
      
      testHelper.waitUnload(c);
      
      assertEqual(1000, c.count());
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test disabled collector
////////////////////////////////////////////////////////////////////////////////

    testCollectorThreadException : function () {
      internal.wal.flush(true, true);
      internal.debugSetFailAt("CollectorThreadCollectException");

      var i = 0;
      for (i = 0; i < 1000; ++i) {
        c.save({ _key: "test" + i });
      }

      assertEqual(1000, c.count());
      internal.wal.flush(true, false);
      
      assertEqual(1000, c.count());
      internal.wait(6);
      internal.debugClearFailAt();
      
      testHelper.waitUnload(c);
      
      assertEqual(1000, c.count());
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test no more available logfiles
////////////////////////////////////////////////////////////////////////////////

    testNoMoreLogfiles : function () {
      internal.debugClearFailAt();

      db._drop(cn);
      c = db._create(cn);
      
      var i;
      for (i = 0; i < 100; ++i) {
        c.save({ _key: "test" + i, a: i });
      } 
      assertEqual(100, c.count());

      internal.wal.flush(true, true);
        
      internal.debugSetFailAt("LogfileManagerGetWriteableLogfile");
      try {
        for (i = 0; i < 500000; ++i) {
          c.save({ _key: "foo" + i, value: "the quick brown foxx jumped over the lazy dog" });
        }

        // we don't know the exact point where the above operation failed, however,
        // it must fail somewhere
        fail();
      }
      catch (err) {
        assertEqual(internal.errors.ERROR_ARANGO_NO_JOURNAL.code, err.errorNum);
      }

      internal.debugClearFailAt();
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test write throttling
////////////////////////////////////////////////////////////////////////////////

    testWriteNoThrottling : function () {
      internal.debugClearFailAt();

      db._drop(cn);
      c = db._create(cn);
      
      internal.wal.flush(true, true);
      internal.wait(5);
      
      internal.debugSetFailAt("CollectorThreadProcessQueuedOperations");
      internal.wal.properties({ throttleWait: 1000, throttleWhenPending: 1000 });

      var i;
      for (i = 0; i < 10; ++i) {
        c.save({ _key: "test" + i, a: i });
      } 
      
      internal.wal.flush(true, true);

      c.save({ _key: "foo" });
      assertEqual("foo", c.document("foo")._key);
      assertEqual(11, c.count());

      internal.debugClearFailAt();
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test write throttling
////////////////////////////////////////////////////////////////////////////////

    testWriteThrottling : function () {
      internal.debugClearFailAt();

      db._drop(cn);
      c = db._create(cn);

      internal.wal.flush(true, true);
      
      internal.debugSetFailAt("CollectorThreadProcessQueuedOperations");
      internal.wal.properties({ throttleWait: 1000, throttleWhenPending: 1000 });

      var i;
      for (i = 0; i < 1005; ++i) {
        c.save({ _key: "test" + i, a: i });
      } 

      internal.wal.flush(true, true);
      internal.wait(5);

      try {
        c.save({ _key: "foo" });
        fail();
      }
      catch (err) {
        assertEqual(internal.errors.ERROR_ARANGO_WRITE_THROTTLE_TIMEOUT.code, err.errorNum);
      }

      internal.debugClearFailAt();
      assertEqual(1005, c.count());
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test write lock timeout issues
////////////////////////////////////////////////////////////////////////////////

    testLockTimeout : function () {
      internal.debugClearFailAt();

      db._drop(cn);
      c = db._create(cn);

      internal.wal.flush(true, true);
      
      internal.debugSetFailAt("CollectorThreadProcessCollectionOperationsLockTimeout");

      var i;
      for (i = 0; i < 1005; ++i) {
        c.save({ a: i });
      } 

      var fig;
      internal.wal.flush(true, true);
      
      // wait for the logfile manager to write and sync the updated status file
      var tries = 0; 
      while (++tries < 20) {
        fig = c.figures();
        if (fig.uncollectedLogfileEntries === 1005) {
          break;
        }
        internal.wait(1, false);
      }

      assertEqual(1005, c.count());
      fig = c.figures();
      assertEqual(1005, fig.uncollectedLogfileEntries);
        
      internal.wait(5, false);
      assertEqual(1005, fig.uncollectedLogfileEntries);
      
      internal.debugClearFailAt();
      
      for (i = 0; i < 1005; ++i) {
        c.save({ a: i });
      } 

      internal.wal.flush(true, true);
      
      // wait for the logfile manager to write and sync the updated status file
      tries = 0; 
      while (++tries < 20) {
        fig = c.figures();
        if (fig.uncollectedLogfileEntries === 0) {
          break;
        }
        internal.wait(1, false);
      }
  
      assertEqual(2010, c.count());
      assertEqual(0, fig.uncollectedLogfileEntries);
    }

  };
}
 

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////

function walSuite () {
  'use strict';
  var cn = "UnitTestsWal";
  var c;
  var props;

  return {

    setUp: function () {
      props = internal.wal.properties();

      db._drop(cn);
      c = db._create(cn);
    },

    tearDown: function () {
      internal.wal.properties(props);
      db._drop(cn);
      c = null;
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief drop a collection a recreate it with the same id,
/// wait for collector, unload the collection and check if documents are still
/// available
////////////////////////////////////////////////////////////////////////////////
    
    testDropAndRecreate : function () {
      db._drop("UnitTestsExample");
      db._create("UnitTestsExample");
      var i;
      for (i = 0; i < 10; ++i) {
        db._collection("UnitTestsExample").save({ _key: "test" + i, value: i });
      }
      var id = db._collection("UnitTestsExample")._id;

      db._drop("UnitTestsExample");
      // wait until directory gets deleted
      require("internal").wait(5, false);

      // re-create using the same id
      db._create("UnitTestsExample", { id: id });
      assertEqual(id, db._collection("UnitTestsExample")._id);

      for (i = 100; i < 110; ++i) {
        // sync last document we're saving
        db._collection("UnitTestsExample").save({ _key: "test" + i, value: i }, { waitForSync: i === 109 });
      }

      // now garbage collect and unload
      require("internal").wal.flush(true, true);
      db._collection("UnitTestsExample").unload();
      
      while (db._collection("UnitTestsExample").status() !== ArangoCollection.STATUS_UNLOADED) {
        db._collection("UnitTestsExample").unload();
        internal.wait(1, false);
      }

      var c = db._collection("UnitTestsExample");
      assertEqual(10, c.count());

      for (i = 0; i < 10; ++i) {
        assertFalse(c.exists("test" + i));
      }

      for (i = 100; i < 110; ++i) {
        assertTrue(c.exists("test" + i));
        assertEqual(i, c.document("test" + i).value);
      }
      db._drop("UnitTestsExample");
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test properties
////////////////////////////////////////////////////////////////////////////////

    testReadProperties : function () {
      var p = internal.wal.properties();
      
      assertTrue(p.hasOwnProperty("allowOversizeEntries"));
      assertTrue(p.hasOwnProperty("logfileSize"));
      assertTrue(p.hasOwnProperty("historicLogfiles"));
      assertTrue(p.hasOwnProperty("reserveLogfiles"));
      assertTrue(p.hasOwnProperty("syncInterval"));
      assertTrue(p.hasOwnProperty("throttleWait"));
      assertTrue(p.hasOwnProperty("throttleWhenPending"));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test properties
////////////////////////////////////////////////////////////////////////////////

    testSetProperties : function () {
      var initial = internal.wal.properties();

      var p = {
        allowOversizeEntries: false,
        logfileSize: 1024 * 1024 * 8,
        historicLogfiles: 4, 
        reserveLogfiles: 4,
        syncInterval: 200,
        throttleWait: 10000,
        throttleWhenPending: 10000
      };

      var result = internal.wal.properties(p);

      assertEqual(p.allowOversizeEntries, result.allowOversizeEntries);
      assertEqual(p.logfileSize, result.logfileSize);
      assertEqual(p.historicLogfiles, result.historicLogfiles);
      assertEqual(p.reserveLogfiles, result.reserveLogfiles);
      assertEqual(initial.syncInterval, result.syncInterval);
      assertEqual(p.throttleWait, result.throttleWait);
      assertEqual(p.throttleWhenPending, result.throttleWhenPending);
      
      var result2 = internal.wal.properties();
      assertEqual(p.allowOversizeEntries, result2.allowOversizeEntries);
      assertEqual(p.logfileSize, result2.logfileSize);
      assertEqual(p.historicLogfiles, result2.historicLogfiles);
      assertEqual(p.reserveLogfiles, result2.reserveLogfiles);
      assertEqual(initial.syncInterval, result2.syncInterval);
      assertEqual(p.throttleWait, result2.throttleWait);
      assertEqual(p.throttleWhenPending, result2.throttleWhenPending);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test max tick
////////////////////////////////////////////////////////////////////////////////

    testMaxTickEmptyCollection : function () {
      var fig = c.figures();
      // we shouldn't have a tick yet
      assertEqual("0", fig.lastTick);
      assertEqual(0, fig.uncollectedLogfileEntries);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief max tick
////////////////////////////////////////////////////////////////////////////////

    testMaxTickNonEmptyCollection : function () {
      internal.wal.flush(true, true);
      var i;

      for (i = 0; i < 100; ++i) {
        c.save({ test:  i });
      }

      // we shouldn't have a tick yet
      var fig = c.figures();
      assertEqual("0", fig.lastTick);
      assertTrue(fig.uncollectedLogfileEntries > 0);

      internal.wal.flush(true, true);

      // wait for the logfile manager to write and sync the updated status file
      var tries = 0; 
      while (++tries < 20) {
        fig = c.figures();
        if (fig.uncollectedLogfileEntries === 0) {
          break;
        }
        internal.wait(1, false);
      }

      assertNotEqual("0", fig.lastTick);
      assertEqual(0, fig.uncollectedLogfileEntries);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief max tick
////////////////////////////////////////////////////////////////////////////////

    testMaxTickAfterUnload : function () {
      var i;

      for (i = 0; i < 100; ++i) {
        c.save({ test:  i });
      }

      internal.wal.flush(true, true);

      testHelper.waitUnload(c);

      // we should have a tick and no uncollected entries
      var fig = c.figures();
      assertNotEqual("0", fig.lastTick);
      assertEqual(0, fig.uncollectedLogfileEntries);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief oversize marker
////////////////////////////////////////////////////////////////////////////////

    testOversizeMarkerAllowed : function () {
      var i, value = "", doc = { _key: "foo", };

      for (i = 0; i < 240; ++i) {
        value += "the quick brown foxx jumped over the lazy dog.";
      }

      for (i = 0; i < 4000; ++i) {
        doc["test" + i] = value; 
      }

      internal.wal.properties({ allowOversizeEntries: true });
      var result = c.save(doc);
      assertEqual("foo", result._key);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief oversize marker
////////////////////////////////////////////////////////////////////////////////

    testOversizeMarkerDisallowed : function () {
      var i, value = "", doc = { };

      for (i = 0; i < 240; ++i) {
        value += "the quick brown foxx jumped over the lazy dog.";
      }

      for (i = 0; i < 4000; ++i) {
        doc["test" + i] = value; 
      }
      
      internal.wal.properties({ allowOversizeEntries: false });
      try {
        c.save(doc);
        fail();
      }
      catch (err) {
        assertEqual(internal.errors.ERROR_ARANGO_DOCUMENT_TOO_LARGE.code, err.errorNum);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test transactions
////////////////////////////////////////////////////////////////////////////////

    testTransactions : function () {
      var result = internal.wal.transactions();
      
      assertTrue(result.hasOwnProperty("runningTransactions"));
      assertTrue(typeof result.runningTransactions === "number");
      assertTrue(result.runningTransactions >= 0);

      assertTrue(result.hasOwnProperty("minLastCollected"));
      assertTrue(result.minLastCollected === null || typeof result.minLastCollected === "string");

      assertTrue(result.hasOwnProperty("minLastSealed"));
      assertTrue(result.minLastSealed === null || typeof result.minLastSealed === "string");
    }

  };
}


////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suites
////////////////////////////////////////////////////////////////////////////////

if (internal.debugCanUseFailAt()) {
  jsunity.run(walFailureSuite);
}
jsunity.run(walSuite);

return jsunity.done();

