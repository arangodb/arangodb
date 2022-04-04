/*jshint globalstrict:true*/
'use strict';
/*global assertEqual, assertTrue, fail */

////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2020-2021 ArangoDB GmbH, Cologne, Germany
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
/// @author Lars Maier
////////////////////////////////////////////////////////////////////////////////

const jsunity = require("jsunity");
const arangodb = require("@arangodb");
const _ = require('lodash');
const db = arangodb.db;
const database = "prototype_shell_test_db";

const {setUpAll, tearDownAll} = (function () {
  let previousDatabase, databaseExisted = true;
  return {
    setUpAll: function () {
      previousDatabase = db._name();
      if (!_.includes(db._databases(), database)) {
        db._createDatabase(database);
        databaseExisted = false;
      }
      db._useDatabase(database);
    },

    tearDownAll: function () {
      db._useDatabase(previousDatabase);
      if (!databaseExisted) {
        db._dropDatabase(database);
      }
    },
  };
}());

function PrototypeStateTestSuite() {

  const config = {
    replicationFactor: 3,
    writeConcern: 2,
    softWriteConcern: 2,
    waitForSync: true
  };

  return {
    setUpAll, tearDownAll,
    setUp: function () {
    },
    tearDown: function () {
    },

    testCreateAndDropPrototypeState: function () {
      const state = db._createPrototypeState({config});
      const same = db._prototypeState(state.id());
      assertEqual(state.id(), same.id());
    },

    testWriteEntries: function () {
      const state = db._createPrototypeState({config});
      const idx1 = state.write("key", "value");
      const idx2 = state.write({"key2": "value2"});
      const idx3 = state.write("key3", "value3", {waitForApplied: false});
      const idx4 = state.write({"key5.1": "value6", "key5.2": "value7"}, {waitForApplied: false});

      const snapshot = state.getSnapshot(idx4);
      assertEqual(snapshot, {
        "key": "value",
        "key2": "value2",
        "key3": "value3",
        "key5.1": "value6",
        "key5.2": "value7"
      });
      assertTrue(idx1 < idx2);
      assertTrue(idx2 < idx3);
      assertTrue(idx3 < idx4);
    },

    testReadEntries: function () {
      const state = db._createPrototypeState({config});
      state.write({"A":"1", "B":"2", "C": "3", "D": "4"}, {waitForApplied: true});

      const valueA = state.read("A");
      assertEqual(valueA, "1");
      const {B: valueB, C: valueC} = state.read(["B", "C"]);
      assertEqual(valueB, "2");
      assertEqual(valueC, "3");

      const nonExisting = state.read("DOES-NOT-EXIST");
      assertEqual(nonExisting, undefined);
      const otherNonEx = state.read(["STILL-DOES-NOT-EXIST"]);
      assertEqual(otherNonEx, {});
    },

    testSnapshotWaitFor: function() {
      const state = db._createPrototypeState({config});
      let lastIndex;
      for(let i = 0; i < 1000; i++) {
        // dump in data, wait for nothing
        lastIndex = state.write(`key${i}`, `value${i}`, {waitForApplied: false, waitForCommit: false});
      }

      const snapshot = state.getSnapshot(lastIndex);
      for(let i = 0; i < 1000; i++) {
        assertEqual(snapshot[`key${i}`], `value${i}`);
      }
    }
  };
}

jsunity.run(PrototypeStateTestSuite);

return jsunity.done();
