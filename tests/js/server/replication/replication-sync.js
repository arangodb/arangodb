/* jshint globalstrict:false, strict:false, unused: false */
/* global arango, assertEqual, assertTrue, assertFalse, arango, ARGUMENTS */

// //////////////////////////////////////////////////////////////////////////////
// / @brief test the sync method of the replication
// /
// / @file
// /
// / DISCLAIMER
// /
// / Copyright 2010-2012 triagens GmbH, Cologne, Germany
// /
// / Licensed under the Apache License, Version 2.0 (the "License");
// / you may not use this file except in compliance with the License.
// / You may obtain a copy of the License at
// /
// /     http://www.apache.org/licenses/LICENSE-2.0
// /
// / Unless required by applicable law or agreed to in writing, software
// / distributed under the License is distributed on an "AS IS" BASIS,
// / WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// / See the License for the specific language governing permissions and
// / limitations under the License.
// /
// / Copyright holder is triAGENS GmbH, Cologne, Germany
// /
// / @author Jan Steemann
// / @author Copyright 2013, triAGENS GmbH, Cologne, Germany
// //////////////////////////////////////////////////////////////////////////////

const jsunity = require('jsunity');
const arangodb = require('@arangodb');
const deriveTestSuite = require('@arangodb/test-helper').deriveTestSuite;
const db = arangodb.db;
const _ = require('lodash');

const replication = require('@arangodb/replication');
const internal = require('internal');
const masterEndpoint = arango.getEndpoint();
const slaveEndpoint = ARGUMENTS[0];

var mmfilesEngine = false;
if (db._engine().name === 'mmfiles') {
  mmfilesEngine = true;
}

const cn = 'UnitTestsReplication';

const connectToMaster = function () {
  arango.reconnect(masterEndpoint, db._name(), 'root', '');
  db._flushCache();
};

const connectToSlave = function () {
  arango.reconnect(slaveEndpoint, db._name(), 'root', '');
  db._flushCache();
};

const collectionChecksum = function (name) {
  return db._collection(name).checksum(true, true).checksum;
};

const collectionCount = function (name) {
  return db._collection(name).count();
};

const compare = function (masterFunc, slaveInitFunc, slaveCompareFunc, incremental) {
  var state = {};

  db._flushCache();
  masterFunc(state);

  db._flushCache();
  connectToSlave();

  slaveInitFunc(state);
  internal.wait(0.1, false);

  replication.syncCollection(cn, {
    endpoint: masterEndpoint,
    verbose: true,
    incremental
  });

  db._flushCache();
  slaveCompareFunc(state);
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief Base Test Config. Identitical part for _system and other DB
// //////////////////////////////////////////////////////////////////////////////

function BaseTestConfig () {
  'use strict';

  return {
    
    // //////////////////////////////////////////////////////////////////////////////
    // / @brief test existing collection
    // //////////////////////////////////////////////////////////////////////////////

    testExistingPatchBrokenSlaveCounters1: function () {
      // can only use this with failure tests enabled and RocksDB engine
      if (db._engine().name !== "rocksdb") {
        return;
      }
          
      let r = arango.GET("/_db/" + db._name() + "/_admin/debug/failat");
      if (String(r) === "false") {
        return;
      }

      connectToMaster();

      compare(
        function (state) {
          let c = db._create(cn);
          let docs = [];

          for (let i = 0; i < 5000; ++i) {
            docs.push({ value: i });
          }
          c.insert(docs);

          state.checksum = collectionChecksum(cn);
          state.count = collectionCount(cn);
          assertEqual(5000, state.count);
        },
        function (state) {
          //  already create the collection on the slave
          replication.syncCollection(cn, {
            endpoint: masterEndpoint,
            incremental: false
          });
          // collection present on slave now
          var c = db._collection(cn);
          assertEqual(5000, c.count());
          assertEqual(5000, c.toArray().length);
          arango.PUT_RAW("/_admin/debug/failat/RocksDBCommitCounts", "");
          c.insert({});
          arango.DELETE_RAW("/_admin/debug/failat", "");
          assertEqual(5000, c.count());
          assertEqual(5001, c.toArray().length);
        },
        function (state) {
          assertEqual(state.count, collectionCount(cn));
          assertEqual(state.checksum, collectionChecksum(cn));
          // now validate counters
          let c = db._collection(cn);
          assertEqual(5000, c.toArray().length);
          assertEqual(5000, c.count());
        },
        true
      );
    },
    
    testExistingPatchBrokenSlaveCounters2: function () {
      // can only use this with failure tests enabled and RocksDB engine
      if (db._engine().name !== "rocksdb") {
        return;
      }
          
      let r = arango.GET("/_db/" + db._name() + "/_admin/debug/failat");
      if (String(r) === "false") {
        return;
      }

      connectToMaster();

      compare(
        function (state) {
          let c = db._create(cn);
          let docs = [];

          for (let i = 0; i < 10000; ++i) {
            docs.push({ value: i, _key: "test" + i });
          }
          c.insert(docs);

          state.checksum = collectionChecksum(cn);
          state.count = collectionCount(cn);
          assertEqual(10000, state.count);
        },
        function (state) {
          //  already create the collection on the slave
          replication.syncCollection(cn, {
            endpoint: masterEndpoint,
            incremental: false
          });
          
          // collection present on slave now
          var c = db._collection(cn);
          for (let i = 0; i < 10000; i += 10) {
            c.remove("test" + i);
          }
          assertEqual(9000, c.count());
          assertEqual(9000, c.toArray().length);
          arango.PUT_RAW("/_admin/debug/failat/RocksDBCommitCounts", "");
          for (let i = 0; i < 100; ++i) {
            c.insert({ _key: "testmann" + i });
          }
          arango.DELETE_RAW("/_admin/debug/failat", "");
          assertEqual(9000, c.count());
          assertEqual(9100, c.toArray().length);
        },
        function (state) {
          assertEqual(state.count, collectionCount(cn));
          assertEqual(state.checksum, collectionChecksum(cn));
          // now validate counters
          let c = db._collection(cn);
          assertEqual(10000, c.count());
          assertEqual(10000, c.toArray().length);
        },
        true
      );
    },

    // //////////////////////////////////////////////////////////////////////////////
    // / @brief test existing collection
    // //////////////////////////////////////////////////////////////////////////////

    testRemoveSomeWithIncremental: function () {
      connectToMaster();

      var st;

      compare(
        function (state) {
          let c = db._create(cn);
          let docs = [];

          for (let i = 0; i < 5000; ++i) {
            docs.push({ _key: 'test' + i });
          }

          c.insert(docs);

          state.checksum = collectionChecksum(cn);
          state.count = collectionCount(cn);
          assertEqual(5000, state.count);

          st = _.clone(state); // save state
        },
        function (state) {
        },
        function (state) {
          assertEqual(state.count, collectionCount(cn));
          assertEqual(state.checksum, collectionChecksum(cn));
        },
        true
      );

      connectToSlave();

      assertEqual(5000, collectionCount(cn));
      var c = db._collection(cn);
      //  remove some random documents
      for (var i = 0; i < 50; ++i) {
        c.remove(c.any());
      }
      assertEqual(4950, collectionCount(cn));

      //  and sync again
      replication.syncCollection(cn, {
        endpoint: masterEndpoint,
        verbose: true,
        incremental: true
      });

      assertEqual(st.count, collectionCount(cn));
      assertEqual(st.checksum, collectionChecksum(cn));
    },
    
    // //////////////////////////////////////////////////////////////////////////////
    // / @brief test existing collection
    // //////////////////////////////////////////////////////////////////////////////

    testInsertSomeWithIncremental: function () {
      connectToMaster();

      var st;

      compare(
        function (state) {
          let c = db._create(cn);
          let docs = [];

          for (let i = 0; i < 5000; ++i) {
            docs.push({ _key: 'test' + i });
          }
          c.insert(docs);

          state.checksum = collectionChecksum(cn);
          state.count = collectionCount(cn);
          assertEqual(5000, state.count);

          st = _.clone(state); // save state
        },
        function (state) {
        },
        function (state) {
          assertEqual(state.count, collectionCount(cn));
          assertEqual(state.checksum, collectionChecksum(cn));
        },
        true
      );

      connectToSlave();

      let c = db._collection(cn);
      //  insert some random documents
      let docs = [];
      for (let i = 0; i < 100; ++i) {
        docs.push({ foo: 'bar' + i });
      }
      c.insert(docs);

      assertEqual(5100, collectionCount(cn));

      //  and sync again
      replication.syncCollection(cn, {
        endpoint: masterEndpoint,
        verbose: true,
        incremental: true
      });

      assertEqual(st.count, collectionCount(cn));
      assertEqual(st.checksum, collectionChecksum(cn));
    },

    // //////////////////////////////////////////////////////////////////////////////
    // / @brief test existing collection
    // //////////////////////////////////////////////////////////////////////////////

    testInsertHugeIncrementalMoreOnMaster: function () {
      connectToMaster();

      var st;

      compare(
        function (state) {
          let c = db._create(cn);
          let docs = [];

          for (let i = 0; i < 5000; ++i) {
            docs.push({ _key: 'test' + i });
          }
          c.insert(docs);

          state.checksum = collectionChecksum(cn);
          state.count = collectionCount(cn);
          assertEqual(5000, state.count);

          st = _.clone(state); // save state
        },
        function (state) {
        },
        function (state) {
          assertEqual(state.count, collectionCount(cn));
          assertEqual(state.checksum, collectionChecksum(cn));
        },
        true
      );

      connectToMaster();
      var c = db._collection(cn);
      let docs = [];
      //  insert some documents 'before'
      for (let i = 0; i < 110000; ++i) {
        docs.push({ _key: 'a' + i });
        if (docs.length === 5000) {
          c.insert(docs);
          docs = [];
        }
      }

      //  insert some documents 'after'
      for (let i = 0; i < 110000; ++i) {
        docs.push({ _key: 'z' + i });
        if (docs.length === 5000) {
          c.insert(docs);
          docs = [];
        }
      }

      //  update the state
      st.checksum = collectionChecksum(cn);
      st.count = collectionCount(cn);
      assertEqual(225000, collectionCount(cn));

      connectToSlave();

      //  and sync again
      replication.syncCollection(cn, {
        endpoint: masterEndpoint,
        verbose: true,
        incremental: true
      });

      assertEqual(st.count, collectionCount(cn));
      assertEqual(st.checksum, collectionChecksum(cn));
    },

    testInsertHugeIncrementalMoreOnSlave: function () {
      connectToMaster();

      var st;

      compare(
        function (state) {
          let c = db._create(cn);
          let docs = [];

          for (let i = 0; i < 5000; ++i) {
            docs.push({ _key: 'test' + i });
          }
          c.insert(docs);

          state.checksum = collectionChecksum(cn);
          state.count = collectionCount(cn);
          assertEqual(5000, state.count);

          st = _.clone(state); // save state
        },
        function (state) {
        },
        function (state) {
          assertEqual(state.count, collectionCount(cn));
          assertEqual(state.checksum, collectionChecksum(cn));
        },
        true
      );

      connectToSlave();

      var c = db._collection(cn);
      let docs = [];
      //  insert some documents 'before'
      for (let i = 0; i < 110000; ++i) {
        docs.push({ _key: 'a' + i });
        if (docs.length === 5000) {
          c.insert(docs);
          docs = [];
        }
      }

      //  insert some documents 'after'
      for (let i = 0; i < 110000; ++i) {
        docs.push({ _key: 'z' + i });
        if (docs.length === 5000) {
          c.insert(docs);
          docs = [];
        }
      }

      assertEqual(225000, collectionCount(cn));

      //  and sync again
      replication.syncCollection(cn, {
        endpoint: masterEndpoint,
        verbose: true,
        incremental: true
      });

      assertEqual(st.count, collectionCount(cn));
      assertEqual(st.checksum, collectionChecksum(cn));
    },

    // //////////////////////////////////////////////////////////////////////////////
    // / @brief test existing collection
    // //////////////////////////////////////////////////////////////////////////////

    testInsertHugeIncrementalLessOnMaster: function () {
      connectToMaster();

      var st;

      compare(
        function (state) {
          let c = db._create(cn);
          let docs = [];
          //  insert some documents 'before'
          for (let i = 0; i < 110000; ++i) {
            docs.push({ _key: 'a' + i });
            if (docs.length === 5000) {
              c.insert(docs);
              docs = [];
            }
          }

          //  insert some documents 'after'
          for (let i = 0; i < 110000; ++i) {
            docs.push({ _key: 'z' + i });
            if (docs.length === 5000) {
              c.insert(docs);
              docs = [];
            }
          }

          state.checksum = collectionChecksum(cn);
          state.count = collectionCount(cn);
          assertEqual(220000, state.count);

          st = _.clone(state); // save state
        },
        function (state) {
        },
        function (state) {
          assertEqual(state.count, collectionCount(cn));
          assertEqual(state.checksum, collectionChecksum(cn));
        },
        true
      );

      connectToMaster();
      var c = db._collection(cn);
      let docs = [];
      //  remove some documents from the 'front'
      for (let i = 0; i < 50000; ++i) {
        docs.push({ _key: 'a' + i });
        if (docs.length === 5000) {
          c.remove(docs);
          docs = [];
        }
      }

      //  remove some documents from the 'back'
      for (let i = 0; i < 50000; ++i) {
        docs.push({ _key: 'z' + i });
        if (docs.length === 5000) {
          c.remove(docs);
          docs = [];
        }
      }

      //  update the state
      st.checksum = collectionChecksum(cn);
      st.count = collectionCount(cn);
      assertEqual(120000, collectionCount(cn));

      connectToSlave();

      //  and sync again
      replication.syncCollection(cn, {
        endpoint: masterEndpoint,
        verbose: true,
        incremental: true
      });

      assertEqual(st.count, collectionCount(cn));
      assertEqual(st.checksum, collectionChecksum(cn));
    },

    // //////////////////////////////////////////////////////////////////////////////
    // / @brief test collection properties
    // //////////////////////////////////////////////////////////////////////////////

    testProperties: function () {
      connectToMaster();

      compare(
        function (state) {
          var c = db._create(cn);

          c.properties({
            indexBuckets: 32,
            waitForSync: true,
            journalSize: 16 * 1024 * 1024
          });
        },
        function (state) {
          //  don't create the collection on the slave
        },
        function (state) {
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

    // //////////////////////////////////////////////////////////////////////////////
    // / @brief test collection properties
    // //////////////////////////////////////////////////////////////////////////////

    testPropertiesOther: function () {
      connectToMaster();

      compare(
        function (state) {
          var c = db._create(cn);

          c.properties({
            indexBuckets: 32,
            waitForSync: true,
            journalSize: 16 * 1024 * 1024
          });
        },
        function (state) {
          //  create the collection on the slave, but with default properties
          db._create(cn);
        },
        function (state) {
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

    // //////////////////////////////////////////////////////////////////////////////
    // / @brief test with indexes
    // //////////////////////////////////////////////////////////////////////////////

    testCreateIndexes: function () {
      connectToMaster();

      compare(
        function (state) {
          let c = db._create(cn);
          let docs = [];

          for (let i = 0; i < 5000; ++i) {
            docs.push({
              _key: 'test' + i,
              'value1': i,
              'value2': 'test' + i
            });
          }
          c.insert(docs);

          c.ensureIndex({
            type: 'hash',
            fields: ['value1', 'value2']
          });
          c.ensureIndex({
            type: 'skiplist',
            fields: ['value1']
          });

          state.checksum = collectionChecksum(cn);
          state.count = collectionCount(cn);
          assertEqual(5000, state.count);
        },
        function (state) {
          //  already create the collection on the slave
          replication.syncCollection(cn, {
            endpoint: masterEndpoint,
            incremental: false
          });
          let c = db._collection(cn);
          c.truncate(); // but empty it

          let docs = [];

          //  create it with different values
          for (let i = 0; i < 5000; ++i) {
            docs.push({
              _key: 'test' + i,
              'value1': i,
              'value2': 'test' + i
            });
          }
          c.insert(docs);
        },
        function (state) {
          assertEqual(state.count, collectionCount(cn));
          assertEqual(state.checksum, collectionChecksum(cn));

          var idx = db._collection(cn).getIndexes();
          assertEqual(3, idx.length); // primary + hash + skiplist
          for (var i = 1; i < idx.length; ++i) {
            assertFalse(idx[i].unique);
            assertFalse(idx[i].sparse);

            if (idx[i].type === 'hash') {
              assertEqual('hash', idx[i].type);
              assertEqual(['value1', 'value2'], idx[i].fields);
            } else {
              assertEqual('skiplist', idx[i].type);
              assertEqual(['value1'], idx[i].fields);
            }
          }
        },
        true
      );
    },

    // //////////////////////////////////////////////////////////////////////////////
    // / @brief test with views
    // //////////////////////////////////////////////////////////////////////////////

    testSyncView: function () {
      connectToMaster();

      //  create view & collection on master
      db._flushCache();
      db._create(cn);
      db._createView(cn + 'View', 'arangosearch');

      db._flushCache();
      connectToSlave();
      internal.wait(0.1, false);
      //  sync on slave
      replication.sync({ endpoint: masterEndpoint });

      db._flushCache();
      {
        //  check state is the same
        let view = db._view(cn + 'View');
        assertTrue(view !== null);
        let props = view.properties();
        assertTrue(props.hasOwnProperty('links'));
        assertEqual(Object.keys(props.links).length, 0);
      }

      connectToMaster();

      //  update view properties
      {
        let view = db._view(cn + 'View');
        let links = {};
        links[cn] = {
          includeAllFields: true,
          fields: {
            text: { analyzers: ['text_en'] }
          }
        };
        view.properties({ links });
      }

      db._flushCache();
      connectToSlave();

      replication.sync({ endpoint: masterEndpoint });

      {
        let view = db._view(cn + 'View');
        assertTrue(view !== null);
        let props = view.properties();
        assertTrue(props.hasOwnProperty('links'));
        assertEqual(Object.keys(props.links).length, 1);
        assertTrue(props.links.hasOwnProperty(cn));
      }
    },

    // //////////////////////////////////////////////////////////////////////////////
    // / @brief test with edges
    // //////////////////////////////////////////////////////////////////////////////

    testEdges: function () {
      connectToMaster();

      compare(
        function (state) {
          var c = db._createEdgeCollection(cn);
          var i;

          for (i = 0; i < 100; ++i) {
            c.save(cn + '/test' + i, cn + '/test' + (i % 10), {
              _key: 'test' + i,
              'value1': i,
              'value2': 'test' + i
            });
          }

          state.checksum = collectionChecksum(cn);
          state.count = collectionCount(cn);
          assertEqual(100, state.count);
        },
        function (state) {
          //  already create the collection on the slave
          replication.syncCollection(cn, {
            endpoint: masterEndpoint,
            incremental: false
          });
          var c = db._collection(cn);
          c.truncate(); // but empty it

          for (var i = 0; i < 100; ++i) {
            c.save(cn + '/test' + i, cn + '/test' + (i % 10), {
              _key: 'test' + i,
              'value1': i,
              'value2': 'test' + i
            });
          }
        },
        function (state) {
          assertEqual(state.count, collectionCount(cn));
          assertEqual(state.checksum, collectionChecksum(cn));

          var c = db._collection(cn);
          assertEqual(3, c.type());

          for (var i = 0; i < 100; ++i) {
            var doc = c.document('test' + i);
            assertEqual(cn + '/test' + i, doc._from);
            assertEqual(cn + '/test' + (i % 10), doc._to);
            assertEqual(i, doc.value1);
            assertEqual('test' + i, doc.value2);
          }
        },
        true
      );
    },

    // //////////////////////////////////////////////////////////////////////////////
    // / @brief test with edges differences
    // //////////////////////////////////////////////////////////////////////////////

    testEdgesDifferences: function () {
      connectToMaster();

      compare(
        function (state) {
          var c = db._createEdgeCollection(cn);
          var i;

          for (i = 0; i < 100; ++i) {
            c.save(cn + '/test' + i, cn + '/test' + (i % 10), {
              _key: 'test' + i,
              'value1': i,
              'value2': 'test' + i
            });
          }

          state.checksum = collectionChecksum(cn);
          state.count = collectionCount(cn);
          assertEqual(100, state.count);
        },
        function (state) {
          //  already create the collection on the slave
          replication.syncCollection(cn, {
            endpoint: masterEndpoint,
            incremental: false
          });
          var c = db._collection(cn);
          c.truncate(); // but empty it

          for (var i = 0; i < 200; ++i) {
            c.save(
              cn + '/test' + (i + 1),
              cn + '/test' + (i % 11), {
                _key: 'test' + i,
                'value1': i,
                'value2': 'test' + i
              }
            );
          }
        },
        function (state) {
          assertEqual(state.count, collectionCount(cn));
          assertEqual(state.checksum, collectionChecksum(cn));

          var c = db._collection(cn);
          assertEqual(3, c.type());

          for (var i = 0; i < 100; ++i) {
            var doc = c.document('test' + i);
            assertEqual(cn + '/test' + i, doc._from);
            assertEqual(cn + '/test' + (i % 10), doc._to);
            assertEqual(i, doc.value1);
            assertEqual('test' + i, doc.value2);
          }
        },
        true
      );
    },

    // //////////////////////////////////////////////////////////////////////////////
    // / @brief test non-present collection
    // //////////////////////////////////////////////////////////////////////////////

    testNonPresent: function () {
      connectToMaster();

      compare(
        function (state) {
          let c = db._create(cn);
          let docs = [];

          for (let i = 0; i < 5000; ++i) {
            docs.push({ value: i });
          }
          c.insert(docs);

          state.checksum = collectionChecksum(cn);
          state.count = collectionCount(cn);
          assertEqual(5000, state.count);
        },
        function (state) { },
        function (state) {
          assertEqual(state.count, collectionCount(cn));
          assertEqual(state.checksum, collectionChecksum(cn));
        },
        false
      );
    },

    // //////////////////////////////////////////////////////////////////////////////
    // / @brief test non-present collection
    // //////////////////////////////////////////////////////////////////////////////

    testNonPresentIncremental: function () {
      connectToMaster();

      compare(
        function (state) {
          let c = db._create(cn);
          let docs = [];

          for (let i = 0; i < 5000; ++i) {
            docs.push({ value: i });
          }
          c.insert(docs);

          state.checksum = collectionChecksum(cn);
          state.count = collectionCount(cn);
          assertEqual(5000, state.count);
        },
        function (state) { },
        function (state) {
          assertEqual(state.count, collectionCount(cn));
          assertEqual(state.checksum, collectionChecksum(cn));
        },
        true
      );
    },

    // //////////////////////////////////////////////////////////////////////////////
    // / @brief test existing collection
    // //////////////////////////////////////////////////////////////////////////////

    testExisting: function () {
      connectToMaster();

      compare(
        function (state) {
          let c = db._create(cn);
          let docs = [];

          for (let i = 0; i < 5000; ++i) {
            docs.push({ value: i });
          }
          c.insert(docs);

          state.checksum = collectionChecksum(cn);
          state.count = collectionCount(cn);
          assertEqual(5000, state.count);
        },
        function (state) {
          //  already create the collection on the slave
          replication.syncCollection(cn, {
            endpoint: masterEndpoint,
            incremental: false
          });
          var c = db._collection(cn);
          c.truncate(); // but empty it
        },
        function (state) {
          assertEqual(state.count, collectionCount(cn));
          assertEqual(state.checksum, collectionChecksum(cn));
        },
        false
      );
    },

    // //////////////////////////////////////////////////////////////////////////////
    // / @brief test existing collection
    // //////////////////////////////////////////////////////////////////////////////

    testExistingIncremental: function () {
      connectToMaster();

      compare(
        function (state) {
          let c = db._create(cn);
          let docs = [];

          for (let i = 0; i < 5000; ++i) {
            docs.push({ value: i });
          }
          c.insert(docs);

          state.checksum = collectionChecksum(cn);
          state.count = collectionCount(cn);
          assertEqual(5000, state.count);
        },
        function (state) {
          //  already create the collection on the slave
          replication.syncCollection(cn, {
            endpoint: masterEndpoint,
            incremental: false
          });
          var c = db._collection(cn);
          c.truncate(); // but empty it
        },
        function (state) {
          assertEqual(state.count, collectionCount(cn));
          assertEqual(state.checksum, collectionChecksum(cn));
        },
        true
      );
    },

    // //////////////////////////////////////////////////////////////////////////////
    // / @brief test with existing documents - but empty on master
    // //////////////////////////////////////////////////////////////////////////////

    testExistingEmptyOnMaster: function () {
      connectToMaster();

      compare(
        function (state) {
          db._create(cn);

          state.checksum = collectionChecksum(cn);
          state.count = collectionCount(cn);
          assertEqual(0, state.count);
        },
        function (state) {
          //  already create the collection on the slave
          replication.syncCollection(cn, {
            endpoint: masterEndpoint,
            incremental: false
          });
          let c = db._collection(cn);
          c.truncate(); // but empty it

          let docs = [];

          for (let i = 0; i < 5000; ++i) {
            docs.push({ value: i });
          }
          c.insert(docs);
        },
        function (state) {
          assertEqual(state.count, collectionCount(cn));
          assertEqual(state.checksum, collectionChecksum(cn));
        },
        false
      );
    },

    // //////////////////////////////////////////////////////////////////////////////
    // / @brief test with existing documents - but empty on master
    // //////////////////////////////////////////////////////////////////////////////

    testExistingEmptyOnMasterIncremental: function () {
      connectToMaster();

      compare(
        function (state) {
          db._create(cn);

          state.checksum = collectionChecksum(cn);
          state.count = collectionCount(cn);
          assertEqual(0, state.count);
        },
        function (state) {
          //  already create the collection on the slave
          replication.syncCollection(cn, {
            endpoint: masterEndpoint,
            incremental: false
          });
          let c = db._collection(cn);
          c.truncate(); // but empty it

          let docs = [];

          for (let i = 0; i < 5000; ++i) {
            docs.push({ value: i });
          }
          c.insert(docs);
        },
        function (state) {
          assertEqual(state.count, collectionCount(cn));
          assertEqual(state.checksum, collectionChecksum(cn));
        },
        true
      );
    },

    // //////////////////////////////////////////////////////////////////////////////
    // / @brief test with existing documents - less on the slave
    // //////////////////////////////////////////////////////////////////////////////

    testExistingDocumentsLess: function () {
      connectToMaster();

      compare(
        function (state) {
          let c = db._create(cn);
          let docs = [];

          for (let i = 0; i < 5000; ++i) {
            docs.push({ value: i });
          }
          c.insert(docs);

          state.checksum = collectionChecksum(cn);
          state.count = collectionCount(cn);
          assertEqual(5000, state.count);
        },
        function (state) {
          //  already create the collection on the slave
          replication.syncCollection(cn, {
            endpoint: masterEndpoint,
            incremental: false
          });
          var c = db._collection(cn);
          c.truncate(); // but empty it

          for (var i = 0; i < 500; ++i) {
            c.save({
              'value': i
            });
          }
        },
        function (state) {
          assertEqual(state.count, collectionCount(cn));
          assertEqual(state.checksum, collectionChecksum(cn));
        },
        false
      );
    },

    // //////////////////////////////////////////////////////////////////////////////
    // / @brief test with existing documents - less on the slave
    // //////////////////////////////////////////////////////////////////////////////

    testExistingDocumentsLessIncremental: function () {
      connectToMaster();

      compare(
        function (state) {
          let c = db._create(cn);
          let docs = [];

          for (let i = 0; i < 5000; ++i) {
            docs.push({ value: i });
          }
          c.insert(docs);

          state.checksum = collectionChecksum(cn);
          state.count = collectionCount(cn);
          assertEqual(5000, state.count);
        },
        function (state) {
          //  already create the collection on the slave
          replication.syncCollection(cn, {
            endpoint: masterEndpoint,
            incremental: false
          });
          var c = db._collection(cn);
          c.truncate(); // but empty it

          for (var i = 0; i < 500; ++i) {
            c.save({
              'value': i
            });
          }
        },
        function (state) {
          assertEqual(state.count, collectionCount(cn));
          assertEqual(state.checksum, collectionChecksum(cn));
        },
        true
      );
    },

    // //////////////////////////////////////////////////////////////////////////////
    // / @brief test with existing documents - more on the slave
    // //////////////////////////////////////////////////////////////////////////////

    testMoreDocuments: function () {
      connectToMaster();

      compare(
        function (state) {
          let c = db._create(cn);
          let docs = [];

          for (let i = 0; i < 5000; ++i) {
            docs.push({ value: i });
          }
          c.insert(docs);

          state.checksum = collectionChecksum(cn);
          state.count = collectionCount(cn);
          assertEqual(5000, state.count);
        },
        function (state) {
          //  already create the collection on the slave
          replication.syncCollection(cn, {
            endpoint: masterEndpoint,
            incremental: false
          });
          let c = db._collection(cn);
          c.truncate(); // but empty it

          let docs = [];
          for (let i = 0; i < 5000; ++i) {
            docs.push({ value: i });
          }
          c.insert(docs);
        },
        function (state) {
          assertEqual(state.count, collectionCount(cn));
          assertEqual(state.checksum, collectionChecksum(cn));
        },
        false
      );
    },

    // //////////////////////////////////////////////////////////////////////////////
    // / @brief test with existing documents - more on the slave
    // //////////////////////////////////////////////////////////////////////////////

    testMoreDocumentsIncremental: function () {
      connectToMaster();

      compare(
        function (state) {
          let c = db._create(cn);
          let docs = [];

          for (let i = 0; i < 5000; ++i) {
            docs.push({ value: i });
          }
          c.insert(docs);

          state.checksum = collectionChecksum(cn);
          state.count = collectionCount(cn);
          assertEqual(5000, state.count);
        },
        function (state) {
          //  already create the collection on the slave
          replication.syncCollection(cn, {
            endpoint: masterEndpoint,
            incremental: false
          });
          let c = db._collection(cn);
          c.truncate(); // but empty it

          let docs = [];
          for (let i = 0; i < 6000; ++i) {
            docs.push({ value: i });
          }
          c.insert(docs);
        },
        function (state) {
          assertEqual(state.count, collectionCount(cn));
          assertEqual(state.checksum, collectionChecksum(cn));
        },
        true
      );
    },

    // //////////////////////////////////////////////////////////////////////////////
    // / @brief test with existing documents - same on the slave
    // //////////////////////////////////////////////////////////////////////////////

    testSameDocuments: function () {
      connectToMaster();

      compare(
        function (state) {
          let c = db._create(cn);
          let docs = [];

          for (let i = 0; i < 5000; ++i) {
            docs.push({
              _key: 'test' + i,
              value: i
            });
          }
          c.insert(docs);

          state.checksum = collectionChecksum(cn);
          state.count = collectionCount(cn);
          assertEqual(5000, state.count);
        },
        function (state) {
          //  already create the collection on the slave
          replication.syncCollection(cn, {
            endpoint: masterEndpoint,
            incremental: false
          });
          let c = db._collection(cn);
          c.truncate(); // but empty it

          let docs = [];

          for (let i = 0; i < 5000; ++i) {
            docs.push({
              _key: 'test' + i,
              value: i
            });
          }
          c.insert(docs);
        },
        function (state) {
          assertEqual(state.count, collectionCount(cn));
          assertEqual(state.checksum, collectionChecksum(cn));
        },
        false
      );
    },

    // //////////////////////////////////////////////////////////////////////////////
    // / @brief test with existing documents - same on the slave
    // //////////////////////////////////////////////////////////////////////////////

    testSameDocumentsIncremental: function () {
      connectToMaster();

      compare(
        function (state) {
          let c = db._create(cn);
          let docs = [];

          for (let i = 0; i < 5000; ++i) {
            docs.push({
              _key: 'test' + i,
              value: i
            });
          }
          c.insert(docs);

          state.checksum = collectionChecksum(cn);
          state.count = collectionCount(cn);
          assertEqual(5000, state.count);
        },
        function (state) {
          //  already create the collection on the slave
          replication.syncCollection(cn, {
            endpoint: masterEndpoint,
            incremental: false
          });
          let c = db._collection(cn);
          c.truncate(); // but empty it

          let docs = [];

          for (let i = 0; i < 5000; ++i) {
            docs.push({
              _key: 'test' + i,
              value: i
            });
          }
          c.insert(docs);
        },
        function (state) {
          assertEqual(state.count, collectionCount(cn));
          assertEqual(state.checksum, collectionChecksum(cn));
        },
        true
      );
    },
    
    // //////////////////////////////////////////////////////////////////////////////
    // / @brief test with existing documents - same on the slave but different keys
    // //////////////////////////////////////////////////////////////////////////////

    testSameSameButDifferentKeys: function () {
      connectToMaster();

      compare(
        function (state) {
          let c = db._create(cn);
          let docs = [];

          for (let i = 0; i < 5000; ++i) {
            docs.push({
              value: i
            });
          }
          c.insert(docs);

          state.checksum = collectionChecksum(cn);
          state.count = collectionCount(cn);
          assertEqual(5000, state.count);
        },
        function (state) {
          //  already create the collection on the slave
          replication.syncCollection(cn, {
            endpoint: masterEndpoint,
            incremental: false
          });
          let c = db._collection(cn);
          c.truncate(); // but empty it

          let docs = [];

          for (let i = 0; i < 5000; ++i) {
            docs.push({
              value: i
            });
          }
          c.insert(docs);
        },
        function (state) {
          assertEqual(state.count, collectionCount(cn));
          assertEqual(state.checksum, collectionChecksum(cn));
        },
        false
      );
    },

    // //////////////////////////////////////////////////////////////////////////////
    // / @brief test with existing documents - same on the slave but different keys
    // //////////////////////////////////////////////////////////////////////////////

    testSameSameButDifferentKeysIncremental: function () {
      connectToMaster();

      compare(
        function (state) {
          let c = db._create(cn);
          let docs = [];

          for (let i = 0; i < 5000; ++i) {
            docs.push({
              value: i
            });
          }
          c.insert(docs);

          state.checksum = collectionChecksum(cn);
          state.count = collectionCount(cn);
          assertEqual(5000, state.count);
        },
        function (state) {
          //  already create the collection on the slave
          replication.syncCollection(cn, {
            endpoint: masterEndpoint,
            incremental: false
          });
          let c = db._collection(cn);
          c.truncate(); // but empty it

          let docs = [];

          for (let i = 0; i < 5000; ++i) {
            docs.push({
              value: i
            });
          }
          c.insert(docs);
        },
        function (state) {
          assertEqual(state.count, collectionCount(cn));
          assertEqual(state.checksum, collectionChecksum(cn));
        },
        true
      );
    },

    // //////////////////////////////////////////////////////////////////////////////
    // / @brief test with existing documents - same on the slave but different values
    // //////////////////////////////////////////////////////////////////////////////

    testSameSameButDifferentValues: function () {
      connectToMaster();

      compare(
        function (state) {
          let c = db._create(cn);
          let docs = [];

          for (let i = 0; i < 5000; ++i) {
            docs.push({
              _key: 'test' + i,
              value1: i,
              value2: 'test' + i
            });
          }
          c.insert(docs);

          state.checksum = collectionChecksum(cn);
          state.count = collectionCount(cn);
          assertEqual(5000, state.count);
        },
        function (state) {
          //  already create the collection on the slave
          replication.syncCollection(cn, {
            endpoint: masterEndpoint,
            incremental: false
          });
          let c = db._collection(cn);
          c.truncate(); // but empty it

          let docs = [];
          for (let i = 0; i < 5000; ++i) {
            docs.push({
              _key: 'test' + i,
              value1: 'test' + i,
              value2: i
            });
          }
          c.insert(docs);
        },
        function (state) {
          assertEqual(state.count, collectionCount(cn));
          assertEqual(state.checksum, collectionChecksum(cn));
        },
        false
      );
    },

    // //////////////////////////////////////////////////////////////////////////////
    // / @brief test with existing documents - same on the slave but different values
    // //////////////////////////////////////////////////////////////////////////////

    testSameSameButDifferentValuesIncremental: function () {
      connectToMaster();

      compare(
        function (state) {
          let c = db._create(cn);
          let docs = [];

          for (let i = 0; i < 5000; ++i) {
            docs.push({
              _key: 'test' + i,
              value1: i,
              value2: 'test' + i
            });
          }
          c.insert(docs);

          state.checksum = collectionChecksum(cn);
          state.count = collectionCount(cn);
          assertEqual(5000, state.count);
        },
        function (state) {
          //  already create the collection on the slave
          replication.syncCollection(cn, {
            endpoint: masterEndpoint,
            incremental: false
          });
          let c = db._collection(cn);
          c.truncate(); // but empty it

          let docs = [];
          for (let i = 0; i < 5000; ++i) {
            docs.push({
              _key: 'test' + i,
              value1: 'test' + i,
              value2: i
            });
          }
          c.insert(docs);
        },
        function (state) {
          assertEqual(state.count, collectionCount(cn));
          assertEqual(state.checksum, collectionChecksum(cn));
        },
        true
      );
    }
  
  };
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief test suite on _system
// //////////////////////////////////////////////////////////////////////////////

function ReplicationSuite () {
  'use strict';

  let suite = {
    // //////////////////////////////////////////////////////////////////////////////
    // / @brief set up
    // //////////////////////////////////////////////////////////////////////////////

    setUp: function () {
      connectToMaster();
      try {
        db._dropView(cn + 'View');
      } catch (ignored) {}
      db._drop(cn);
    },

    // //////////////////////////////////////////////////////////////////////////////
    // / @brief tear down
    // //////////////////////////////////////////////////////////////////////////////

    tearDown: function () {
      connectToMaster();
      try {
        db._dropView(cn + 'View');
      } catch (ignored) {}
      db._drop(cn);

      connectToSlave();
      db._drop(cn);
    }
  };
  deriveTestSuite(BaseTestConfig(), suite, '_Repl');

  return suite;
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief test suite on other database
// //////////////////////////////////////////////////////////////////////////////

function ReplicationOtherDBSuite () {
  'use strict';

  const dbName = 'UnitTestDB';

  let suite = {

    // //////////////////////////////////////////////////////////////////////////////
    // / @brief set up
    // //////////////////////////////////////////////////////////////////////////////

    setUp: function () {
      connectToMaster();
      try {
        db._dropDatabase(dbName);
      } catch (e) {
      }
      db._createDatabase(dbName);
      connectToSlave();
      try {
        db._dropDatabase(dbName);
      } catch (e) {
      }
      db._createDatabase(dbName);
      db._useDatabase(dbName);
      connectToMaster();
    },

    // //////////////////////////////////////////////////////////////////////////////
    // / @brief tear down
    // //////////////////////////////////////////////////////////////////////////////

    tearDown: function () {
      db._useDatabase('_system');
      connectToMaster();
      try {
        db._dropDatabase(dbName);
      } catch (e) {
      }

      connectToSlave();
      try {
        db._dropDatabase(dbName);
      } catch (e) {
      }
    }
  };

  deriveTestSuite(BaseTestConfig(), suite, '_OtherRepl');
  return suite;
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief test suite for incremental
// //////////////////////////////////////////////////////////////////////////////

function ReplicationIncrementalKeyConflict () {
  'use strict';

  return {

    // //////////////////////////////////////////////////////////////////////////////
    // / @brief set up
    // //////////////////////////////////////////////////////////////////////////////

    setUp: function () {
      connectToMaster();
      db._drop(cn);
    },

    // //////////////////////////////////////////////////////////////////////////////
    // / @brief tear down
    // //////////////////////////////////////////////////////////////////////////////

    tearDown: function () {
      connectToMaster();
      db._drop(cn);

      connectToSlave();
      db._drop(cn);
    },

    testKeyConflicts: function () {
      var c = db._create(cn);
      c.ensureIndex({
        type: 'hash',
        fields: ['value'],
        unique: true
      });

      c.insert({
        _key: 'x',
        value: 1
      });
      c.insert({
        _key: 'y',
        value: 2
      });
      c.insert({
        _key: 'z',
        value: 3
      });

      connectToSlave();
      replication.syncCollection(cn, {
        endpoint: masterEndpoint,
        verbose: true
      });
      db._flushCache();
      c = db._collection(cn);

      assertEqual(3, c.count());
      assertEqual(1, c.document('x').value);
      assertEqual(2, c.document('y').value);
      assertEqual(3, c.document('z').value);

      assertEqual('hash', c.getIndexes()[1].type);
      assertTrue(c.getIndexes()[1].unique);

      connectToMaster();
      db._flushCache();
      c = db._collection(cn);
      c.remove('z');
      c.insert({
        _key: 'w',
        value: 3
      });

      assertEqual(3, c.count());
      assertEqual(3, c.document('w').value);
      assertEqual(1, c.document('x').value);
      assertEqual(2, c.document('y').value);

      connectToSlave();
      replication.syncCollection(cn, {
        endpoint: masterEndpoint,
        verbose: true,
        incremental: true
      });

      db._flushCache();

      c = db._collection(cn);
      assertEqual(3, c.count());
      assertEqual(3, c.document('w').value);
      assertEqual(1, c.document('x').value);
      assertEqual(2, c.document('y').value);

      assertEqual('hash', c.getIndexes()[1].type);
      assertTrue(c.getIndexes()[1].unique);
    },

    testKeyConflictsManyDocuments: function () {
      var c = db._create(cn);
      var i;
      c.ensureIndex({
        type: 'hash',
        fields: ['value'],
        unique: true
      });

      for (i = 0; i < 10000; ++i) {
        c.insert({
          _key: 'test' + i,
          value: i
        });
      }

      connectToSlave();
      replication.syncCollection(cn, {
        endpoint: masterEndpoint,
        verbose: true
      });
      db._flushCache();
      c = db._collection(cn);

      assertEqual(10000, c.count());

      assertEqual('hash', c.getIndexes()[1].type);
      assertTrue(c.getIndexes()[1].unique);

      connectToMaster();
      db._flushCache();
      c = db._collection(cn);

      c.remove('test0');
      c.remove('test1');
      c.remove('test9998');
      c.remove('test9999');

      c.insert({
        _key: 'test0',
        value: 9999
      });
      c.insert({
        _key: 'test1',
        value: 9998
      });
      c.insert({
        _key: 'test9998',
        value: 1
      });
      c.insert({
        _key: 'test9999',
        value: 0
      });

      assertEqual(10000, c.count());

      connectToSlave();
      replication.syncCollection(cn, {
        endpoint: masterEndpoint,
        verbose: true,
        incremental: true
      });

      db._flushCache();

      c = db._collection(cn);
      assertEqual(10000, c.count());

      assertEqual('hash', c.getIndexes()[1].type);
      assertTrue(c.getIndexes()[1].unique);
    }
  };
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief executes the test suite
// //////////////////////////////////////////////////////////////////////////////

jsunity.run(ReplicationSuite);
jsunity.run(ReplicationOtherDBSuite);
jsunity.run(ReplicationIncrementalKeyConflict);

return jsunity.done();
