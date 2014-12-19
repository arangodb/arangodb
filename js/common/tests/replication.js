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
var errors = arangodb.errors;
var db = arangodb.db;
var internal = require("internal");

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

  var waitForSync = function () {
    internal.wait(1.5, false);
  };

  var getLogEntries = function (tick, type) {
    var result = [ ];
    waitForSync();

    var entries = REPLICATION_LOGGER_LAST(tick, "999999999999999999999");

    if (Array.isArray(type)) {
      var found = false;
      entries.forEach(function(e) {
        if (! found) {
          if (e.type === type[0]) {
            found = true;
          }
        }
        if (found && type.indexOf(e.type) !== -1) {
          result.push(e);
        }
      });
    }
    else {
      entries.forEach(function(e) {
        if (e.type === type) {
          result.push(e);
        }
      });
    }

    return result;
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

  var waitForTick = function (value) {
    while (true) {
      var state = replication.logger.state().state;
      if (compareTicks(state.lastLogTick, value) > 0) {
        break;
      }
      internal.wait(0.1, false);
    }
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
      db._drop(cn);
      db._drop(cn2);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief get state
////////////////////////////////////////////////////////////////////////////////

    testGetLoggerState : function () {
      var state, tick, server, clients;

      state = replication.logger.state().state;
      assertTrue(state.running);
      tick = state.lastLogTick;
      assertTrue(typeof tick === 'string');
      assertNotEqual("", state.time);
      assertMatch(/^\d+-\d+-\d+T\d+:\d+:\d+Z$/, state.time);

      state = replication.logger.state().state;
      assertTrue(state.running);

      assertEqual(tick, state.lastLogTick);
      assertTrue(typeof state.lastLogTick === 'string');
      assertMatch(/^\d+$/, state.lastLogTick);
      assertTrue(state.totalEvents >= 0);

      server = replication.logger.state().server;
      assertEqual(server.version, db._version());
      assertNotEqual("", server.serverId);
      assertMatch(/^\d+$/, server.serverId);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test logging disabled
////////////////////////////////////////////////////////////////////////////////

    testDisabledLogger : function () {
      var state, tick, events;

      state = replication.logger.state().state;
      assertTrue(state.running);
      tick = state.lastLogTick;
      assertTrue(typeof tick === 'string');
      assertMatch(/^\d+$/, state.lastLogTick);
      events = state.totalEvents;
      assertTrue(state.totalEvents >= 0);

      // do something that will cause logging (if it was enabled...)
      var c = db._create(cn);
      c.save({ "test" : 1 });

      waitForTick(tick);

      state = replication.logger.state().state;
      assertTrue(state.running);
      assertMatch(/^\d+$/, state.lastLogTick);
      assertEqual(1, compareTicks(state.lastLogTick, tick));
      assertTrue(state.totalEvents > events);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test logging enabled
////////////////////////////////////////////////////////////////////////////////

    testEnabledLogger : function () {
      var state, tick;

      state = replication.logger.state().state;
      assertTrue(state.running);
      tick = state.lastLogTick;
      assertTrue(typeof tick === 'string');
      assertMatch(/^\d+$/, state.lastLogTick);
      events = state.totalEvents;
      assertTrue(state.totalEvents >= 0);

      // do something that will cause logging
      var c = db._create(cn);
      c.save({ "test" : 1 });

      waitForTick(tick);

      state = replication.logger.state().state;
      assertTrue(state.running);
      assertMatch(/^\d+$/, state.lastLogTick);
      assertTrue(state.totalEvents >= 1);
      assertNotEqual(tick, state.lastLogTick);
      assertEqual(1, compareTicks(state.lastLogTick, tick));
      assertTrue(state.totalEvents > events);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test actions
////////////////////////////////////////////////////////////////////////////////

    testLoggerCreateCollection : function () {
      var state, tick;

      waitForSync();
      state = replication.logger.state().state;
      tick = state.lastLogTick;

      var c = db._create(cn);
      var entry = getLogEntries(tick, 2000)[0];

      assertEqual(2000, entry.type);
      assertEqual(2, entry.collection.type);
      assertEqual(c._id, entry.cid);
      assertEqual(c._id, entry.collection.cid);
      assertFalse(entry.collection.deleted);
      assertEqual(cn, entry.collection.name);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test actions
////////////////////////////////////////////////////////////////////////////////

    testLoggerDropCollection : function () {
      var state, tick;

      var c = db._create(cn);
      waitForSync();

      state = replication.logger.state().state;
      tick = state.lastLogTick;
      db._drop(cn);

      var entry = getLogEntries(tick, 2001)[0];

      assertEqual(2001, entry.type);
      assertEqual(c._id, entry.cid);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test actions
////////////////////////////////////////////////////////////////////////////////

    testLoggerRenameCollection : function () {
      var state, tick;

      var c = db._create(cn);
      waitForSync();

      state = replication.logger.state().state;
      tick = state.lastLogTick;
      c.rename(cn2);

      var entry = getLogEntries(tick, 2002)[0];

      assertEqual(2002, entry.type);
      assertEqual(c._id, entry.cid);
      assertEqual(cn2, entry.collection.name);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test actions
////////////////////////////////////////////////////////////////////////////////

    testLoggerPropertiesCollection : function () {
      var state, tick;

      var c = db._create(cn);
      waitForSync();

      state = replication.logger.state().state;
      tick = state.lastLogTick;
      c.properties({ waitForSync: true, journalSize: 2097152 });

      var entry = getLogEntries(tick, 2003)[0];

      assertEqual(2003, entry.type);
      assertEqual(c._id, entry.cid);
      assertEqual(c._id, entry.collection.cid);
      assertEqual(cn, entry.collection.name);
      assertEqual(2, entry.collection.type);
      assertEqual(false, entry.collection.deleted);
      assertEqual(2097152, entry.collection.maximalSize);
      assertEqual(true, entry.collection.waitForSync);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test actions
////////////////////////////////////////////////////////////////////////////////

    testLoggerIncludedSystemCollection1 : function () {
      var state, tick;

      c = db._collection("_graphs");

      state = replication.logger.state().state;
      tick = state.lastLogTick;
      var doc = c.save({ "test": 1 });

      var entry = getLogEntries(tick, 2300)[0];
      assertEqual(2300, entry.type);
      assertEqual(c._id, entry.cid);

      state = replication.logger.state().state;
      tick = state.lastLogTick;
      c.remove(doc._key);

      entry = getLogEntries(tick, 2302)[0];
      assertEqual(2302, entry.type);
      assertEqual(c._id, entry.cid);
      assertEqual(doc._key, entry.key);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test actions
////////////////////////////////////////////////////////////////////////////////

    testLoggerIncludedSystemCollection2 : function () {
      var state, tick;

      c = db._collection("_users");

      state = replication.logger.state().state;
      tick = state.lastLogTick;
      var doc = c.save({ "test": 1 });

      var entry = getLogEntries(tick, 2300)[0];
      assertEqual(2300, entry.type);
      assertEqual(c._id, entry.cid);

      state = replication.logger.state().state;
      tick = state.lastLogTick;
      c.remove(doc._key);

      entry = getLogEntries(tick, 2302)[0];
      assertEqual(2302, entry.type);
      assertEqual(c._id, entry.cid);
      assertEqual(doc._key, entry.key);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test actions
////////////////////////////////////////////////////////////////////////////////

    testLoggerSystemCollection : function () {
      var state, tick, i;

      db._drop("_unitfoxx");
      db._drop("_unittests");

      state = replication.logger.state().state;
      tick = state.lastLogTick;

      var c = db._create("_unittests", { isSystem : true });

      var entry = getLogEntries(tick, 2000)[0];

      assertEqual(2000, entry.type);
      assertEqual(c._id, entry.cid);
      assertEqual(c.name(), entry.collection.name);

      state = replication.logger.state().state;
      tick = state.lastLogTick;
      c.properties({ waitForSync : true });

      entry = getLogEntries(tick, 2003)[0];
      assertEqual(2003, entry.type);
      assertEqual(c._id, entry.cid);
      assertEqual(true, entry.collection.waitForSync);

      state = replication.logger.state().state;
      tick = state.lastLogTick;
      c.rename("_unitfoxx");

      entry = getLogEntries(tick, 2002)[0];
      assertEqual(2002, entry.type);
      assertEqual(c._id, entry.cid);
      assertEqual("_unitfoxx", entry.collection.name);

      state = replication.logger.state().state;
      tick = state.lastLogTick;
      c.rename("_unittests");

      entry = getLogEntries(tick, 2002)[0];
      assertEqual(2002, entry.type);
      assertEqual(c._id, entry.cid);
      assertEqual("_unittests", entry.collection.name);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test actions
////////////////////////////////////////////////////////////////////////////////

    testLoggerTruncateCollection1 : function () {
      var state, tick, i;

      var c = db._create(cn);
      c.save({ "test": 1, "_key": "abc" });
      waitForSync();

      state = replication.logger.state().state;
      tick = state.lastLogTick;
      c.truncate();

      var entry = getLogEntries(tick, 2302);
      assertEqual(1, entry.length);
      assertNotEqual("0", entry[0].tid);
      assertEqual(c._id, entry[0].cid);
      assertEqual("abc", entry[0].key);

      c.save({ "test": 1, "_key": "abc" });

      waitForSync();
      state = replication.logger.state().state;
      tick = state.lastLogTick;
      c.truncate();

      entry = getLogEntries(tick, 2302);
      assertNotEqual("0", entry[0].tid);
      assertEqual(c._id, entry[0].cid);
      assertEqual("abc", entry[0].key);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test actions
////////////////////////////////////////////////////////////////////////////////

    testLoggerTruncateCollection2 : function () {
      var state, tick, i;

      var c = db._create(cn);
      for (var i = 0; i < 100; ++i) {
        c.save({ "test": 1, "_key": "test" + i });
      }

      waitForSync();
      state = replication.logger.state().state;
      tick = state.lastLogTick;
      c.truncate();
      waitForSync();
      var entry = getLogEntries(tick, [ 2200, 2201, 2202, 2302 ]);

      assertEqual(102, entry.length);
      // trx start
      assertEqual(2200, entry[0].type);
      assertNotEqual("", entry[0].tid);
      var tid = entry[0].tid;

      // remove
      var keys = { };
      for (i = 0; i < 100; ++i) {
        assertEqual(2302, entry[i + 1].type);
        assertEqual(tid, entry[i + 1].tid);
        assertEqual(c._id, entry[i + 1].cid);
        keys[entry[i + 1].key] = true;
      }
      assertEqual(100, Object.keys(keys).length);

      // commit
      assertEqual(2201, entry[101].type);
      assertEqual(tid, entry[101].tid);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test actions
////////////////////////////////////////////////////////////////////////////////

    testLoggerCreateIndexHash1 : function () {
      var state, tick;

      var c = db._create(cn);

      state = replication.logger.state().state;
      tick = state.lastLogTick;

      c.ensureUniqueConstraint("a", "b");
      var idx = c.getIndexes()[1];

      var entry = getLogEntries(tick, 2100)[0];
      assertTrue(2100, entry.type);
      assertEqual(c._id, entry.cid);
      assertEqual(idx.id.replace(/^.*\//, ''), entry.index.id);
      assertEqual("hash", entry.index.type);
      assertEqual(true, entry.index.unique);
      assertEqual([ "a", "b" ], entry.index.fields);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test actions
////////////////////////////////////////////////////////////////////////////////

    testLoggerCreateIndexHash2 : function () {
      var state, tick;

      var c = db._create(cn);

      state = replication.logger.state().state;
      tick = state.lastLogTick;

      c.ensureHashIndex("a");
      var idx = c.getIndexes()[1];

      var entry = getLogEntries(tick, 2100)[0];
      assertEqual(c._id, entry.cid);
      assertEqual(idx.id.replace(/^.*\//, ''), entry.index.id);
      assertEqual("hash", entry.index.type);
      assertEqual(false, entry.index.unique);
      assertEqual([ "a" ], entry.index.fields);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test actions
////////////////////////////////////////////////////////////////////////////////

    testLoggerCreateIndexSkiplist1 : function () {
      var state, tick;

      var c = db._create(cn);

      state = replication.logger.state().state;
      tick = state.lastLogTick;

      c.ensureSkiplist("a", "b", "c");
      var idx = c.getIndexes()[1];
      var entry = getLogEntries(tick, 2100)[0];

      assertTrue(2100, entry.type);
      assertEqual(c._id, entry.cid);
      assertEqual(idx.id.replace(/^.*\//, ''), entry.index.id);
      assertEqual("skiplist", entry.index.type);
      assertEqual(false, entry.index.unique);
      assertEqual([ "a", "b", "c" ], entry.index.fields);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test actions
////////////////////////////////////////////////////////////////////////////////

    testLoggerCreateIndexSkiplist2 : function () {
      var state, tick;

      var c = db._create(cn);

      state = replication.logger.state().state;
      tick = state.lastLogTick;

      c.ensureUniqueSkiplist("a");
      var idx = c.getIndexes()[1];
      var entry = getLogEntries(tick, 2100)[0];

      var entry = getLogEntries(tick, 2100)[0];
      assertEqual(c._id, entry.cid);
      assertEqual(idx.id.replace(/^.*\//, ''), entry.index.id);
      assertEqual("skiplist", entry.index.type);
      assertEqual(true, entry.index.unique);
      assertEqual([ "a" ], entry.index.fields);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test actions
////////////////////////////////////////////////////////////////////////////////

    testLoggerCreateIndexFulltext1 : function () {
      var state, tick;

      var c = db._create(cn);

      state = replication.logger.state().state;
      tick = state.lastLogTick;
      c.ensureFulltextIndex("a", 5);
      var idx = c.getIndexes()[1];

      var entry = getLogEntries(tick, 2100)[0];
      assertTrue(2100, entry.type);
      assertEqual(c._id, entry.cid);
      assertEqual(idx.id.replace(/^.*\//, ''), entry.index.id);
      assertEqual("fulltext", entry.index.type);
      assertEqual(false, entry.index.unique);
      assertEqual(5, entry.index.minLength);
      assertEqual([ "a" ], entry.index.fields);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test actions
////////////////////////////////////////////////////////////////////////////////

    testLoggerCreateIndexGeo1 : function () {
      var state, tick;

      var c = db._create(cn);

      state = replication.logger.state().state;
      tick = state.lastLogTick;
      var count = getLogEntries();

      c.ensureGeoIndex("a", "b");
      var idx = c.getIndexes()[1];
      var entry = getLogEntries(tick, 2100)[0];

      assertTrue(2100, entry.type);
      assertEqual(c._id, entry.cid);
      assertEqual(idx.id.replace(/^.*\//, ''), entry.index.id);
      assertEqual("geo2", entry.index.type);
      assertEqual(false, entry.index.unique);
      assertEqual(false, entry.index.constraint);
      assertEqual([ "a", "b" ], entry.index.fields);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test actions
////////////////////////////////////////////////////////////////////////////////

    testLoggerCreateIndexGeo2 : function () {
      var state, tick;

      var c = db._create(cn);

      state = replication.logger.state().state;
      tick = state.lastLogTick;

      c.ensureGeoIndex("a", true);
      var idx = c.getIndexes()[1];
      var entry = getLogEntries(tick, 2100)[0];

      assertTrue(2100, entry.type);
      assertEqual(c._id, entry.cid);
      assertEqual(idx.id.replace(/^.*\//, ''), entry.index.id);
      assertEqual("geo1", entry.index.type);
      assertEqual(false, entry.index.unique);
      assertEqual(false, entry.index.constraint);
      assertEqual([ "a" ], entry.index.fields);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test actions
////////////////////////////////////////////////////////////////////////////////

    testLoggerCreateIndexGeo3 : function () {
      var state, tick;

      var c = db._create(cn);

      state = replication.logger.state().state;
      tick = state.lastLogTick;

      c.ensureGeoConstraint("a", "b", true);
      var idx = c.getIndexes()[1];
      var entry = getLogEntries(tick, 2100)[0];

      assertTrue(2100, entry.type);
      assertEqual(c._id, entry.cid);
      assertEqual(idx.id.replace(/^.*\//, ''), entry.index.id);
      assertEqual("geo2", entry.index.type);
      assertEqual(true, entry.index.unique);
      assertEqual(true, entry.index.constraint);
      assertEqual(true, entry.index.ignoreNull);
      assertEqual([ "a", "b" ], entry.index.fields);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test actions
////////////////////////////////////////////////////////////////////////////////

    testLoggerCreateIndexGeo4 : function () {
      var state, tick;

      var c = db._create(cn);

      state = replication.logger.state().state;
      tick = state.lastLogTick;

      c.ensureGeoConstraint("a", "b", false);
      var idx = c.getIndexes()[1];
      var entry = getLogEntries(tick, 2100)[0];

      assertTrue(2100, entry.type);
      assertEqual(c._id, entry.cid);
      assertEqual(idx.id.replace(/^.*\//, ''), entry.index.id);
      assertEqual("geo2", entry.index.type);
      assertEqual(true, entry.index.unique);
      assertEqual(true, entry.index.constraint);
      assertEqual(false, entry.index.ignoreNull);
      assertEqual([ "a", "b" ], entry.index.fields);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test actions
////////////////////////////////////////////////////////////////////////////////

    testLoggerCreateIndexGeo5 : function () {
      var state, tick;

      var c = db._create(cn);

      state = replication.logger.state().state;
      tick = state.lastLogTick;

      c.ensureGeoConstraint("a", true);
      var idx = c.getIndexes()[1];
      var entry = getLogEntries(tick, 2100)[0];

      assertTrue(2100, entry.type);
      assertEqual(c._id, entry.cid);
      assertEqual(idx.id.replace(/^.*\//, ''), entry.index.id);
      assertEqual("geo1", entry.index.type);
      assertEqual(true, entry.index.unique);
      assertEqual(true, entry.index.constraint);
      assertEqual([ "a" ], entry.index.fields);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test actions
////////////////////////////////////////////////////////////////////////////////

    testLoggerCreateIndexCap1 : function () {
      var state, tick;

      var c = db._create(cn);

      state = replication.logger.state().state;
      tick = state.lastLogTick;

      c.ensureCapConstraint(100);
      var idx = c.getIndexes()[1];
      var entry = getLogEntries(tick, 2100)[0];

      assertTrue(2100, entry.type);
      assertEqual(c._id, entry.cid);
      assertEqual(idx.id.replace(/^.*\//, ''), entry.index.id);
      assertEqual(idx.id.replace(/^.*\//, ''), entry.id);
      assertEqual("cap", entry.index.type);
      assertEqual(false, entry.index.unique);
      assertEqual(100, entry.index.size);
      assertEqual(0, entry.index.byteSize);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test actions
////////////////////////////////////////////////////////////////////////////////

    testLoggerCreateIndexCap2 : function () {
      var state, tick;

      var c = db._create(cn);

      state = replication.logger.state().state;
      tick = state.lastLogTick;

      c.ensureCapConstraint(null, 1048576);
      var idx = c.getIndexes()[1];
      var entry = getLogEntries(tick, 2100)[0];

      assertTrue(2100, entry.type);
      assertEqual(c._id, entry.cid);
      assertEqual(idx.id.replace(/^.*\//, ''), entry.index.id);
      assertEqual(idx.id.replace(/^.*\//, ''), entry.id);
      assertEqual("cap", entry.index.type);
      assertEqual(false, entry.index.unique);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test actions
////////////////////////////////////////////////////////////////////////////////

    testLoggerDropIndex : function () {
      var state, tick;

      var c = db._create(cn);
      c.ensureUniqueConstraint("a", "b");

      state = replication.logger.state().state;
      tick = state.lastLogTick;

      // use index at #1 (#0 is primary index)
      var idx = c.getIndexes()[1];
      c.dropIndex(idx);
      var entry = getLogEntries(tick, 2101)[0];

      assertTrue(2101, entry.type);
      assertEqual(c._id, entry.cid);
      assertEqual(idx.id.replace(/^.*\//, ''), entry.id);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test actions
////////////////////////////////////////////////////////////////////////////////

    testLoggerSaveDocument : function () {
      var state, tick;

      var c = db._create(cn);

      state = replication.logger.state().state;
      tick = state.lastLogTick;

      c.save({ "test": 1, "_key": "abc" });
      var rev = c.document("abc")._rev;
      var entry = getLogEntries(tick, 2300)[0];

      assertEqual(2300, entry.type);
      assertEqual(c._id, entry.cid);
      assertEqual("abc", entry.key);
      assertEqual("abc", entry.data._key);
      assertEqual(rev, entry.data._rev);
      assertEqual(1, entry.data.test);

      state = replication.logger.state().state;
      tick = state.lastLogTick;

      c.save({ "test": 2, "foo" : "bar", "_key": "12345" });
      rev = c.document("12345")._rev;

      entry = getLogEntries(tick, 2300)[0];
      assertEqual(2300, entry.type);
      assertEqual(c._id, entry.cid);
      assertEqual("12345", entry.key);
      assertEqual("12345", entry.data._key);
      assertEqual(rev, entry.data._rev);
      assertEqual(2, entry.data.test);
      assertEqual("bar", entry.data.foo);

      try {
        c.save({ "test": 1, "_key": "12345" });
        fail();
      }
      catch (err) {
      }

      entry = getLogEntries(tick, 2300);
      assertEqual(1, entry.length);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test actions
////////////////////////////////////////////////////////////////////////////////

    testLoggerSaveDocuments : function () {
      var state, tick;

      var c = db._create(cn);

      state = replication.logger.state().state;
      tick = state.lastLogTick;

      for (var i = 0; i < 100; ++i) {
        c.save({ "test": 1, "_key": "test" + i });
      }

      var entry = getLogEntries(tick, 2300);
      assertEqual(100, entry.length);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test actions
////////////////////////////////////////////////////////////////////////////////

    testLoggerDeleteDocument : function () {
      var state, tick;

      var c = db._create(cn);
      c.save({ "test": 1, "_key": "abc" });
      c.save({ "test": 1, "_key": "12345" });

      state = replication.logger.state().state;
      tick = state.lastLogTick;

      c.remove("abc");

      var entry = getLogEntries(tick, 2302)[0];
      assertEqual(2302, entry.type);
      assertEqual(c._id, entry.cid);
      assertEqual("abc", entry.key);

      state = replication.logger.state().state;
      tick = state.lastLogTick;
      c.remove("12345");
      entry = getLogEntries(tick, 2302)[0];

      assertEqual(2302, entry.type);
      assertEqual(c._id, entry.cid);
      assertEqual("12345", entry.key);

      state = replication.logger.state().state;
      tick = state.lastLogTick;

      try {
        c.remove("12345");
        fail();
      }
      catch (err) {
      }

      entry = getLogEntries(tick, 2302);
      assertEqual(0, entry.length);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test actions
////////////////////////////////////////////////////////////////////////////////

    testLoggerUpdateDocument : function () {
      var state, tick;

      var c = db._create(cn);
      c.save({ "test": 2, "_key": "abc" });
      c.save({ "test": 1, "_key": "12345" });

      state = replication.logger.state().state;
      tick = state.lastLogTick;

      c.update("abc", { "test" : 2 });
      var entry = getLogEntries(tick, 2300);

      assertEqual(2300, entry[0].type);
      assertEqual(c._id, entry[0].cid);
      assertEqual("abc", entry[0].key);
      assertEqual("abc", entry[0].data._key);
      assertEqual(2, entry[0].data.test);

      assertEqual(2300, entry[1].type);
      assertEqual(c._id, entry[1].cid);
      assertEqual("12345", entry[1].key);
      assertEqual("12345", entry[1].data._key);
      assertEqual(1, entry[1].data.test);


      state = replication.logger.state().state;
      tick = state.lastLogTick;
      c.update("abc", { "test" : 3 });
      entry = getLogEntries(tick, 2300)[0];

      assertEqual(2300, entry.type);
      assertEqual(c._id, entry.cid);
      assertEqual("abc", entry.key);
      assertEqual("abc", entry.data._key);
      assertEqual(3, entry.data.test);

      state = replication.logger.state().state;
      tick = state.lastLogTick;

      c.update("abc", { "test" : 3 });
      c.update("12345", { "test" : 2 });
      c.update("abc", { "test" : 4 });
      entry = getLogEntries(tick, 2300);
      assertEqual(3, entry.length);

      waitForSync();
      state = replication.logger.state().state;
      tick = state.lastLogTick;

      try {
        c.update("thefoxx", { });
        fail();
      }
      catch (err) {
      }

      entry = getLogEntries(tick, 2300);
      assertEqual(0, entry.length);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test actions
////////////////////////////////////////////////////////////////////////////////

    testLoggerReplaceDocument : function () {
      var state, tick;

      var c = db._create(cn);
      c.save({ "test": 2, "_key": "abc" });
      c.save({ "test": 1, "_key": "12345" });

      state = replication.logger.state().state;
      tick = state.lastLogTick;

      c.replace("abc", { "test" : 2 });
      var entry = getLogEntries(tick, 2300);
      assertEqual(2300, entry[0].type);
      assertEqual(c._id, entry[0].cid);
      assertEqual("abc", entry[0].key);
      assertEqual("abc", entry[0].data._key);
      assertEqual(2, entry[0].data.test);

      assertEqual(2300, entry[1].type);
      assertEqual(c._id, entry[1].cid);
      assertEqual("12345", entry[1].key);
      assertEqual("12345", entry[1].data._key);
      assertEqual(1, entry[1].data.test);

      state = replication.logger.state().state;
      tick = state.lastLogTick;
      c.replace("abc", { "test" : 3 });
      c.replace("abc", { "test" : 3 });
      c.replace("12345", { "test" : 2 });
      c.replace("abc", { "test" : 4 });

      entry = getLogEntries(tick, 2300);
      assertEqual(4, entry.length);

      state = replication.logger.state().state;
      tick = state.lastLogTick;

      try {
        c.replace("thefoxx", { });
        fail();
      }
      catch (err) {
      }

      entry = getLogEntries(tick, 2300);
      assertEqual(0, entry.length);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test actions
////////////////////////////////////////////////////////////////////////////////

    testLoggerSaveEdge : function () {
      var state, tick;

      var c = db._create(cn);
      var e = db._createEdgeCollection(cn2);

      state = replication.logger.state().state;
      tick = state.lastLogTick;

      e.save(cn + "/test1", cn + "/test2", { "test": 1, "_key": "abc" });

      var entry = getLogEntries(tick, 2301)[0];
      assertEqual(2301, entry.type);
      assertEqual(e._id, entry.cid);
      assertEqual("abc", entry.key);
      assertEqual("abc", entry.data._key);
      assertEqual(c._id + "/test1", entry.data._from);
      assertEqual(c._id + "/test2", entry.data._to);
      assertEqual(1, entry.data.test);

      state = replication.logger.state().state;
      tick = state.lastLogTick;
      e.save(cn + "/test3", cn + "/test4", { "test": [ 99, false ], "_key": "12345" });
      entry = getLogEntries(tick, 2301)[0];

      assertEqual(2301, entry.type);
      assertEqual(e._id, entry.cid);
      assertEqual("12345", entry.key);
      assertEqual("12345", entry.data._key);
      assertEqual(c._id + "/test3", entry.data._from);
      assertEqual(c._id + "/test4", entry.data._to);
      assertEqual([ 99, false ], entry.data.test);

      state = replication.logger.state().state;
      tick = state.lastLogTick;

      try {
        e.save();
        fail();
      }
      catch (err) {
      }

      entry = getLogEntries(tick, 2301);
      assertEqual(0, entry.length);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test actions
////////////////////////////////////////////////////////////////////////////////

    testLoggerDeleteEdge : function () {
      var state, tick;

      db._create(cn);
      var e = db._createEdgeCollection(cn2);

      e.save(cn + "/test1", cn + "/test2", { "test": 1, "_key": "abc" });
      e.save(cn + "/test3", cn + "/test4", { "test": 1, "_key": "12345" });

      state = replication.logger.state().state;
      tick = state.lastLogTick;

      e.remove("abc");

      var entry = getLogEntries(tick, 2302)[0];
      assertEqual(2302, entry.type);
      assertEqual(e._id, entry.cid);
      assertEqual("abc", entry.key);

      e.remove("12345");

      waitForSync();
      state = replication.logger.state().state;
      tick = state.lastLogTick;

      try {
        e.remove("12345");
        fail();
      }
      catch (err) {
      }

      entry = getLogEntries(tick, 2302);
      assertEqual(0, entry.length);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test actions
////////////////////////////////////////////////////////////////////////////////

    testLoggerUpdateEdge : function () {
      var state, tick;

      var c = db._create(cn);
      var e = db._createEdgeCollection(cn2);
      e.save(cn + "/test1", cn + "/test2", { "test": 2, "_key": "abc" });
      e.save(cn + "/test3", cn + "/test4", { "test": 1, "_key": "12345" });

      state = replication.logger.state().state;
      tick = state.lastLogTick;

      e.update("abc", { "test" : 2 });
      var entry = getLogEntries(tick, 2301);
      assertEqual(2301, entry[0].type);
      assertEqual(e._id, entry[0].cid);
      assertEqual("abc", entry[0].key);
      assertEqual("abc", entry[0].data._key);
      assertEqual(2, entry[0].data.test);
      assertEqual(c._id + "/test1", entry[0].data._from);
      assertEqual(c._id + "/test2", entry[0].data._to);

      assertEqual(2301, entry[1].type);
      assertEqual(e._id, entry[1].cid);
      assertEqual("12345", entry[1].key);
      assertEqual("12345", entry[1].data._key);
      assertEqual(1, entry[1].data.test);
      assertEqual(c._id + "/test3", entry[1].data._from);
      assertEqual(c._id + "/test4", entry[1].data._to);

      state = replication.logger.state().state;
      tick = state.lastLogTick;

      e.update("abc", { "test" : 3 });
      e.update("12345", { "test" : 2 });
      e.update("abc", { "test" : 4 });
      entry = getLogEntries(tick, 2301);

      assertEqual(3, entry.length);

      state = replication.logger.state().state;
      tick = state.lastLogTick;

      try {
        e.update("thefoxx", { });
        fail();
      }
      catch (err) {
      }

      entry = getLogEntries(tick, 2301);
      assertEqual(0, entry.length);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test actions
////////////////////////////////////////////////////////////////////////////////

    testLoggerReplaceEdge : function () {
      var state, tick;

      var c = db._create(cn);
      var e = db._createEdgeCollection(cn2);
      e.save(cn + "/test1", cn + "/test2", { "test": 2, "_key": "abc" });
      e.save(cn + "/test3", cn + "/test4", { "test": 1, "_key": "12345" });

      state = replication.logger.state().state;
      tick = state.lastLogTick;

      e.replace("abc", { "test" : 2 });
      var entry = getLogEntries(tick, 2301);
      assertEqual(2301, entry[0].type);
      assertEqual(e._id, entry[0].cid);
      assertEqual("abc", entry[0].key);
      assertEqual("abc", entry[0].data._key);
      assertEqual(2, entry[0].data.test);
      assertEqual(c._id + "/test1", entry[0].data._from);
      assertEqual(c._id + "/test2", entry[0].data._to);

      assertEqual(2301, entry[1].type);
      assertEqual(e._id, entry[1].cid);
      assertEqual("12345", entry[1].key);
      assertEqual("12345", entry[1].data._key);
      assertEqual(1, entry[1].data.test);
      assertEqual(c._id + "/test3", entry[1].data._from);
      assertEqual(c._id + "/test4", entry[1].data._to);

      state = replication.logger.state().state;
      tick = state.lastLogTick;

      e.replace("abc", { "test" : 3 });
      e.replace("abc", { "test" : 3 });
      e.replace("12345", { "test" : 2 });
      e.replace("abc", { "test" : 4 });

      entry = getLogEntries(tick, 2301);
      assertEqual(4, entry.length);

      state = replication.logger.state().state;
      tick = state.lastLogTick;

      try {
        e.replace("thefoxx", { });
        fail();
      }
      catch (err) {
      }

      entry = getLogEntries(tick, 2301);
      assertEqual(0, entry.length);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test actions
////////////////////////////////////////////////////////////////////////////////

    testLoggerTransactionEmpty : function () {
      var state, tick;

      c = db._create(cn);

      state = replication.logger.state().state;
      tick = state.lastLogTick;

      var actual = db._executeTransaction({
        collections: {
        },
        action: function () {
          return true;
        }
      });
      assertTrue(actual);

      var entry = getLogEntries(tick, 2200);
      assertEqual(0, entry.length);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test actions
////////////////////////////////////////////////////////////////////////////////

    testLoggerTransactionRead1 : function () {
      var state, tick;

      var c = db._create(cn);
      c.save({ "test" : 1 });

      state = replication.logger.state().state;
      tick = state.lastLogTick;

      var actual = db._executeTransaction({
        collections: {
          read: cn
        },
        action: function () {
          return true;
        }
      });
      assertTrue(actual);

      var entry = getLogEntries(tick, 2200);
      assertEqual(0, entry.length);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test actions
////////////////////////////////////////////////////////////////////////////////

    testLoggerTransactionRead2 : function () {
      var state, tick;

      var c = db._create(cn);
      c.save({ "test" : 1, "_key": "abc" });

      state = replication.logger.state().state;
      tick = state.lastLogTick;

      var actual = db._executeTransaction({
        collections: {
          read: cn
        },
        action: function (params) {
          var c = require("internal").db._collection(params.cn);

          c.document("abc");

          return true;
        },
        params: {
          cn: cn
        }
      });
      assertTrue(actual);

      var entry = getLogEntries(tick, [ 2200, 2201, 2202 ]);
      assertEqual(0, entry.length);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test actions
////////////////////////////////////////////////////////////////////////////////

    testLoggerTransactionRead3 : function () {
      var state, tick;

      db._create(cn);

      state = replication.logger.state().state;
      tick = state.lastLogTick;

      try {
        actual = db._executeTransaction({
          collections: {
            read: cn
          },
          action: function (params) {
            var c = require("internal").db._collection(params.cn);

            c.save({ "foo" : "bar" });

            return true;
          },
          params: {
            cn: cn
          }
        });
        fail();
      }
      catch (err) {
      }

      var entry = getLogEntries(tick, [ 2200, 2201, 2202 ]);
      assertEqual(0, entry.length);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test actions
////////////////////////////////////////////////////////////////////////////////

    testLoggerTransactionWrite1 : function () {
      var state, tick;

      var c = db._create(cn);

      state = replication.logger.state().state;
      tick = state.lastLogTick;

      var actual = db._executeTransaction({
        collections: {
          write: cn
        },
        action: function () {
          return true;
        }
      });
      assertTrue(actual);

      var entry = getLogEntries(tick, [ 2200, 2201, 2202 ]);
      assertEqual(0, entry.length);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test actions
////////////////////////////////////////////////////////////////////////////////

    testLoggerTransactionWrite2 : function () {
      var state, tick;

      var c = db._create(cn);

      state = replication.logger.state().state;
      tick = state.lastLogTick;

      var actual = db._executeTransaction({
        collections: {
          write: cn
        },
        action: function (params) {
          var c = require("internal").db._collection(params.cn);

          c.save({ "test" : 2, "_key": "abc" });
          return true;
        },
        params: {
          cn: cn
        }
      });
      assertTrue(actual);

      var entry = getLogEntries(tick, [ 2200, 2201, 2202, 2300 ]);
      assertEqual(3, entry.length);
      assertEqual(2200, entry[0].type);
      assertEqual(2300, entry[1].type);
      assertEqual(2201, entry[2].type);
      assertEqual(entry[0].tid, entry[1].tid);
      assertEqual(entry[1].tid, entry[2].tid);

      assertEqual(c._id, entry[1].cid);
      assertEqual("abc", entry[1].key);
      assertEqual("abc", entry[1].data._key);
      assertEqual(c._id, entry[1].cid);
      assertEqual(2, entry[1].data.test);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test actions
////////////////////////////////////////////////////////////////////////////////

    testLoggerTransactionWrite3 : function () {
      var state, tick;

      db._create(cn);

      state = replication.logger.state().state;
      tick = state.lastLogTick;

      try {
        db._executeTransaction({
          collections: {
            write: cn
          },
          action: function (params) {
            var c = require("internal").db._collection(params.cn);

            c.save({ "test" : 2, "_key": "abc" });
            throw "fail";
          },
          params: {
            cn: cn
          }
        });
        fail();
      }
      catch (err) {
      }

      var entry = getLogEntries(tick, [ 2200, 2201, 2202, 2300 ]);
      assertEqual(3, entry.length);

      assertEqual(2200, entry[0].type);
      assertEqual(2300, entry[1].type);
      assertEqual(2202, entry[2].type);

      assertEqual(entry[0].tid, entry[1].tid);
      assertEqual(entry[1].tid, entry[2].tid);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test actions
////////////////////////////////////////////////////////////////////////////////

    testLoggerTransactionWrite4 : function () {
      var state, tick;

      db._create(cn);
      db._create(cn2);

      state = replication.logger.state().state;
      tick = state.lastLogTick;

      try {
        db._executeTransaction({
          collections: {
            write: cn
          },
          action: function (params) {
            var c2 = require("internal").db._collection(params.cn2);

            // we're using a wrong collection here
            c2.save({ "test" : 2, "_key": "abc" });
          },
          params: {
            cn2: cn2
          }
        });
        fail();
      }
      catch (err) {
      }

      var entry = getLogEntries(tick, [ 2200, 2201, 2202, 2300 ]);
      assertEqual(0, entry.length);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test actions
////////////////////////////////////////////////////////////////////////////////

    testLoggerTransactionWrite5 : function () {
      var state, tick;

      var c1 = db._create(cn);
      db._create(cn2);

      state = replication.logger.state().state;
      tick = state.lastLogTick;

      db._executeTransaction({
        collections: {
          write: [ cn, cn2 ]
        },
        action: function (params) {
          var c1 = require("internal").db._collection(params.cn);

          c1.save({ "test" : 1, "_key": "12345" });
          c1.save({ "test" : 2, "_key": "abc" });
        },
        params: {
          cn: cn
        }
      });

      var entry = getLogEntries(tick, [ 2200, 2201, 2202, 2300 ]);
      assertEqual(4, entry.length);

      assertEqual(2200, entry[0].type);
      assertEqual(2300, entry[1].type);
      assertEqual(2300, entry[2].type);
      assertEqual(2201, entry[3].type);

      assertEqual(entry[0].tid, entry[1].tid);
      assertEqual(entry[1].tid, entry[2].tid);
      assertEqual(entry[2].tid, entry[3].tid);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test actions
////////////////////////////////////////////////////////////////////////////////

    testLoggerTransactionWrite6 : function () {
      var state, tick;

      var c1 = db._create(cn);
      var c2 = db._create(cn2);

      state = replication.logger.state().state;
      tick = state.lastLogTick;

      db._executeTransaction({
        collections: {
          write: [ cn, cn2 ]
        },
        action: function (params) {
          var c1 = require("internal").db._collection(params.cn);
          var c2 = require("internal").db._collection(params.cn2);

          c1.save({ "test" : 1, "_key": "12345" });
          c2.save({ "test" : 2, "_key": "abc" });
        },
        params: {
          cn: cn,
          cn2: cn2
        }
      });

      var entry = getLogEntries(tick, [ 2200, 2201, 2202, 2300 ]);
      assertEqual(4, entry.length);

      assertEqual(2200, entry[0].type);
      assertEqual(2300, entry[1].type);
      assertEqual(2300, entry[2].type);
      assertEqual(2201, entry[3].type);

      assertEqual(entry[0].tid, entry[1].tid);
      assertEqual(entry[1].tid, entry[2].tid);
      assertEqual(entry[2].tid, entry[3].tid);
      assertEqual(c1._id, entry[1].cid);
      assertEqual(c2._id, entry[2].cid);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test collection exclusion
////////////////////////////////////////////////////////////////////////////////

    testLoggerTransactionExcluded : function () {
      var state, tick;
      var c = db._create(cn);

      state = replication.logger.state().state;
      tick = state.lastLogTick;

      db._executeTransaction({
        collections: {
          write: [ cn, "_users" ]
        },
        action: function (params) {
          var c = require("internal").db._collection(params.cn);
          var users = require("internal").db._collection("_users");

          c.save({ "test" : 2, "_key": "12345" });
          users.save({ "_key": "unittests1", "foo": false });
          users.remove("unittests1");
        },
        params: {
          cn: cn
        }
      });

      var entry = getLogEntries(tick, [ 2200, 2201, 2202, 2300, 2302 ]);
      assertEqual(5, entry.length);

      assertEqual(2200, entry[0].type);
      assertEqual(2300, entry[1].type);
      assertEqual(2300, entry[2].type);
      assertEqual(2302, entry[3].type);
      assertEqual(2201, entry[4].type);

      assertEqual(entry[0].tid, entry[1].tid);
      assertEqual(entry[1].tid, entry[2].tid);
      assertEqual(c._id, entry[1].cid);
    }

  };
}

// -----------------------------------------------------------------------------
// --SECTION--                                          replication applier test
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////

function ReplicationApplierSuite () {

  return {

////////////////////////////////////////////////////////////////////////////////
/// @brief set up
////////////////////////////////////////////////////////////////////////////////

    setUp : function () {
      replication.applier.shutdown();
      replication.applier.forget();
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief tear down
////////////////////////////////////////////////////////////////////////////////

    tearDown : function () {
      replication.applier.shutdown();
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief start applier w/o configuration
////////////////////////////////////////////////////////////////////////////////

    testStartApplierNoConfig : function () {
      var state;

      state = replication.applier.state();

      assertFalse(state.state.running);

      try {
        // start
        replication.applier.start();
        fail();
      }
      catch (err) {
        assertEqual(errors.ERROR_REPLICATION_INVALID_APPLIER_CONFIGURATION.code, err.errorNum);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief start applier with configuration
////////////////////////////////////////////////////////////////////////////////

    testStartApplierInvalidEndpoint1 : function () {
      var state;

      state = replication.applier.state();

      assertFalse(state.state.running);

      // configure && start
      replication.applier.properties({
        endpoint: "tcp://9.9.9.9:9999", // should not exist
        connectTimeout: 3,
        maxConnectRetries: 1
      });

      replication.applier.start();
      state = replication.applier.state();
      assertEqual(errors.ERROR_NO_ERROR.code, state.state.lastError.errorNum);
      assertTrue(state.state.running);

      var i = 0, max = 30;
      while (i++ < max) {
        internal.wait(1);

        state = replication.applier.state();
        if (state.state.running) {
          continue;
        }

        assertFalse(state.state.running);
        assertTrue(state.state.totalFailedConnects > 0);
        assertTrue(state.state.progress.failedConnects > 0);
        assertTrue(errors.ERROR_REPLICATION_NO_RESPONSE.code, state.state.lastError.errorNum);
        break;
      }

      if (i >= max) {
        fail();
      }

      // call start again
      replication.applier.start();

      state = replication.applier.state();
      assertTrue(state.state.running);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief start applier with configuration
////////////////////////////////////////////////////////////////////////////////

    testStartApplierInvalidEndpoint2 : function () {
      var state;

      state = replication.applier.state();

      assertFalse(state.state.running);
      // configure && start
      replication.applier.properties({
        endpoint: "tcp://www.arangodb.com:7999", // should not exist
        connectTimeout: 3,
        maxConnectRetries: 1
      });

      replication.applier.start();
      state = replication.applier.state();
      assertTrue(state.state.running);

      var i = 0, max = 30;
      while (i++ < max) {
        internal.wait(1);

        state = replication.applier.state();
        if (state.state.running) {
          continue;
        }

        assertFalse(state.state.running);
        assertTrue(state.state.totalFailedConnects > 0);
        assertTrue(state.state.progress.failedConnects > 0);
        assertTrue(state.state.lastError.errorNum === errors.ERROR_REPLICATION_INVALID_RESPONSE.code ||
                   state.state.lastError.errorNum === errors.ERROR_REPLICATION_MASTER_ERROR.code ||
                   state.state.lastError.errorNum === errors.ERROR_REPLICATION_NO_RESPONSE.code);
        break;
      }

      if (i >= max) {
        fail();
      }

      // call start again
      replication.applier.start();

      state = replication.applier.state();
      assertTrue(state.state.running);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief stop applier
////////////////////////////////////////////////////////////////////////////////

    testStopApplier : function () {
      var state;

      state = replication.applier.state();

      assertFalse(state.state.running);

      // configure && start
      replication.applier.properties({
        endpoint: "tcp://9.9.9.9:9999" // should not exist
      });

      replication.applier.start();
      state = replication.applier.state();
      assertEqual(errors.ERROR_NO_ERROR.code, state.state.lastError.errorNum);
      assertTrue(state.state.running);

      // stop
      replication.applier.shutdown();

      state = replication.applier.state();
      assertFalse(state.state.running);

      // stop again
      replication.applier.shutdown();

      state = replication.applier.state();
      assertFalse(state.state.running);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief properties
////////////////////////////////////////////////////////////////////////////////

    testApplierProperties : function () {
      var properties;

      properties = replication.applier.properties();

      assertEqual(300, properties.requestTimeout);
      assertEqual(10, properties.connectTimeout);
      assertEqual(100, properties.maxConnectRetries);
      assertEqual(0, properties.chunkSize);
      assertFalse(properties.autoStart);
      assertTrue(properties.adaptivePolling);
      assertUndefined(properties.endpoint);
      assertTrue(properties.includeSystem);
      assertEqual("", properties.restrictType);
      assertEqual([ ], properties.restrictCollections);

      try {
        replication.applier.properties({ });
        fail();
      }
      catch (err) {
        assertEqual(errors.ERROR_REPLICATION_INVALID_APPLIER_CONFIGURATION.code, err.errorNum);
      }

      replication.applier.properties({
        endpoint: "tcp://9.9.9.9:9999"
      });

      properties = replication.applier.properties();
      assertEqual(properties.endpoint, "tcp://9.9.9.9:9999");
      assertEqual(300, properties.requestTimeout);
      assertEqual(10, properties.connectTimeout);
      assertEqual(100, properties.maxConnectRetries);
      assertEqual(0, properties.chunkSize);
      assertFalse(properties.autoStart);
      assertTrue(properties.adaptivePolling);

      replication.applier.properties({
        endpoint: "tcp://9.9.9.9:9998",
        autoStart: true,
        adaptivePolling: false,
        requestTimeout: 5,
        connectTimeout: 9,
        maxConnectRetries: 4,
        chunkSize: 65536,
        includeSystem: false,
        restrictType: "include",
        restrictCollections: [ "_users" ]
      });

      properties = replication.applier.properties();
      assertEqual(properties.endpoint, "tcp://9.9.9.9:9998");
      assertEqual(5, properties.requestTimeout);
      assertEqual(9, properties.connectTimeout);
      assertEqual(4, properties.maxConnectRetries);
      assertEqual(65536, properties.chunkSize);
      assertTrue(properties.autoStart);
      assertFalse(properties.adaptivePolling);
      assertFalse(properties.includeSystem);
      assertEqual("include", properties.restrictType);
      assertEqual([ "_users" ], properties.restrictCollections);
      
      replication.applier.properties({
        endpoint: "tcp://9.9.9.9:9998",
        autoStart: false,
        maxConnectRetries: 10,
        chunkSize: 128 * 1024,
        includeSystem: true,
        restrictType: "exclude",
        restrictCollections: [ "foo", "bar", "baz" ]
      });
      
      properties = replication.applier.properties();
      assertEqual(properties.endpoint, "tcp://9.9.9.9:9998");
      assertEqual(5, properties.requestTimeout);
      assertEqual(9, properties.connectTimeout);
      assertEqual(10, properties.maxConnectRetries);
      assertEqual(128 * 1024, properties.chunkSize);
      assertFalse(properties.autoStart);
      assertFalse(properties.adaptivePolling);
      assertTrue(properties.includeSystem);
      assertEqual("exclude", properties.restrictType);
      assertEqual([ "bar", "baz", "foo" ], properties.restrictCollections.sort());
      
      replication.applier.properties({
        restrictType: "",
        restrictCollections: [ ]
      });
      
      properties = replication.applier.properties();
      assertEqual("", properties.restrictType);
      assertEqual([ ], properties.restrictCollections);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief start property change while running
////////////////////////////////////////////////////////////////////////////////

    testApplierPropertiesChange : function () {
      var state;

      replication.applier.properties({ endpoint: "tcp://9.9.9.9:9999" });
      replication.applier.start();

      state = replication.applier.state();
      assertTrue(state.state.running);

      try {
        replication.applier.properties({ endpoint: "tcp://9.9.9.9:9998" });
        fail();
      }
      catch (err) {
        assertEqual(errors.ERROR_REPLICATION_RUNNING.code, err.errorNum);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief applier state
////////////////////////////////////////////////////////////////////////////////

    testStateApplier : function () {
      var state;

      state = replication.applier.state();

      assertFalse(state.state.running);
      assertMatch(/^\d+-\d+-\d+T\d+:\d+:\d+Z$/, state.state.time);

      assertEqual(state.server.version, db._version());
      assertNotEqual("", state.server.serverId);
      assertMatch(/^\d+$/, state.server.serverId);

      assertUndefined(state.endpoint);
    }

  };
}

// -----------------------------------------------------------------------------
// --SECTION--                                                              sync
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////

function ReplicationSyncSuite () {

  return {

////////////////////////////////////////////////////////////////////////////////
/// @brief set up
////////////////////////////////////////////////////////////////////////////////

    setUp : function () {
      replication.applier.shutdown();
      replication.applier.forget();
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief tear down
////////////////////////////////////////////////////////////////////////////////

    tearDown : function () {
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief server id
////////////////////////////////////////////////////////////////////////////////

    testServerId : function () {
      var result = replication.serverId();

      assertTrue(typeof result === 'string');
      assertMatch(/^\d+$/, result);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief no endpoint
////////////////////////////////////////////////////////////////////////////////

    testSyncNoEndpoint : function () {
      try {
        replication.sync();
        fail();
      }
      catch (err) {
        assertEqual(errors.ERROR_BAD_PARAMETER.code, err.errorNum);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief invalid endpoint
////////////////////////////////////////////////////////////////////////////////

    testSyncNoEndpoint : function () {
      try {
        replication.sync({
          endpoint: "tcp://9.9.9.9:9999"
        });
        fail();
      }
      catch (err) {
        assertEqual(errors.ERROR_REPLICATION_NO_RESPONSE.code, err.errorNum);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief invalid response
////////////////////////////////////////////////////////////////////////////////

    testSyncInvalidResponse : function () {
      try {
        replication.sync({
          endpoint: "tcp://www.arangodb.com:80"
        });
        fail();
      }
      catch (err) {
        assertTrue(err.errorNum === errors.ERROR_REPLICATION_INVALID_RESPONSE.code ||
                   err.errorNum === errors.ERROR_REPLICATION_MASTER_ERROR.code);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief invalid restrictType
////////////////////////////////////////////////////////////////////////////////

    testSyncRestrict1 : function () {
      try {
        replication.sync({
          endpoint: "tcp://9.9.9.9:9999",
          restrictType: "foo"
        });
        fail();
      }
      catch (err) {
        assertTrue(errors.ERROR_BAD_PARAMETER.code === err.errorNum ||
                   errors.ERROR_HTTP_BAD_PARAMETER.code === err.errorNum);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief invalid restrictCollections
////////////////////////////////////////////////////////////////////////////////

    testSyncRestrict2 : function () {
      try {
        replication.sync({
          endpoint: "tcp://9.9.9.9:9999",
          restrictType: "exclude"
        });
        fail();
      }
      catch (err) {
        assertTrue(errors.ERROR_BAD_PARAMETER.code === err.errorNum ||
                   errors.ERROR_HTTP_BAD_PARAMETER.code === err.errorNum);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief invalid restrictCollections
////////////////////////////////////////////////////////////////////////////////

    testSyncRestrict3 : function () {
      try {
        replication.sync({
          endpoint: "tcp://9.9.9.9:9999",
          restrictType: "include"
        });
        fail();
      }
      catch (err) {
        assertTrue(errors.ERROR_BAD_PARAMETER.code === err.errorNum ||
                   errors.ERROR_HTTP_BAD_PARAMETER.code === err.errorNum);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief invalid restrictCollections
////////////////////////////////////////////////////////////////////////////////

    testSyncRestrict4 : function () {
      try {
        replication.sync({
          endpoint: "tcp://9.9.9.9:9999",
          restrictCollections: [ "foo" ]
        });
        fail();
      }
      catch (err) {
        assertTrue(errors.ERROR_BAD_PARAMETER.code === err.errorNum ||
                   errors.ERROR_HTTP_BAD_PARAMETER.code === err.errorNum);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief invalid restrictCollections
////////////////////////////////////////////////////////////////////////////////

    testSyncRestrict5 : function () {
      try {
        replication.sync({
          endpoint: "tcp://9.9.9.9:9999",
          restrictType: "include",
          restrictCollections: "foo"
        });
        fail();
      }
      catch (err) {
        assertTrue(errors.ERROR_BAD_PARAMETER.code === err.errorNum ||
                   errors.ERROR_HTTP_BAD_PARAMETER.code === err.errorNum);
      }
    }

  };
}

// -----------------------------------------------------------------------------
// --SECTION--                                                              main
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suites
////////////////////////////////////////////////////////////////////////////////

jsunity.run(ReplicationLoggerSuite);
jsunity.run(ReplicationApplierSuite);
jsunity.run(ReplicationSyncSuite);

return jsunity.done();

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// @addtogroup\\|// --SECTION--\\|/// @page\\|/// @}\\)"
// End:
