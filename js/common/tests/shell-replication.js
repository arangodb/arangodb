/*jslint indent: 2, nomen: true, maxlen: 80 */
/*global require, assertEqual, assertTrue */

////////////////////////////////////////////////////////////////////////////////
/// @brief test the replication functions
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
/// @author Jan Steemann
/// @author Copyright 2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

var jsunity = require("jsunity");
var arangodb = require("org/arangodb");
var db = arangodb.db;

var replication = require("org/arangodb/replication");

// -----------------------------------------------------------------------------
// --SECTION--                                           replication logger test
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////

function ReplicationLoggerSuite () {
  var cn  = "UnitTestsReplication";
  var cn2 = "UnitTestsReplication2";

  var getLogEntries = function () {
    return db._collection('_replication').count();
  };

  var compareTicks = function (l, r) {
    if (l.length != r.length) {
      return l.length - r.length < 0 ? -1 : 1;
    }

    // length is equal
    for (i = 0; i < l.length; ++i) {
      if (l[i] != r[i]) {
        return l[i] < r[i] ? -1 : 1;
      }
    }

    return 0;
  };

  return {

////////////////////////////////////////////////////////////////////////////////
/// @brief set up
////////////////////////////////////////////////////////////////////////////////

    setUp : function () {
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief tear down
////////////////////////////////////////////////////////////////////////////////

    tearDown : function () {
      replication.stopLogger();
      db._drop(cn);
      db._drop(cn2);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief start logger
////////////////////////////////////////////////////////////////////////////////

    testStartLogger : function () {
      var actual, state;
      
      // start 
      actual = replication.startLogger();
      assertTrue(actual);

      state = replication.getLoggerState();
      assertTrue(state.running);
      assertTrue(typeof state.lastLogTick === 'string');
    
      // start again  
      actual = replication.startLogger();
      assertTrue(actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief stop logger
////////////////////////////////////////////////////////////////////////////////

    testStopLogger : function () {
      var actual, state;
      
      // start 
      actual = replication.startLogger();
      assertTrue(actual);
      
      state = replication.getLoggerState();
      assertTrue(state.running);
      assertTrue(typeof state.lastLogTick === 'string');
    
      // stop
      actual = replication.stopLogger();
      assertTrue(actual);
      
      state = replication.getLoggerState();
      assertFalse(state.running);
      assertTrue(typeof state.lastLogTick === 'string');
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief get state
////////////////////////////////////////////////////////////////////////////////

    testGetLoggerState : function () {
      var state, tick;
      
      state = replication.getLoggerState();
      assertFalse(state.running);
      tick = state.lastLogTick;
      assertTrue(typeof tick === 'string');
      
      state = replication.getLoggerState();
      assertFalse(state.running);
      assertTrue(tick, state.lastLogTick);
      assertTrue(typeof state.lastLogTick === 'string');
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test logging disabled
////////////////////////////////////////////////////////////////////////////////

    testDisabledLogger : function () {
      var state, tick;
      
      state = replication.getLoggerState();
      assertFalse(state.running);
      tick = state.lastLogTick;
      assertTrue(typeof tick === 'string');

      // do something that will cause logging (if it was enabled...)
      var c = db._create(cn);
      c.save({ "test" : 1 });

      state = replication.getLoggerState();
      assertFalse(state.running);
      assertEqual(tick, state.lastLogTick);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test logging enabled
////////////////////////////////////////////////////////////////////////////////

    testEnabledLogger : function () {
      var state, tick;
      
      state = replication.getLoggerState();
      assertFalse(state.running);
      tick = state.lastLogTick;
      assertTrue(typeof tick === 'string');

      replication.startLogger();

      // do something that will cause logging
      var c = db._create(cn);
      c.save({ "test" : 1 });

      state = replication.getLoggerState();
      assertTrue(state.running);
      assertNotEqual(tick, state.lastLogTick);
      assertEqual(1, compareTicks(state.lastLogTick, tick));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test actions
////////////////////////////////////////////////////////////////////////////////

    testLoggerCreateCollection : function () {
      var state, tick;
      
      replication.startLogger();
      
      state = replication.getLoggerState();
      tick = state.lastLogTick;
      var count = getLogEntries();

      db._create(cn);

      state = replication.getLoggerState();
      assertNotEqual(tick, state.lastLogTick);
      assertEqual(1, compareTicks(state.lastLogTick, tick));
      assertEqual(count + 1, getLogEntries());
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test actions
////////////////////////////////////////////////////////////////////////////////

    testLoggerDropCollection : function () {
      var state, tick;
      
      db._create(cn);

      replication.startLogger();
      
      state = replication.getLoggerState();
      tick = state.lastLogTick;
      var count = getLogEntries();

      db._drop(cn);

      state = replication.getLoggerState();
      assertNotEqual(tick, state.lastLogTick);
      assertEqual(1, compareTicks(state.lastLogTick, tick));
      assertEqual(count + 1, getLogEntries());
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test actions
////////////////////////////////////////////////////////////////////////////////

    testLoggerRenameCollection : function () {
      var state, tick;
      
      var c = db._create(cn);

      replication.startLogger();
      
      state = replication.getLoggerState();
      tick = state.lastLogTick;
      var count = getLogEntries();

      c.rename(cn2);

      state = replication.getLoggerState();
      assertNotEqual(tick, state.lastLogTick);
      assertEqual(1, compareTicks(state.lastLogTick, tick));
      assertEqual(count + 1, getLogEntries());
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test actions
////////////////////////////////////////////////////////////////////////////////

    testLoggerPropertiesCollection1 : function () {
      var state, tick;
      
      var c = db._create(cn);

      replication.startLogger();
      
      state = replication.getLoggerState();
      tick = state.lastLogTick;
      var count = getLogEntries();

      c.properties();

      state = replication.getLoggerState();
      assertEqual(tick, state.lastLogTick);
      assertEqual(0, compareTicks(state.lastLogTick, tick));
      assertEqual(count, getLogEntries());
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test actions
////////////////////////////////////////////////////////////////////////////////

    testLoggerPropertiesCollection2 : function () {
      var state, tick;
      
      var c = db._create(cn);

      replication.startLogger();
      
      state = replication.getLoggerState();
      tick = state.lastLogTick;
      var count = getLogEntries();

      c.properties({ waitForSync: true, journalSize: 2097152 });

      state = replication.getLoggerState();
      assertNotEqual(tick, state.lastLogTick);
      assertEqual(1, compareTicks(state.lastLogTick, tick));
      assertEqual(count + 1, getLogEntries());
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test actions
////////////////////////////////////////////////////////////////////////////////

    testLoggerTruncateCollection1 : function () {
      var state, tick;
      
      var c = db._create(cn);
      c.save({ "test": 1, "_key": "abc" });

      replication.startLogger();
      
      state = replication.getLoggerState();
      tick = state.lastLogTick;
      var count = getLogEntries();

      c.truncate();
      
      state = replication.getLoggerState();
      assertNotEqual(tick, state.lastLogTick);
      assertEqual(1, compareTicks(state.lastLogTick, tick));
      assertEqual(count + 3, getLogEntries());
      tick = state.lastLogTick;
      count = getLogEntries();

      c.truncate();
      
      state = replication.getLoggerState();
      assertEqual(tick, state.lastLogTick);
      assertEqual(count, getLogEntries());
      
      c.save({ "test": 1, "_key": "abc" });
      
      state = replication.getLoggerState();
      tick = state.lastLogTick;
      count = getLogEntries();
      
      c.truncate();

      state = replication.getLoggerState();
      assertNotEqual(tick, state.lastLogTick);
      assertEqual(1, compareTicks(state.lastLogTick, tick));
      assertEqual(count + 3, getLogEntries());
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test actions
////////////////////////////////////////////////////////////////////////////////

    testLoggerTruncateCollection2 : function () {
      var state, tick;
      
      var c = db._create(cn);
      for (var i = 0; i < 100; ++i) {
        c.save({ "test": 1, "_key": "test" + i });
      }

      replication.startLogger();
      
      state = replication.getLoggerState();
      tick = state.lastLogTick;
      var count = getLogEntries();

      c.truncate();
      
      state = replication.getLoggerState();
      assertNotEqual(tick, state.lastLogTick);
      assertEqual(1, compareTicks(state.lastLogTick, tick));
      assertEqual(count + 102, getLogEntries());
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test actions
////////////////////////////////////////////////////////////////////////////////

    testLoggerSaveDocument : function () {
      var state, tick;
      
      var c = db._create(cn);

      replication.startLogger();
      
      state = replication.getLoggerState();
      tick = state.lastLogTick;
      var count = getLogEntries();

      c.save({ "test": 1, "_key": "abc" });

      state = replication.getLoggerState();
      assertNotEqual(tick, state.lastLogTick);
      assertEqual(1, compareTicks(state.lastLogTick, tick));
      assertEqual(count + 1, getLogEntries());
      tick = state.lastLogTick;
      count = getLogEntries();
      
      c.save({ "test": 1, "_key": "12345" });
      
      state = replication.getLoggerState();
      assertNotEqual(tick, state.lastLogTick);
      assertEqual(1, compareTicks(state.lastLogTick, tick));
      assertEqual(count + 1, getLogEntries());
      tick = state.lastLogTick;
      count = getLogEntries();

      try {
        c.save({ "test": 1, "_key": "12345" });
        fail();
      }
      catch (err) {
        state = replication.getLoggerState();
        assertEqual(tick, state.lastLogTick);
        assertEqual(count, getLogEntries());
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test actions
////////////////////////////////////////////////////////////////////////////////

    testLoggerSaveDocuments : function () {
      var state, tick;
      
      var c = db._create(cn);

      replication.startLogger();
      
      state = replication.getLoggerState();
      tick = state.lastLogTick;
      var count = getLogEntries();

      for (var i = 0; i < 100; ++i) {
        c.save({ "test": 1, "_key": "test" + i });
      }

      state = replication.getLoggerState();
      assertNotEqual(tick, state.lastLogTick);
      assertEqual(1, compareTicks(state.lastLogTick, tick));
      assertEqual(count + 100, getLogEntries());
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test actions
////////////////////////////////////////////////////////////////////////////////

    testLoggerDeleteDocument : function () {
      var state, tick;
      
      var c = db._create(cn);
      c.save({ "test": 1, "_key": "abc" });
      c.save({ "test": 1, "_key": "12345" });

      replication.startLogger();
      
      state = replication.getLoggerState();
      tick = state.lastLogTick;
      var count = getLogEntries();
      
      c.remove("abc");

      state = replication.getLoggerState();
      assertNotEqual(tick, state.lastLogTick);
      assertEqual(1, compareTicks(state.lastLogTick, tick));
      assertEqual(count + 1, getLogEntries());
      tick = state.lastLogTick;
      count = getLogEntries();

      c.remove("12345");
      
      state = replication.getLoggerState();
      assertNotEqual(tick, state.lastLogTick);
      assertEqual(1, compareTicks(state.lastLogTick, tick));
      assertEqual(count + 1, getLogEntries());
      tick = state.lastLogTick;
      count = getLogEntries();

      try {
        c.remove("12345");
        fail();
      }
      catch (err) {
        state = replication.getLoggerState();
        assertEqual(tick, state.lastLogTick);
        assertEqual(count, getLogEntries());
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test actions
////////////////////////////////////////////////////////////////////////////////

    testLoggerUpdateDocument : function () {
      var state, tick;
      
      var c = db._create(cn);
      c.save({ "test": 1, "_key": "abc" });
      c.save({ "test": 1, "_key": "12345" });

      replication.startLogger();
      
      state = replication.getLoggerState();
      tick = state.lastLogTick;
      var count = getLogEntries();
      
      c.update("abc", { "test" : 2 });

      state = replication.getLoggerState();
      assertNotEqual(tick, state.lastLogTick);
      assertEqual(1, compareTicks(state.lastLogTick, tick));
      assertEqual(count + 1, getLogEntries());
      tick = state.lastLogTick;
      count = getLogEntries();

      c.update("abc", { "test" : 3 });
      
      state = replication.getLoggerState();
      assertNotEqual(tick, state.lastLogTick);
      assertEqual(1, compareTicks(state.lastLogTick, tick));
      assertEqual(count + 1, getLogEntries());
      tick = state.lastLogTick;
      count = getLogEntries();

      c.update("abc", { "test" : 3 });

      state = replication.getLoggerState();
      assertNotEqual(tick, state.lastLogTick);
      assertEqual(1, compareTicks(state.lastLogTick, tick));
      assertEqual(count + 1, getLogEntries());
      tick = state.lastLogTick;
      count = getLogEntries();

      c.update("12345", { "test" : 2 });
      c.update("abc", { "test" : 4 });
      
      state = replication.getLoggerState();
      assertNotEqual(tick, state.lastLogTick);
      assertEqual(1, compareTicks(state.lastLogTick, tick));
      assertEqual(count + 2, getLogEntries());
      tick = state.lastLogTick;
      count = getLogEntries();

      try {
        c.update("thefoxx", { });
        fail();
      }
      catch (err) {
        state = replication.getLoggerState();
        assertEqual(tick, state.lastLogTick);
        assertEqual(count, getLogEntries());
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test actions
////////////////////////////////////////////////////////////////////////////////

    testLoggerReplaceDocument : function () {
      var state, tick;
      
      var c = db._create(cn);
      c.save({ "test": 1, "_key": "abc" });
      c.save({ "test": 1, "_key": "12345" });

      replication.startLogger();
      
      state = replication.getLoggerState();
      tick = state.lastLogTick;
      var count = getLogEntries();
      
      c.replace("abc", { "test" : 2 });

      state = replication.getLoggerState();
      assertNotEqual(tick, state.lastLogTick);
      assertEqual(1, compareTicks(state.lastLogTick, tick));
      assertEqual(count + 1, getLogEntries());
      tick = state.lastLogTick;
      count = getLogEntries();

      c.replace("abc", { "test" : 3 });
      
      state = replication.getLoggerState();
      assertNotEqual(tick, state.lastLogTick);
      assertEqual(1, compareTicks(state.lastLogTick, tick));
      assertEqual(count + 1, getLogEntries());
      tick = state.lastLogTick;
      count = getLogEntries();

      c.replace("abc", { "test" : 3 });

      state = replication.getLoggerState();
      assertNotEqual(tick, state.lastLogTick);
      assertEqual(1, compareTicks(state.lastLogTick, tick));
      assertEqual(count + 1, getLogEntries());
      tick = state.lastLogTick;
      count = getLogEntries();

      c.replace("12345", { "test" : 2 });
      c.replace("abc", { "test" : 4 });
      
      state = replication.getLoggerState();
      assertNotEqual(tick, state.lastLogTick);
      assertEqual(1, compareTicks(state.lastLogTick, tick));
      assertEqual(count + 2, getLogEntries());
      tick = state.lastLogTick;
      count = getLogEntries();

      try {
        c.replace("thefoxx", { });
        fail();
      }
      catch (err) {
        state = replication.getLoggerState();
        assertEqual(tick, state.lastLogTick);
        assertEqual(count, getLogEntries());
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test actions
////////////////////////////////////////////////////////////////////////////////

    testLoggerSaveEdge : function () {
      var state, tick;
      
      var v = db._create(cn);
      var e = db._createEdgeCollection(cn2);

      replication.startLogger();
      
      state = replication.getLoggerState();
      tick = state.lastLogTick;
      var count = getLogEntries();
      
      e.save(cn + "/test1", cn + "/test2", { "test": 1, "_key": "abc" });
      
      state = replication.getLoggerState();
      assertNotEqual(tick, state.lastLogTick);
      assertEqual(1, compareTicks(state.lastLogTick, tick));
      assertEqual(count + 1, getLogEntries());
      tick = state.lastLogTick;
      count = getLogEntries();
      
      e.save(cn + "/test3", cn + "/test4", { "test": 1, "_key": "12345" });
      
      state = replication.getLoggerState();
      assertNotEqual(tick, state.lastLogTick);
      assertEqual(1, compareTicks(state.lastLogTick, tick));
      assertEqual(count + 1, getLogEntries());
      tick = state.lastLogTick;
      count = getLogEntries();

      try {
        e.save();
        fail();
      }
      catch (err) {
        state = replication.getLoggerState();
        assertEqual(tick, state.lastLogTick);
        assertEqual(count, getLogEntries());
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test actions
////////////////////////////////////////////////////////////////////////////////

    testLoggerDeleteEdge : function () {
      var state, tick;
      
      var v = db._create(cn);
      var e = db._createEdgeCollection(cn2);
      
      e.save(cn + "/test1", cn + "/test2", { "test": 1, "_key": "abc" });
      e.save(cn + "/test3", cn + "/test4", { "test": 1, "_key": "12345" });

      replication.startLogger();
      
      state = replication.getLoggerState();
      tick = state.lastLogTick;
      var count = getLogEntries();
       
      e.remove("abc");
       
      state = replication.getLoggerState();
      assertNotEqual(tick, state.lastLogTick);
      assertEqual(1, compareTicks(state.lastLogTick, tick));
      assertEqual(count + 1, getLogEntries());
      tick = state.lastLogTick;
      count = getLogEntries();
      
      e.remove("12345");
      
      state = replication.getLoggerState();
      assertNotEqual(tick, state.lastLogTick);
      assertEqual(1, compareTicks(state.lastLogTick, tick));
      assertEqual(count + 1, getLogEntries());
      tick = state.lastLogTick;
      count = getLogEntries();

      try {
        e.remove("12345");
        fail();
      }
      catch (err) {
        state = replication.getLoggerState();
        assertEqual(tick, state.lastLogTick);
        assertEqual(count, getLogEntries());
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test actions
////////////////////////////////////////////////////////////////////////////////

    testLoggerUpdateEdge : function () {
      var state, tick;
      
      var v = db._create(cn);
      var e = db._createEdgeCollection(cn2);
      e.save(cn + "/test1", cn + "/test2", { "test": 1, "_key": "abc" });
      e.save(cn + "/test3", cn + "/test4", { "test": 1, "_key": "12345" });

      replication.startLogger();
      
      state = replication.getLoggerState();
      tick = state.lastLogTick;
      var count = getLogEntries();
      
      e.update("abc", { "test" : 2 });

      state = replication.getLoggerState();
      assertNotEqual(tick, state.lastLogTick);
      assertEqual(1, compareTicks(state.lastLogTick, tick));
      assertEqual(count + 1, getLogEntries());
      tick = state.lastLogTick;
      count = getLogEntries();

      e.update("abc", { "test" : 3 });
      
      state = replication.getLoggerState();
      assertNotEqual(tick, state.lastLogTick);
      assertEqual(1, compareTicks(state.lastLogTick, tick));
      assertEqual(count + 1, getLogEntries());
      tick = state.lastLogTick;
      count = getLogEntries();

      e.update("abc", { "test" : 3 });

      state = replication.getLoggerState();
      assertNotEqual(tick, state.lastLogTick);
      assertEqual(1, compareTicks(state.lastLogTick, tick));
      assertEqual(count + 1, getLogEntries());
      tick = state.lastLogTick;
      count = getLogEntries();

      e.update("12345", { "test" : 2 });
      e.update("abc", { "test" : 4 });
      
      state = replication.getLoggerState();
      assertNotEqual(tick, state.lastLogTick);
      assertEqual(1, compareTicks(state.lastLogTick, tick));
      assertEqual(count + 2, getLogEntries());
      tick = state.lastLogTick;
      count = getLogEntries();

      try {
        e.update("thefoxx", { });
        fail();
      }
      catch (err) {
        state = replication.getLoggerState();
        assertEqual(tick, state.lastLogTick);
        assertEqual(count, getLogEntries());
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test actions
////////////////////////////////////////////////////////////////////////////////

    testLoggerReplaceEdge : function () {
      var state, tick;
      
      var v = db._create(cn);
      var e = db._createEdgeCollection(cn2);
      e.save(cn + "/test1", cn + "/test2", { "test": 1, "_key": "abc" });
      e.save(cn + "/test3", cn + "/test4", { "test": 1, "_key": "12345" });

      replication.startLogger();
      
      state = replication.getLoggerState();
      tick = state.lastLogTick;
      var count = getLogEntries();
      
      e.replace("abc", { "test" : 2 });

      state = replication.getLoggerState();
      assertNotEqual(tick, state.lastLogTick);
      assertEqual(1, compareTicks(state.lastLogTick, tick));
      assertEqual(count + 1, getLogEntries());
      tick = state.lastLogTick;
      count = getLogEntries();

      e.replace("abc", { "test" : 3 });
      
      state = replication.getLoggerState();
      assertNotEqual(tick, state.lastLogTick);
      assertEqual(1, compareTicks(state.lastLogTick, tick));
      assertEqual(count + 1, getLogEntries());
      tick = state.lastLogTick;
      count = getLogEntries();

      e.replace("abc", { "test" : 3 });

      state = replication.getLoggerState();
      assertNotEqual(tick, state.lastLogTick);
      assertEqual(1, compareTicks(state.lastLogTick, tick));
      assertEqual(count + 1, getLogEntries());
      tick = state.lastLogTick;
      count = getLogEntries();

      e.replace("12345", { "test" : 2 });
      e.replace("abc", { "test" : 4 });
      
      state = replication.getLoggerState();
      assertNotEqual(tick, state.lastLogTick);
      assertEqual(1, compareTicks(state.lastLogTick, tick));
      assertEqual(count + 2, getLogEntries());
      tick = state.lastLogTick;
      count = getLogEntries();

      try {
        e.replace("thefoxx", { });
        fail();
      }
      catch (err) {
        state = replication.getLoggerState();
        assertEqual(tick, state.lastLogTick);
        assertEqual(count, getLogEntries());
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test actions
////////////////////////////////////////////////////////////////////////////////

    testLoggerTransactionEmpty : function () {
      var state, tick;
      
      var c = db._create(cn);

      replication.startLogger();
      
      state = replication.getLoggerState();
      tick = state.lastLogTick;
      var count = getLogEntries();

      var actual = TRANSACTION({
        collections: {
        },
        action: function () {
          return true;
        }
      });
      assertTrue(actual);
      
      state = replication.getLoggerState();
      assertEqual(tick, state.lastLogTick);
      assertEqual(count, getLogEntries());
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test actions
////////////////////////////////////////////////////////////////////////////////

    testLoggerTransactionRead1 : function () {
      var state, tick;
      
      var c = db._create(cn);
      c.save({ "test" : 1 });

      replication.startLogger();
      
      state = replication.getLoggerState();
      tick = state.lastLogTick;
      var count = getLogEntries();

      var actual = TRANSACTION({
        collections: {
          read: cn
        },
        action: function () {
          return true;
        }
      });
      assertTrue(actual);
      
      state = replication.getLoggerState();
      assertEqual(tick, state.lastLogTick);
      assertEqual(count, getLogEntries());
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test actions
////////////////////////////////////////////////////////////////////////////////

    testLoggerTransactionRead2 : function () {
      var state, tick;
      
      var c = db._create(cn);
      c.save({ "test" : 1, "_key": "abc" });

      replication.startLogger();
      
      state = replication.getLoggerState();
      tick = state.lastLogTick;
      var count = getLogEntries();

      var actual = TRANSACTION({
        collections: {
          read: cn
        },
        action: function () {
          c.document("abc");

          return true;
        }
      });
      assertTrue(actual);
      
      state = replication.getLoggerState();
      assertEqual(tick, state.lastLogTick);
      assertEqual(count, getLogEntries());
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test actions
////////////////////////////////////////////////////////////////////////////////

    testLoggerTransactionRead3 : function () {
      var state, tick;
      
      var c = db._create(cn);

      replication.startLogger();
      
      state = replication.getLoggerState();
      tick = state.lastLogTick;
      var count = getLogEntries();

      try {
        actual = TRANSACTION({
          collections: {
            read: cn
          },
          action: function () {
            c.save({ "foo" : "bar" });

            return true;
          }
        });
        fail();
      }
      catch (err) {
      }
      
      state = replication.getLoggerState();
      assertEqual(tick, state.lastLogTick);
      assertEqual(count, getLogEntries());
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test actions
////////////////////////////////////////////////////////////////////////////////

    testLoggerTransactionWrite1 : function () {
      var state, tick;
      
      var c = db._create(cn);

      replication.startLogger();
      
      state = replication.getLoggerState();
      tick = state.lastLogTick;
      var count = getLogEntries();

      var actual = TRANSACTION({
        collections: {
          write: cn
        },
        action: function () {
          return true;
        }
      });
      assertTrue(actual);
      
      state = replication.getLoggerState();
      assertEqual(tick, state.lastLogTick);
      assertEqual(count, getLogEntries());
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test actions
////////////////////////////////////////////////////////////////////////////////

    testLoggerTransactionWrite2 : function () {
      var state, tick;
      
      var c = db._create(cn);

      replication.startLogger();
      
      state = replication.getLoggerState();
      tick = state.lastLogTick;
      var count = getLogEntries();

      var actual = TRANSACTION({
        collections: {
          write: cn
        },
        action: function () {
          c.save({ "test" : 2, "_key": "abc" });
          return true;
        }
      });
      assertTrue(actual);
      
      state = replication.getLoggerState();
      assertNotEqual(tick, state.lastLogTick);
      assertEqual(1, compareTicks(state.lastLogTick, tick));
      assertEqual(count + 3, getLogEntries());
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test actions
////////////////////////////////////////////////////////////////////////////////

    testLoggerTransactionWrite3 : function () {
      var state, tick;
      
      var c = db._create(cn);

      replication.startLogger();
      
      state = replication.getLoggerState();
      tick = state.lastLogTick;
      var count = getLogEntries();

      try {
        TRANSACTION({
          collections: {
            write: cn
          },
          action: function () {
            c.save({ "test" : 2, "_key": "abc" });
            throw "fail";
          }
        });
        fail();
      }
      catch (err) {
        state = replication.getLoggerState();
        assertEqual(tick, state.lastLogTick);
        assertEqual(count, getLogEntries());
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test actions
////////////////////////////////////////////////////////////////////////////////

    testLoggerTransactionWrite4 : function () {
      var state, tick;
      
      var c1 = db._create(cn);
      var c2 = db._create(cn2);

      replication.startLogger();
      
      state = replication.getLoggerState();
      tick = state.lastLogTick;
      var count = getLogEntries();

      try {
        TRANSACTION({
          collections: {
            write: cn
          },
          action: function () {
            c2.save({ "test" : 2, "_key": "abc" });
          }
        });
        fail();
      }
      catch (err) {
        state = replication.getLoggerState();
        assertEqual(tick, state.lastLogTick);
        assertEqual(count, getLogEntries());
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test actions
////////////////////////////////////////////////////////////////////////////////

    testLoggerTransactionWrite5 : function () {
      var state, tick;
      
      var c1 = db._create(cn);
      var c2 = db._create(cn2);

      replication.startLogger();
      
      state = replication.getLoggerState();
      tick = state.lastLogTick;
      var count = getLogEntries();

      TRANSACTION({
        collections: {
          write: [ cn, cn2 ]
        },
        action: function () {
          c1.save({ "test" : 2, "_key": "abc" });
        }
      });
      
      state = replication.getLoggerState();
      assertNotEqual(tick, state.lastLogTick);
      assertEqual(1, compareTicks(state.lastLogTick, tick));
      assertEqual(count + 3, getLogEntries());
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test actions
////////////////////////////////////////////////////////////////////////////////

    testLoggerTransactionWrite6 : function () {
      var state, tick;
      
      var c1 = db._create(cn);
      var c2 = db._create(cn2);

      replication.startLogger();
      
      state = replication.getLoggerState();
      tick = state.lastLogTick;
      var count = getLogEntries();

      TRANSACTION({
        collections: {
          write: [ cn, cn2 ]
        },
        action: function () {
          c1.save({ "test" : 2, "_key": "abc" });
          c2.save({ "test" : 2, "_key": "abc" });
        }
      });
      
      state = replication.getLoggerState();
      assertNotEqual(tick, state.lastLogTick);
      assertEqual(1, compareTicks(state.lastLogTick, tick));
      assertEqual(count + 4, getLogEntries());
    },

  };
}

// -----------------------------------------------------------------------------
// --SECTION--                                                              main
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suite
////////////////////////////////////////////////////////////////////////////////

jsunity.run(ReplicationLoggerSuite);

return jsunity.done();

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// @addtogroup\\|// --SECTION--\\|/// @page\\|/// @}\\)"
// End:
