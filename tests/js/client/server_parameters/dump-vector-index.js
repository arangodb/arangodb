/* jshint globalstrict:false, strict:false, maxlen: 200 */
/* global getOptions */

// //////////////////////////////////////////////////////////////////////////////
// / DISCLAIMER
// /
// / Copyright 2014-2025 ArangoDB GmbH, Cologne, Germany
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
// //////////////////////////////////////////////////////////////////////////////

if (getOptions === true) {
  return {
    'vector-index': 'true'
  };
}

const jsunity = require('jsunity');
const { assertTrue, assertEqual, assertNotNull } = jsunity.jsUnity.assertions;
const arangodb = require('@arangodb');
const fs = require('fs');
const internal = require('internal');
const pu = require('@arangodb/testutils/process-utils');
const db = arangodb.db;
const arango = arangodb.arango;
const { executeExternalAndWaitWithSanitizer } = require('@arangodb/test-helper');
const { VectorIndexTrainingState, waitForVectorIndexState } =
  require('@arangodb/testutils/vector-index-common');

let IM = global.instanceManager;

const FP_PAUSE_TRAINING = 'RocksDBVectorIndex::pauseBeforeTraining';
const FP_PAUSE_INGESTION = 'RocksDBVectorIndex::pauseBeforeIngestion';

function dumpVectorIndexSuite() {
  'use strict';
  const cn = 'UnitTestsVectorIndexDump';
  const arangodump = pu.ARANGODUMP_BIN;

  assertTrue(fs.isFile(arangodump), 'arangodump not found!');

  const vectorIndexOf = (indexes) =>
    indexes.filter((i) => i.type === 'vector')[0] || null;

  const fillCollection = function (c, n, dim) {
    let docs = [];
    for (let i = 0; i < n; ++i) {
      let v = [];
      for (let d = 0; d < dim; ++d) {
        v.push((i + d) / (n + dim));
      }
      docs.push({ value: i, vector: v });
      if (docs.length >= 5000) { c.insert(docs); docs = []; }
    }
    if (docs.length) { c.insert(docs); }
  };

  // Dump the collection and return the vector index from its structure file,
  // or null if the dump does not contain one.
  const dumpVectorIndex = function () {
    let path = fs.getTempFile();
    fs.makeDirectory(path);
    try {
      let args = ['--collection', cn, '--output-directory', path,
                  '--overwrite', 'true'];
      args.push('--server.endpoint');
      args.push(IM.endpoint);
      args.push('--server.database');
      args.push(arango.getDatabaseName());
      args.push('--server.username');
      args.push(arango.connectedUser());

      const rc = executeExternalAndWaitWithSanitizer(
        arangodump, args, 'dump-vector-index');
      assertTrue(rc.hasOwnProperty('exit'), rc);
      assertEqual(0, rc.exit, rc);

      let structFile = fs.list(path).filter(
        (f) => f.endsWith('.structure.json'))[0];
      assertTrue(structFile !== undefined, 'no structure file in dump: ' +
                 JSON.stringify(fs.list(path)));
      let s = JSON.parse(fs.read(fs.join(path, structFile)));
      let indexes = (s.indexes && s.indexes.length)
        ? s.indexes
        : (s.parameters.indexes || []);
      return vectorIndexOf(indexes);
    } finally {
      fs.removeDirectoryRecursive(path, true);
    }
  };

  const createBackgroundVectorIndex = function (nLists) {
    return db._collection(cn).ensureIndex({
      name: 'vector',
      type: 'vector',
      fields: ['vector'],
      inBackground: true,
      params: { metric: 'l2', dimension: 4, nLists: nLists }
    });
  };

  const waitForState = function (indexName, states) {
    assertTrue(
      waitForVectorIndexState(db._collection(cn), indexName, states, 60),
      'vector index did not reach state ' + JSON.stringify(states));
  };

  // A dump in any phase must carry the vector index, else dump/restore loses it.
  const assertVectorIndexInDump = function (phase) {
    let idx = dumpVectorIndex();
    assertNotNull(idx, 'vector index missing from dump taken in phase: ' + phase);
    assertEqual('vector', idx.type);
    assertEqual(['vector'], idx.fields);
  };

  // Wait until no background build is in flight (index in a terminal state, or
  // gone) so we never drop the collection mid-build. indexes(false, true)
  // includes the hidden builder wrapper present during ingestion.
  const waitBuildSettled = function () {
    const end = internal.time() + 60;
    while (internal.time() < end) {
      let c = db._collection(cn);
      if (c === null) { return; }
      let v = vectorIndexOf(c.indexes(false, true));
      if (v === null ||
          v.trainingState === VectorIndexTrainingState.kReady ||
          v.trainingState === VectorIndexTrainingState.kUnusable) {
        return;
      }
      internal.sleep(0.05);
    }
  };

  return {
    setUpAll: function () {
      assertTrue(IM.debugCanUseFailAt(),
                 'this test requires failure points to be enabled');
    },

    setUp: function () {
      db._drop(cn);
      db._create(cn);
    },

    tearDown: function () {
      IM.debugClearFailAt();
      waitBuildSettled();
      db._drop(cn);
    },

    testDumpWhileUnusable: function () {
      let idx = createBackgroundVectorIndex(5);
      waitForState(idx.name, VectorIndexTrainingState.kUnusable);
      assertVectorIndexInDump('unusable');
    },

    testDumpWhileTraining: function () {
      fillCollection(db._collection(cn), 100, 4);
      IM.debugSetFailAt(FP_PAUSE_TRAINING);

      let idx = createBackgroundVectorIndex(1);
      waitForState(idx.name, VectorIndexTrainingState.kTraining);
      assertVectorIndexInDump('training');
    },

    testDumpWhileIngesting: function () {
      fillCollection(db._collection(cn), 100, 4);
      IM.debugSetFailAt(FP_PAUSE_INGESTION);

      let idx = createBackgroundVectorIndex(1);
      waitForState(idx.name, VectorIndexTrainingState.kIngesting);
      assertVectorIndexInDump('ingesting');
    },

    testDumpWhileReady: function () {
      fillCollection(db._collection(cn), 100, 4);

      let idx = createBackgroundVectorIndex(1);
      waitForState(idx.name, VectorIndexTrainingState.kReady);
      assertVectorIndexInDump('ready');
    }
  };
}

jsunity.run(dumpVectorIndexSuite);

return jsunity.done();
