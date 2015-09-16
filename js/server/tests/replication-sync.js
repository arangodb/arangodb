/*jshint globalstrict:false, strict:false, unused: false */
/*global fail, assertEqual, assertTrue, assertFalse, assertNull, arango */

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
var arangodb = require("org/arangodb");
var errors = arangodb.errors;
var db = arangodb.db;

var replication = require("org/arangodb/replication");
var console = require("console");
var internal = require("internal");
var masterEndpoint = arango.getEndpoint();
var slaveEndpoint = masterEndpoint.replace(/:3(\d+)$/, ':4$1');

// -----------------------------------------------------------------------------
// --SECTION--                                                 replication tests
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////

function ReplicationSuite () {
  'use strict';
  var cn  = "UnitTestsReplication";

  var connectToMaster = function () {
    arango.reconnect(masterEndpoint, db._name(), "root", "");
    db._flushCache();
  };

  var connectToSlave = function () {
    arango.reconnect(slaveEndpoint, db._name(), "root", "");
    db._flushCache();
  };

  var collectionChecksum = function (name) {
    var c = db._collection(name).checksum(true, true);
    return c.checksum;
  };

  var collectionCount = function (name) {
    var res = db._collection(name).count();
    return res;
  };

  var compare = function (masterFunc, slaveInitFunc, slaveCompareFunc, incremental) {
    var state = { };

    db._flushCache();
    masterFunc(state);

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

    setUp : function () {
      connectToMaster();
      db._drop(cn);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief tear down
////////////////////////////////////////////////////////////////////////////////

    tearDown : function () {
      connectToMaster();
      db._drop(cn);

      connectToSlave();
      replication.applier.stop();
      db._drop(cn);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test non-present collection
////////////////////////////////////////////////////////////////////////////////

    testNonPresent : function () {
      connectToMaster();

      compare(
        function (state) {
          var c = db._create(cn), i;

          for (i = 0; i < 5000; ++i) {
            c.save({ "value" : i });
          }

          state.checksum = collectionChecksum(cn);
          state.count = collectionCount(cn);
          assertEqual(5000, state.count);
        },
        function (state) {
        },
        function (state) {
          assertEqual(state.count, collectionCount(cn));
          assertEqual(state.checksum, collectionChecksum(cn));
        },
        false
      );
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test non-present collection
////////////////////////////////////////////////////////////////////////////////

    testNonPresentIncremental : function () {
      connectToMaster();

      compare(
        function (state) {
          var c = db._create(cn), i;

          for (i = 0; i < 5000; ++i) {
            c.save({ "value" : i });
          }

          state.checksum = collectionChecksum(cn);
          state.count = collectionCount(cn);
          assertEqual(5000, state.count);
        },
        function (state) {
        },
        function (state) {
          assertEqual(state.count, collectionCount(cn));
          assertEqual(state.checksum, collectionChecksum(cn));
        },
        true
      );
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test existing collection
////////////////////////////////////////////////////////////////////////////////

    testExisting : function () {
      connectToMaster();

      compare(
        function (state) {
          var c = db._create(cn), i;

          for (i = 0; i < 5000; ++i) {
            c.save({ "value" : i });
          }

          state.checksum = collectionChecksum(cn);
          state.count = collectionCount(cn);
          assertEqual(5000, state.count);
        },
        function (state) {
          // already create the collection on the slave
          db._create(cn);
        },
        function (state) {
          assertEqual(state.count, collectionCount(cn));
          assertEqual(state.checksum, collectionChecksum(cn));
        },
        false
      );
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test existing collection
////////////////////////////////////////////////////////////////////////////////

    testExistingIncremental : function () {
      connectToMaster();

      compare(
        function (state) {
          var c = db._create(cn), i;

          for (i = 0; i < 5000; ++i) {
            c.save({ "value" : i });
          }

          state.checksum = collectionChecksum(cn);
          state.count = collectionCount(cn);
          assertEqual(5000, state.count);
        },
        function (state) {
          // already create the collection on the slave
          db._create(cn);
        },
        function (state) {
          assertEqual(state.count, collectionCount(cn));
          assertEqual(state.checksum, collectionChecksum(cn));
        },
        true
      );
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test with existing documents - but empty on master
////////////////////////////////////////////////////////////////////////////////

    testExistingEmptyOnMaster : function () {
      connectToMaster();

      compare(
        function (state) {
          var c = db._create(cn), i;

          state.checksum = collectionChecksum(cn);
          state.count = collectionCount(cn);
          assertEqual(0, state.count);
        },
        function (state) {
          // already create the collection on the slave
          var c = db._create(cn);
          
          for (var i = 0; i < 5000; ++i) {
            c.save({ "value" : i });
          }
        },
        function (state) {
          assertEqual(state.count, collectionCount(cn));
          assertEqual(state.checksum, collectionChecksum(cn));
        },
        false
      );
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test with existing documents - but empty on master
////////////////////////////////////////////////////////////////////////////////

    testExistingEmptyOnMasterIncremental : function () {
      connectToMaster();

      compare(
        function (state) {
          var c = db._create(cn), i;

          state.checksum = collectionChecksum(cn);
          state.count = collectionCount(cn);
          assertEqual(0, state.count);
        },
        function (state) {
          // already create the collection on the slave
          var c = db._create(cn);
          
          for (var i = 0; i < 5000; ++i) {
            c.save({ "value" : i });
          }
        },
        function (state) {
          assertEqual(state.count, collectionCount(cn));
          assertEqual(state.checksum, collectionChecksum(cn));
        },
        true
      );
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test with existing documents - less on the slave
////////////////////////////////////////////////////////////////////////////////

    testExistingDocumentsLess : function () {
      connectToMaster();

      compare(
        function (state) {
          var c = db._create(cn), i;

          for (i = 0; i < 5000; ++i) {
            c.save({ "value" : i });
          }

          state.checksum = collectionChecksum(cn);
          state.count = collectionCount(cn);
          assertEqual(5000, state.count);
        },
        function (state) {
          // already create the collection on the slave
          var c = db._create(cn);
          
          for (var i = 0; i < 500; ++i) {
            c.save({ "value" : i });
          }
        },
        function (state) {
          assertEqual(state.count, collectionCount(cn));
          assertEqual(state.checksum, collectionChecksum(cn));
        },
        false
      );
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test with existing documents - less on the slave
////////////////////////////////////////////////////////////////////////////////

    testExistingDocumentsLessIncremental : function () {
      connectToMaster();

      compare(
        function (state) {
          var c = db._create(cn), i;

          for (i = 0; i < 5000; ++i) {
            c.save({ "value" : i });
          }

          state.checksum = collectionChecksum(cn);
          state.count = collectionCount(cn);
          assertEqual(5000, state.count);
        },
        function (state) {
          // already create the collection on the slave
          var c = db._create(cn);
          
          for (var i = 0; i < 500; ++i) {
            c.save({ "value" : i });
          }
        },
        function (state) {
          assertEqual(state.count, collectionCount(cn));
          assertEqual(state.checksum, collectionChecksum(cn));
        },
        true
      );
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test with existing documents - more on the slave
////////////////////////////////////////////////////////////////////////////////

    testMoreDocuments : function () {
      connectToMaster();

      compare(
        function (state) {
          var c = db._create(cn), i;

          for (i = 0; i < 5000; ++i) {
            c.save({ "value" : i });
          }

          state.checksum = collectionChecksum(cn);
          state.count = collectionCount(cn);
          assertEqual(5000, state.count);
        },
        function (state) {
          // already create the collection on the slave
          var c = db._create(cn);
          
          for (var i = 0; i < 6000; ++i) {
            c.save({ "value" : i });
          }
        },
        function (state) {
          assertEqual(state.count, collectionCount(cn));
          assertEqual(state.checksum, collectionChecksum(cn));
        },
        false
      );
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test with existing documents - more on the slave
////////////////////////////////////////////////////////////////////////////////

    testMoreDocumentsIncremental : function () {
      connectToMaster();

      compare(
        function (state) {
          var c = db._create(cn), i;

          for (i = 0; i < 5000; ++i) {
            c.save({ "value" : i });
          }

          state.checksum = collectionChecksum(cn);
          state.count = collectionCount(cn);
          assertEqual(5000, state.count);
        },
        function (state) {
          // already create the collection on the slave
          var c = db._create(cn);
          
          for (var i = 0; i < 6000; ++i) {
            c.save({ "value" : i });
          }
        },
        function (state) {
          assertEqual(state.count, collectionCount(cn));
          assertEqual(state.checksum, collectionChecksum(cn));
        },
        true
      );
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test with existing documents - same on the slave
////////////////////////////////////////////////////////////////////////////////

    testSameDocuments : function () {
      connectToMaster();

      compare(
        function (state) {
          var c = db._create(cn), i;

          for (i = 0; i < 5000; ++i) {
            c.save({ _key: "test" + i, "value" : i });
          }

          state.checksum = collectionChecksum(cn);
          state.count = collectionCount(cn);
          assertEqual(5000, state.count);
        },
        function (state) {
          // already create the collection on the slave
          var c = db._create(cn);
          
          for (var i = 0; i < 5000; ++i) {
            c.save({ _key: "test" + i, "value" : i });
          }
        },
        function (state) {
          assertEqual(state.count, collectionCount(cn));
          assertEqual(state.checksum, collectionChecksum(cn));
        },
        false
      );
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test with existing documents - same on the slave
////////////////////////////////////////////////////////////////////////////////

    testSameDocumentsIncremental : function () {
      connectToMaster();

      compare(
        function (state) {
          var c = db._create(cn), i;

          for (i = 0; i < 5000; ++i) {
            c.save({ _key: "test" + i, "value" : i });
          }

          state.checksum = collectionChecksum(cn);
          state.count = collectionCount(cn);
          assertEqual(5000, state.count);
        },
        function (state) {
          // already create the collection on the slave
          var c = db._create(cn);
          
          for (var i = 0; i < 5000; ++i) {
            c.save({ _key: "test" + i, "value" : i });
          }
        },
        function (state) {
          assertEqual(state.count, collectionCount(cn));
          assertEqual(state.checksum, collectionChecksum(cn));
        },
        true
      );
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test with existing documents - same on the slave but different keys
////////////////////////////////////////////////////////////////////////////////

    testSameSameButDifferentKeys : function () {
      connectToMaster();

      compare(
        function (state) {
          var c = db._create(cn), i;

          for (i = 0; i < 5000; ++i) {
            c.save({ "value" : i });
          }

          state.checksum = collectionChecksum(cn);
          state.count = collectionCount(cn);
          assertEqual(5000, state.count);
        },
        function (state) {
          // already create the collection on the slave
          var c = db._create(cn);
          
          for (var i = 0; i < 5000; ++i) {
            c.save({ "value" : i });
          }
        },
        function (state) {
          assertEqual(state.count, collectionCount(cn));
          assertEqual(state.checksum, collectionChecksum(cn));
        },
        false
      );
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test with existing documents - same on the slave but different keys
////////////////////////////////////////////////////////////////////////////////

    testSameSameButDifferentKeysIncremental : function () {
      connectToMaster();

      compare(
        function (state) {
          var c = db._create(cn), i;

          for (i = 0; i < 5000; ++i) {
            c.save({ "value" : i });
          }

          state.checksum = collectionChecksum(cn);
          state.count = collectionCount(cn);
          assertEqual(5000, state.count);
        },
        function (state) {
          // already create the collection on the slave
          var c = db._create(cn);
          
          for (var i = 0; i < 5000; ++i) {
            c.save({ "value" : i });
          }
        },
        function (state) {
          assertEqual(state.count, collectionCount(cn));
          assertEqual(state.checksum, collectionChecksum(cn));
        },
        true
      );
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test with existing documents - same on the slave but different values
////////////////////////////////////////////////////////////////////////////////

    testSameSameButDifferentValues : function () {
      connectToMaster();

      compare(
        function (state) {
          var c = db._create(cn), i;

          for (i = 0; i < 5000; ++i) {
            c.save({ _key: "test" + i, "value1" : i, "value2": "test" + i });
          }

          state.checksum = collectionChecksum(cn);
          state.count = collectionCount(cn);
          assertEqual(5000, state.count);
        },
        function (state) {
          // already create the collection on the slave
          var c = db._create(cn);
          
          for (var i = 0; i < 5000; ++i) {
            c.save({ _key: "test" + i, "value1" : "test" + i, "value2": i });
          }
        },
        function (state) {
          assertEqual(state.count, collectionCount(cn));
          assertEqual(state.checksum, collectionChecksum(cn));
        },
        false
      );
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test with existing documents - same on the slave but different values
////////////////////////////////////////////////////////////////////////////////

    testSameSameButDifferentValuesIncremental : function () {
      connectToMaster();

      compare(
        function (state) {
          var c = db._create(cn), i;

          for (i = 0; i < 5000; ++i) {
            c.save({ _key: "test" + i, "value1" : i, "value2": "test" + i });
          }

          state.checksum = collectionChecksum(cn);
          state.count = collectionCount(cn);
          assertEqual(5000, state.count);
        },
        function (state) {
          // already create the collection on the slave
          var c = db._create(cn);
          
          for (var i = 0; i < 5000; ++i) {
            c.save({ _key: "test" + i, "value1" : "test" + i, "value2": i });
          }
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

// -----------------------------------------------------------------------------
// --SECTION--                                                              main
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suite
////////////////////////////////////////////////////////////////////////////////

jsunity.run(ReplicationSuite);

return jsunity.done();

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// @addtogroup\\|// --SECTION--\\|/// @page\\|/// @}\\)"
// End:
