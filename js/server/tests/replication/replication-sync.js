/*jshint globalstrict:false, strict:false, unused: false */
/*global fail, assertEqual, assertTrue, assertFalse, assertNull, arango, ARGUMENTS */

////////////////////////////////////////////////////////////////////////////////
/// @brief test the sync method of the replication
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
var arangodb = require("@arangodb");
var errors = arangodb.errors;
var db = arangodb.db;
var _ = require('lodash');

var replication = require("@arangodb/replication");
var console = require("console");
var internal = require("internal");
var masterEndpoint = arango.getEndpoint();
var slaveEndpoint = ARGUMENTS[0];

var mmfilesEngine = false;
if (db._engine().name === "mmfiles") {
  mmfilesEngine = true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////

function ReplicationSuite() {
  'use strict';
  var cn = "UnitTestsReplication";

  var connectToMaster = function() {
    arango.reconnect(masterEndpoint, db._name(), "root", "");
    db._flushCache();
  };

  var connectToSlave = function() {
    arango.reconnect(slaveEndpoint, db._name(), "root", "");
    db._flushCache();
  };

  var collectionChecksum = function(name) {
    var c = db._collection(name).checksum(true, true);
    return c.checksum;
  };

  var collectionCount = function(name) {
    var res = db._collection(name).count();
    return res;
  };

  var compare = function(masterFunc, slaveInitFunc, slaveCompareFunc, incremental) {
    var state = {};

    db._flushCache();
    masterFunc(state);
    require("internal").wal.flush(true, true);

    db._flushCache();
    connectToSlave();

    slaveInitFunc(state);
    internal.wait(1, false);

    var syncResult = replication.syncCollection(cn, {
      endpoint: masterEndpoint,
      verbose: true,
      incremental: incremental
    });

    db._flushCache();
    slaveCompareFunc(state);
  };

  return {

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief set up
    ////////////////////////////////////////////////////////////////////////////////

    setUp: function() {
      connectToMaster();
      db._drop(cn);
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief tear down
    ////////////////////////////////////////////////////////////////////////////////

    tearDown: function() {
      connectToMaster();
      db._drop(cn);

      connectToSlave();
      db._drop(cn);
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief test existing collection
    ////////////////////////////////////////////////////////////////////////////////

    testRemoveSomeWithIncremental: function() {
      connectToMaster();

      var st;

      compare(
        function(state) {
          var c = db._create(cn),
            i;

          for (i = 0; i < 5000; ++i) {
            c.save({
              _key: "test" + i
            });
          }

          state.checksum = collectionChecksum(cn);
          state.count = collectionCount(cn);
          assertEqual(5000, state.count);

          st = _.clone(state); // save state
        },
        function(state) {
        },
        function(state) {
          assertEqual(state.count, collectionCount(cn));
          assertEqual(state.checksum, collectionChecksum(cn));
        },
        true
      );
  
      connectToSlave();

      assertEqual(5000, collectionCount(cn));
      var c = db._collection(cn);
      // remove some random documents
      for (var i = 0; i < 50; ++i) {
        c.remove(c.any());
      } 
      assertEqual(4950, collectionCount(cn));
  
      // and sync again    
      var syncResult = replication.syncCollection(cn, {
        endpoint: masterEndpoint,
        verbose: true,
        incremental: true
      });
  
      assertEqual(st.count, collectionCount(cn));
      assertEqual(st.checksum, collectionChecksum(cn));
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief test existing collection
    ////////////////////////////////////////////////////////////////////////////////

    testInsertSomeWithIncremental: function() {
      connectToMaster();

      var st;

      compare(
        function(state) {
          var c = db._create(cn),
            i;

          for (i = 0; i < 5000; ++i) {
            c.save({
              _key: "test" + i
            });
          }

          state.checksum = collectionChecksum(cn);
          state.count = collectionCount(cn);
          assertEqual(5000, state.count);

          st = _.clone(state); // save state
        },
        function(state) {
        },
        function(state) {
          assertEqual(state.count, collectionCount(cn));
          assertEqual(state.checksum, collectionChecksum(cn));
        },
        true
      );
   
      connectToSlave();

      var c = db._collection(cn);
      // insert some random documents
      for (var i = 0; i < 100; ++i) {
        c.insert({ foo: "bar" + i });
      } 
      
      assertEqual(5100, collectionCount(cn));
  
      // and sync again    
      var syncResult = replication.syncCollection(cn, {
        endpoint: masterEndpoint,
        verbose: true,
        incremental: true
      });
          
      assertEqual(st.count, collectionCount(cn));
      assertEqual(st.checksum, collectionChecksum(cn));
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief test collection properties
    ////////////////////////////////////////////////////////////////////////////////

    testProperties: function() {
      connectToMaster();

      compare(
        function(state) {
          var c = db._create(cn);

          c.properties({
            indexBuckets: 32,
            waitForSync: true,
            journalSize: 16 * 1024 * 1024
          });
        },
        function(state) {
          // don't create the collection on the slave
        },
        function(state) {
          var c = db._collection(cn);
          var p = c.properties();
          assertTrue(p.waitForSync);
          if (mmfilesEngine) {
            assertEqual(32, p.indexBuckets);
            assertEqual(16 * 1024 * 1024, p.journalSize);
          }
        },
        true
      );
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief test collection properties
    ////////////////////////////////////////////////////////////////////////////////

    testPropertiesOther: function() {
      connectToMaster();

      compare(
        function(state) {
          var c = db._create(cn);

          c.properties({
            indexBuckets: 32,
            waitForSync: true,
            journalSize: 16 * 1024 * 1024
          });
        },
        function(state) {
          // create the collection on the slave, but with default properties
          db._create(cn);
        },
        function(state) {
          var c = db._collection(cn);
          var p = c.properties();
          assertTrue(p.waitForSync);
          if (mmfilesEngine) {
            assertEqual(32, p.indexBuckets);
            assertEqual(16 * 1024 * 1024, p.journalSize);
          }
        },
        true
      );
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief test with indexes
    ////////////////////////////////////////////////////////////////////////////////

    testCreateIndexes: function() {
      connectToMaster();

      compare(
        function(state) {
          var c = db._create(cn),
            i;

          for (i = 0; i < 5000; ++i) {
            c.save({
              _key: "test" + i,
              "value1": i,
              "value2": "test" + i
            });
          }

          c.ensureIndex({
            type: "hash",
            fields: ["value1", "value2"]
          });
          c.ensureIndex({
            type: "skiplist",
            fields: ["value1"]
          });

          state.checksum = collectionChecksum(cn);
          state.count = collectionCount(cn);
          assertEqual(5000, state.count);
        },
        function(state) {
          // already create the collection on the slave
          var c = db._create(cn);

          for (var i = 0; i < 5000; ++i) {
            c.save({
              _key: "test" + i,
              "value1": "test" + i,
              "value2": i
            });
          }
        },
        function(state) {
          assertEqual(state.count, collectionCount(cn));
          assertEqual(state.checksum, collectionChecksum(cn));

          var idx = db._collection(cn).getIndexes();
          assertEqual(3, idx.length); // primary + hash + skiplist
          for (var i = 1; i < idx.length; ++i) {
            assertFalse(idx[i].unique);
            assertFalse(idx[i].sparse);

            if (idx[i].type === 'hash') {
              assertEqual("hash", idx[i].type);
              assertEqual(["value1", "value2"], idx[i].fields);
            } else {
              assertEqual("skiplist", idx[i].type);
              assertEqual(["value1"], idx[i].fields);
            }
          }
        },
        true
      );
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief test with edges
    ////////////////////////////////////////////////////////////////////////////////

    testEdges: function() {
      connectToMaster();

      compare(
        function(state) {
          var c = db._createEdgeCollection(cn),
            i;

          for (i = 0; i < 100; ++i) {
            c.save(cn + "/test" + i, cn + "/test" + (i % 10), {
              _key: "test" + i,
              "value1": i,
              "value2": "test" + i
            });
          }

          state.checksum = collectionChecksum(cn);
          state.count = collectionCount(cn);
          assertEqual(100, state.count);
        },
        function(state) {
          // already create the collection on the slave
          var c = db._createEdgeCollection(cn);

          for (var i = 0; i < 100; ++i) {
            c.save(cn + "/test" + i, cn + "/test" + (i % 10), {
              _key: "test" + i,
              "value1": i,
              "value2": "test" + i
            });
          }
        },
        function(state) {
          assertEqual(state.count, collectionCount(cn));
          assertEqual(state.checksum, collectionChecksum(cn));

          var c = db._collection(cn);
          assertEqual(3, c.type());

          for (var i = 0; i < 100; ++i) {
            var doc = c.document("test" + i);
            assertEqual(cn + "/test" + i, doc._from);
            assertEqual(cn + "/test" + (i % 10), doc._to);
            assertEqual(i, doc.value1);
            assertEqual("test" + i, doc.value2);
          }
        },
        true
      );
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief test with edges differences
    ////////////////////////////////////////////////////////////////////////////////

    testEdgesDifferences: function() {
      connectToMaster();

      compare(
        function(state) {
          var c = db._createEdgeCollection(cn),
            i;

          for (i = 0; i < 100; ++i) {
            c.save(cn + "/test" + i, cn + "/test" + (i % 10), {
              _key: "test" + i,
              "value1": i,
              "value2": "test" + i
            });
          }

          state.checksum = collectionChecksum(cn);
          state.count = collectionCount(cn);
          assertEqual(100, state.count);
        },
        function(state) {
          // already create the collection on the slave
          var c = db._createEdgeCollection(cn);

          for (var i = 0; i < 200; ++i) {
            c.save(
              cn + "/test" + (i + 1),
              cn + "/test" + (i % 11), {
                _key: "test" + i,
                "value1": i,
                "value2": "test" + i
              }
            );
          }
        },
        function(state) {
          assertEqual(state.count, collectionCount(cn));
          assertEqual(state.checksum, collectionChecksum(cn));

          var c = db._collection(cn);
          assertEqual(3, c.type());

          for (var i = 0; i < 100; ++i) {
            var doc = c.document("test" + i);
            assertEqual(cn + "/test" + i, doc._from);
            assertEqual(cn + "/test" + (i % 10), doc._to);
            assertEqual(i, doc.value1);
            assertEqual("test" + i, doc.value2);
          }
        },
        true
      );
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief test non-present collection
    ////////////////////////////////////////////////////////////////////////////////

    testNonPresent: function() {
      connectToMaster();

      compare(
        function(state) {
          var c = db._create(cn),
            i;

          for (i = 0; i < 5000; ++i) {
            c.save({
              "value": i
            });
          }

          state.checksum = collectionChecksum(cn);
          state.count = collectionCount(cn);
          assertEqual(5000, state.count);
        },
        function(state) {},
        function(state) {
          assertEqual(state.count, collectionCount(cn));
          assertEqual(state.checksum, collectionChecksum(cn));
        },
        false
      );
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief test non-present collection
    ////////////////////////////////////////////////////////////////////////////////

    testNonPresentIncremental: function() {
      connectToMaster();

      compare(
        function(state) {
          var c = db._create(cn),
            i;

          for (i = 0; i < 5000; ++i) {
            c.save({
              "value": i
            });
          }

          state.checksum = collectionChecksum(cn);
          state.count = collectionCount(cn);
          assertEqual(5000, state.count);
        },
        function(state) {},
        function(state) {
          assertEqual(state.count, collectionCount(cn));
          assertEqual(state.checksum, collectionChecksum(cn));
        },
        true
      );
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief test existing collection
    ////////////////////////////////////////////////////////////////////////////////

    testExisting: function() {
      connectToMaster();

      compare(
        function(state) {
          var c = db._create(cn),
            i;

          for (i = 0; i < 5000; ++i) {
            c.save({
              "value": i
            });
          }

          state.checksum = collectionChecksum(cn);
          state.count = collectionCount(cn);
          assertEqual(5000, state.count);
        },
        function(state) {
          // already create the collection on the slave
          db._create(cn);
        },
        function(state) {
          assertEqual(state.count, collectionCount(cn));
          assertEqual(state.checksum, collectionChecksum(cn));
        },
        false
      );
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief test existing collection
    ////////////////////////////////////////////////////////////////////////////////

    testExistingIncremental: function() {
      connectToMaster();

      compare(
        function(state) {
          var c = db._create(cn),
            i;

          for (i = 0; i < 5000; ++i) {
            c.save({
              "value": i
            });
          }

          state.checksum = collectionChecksum(cn);
          state.count = collectionCount(cn);
          assertEqual(5000, state.count);
        },
        function(state) {
          // already create the collection on the slave
          db._create(cn);
        },
        function(state) {
          assertEqual(state.count, collectionCount(cn));
          assertEqual(state.checksum, collectionChecksum(cn));
        },
        true
      );
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief test with existing documents - but empty on master
    ////////////////////////////////////////////////////////////////////////////////

    testExistingEmptyOnMaster: function() {
      connectToMaster();

      compare(
        function(state) {
          var c = db._create(cn),
            i;

          state.checksum = collectionChecksum(cn);
          state.count = collectionCount(cn);
          assertEqual(0, state.count);
        },
        function(state) {
          // already create the collection on the slave
          var c = db._create(cn);

          for (var i = 0; i < 5000; ++i) {
            c.save({
              "value": i
            });
          }
        },
        function(state) {
          assertEqual(state.count, collectionCount(cn));
          assertEqual(state.checksum, collectionChecksum(cn));
        },
        false
      );
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief test with existing documents - but empty on master
    ////////////////////////////////////////////////////////////////////////////////

    testExistingEmptyOnMasterIncremental: function() {
      connectToMaster();

      compare(
        function(state) {
          var c = db._create(cn),
            i;

          state.checksum = collectionChecksum(cn);
          state.count = collectionCount(cn);
          assertEqual(0, state.count);
        },
        function(state) {
          // already create the collection on the slave
          var c = db._create(cn);

          for (var i = 0; i < 5000; ++i) {
            c.save({
              "value": i
            });
          }
        },
        function(state) {
          assertEqual(state.count, collectionCount(cn));
          assertEqual(state.checksum, collectionChecksum(cn));
        },
        true
      );
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief test with existing documents - less on the slave
    ////////////////////////////////////////////////////////////////////////////////

    testExistingDocumentsLess: function() {
      connectToMaster();

      compare(
        function(state) {
          var c = db._create(cn),
            i;

          for (i = 0; i < 5000; ++i) {
            c.save({
              "value": i
            });
          }

          state.checksum = collectionChecksum(cn);
          state.count = collectionCount(cn);
          assertEqual(5000, state.count);
        },
        function(state) {
          // already create the collection on the slave
          var c = db._create(cn);

          for (var i = 0; i < 500; ++i) {
            c.save({
              "value": i
            });
          }
        },
        function(state) {
          assertEqual(state.count, collectionCount(cn));
          assertEqual(state.checksum, collectionChecksum(cn));
        },
        false
      );
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief test with existing documents - less on the slave
    ////////////////////////////////////////////////////////////////////////////////

    testExistingDocumentsLessIncremental: function() {
      connectToMaster();

      compare(
        function(state) {
          var c = db._create(cn),
            i;

          for (i = 0; i < 5000; ++i) {
            c.save({
              "value": i
            });
          }

          state.checksum = collectionChecksum(cn);
          state.count = collectionCount(cn);
          assertEqual(5000, state.count);
        },
        function(state) {
          // already create the collection on the slave
          var c = db._create(cn);

          for (var i = 0; i < 500; ++i) {
            c.save({
              "value": i
            });
          }
        },
        function(state) {
          assertEqual(state.count, collectionCount(cn));
          assertEqual(state.checksum, collectionChecksum(cn));
        },
        true
      );
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief test with existing documents - more on the slave
    ////////////////////////////////////////////////////////////////////////////////

    testMoreDocuments: function() {
      connectToMaster();

      compare(
        function(state) {
          var c = db._create(cn),
            i;

          for (i = 0; i < 5000; ++i) {
            c.save({
              "value": i
            });
          }

          state.checksum = collectionChecksum(cn);
          state.count = collectionCount(cn);
          assertEqual(5000, state.count);
        },
        function(state) {
          // already create the collection on the slave
          var c = db._create(cn);

          for (var i = 0; i < 6000; ++i) {
            c.save({
              "value": i
            });
          }
        },
        function(state) {
          assertEqual(state.count, collectionCount(cn));
          assertEqual(state.checksum, collectionChecksum(cn));
        },
        false
      );
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief test with existing documents - more on the slave
    ////////////////////////////////////////////////////////////////////////////////

    testMoreDocumentsIncremental: function() {
      connectToMaster();

      compare(
        function(state) {
          var c = db._create(cn),
            i;

          for (i = 0; i < 5000; ++i) {
            c.save({
              "value": i
            });
          }

          state.checksum = collectionChecksum(cn);
          state.count = collectionCount(cn);
          assertEqual(5000, state.count);
        },
        function(state) {
          // already create the collection on the slave
          var c = db._create(cn);

          for (var i = 0; i < 6000; ++i) {
            c.save({
              "value": i
            });
          }
        },
        function(state) {
          assertEqual(state.count, collectionCount(cn));
          assertEqual(state.checksum, collectionChecksum(cn));
        },
        true
      );
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief test with existing documents - same on the slave
    ////////////////////////////////////////////////////////////////////////////////

    testSameDocuments: function() {
      connectToMaster();

      compare(
        function(state) {
          var c = db._create(cn),
            i;

          for (i = 0; i < 5000; ++i) {
            c.save({
              _key: "test" + i,
              "value": i
            });
          }

          state.checksum = collectionChecksum(cn);
          state.count = collectionCount(cn);
          assertEqual(5000, state.count);
        },
        function(state) {
          // already create the collection on the slave
          var c = db._create(cn);

          for (var i = 0; i < 5000; ++i) {
            c.save({
              _key: "test" + i,
              "value": i
            });
          }
        },
        function(state) {
          assertEqual(state.count, collectionCount(cn));
          assertEqual(state.checksum, collectionChecksum(cn));
        },
        false
      );
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief test with existing documents - same on the slave
    ////////////////////////////////////////////////////////////////////////////////

    testSameDocumentsIncremental: function() {
      connectToMaster();

      compare(
        function(state) {
          var c = db._create(cn),
            i;

          for (i = 0; i < 5000; ++i) {
            c.save({
              _key: "test" + i,
              "value": i
            });
          }

          state.checksum = collectionChecksum(cn);
          state.count = collectionCount(cn);
          assertEqual(5000, state.count);
        },
        function(state) {
          // already create the collection on the slave
          var c = db._create(cn);

          for (var i = 0; i < 5000; ++i) {
            c.save({
              _key: "test" + i,
              "value": i
            });
          }
        },
        function(state) {
          assertEqual(state.count, collectionCount(cn));
          assertEqual(state.checksum, collectionChecksum(cn));
        },
        true
      );
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief test with existing documents - same on the slave but different keys
    ////////////////////////////////////////////////////////////////////////////////

    testSameSameButDifferentKeys: function() {
      connectToMaster();

      compare(
        function(state) {
          var c = db._create(cn),
            i;

          for (i = 0; i < 5000; ++i) {
            c.save({
              "value": i
            });
          }

          state.checksum = collectionChecksum(cn);
          state.count = collectionCount(cn);
          assertEqual(5000, state.count);
        },
        function(state) {
          // already create the collection on the slave
          var c = db._create(cn);

          for (var i = 0; i < 5000; ++i) {
            c.save({
              "value": i
            });
          }
        },
        function(state) {
          assertEqual(state.count, collectionCount(cn));
          assertEqual(state.checksum, collectionChecksum(cn));
        },
        false
      );
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief test with existing documents - same on the slave but different keys
    ////////////////////////////////////////////////////////////////////////////////

    testSameSameButDifferentKeysIncremental: function() {
      connectToMaster();

      compare(
        function(state) {
          var c = db._create(cn),
            i;

          for (i = 0; i < 5000; ++i) {
            c.save({
              "value": i
            });
          }

          state.checksum = collectionChecksum(cn);
          state.count = collectionCount(cn);
          assertEqual(5000, state.count);
        },
        function(state) {
          // already create the collection on the slave
          var c = db._create(cn);

          for (var i = 0; i < 5000; ++i) {
            c.save({
              "value": i
            });
          }
        },
        function(state) {
          assertEqual(state.count, collectionCount(cn));
          assertEqual(state.checksum, collectionChecksum(cn));
        },
        true
      );
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief test with existing documents - same on the slave but different values
    ////////////////////////////////////////////////////////////////////////////////

    testSameSameButDifferentValues: function() {
      connectToMaster();

      compare(
        function(state) {
          var c = db._create(cn),
            i;

          for (i = 0; i < 5000; ++i) {
            c.save({
              _key: "test" + i,
              "value1": i,
              "value2": "test" + i
            });
          }

          state.checksum = collectionChecksum(cn);
          state.count = collectionCount(cn);
          assertEqual(5000, state.count);
        },
        function(state) {
          // already create the collection on the slave
          var c = db._create(cn);

          for (var i = 0; i < 5000; ++i) {
            c.save({
              _key: "test" + i,
              "value1": "test" + i,
              "value2": i
            });
          }
        },
        function(state) {
          assertEqual(state.count, collectionCount(cn));
          assertEqual(state.checksum, collectionChecksum(cn));
        },
        false
      );
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief test with existing documents - same on the slave but different values
    ////////////////////////////////////////////////////////////////////////////////

    testSameSameButDifferentValuesIncremental: function() {
      connectToMaster();

      compare(
        function(state) {
          var c = db._create(cn),
            i;

          for (i = 0; i < 5000; ++i) {
            c.save({
              _key: "test" + i,
              "value1": i,
              "value2": "test" + i
            });
          }

          state.checksum = collectionChecksum(cn);
          state.count = collectionCount(cn);
          assertEqual(5000, state.count);
        },
        function(state) {
          // already create the collection on the slave
          var c = db._create(cn);

          for (var i = 0; i < 5000; ++i) {
            c.save({
              _key: "test" + i,
              "value1": "test" + i,
              "value2": i
            });
          }
        },
        function(state) {
          assertEqual(state.count, collectionCount(cn));
          assertEqual(state.checksum, collectionChecksum(cn));
        },
        true
      );
    }
  
  };
}


////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suite
////////////////////////////////////////////////////////////////////////////////

jsunity.run(ReplicationSuite);

return jsunity.done();
