/*jshint globalstrict:false, strict:false, unused: false */
/*global assertEqual, assertTrue, assertFalse, arango, ARGUMENTS */

////////////////////////////////////////////////////////////////////////////////
/// @brief test the replication
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
/// @author Copyright 2017, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

var jsunity = require("jsunity");
var arangodb = require("@arangodb");
var db = arangodb.db;

const replication = require("@arangodb/replication");
const compareTicks = replication.compareTicks;
const console = require("console");
const internal = require("internal");
const masterEndpoint = arango.getEndpoint();
const slaveEndpoint = ARGUMENTS[0];

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
    return db._collection(name).count();
  };

  var compare = function(masterFunc, masterFunc2, slaveFuncFinal) {
    var state = {};

    assertEqual(cn, db._name());
    db._flushCache();
    masterFunc(state);
    
    connectToSlave();
    assertEqual(cn, db._name());

    var syncResult = replication.sync({
      endpoint: masterEndpoint,
      username: "root",
      password: "",
      verbose: true,
      includeSystem: false,
      keepBarrier: true,
      requireFromPresent: true,
    });

    assertTrue(syncResult.hasOwnProperty('lastLogTick'));

    connectToMaster();
    masterFunc2(state);

    // use lastLogTick as of now
    state.lastLogTick = replication.logger.state().state.lastLogTick;

    let applierConfiguration = {
      endpoint: masterEndpoint,
      username: "root",
      password: "" 
    };

    connectToSlave();
    assertEqual(cn, db._name());

    replication.applier.properties(applierConfiguration);
    replication.applier.start(syncResult.lastLogTick, syncResult.barrierId);

    var printed = false;

    while (true) {
      var slaveState = replication.applier.state();

      if (slaveState.state.lastError.errorNum > 0) {
        console.topic("replication=error", "slave has errored:", JSON.stringify(slaveState.state.lastError));
        break;
      }

      if (!slaveState.state.running) {
        console.topic("replication=error", "slave is not running");
        break;
      }

      if (compareTicks(slaveState.state.lastAppliedContinuousTick, state.lastLogTick) >= 0 ||
          compareTicks(slaveState.state.lastProcessedContinuousTick, state.lastLogTick) >= 0) { // ||
        console.topic("replication=debug", "slave has caught up. state.lastLogTick:", state.lastLogTick, "slaveState.lastAppliedContinuousTick:", slaveState.state.lastAppliedContinuousTick, "slaveState.lastProcessedContinuousTick:", slaveState.state.lastProcessedContinuousTick);
        break;
      }
        
      if (!printed) {
        console.topic("replication=debug", "waiting for slave to catch up");
        printed = true;
      }
      internal.wait(0.5, false);
    }

    db._flushCache();
    slaveFuncFinal(state);
  };

  return {

    setUp: function() {
      db._useDatabase("_system");
      connectToMaster();
      try {
        db._dropDatabase(cn);
      } catch (err) {}

      db._createDatabase(cn);
      db._useDatabase(cn);

      db._useDatabase("_system");
      connectToSlave();
      
      try {
        db._dropDatabase(cn);
      } catch (err) {}

      db._createDatabase(cn);
    },

    tearDown: function() {
      db._useDatabase("_system");
      connectToMaster();

      db._useDatabase(cn);
      connectToSlave();
      replication.applier.stop();
      replication.applier.forget();
      
      db._useDatabase("_system");
      db._dropDatabase(cn);
    },
    
    testAqlInsert: function() {
      db._useDatabase(cn);
      connectToMaster();

      compare(
        function(state) {
          db._create(cn); 
        },

        function(state) {
          for (let i = 0; i < 2000; ++i) {
            db._query("INSERT { _key: \"test" + i + "\", value1: " + i + ", value2: " + (i % 100) + " } IN " + cn);
          }

          assertEqual(2000, collectionCount(cn));
          state.checksum = collectionChecksum(cn);
          state.count = collectionCount(cn);
        },

        function(state) {
          assertEqual(state.checksum, collectionChecksum(cn));
          assertEqual(state.count, collectionCount(cn));
          
          for (let i = 0; i < 2000; ++i) {
            let docs = db._query("FOR doc IN " + cn + " FILTER doc._key == \"test" + i + "\" RETURN doc").toArray(); 
            assertEqual(1, docs.length);
            assertEqual("test" + i, docs[0]._key);
            assertEqual(i, docs[0].value1);
            assertEqual(i % 100, docs[0].value2);
          }
        }
      );
    },
    
    testAqlRemove: function() {
      db._useDatabase(cn);
      connectToMaster();

      compare(
        function(state) {
          let c = db._create(cn); 
          for (let i = 0; i < 100; ++i) {
            c.insert({ _key: "test" + i, value1: i, value2: (i % 100) });
          }
        },

        function(state) {
          for (let i = 0; i < 100; ++i) {
            db._query("FOR doc IN " + cn + " FILTER doc.value1 == " + i + " REMOVE doc IN " + cn);
          }

          assertEqual(0, collectionCount(cn));
          state.checksum = collectionChecksum(cn);
          state.count = collectionCount(cn);
        },

        function(state) {
          assertEqual(0, collectionCount(cn));
          assertEqual(state.checksum, collectionChecksum(cn));
          assertEqual(state.count, collectionCount(cn));
          
          for (let i = 0; i < 100; ++i) {
            let docs = db._query("FOR doc IN " + cn + " FILTER doc.value1 == " + i + " RETURN doc").toArray(); 
            assertEqual(0, docs.length);
          }
        }
      );
    },
    
    testAqlRemoveMulti: function() {
      db._useDatabase(cn);
      connectToMaster();

      compare(
        function(state) {
          let c = db._create(cn); 
          for (let i = 0; i < 5000; ++i) {
            c.insert({ _key: "test" + i, value1: i, value2: (i % 100) });
          }
          c.ensureIndex({ type: "hash", fields: ["value2"] });
        },

        function(state) {
          for (let i = 0; i < 100; ++i) {
            db._query("FOR doc IN " + cn + " FILTER doc.value2 == " + i + " REMOVE doc IN " + cn);
          }
   
          assertEqual(0, collectionCount(cn));
          state.checksum = collectionChecksum(cn);
          state.count = collectionCount(cn);
        },

        function(state) {
          assertEqual(0, collectionCount(cn));
          assertEqual(state.checksum, collectionChecksum(cn));
          assertEqual(state.count, collectionCount(cn));
        }
      );
    },
    
    testAqlUpdate: function() {
      db._useDatabase(cn);
      connectToMaster();

      compare(
        function(state) {
          let c = db._create(cn); 
          for (let i = 0; i < 100; ++i) {
            c.insert({ _key: "test" + i, value1: i, value2: (i % 100) });
          }
        },

        function(state) {
          for (let i = 0; i < 100; ++i) {
            db._query("FOR doc IN " + cn + " FILTER doc.value1 == " + i + " UPDATE doc WITH { value3: doc.value1 + 1 } IN " + cn);
          }

          state.checksum = collectionChecksum(cn);
          state.count = collectionCount(cn);
        },

        function(state) {
          assertEqual(state.checksum, collectionChecksum(cn));
          assertEqual(state.count, collectionCount(cn));
          
          for (let i = 0; i < 100; ++i) {
            let docs = db._query("FOR doc IN " + cn + " FILTER doc.value1 == " + i + " RETURN doc").toArray(); 
            assertEqual(1, docs.length);
            assertEqual("test" + i, docs[0]._key);
            assertEqual(i, docs[0].value1);
            assertEqual(i % 100, docs[0].value2);
            assertEqual(i + 1, docs[0].value3);
          }
        }
      );
    },

    testAqlUpdateMulti: function() {
      db._useDatabase(cn);
      connectToMaster();

      compare(
        function(state) {
          let c = db._create(cn); 
          for (let i = 0; i < 5000; ++i) {
            c.insert({ _key: "test" + i, value1: i, value2: (i % 100) });
          }
          c.ensureIndex({ type: "hash", fields: ["value2"] });
        },

        function(state) {
          for (let i = 0; i < 100; ++i) {
            db._query("FOR doc IN " + cn + " FILTER doc.value2 == " + i + " UPDATE doc WITH { value3: doc.value1 + 1 } IN " + cn);
          }

          state.checksum = collectionChecksum(cn);
          state.count = collectionCount(cn);
        },

        function(state) {
          assertEqual(state.checksum, collectionChecksum(cn));
          assertEqual(state.count, collectionCount(cn));
        }
      );
    },

    testAqlUpdateEdge: function() {
      db._useDatabase(cn);
      connectToMaster();

      compare(
        function(state) {
          let c = db._createEdgeCollection(cn); 
          for (let i = 0; i < 100; ++i) {
            c.insert({ _key: "test" + i, _from: "test/v" + i, _to: "test/y" + i });
          }
        },

        function(state) {
          for (let i = 0; i < 100; ++i) {
            db._query("FOR doc IN " + cn + " FILTER doc._from == \"test/v" + i + "\" UPDATE doc WITH { _from: \"test/x" + i + "\" } IN " + cn);
          }

          state.checksum = collectionChecksum(cn);
          state.count = collectionCount(cn);
        },

        function(state) {
          assertEqual(state.checksum, collectionChecksum(cn));
          assertEqual(state.count, collectionCount(cn));
          
          for (let i = 0; i < 100; ++i) {
            let docs = db._query("FOR doc IN " + cn + " FILTER doc._from == \"test/v" + i + "\" RETURN doc").toArray(); 
            assertEqual(0, docs.length);
          }

          for (let i = 0; i < 100; ++i) {
            let docs = db._query("FOR doc IN " + cn + " FILTER doc._from == \"test/x" + i + "\" RETURN doc").toArray(); 
            assertEqual(1, docs.length);
            assertEqual("test" + i, docs[0]._key);
            assertEqual("test/x" + i, docs[0]._from);
            assertEqual("test/y" + i, docs[0]._to);
          }
        }
      );
    },
    
    testAqlUpdateEdgeMulti: function() {
      db._useDatabase(cn);
      connectToMaster();

      compare(
        function(state) {
          let c = db._createEdgeCollection(cn); 
          for (let i = 0; i < 1000; ++i) {
            c.insert({ _key: "test" + i, _from: "test/v" + (i % 100), _to: "test/y" + (i % 100) });
          }
        },

        function(state) {
          for (let i = 0; i < 100; ++i) {
            db._query("FOR doc IN " + cn + " FILTER doc._from == \"test/v" + i + "\" UPDATE doc WITH { _from: \"test/x" + i + "\" } IN " + cn);
          }

          state.checksum = collectionChecksum(cn);
          state.count = collectionCount(cn);
        },

        function(state) {
          assertEqual(state.checksum, collectionChecksum(cn));
          assertEqual(state.count, collectionCount(cn));
          
          for (let i = 0; i < 100; ++i) {
            let docs = db._query("FOR doc IN " + cn + " FILTER doc._from == \"test/v" + i + "\" RETURN doc").toArray(); 
            assertEqual(0, docs.length);
          }

          for (let i = 0; i < 100; ++i) {
            let docs = db._query("FOR doc IN " + cn + " FILTER doc._from == \"test/x" + i + "\" RETURN doc").toArray(); 
            assertEqual(10, docs.length);
            assertEqual("test/x" + i, docs[0]._from);
            assertEqual("test/y" + i, docs[0]._to);
          }
        }
      );
    },
    
    testAqlUpdateEdgeExtraIndex: function() {
      db._useDatabase(cn);
      connectToMaster();

      compare(
        function(state) {
          let c = db._createEdgeCollection(cn); 
          for (let i = 0; i < 100; ++i) {
            c.insert({ _key: "test" + i, _from: "test/v" + i, _to: "test/y" + i });
          }
          c.ensureIndex({ type: "hash", fields: ["_from", "_to"], unique: true });
        },

        function(state) {
          for (let i = 0; i < 100; ++i) {
            db._query("FOR doc IN " + cn + " FILTER doc._from == \"test/v" + i + "\" UPDATE doc WITH { _from: \"test/x" + i + "\" } IN " + cn);
          }

          state.checksum = collectionChecksum(cn);
          state.count = collectionCount(cn);
        },

        function(state) {
          assertEqual(state.checksum, collectionChecksum(cn));
          assertEqual(state.count, collectionCount(cn));
          
          for (let i = 0; i < 100; ++i) {
            let docs = db._query("FOR doc IN " + cn + " FILTER doc._from == \"test/v" + i + "\" RETURN doc").toArray(); 
            assertEqual(0, docs.length);
          }

          for (let i = 0; i < 100; ++i) {
            let docs = db._query("FOR doc IN " + cn + " FILTER doc._from == \"test/x" + i + "\" RETURN doc").toArray(); 
            assertEqual(1, docs.length);
            assertEqual("test" + i, docs[0]._key);
            assertEqual("test/x" + i, docs[0]._from);
            assertEqual("test/y" + i, docs[0]._to);
          }
        }
      );
    },
    
    testAqlReplace: function() {
      db._useDatabase(cn);
      connectToMaster();

      compare(
        function(state) {
          let c = db._create(cn); 
          for (let i = 0; i < 100; ++i) {
            c.insert({ _key: "test" + i, value1: i, value2: (i % 100) });
          }
        },

        function(state) {
          for (let i = 0; i < 100; ++i) {
            db._query("FOR doc IN " + cn + " FILTER doc.value1 == " + i + " REPLACE doc WITH { value3: doc.value1 + 1 } IN " + cn);
          }

          state.checksum = collectionChecksum(cn);
          state.count = collectionCount(cn);
        },

        function(state) {
          assertEqual(state.checksum, collectionChecksum(cn));
          assertEqual(state.count, collectionCount(cn));
          
          for (let i = 0; i < 100; ++i) {
            let docs = db._query("FOR doc IN " + cn + " FILTER doc._key == 'test" + i + "' RETURN doc").toArray(); 
            assertEqual(1, docs.length);
            assertEqual("test" + i, docs[0]._key);
            assertFalse(docs[0].hasOwnProperty('value1'));
            assertFalse(docs[0].hasOwnProperty('value2'));
            assertEqual(i + 1, docs[0].value3);
          }
        }
      );
    },
    
    testAqlReplaceMulti: function() {
      db._useDatabase(cn);
      connectToMaster();

      compare(
        function(state) {
          let c = db._create(cn); 
          for (let i = 0; i < 5000; ++i) {
            c.insert({ _key: "test" + i, value1: i, value2: (i % 100) });
          }
          c.ensureIndex({ type: "hash", fields: ["value2"] });
        },

        function(state) {
          for (let i = 0; i < 100; ++i) {
            db._query("FOR doc IN " + cn + " FILTER doc.value2 == " + i + " REPLACE doc WITH { value3: doc.value1 + 1 } IN " + cn);
          }

          state.checksum = collectionChecksum(cn);
          state.count = collectionCount(cn);
        },

        function(state) {
          assertEqual(state.checksum, collectionChecksum(cn));
          assertEqual(state.count, collectionCount(cn));
        }
      );
    },
    
    testAqlReplaceEdge: function() {
      db._useDatabase(cn);
      connectToMaster();

      compare(
        function(state) {
          let c = db._createEdgeCollection(cn); 
          for (let i = 0; i < 100; ++i) {
            c.insert({ _key: "test" + i, _from: "test/v" + i, _to: "test/y" + i });
          }
        },

        function(state) {
          for (let i = 0; i < 100; ++i) {
            db._query("FOR doc IN " + cn + " FILTER doc._from == \"test/v" + i + "\" REPLACE doc WITH { _from: \"test/x" + i + "\", _to: \"test/y" + i + "\" } IN " + cn);
          }

          state.checksum = collectionChecksum(cn);
          state.count = collectionCount(cn);
        },

        function(state) {
          assertEqual(state.checksum, collectionChecksum(cn));
          assertEqual(state.count, collectionCount(cn));
          
          for (let i = 0; i < 100; ++i) {
            let docs = db._query("FOR doc IN " + cn + " FILTER doc._from == \"test/v" + i + "\" RETURN doc").toArray(); 
            assertEqual(0, docs.length);
          }

          for (let i = 0; i < 100; ++i) {
            let docs = db._query("FOR doc IN " + cn + " FILTER doc._from == \"test/x" + i + "\" RETURN doc").toArray(); 
            assertEqual(1, docs.length);
            assertEqual("test" + i, docs[0]._key);
            assertEqual("test/x" + i, docs[0]._from);
            assertEqual("test/y" + i, docs[0]._to);
          }
        }
      );
    },
    
    testAqlReplaceEdgeMulti: function() {
      db._useDatabase(cn);
      connectToMaster();

      compare(
        function(state) {
          let c = db._createEdgeCollection(cn); 
          for (let i = 0; i < 1000; ++i) {
            c.insert({ _key: "test" + i, _from: "test/v" + (i % 100), _to: "test/y" + (i % 100) });
          }
        },

        function(state) {
          for (let i = 0; i < 100; ++i) {
            db._query("FOR doc IN " + cn + " FILTER doc._from == \"test/v" + i + "\" REPLACE doc WITH { _from: \"test/x" + i + "\", _to: doc._to } IN " + cn);
          }

          state.checksum = collectionChecksum(cn);
          state.count = collectionCount(cn);
        },

        function(state) {
          assertEqual(state.checksum, collectionChecksum(cn));
          assertEqual(state.count, collectionCount(cn));
          
          for (let i = 0; i < 100; ++i) {
            let docs = db._query("FOR doc IN " + cn + " FILTER doc._from == \"test/v" + i + "\" RETURN doc").toArray(); 
            assertEqual(0, docs.length);
          }

          for (let i = 0; i < 100; ++i) {
            let docs = db._query("FOR doc IN " + cn + " FILTER doc._from == \"test/x" + i + "\" RETURN doc").toArray(); 
            assertEqual(10, docs.length);
            assertEqual("test/x" + i, docs[0]._from);
            assertEqual("test/y" + i, docs[0]._to);
          }
        }
      );
    },

  };
}


////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suite
////////////////////////////////////////////////////////////////////////////////

jsunity.run(ReplicationSuite);

return jsunity.done();
