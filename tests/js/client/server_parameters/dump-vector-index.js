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
    'vector-index': true
  };
}

const jsunity = require('jsunity');
const { assertTrue, assertFalse, assertEqual, assertNotNull } = jsunity.jsUnity.assertions;
const arangodb = require('@arangodb');
const fs = require('fs');
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
  const dim = 4;
  const docCount = 100;
  const arangodump = pu.ARANGODUMP_BIN;

  assertTrue(fs.isFile(arangodump), 'arangodump not found!');

  const vectorIndexOf = (indexes) =>
    indexes.filter((i) => i.type === 'vector')[0] || null;

  const fillCollection = function () {
    let docs = [];
    for (let i = 0; i < docCount; ++i) {
      let v = [];
      for (let d = 0; d < dim; ++d) { v.push((i + d) / (docCount + dim)); }
      docs.push({ value: i, vector: v });
    }
    db._collection(cn).insert(docs);
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

  const createBackgroundVectorIndex = function (name, nLists) {
    return db._collection(cn).ensureIndex({
      name: name,
      type: 'vector',
      fields: ['vector'],
      inBackground: true,
      params: { metric: 'l2', dimension: dim, nLists: nLists }
    });
  };

  // A dump in any phase must carry the vector index, else dump/restore loses it.
  // The builder-only fields must not leak into the external (dump) shape.
  const assertVectorIndexInDump = function (phase) {
    let idx = dumpVectorIndex();
    assertNotNull(idx, `vector index missing from dump taken in phase: ${phase}`);
    assertEqual('vector', idx.type);
    assertEqual(['vector'], idx.fields);
    assertFalse(idx.hasOwnProperty('_inprogress'),
                `_inprogress leaked into dump in phase ${phase}: ${JSON.stringify(idx._inprogress)}`);
    assertFalse(idx.hasOwnProperty('documentsProcessed'),
                `documentsProcessed leaked into dump in phase ${phase}: ${JSON.stringify(idx.documentsProcessed)}`);
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
      db._drop(cn);
    },

    testDumpWhileUnusable: function () {
      const idx = createBackgroundVectorIndex('vecIdxUnusable', 5);
      assertTrue(waitForVectorIndexState(db._collection(cn), idx.name,
                                         VectorIndexTrainingState.kUnusable, 60));
      assertVectorIndexInDump('unusable');
    },

    testDumpWhileTraining: function () {
      fillCollection();
      assertEqual(db._collection(cn).count(), docCount);
      IM.debugSetFailAt(FP_PAUSE_TRAINING);

      const idx = createBackgroundVectorIndex('vecIdxTraining', 1);
      assertTrue(waitForVectorIndexState(db._collection(cn), idx.name,
                                         VectorIndexTrainingState.kTraining, 60));
      assertVectorIndexInDump('training');
    },

    testDumpWhileIngesting: function () {
      fillCollection();
      assertEqual(db._collection(cn).count(), docCount);
      IM.debugSetFailAt(FP_PAUSE_INGESTION);

      const idx = createBackgroundVectorIndex('vecIdxIngesting', 1);
      assertTrue(waitForVectorIndexState(db._collection(cn), idx.name,
                                         VectorIndexTrainingState.kIngesting, 60));
      assertVectorIndexInDump('ingesting');
    },

    testDumpWhileReady: function () {
      fillCollection();
      assertEqual(db._collection(cn).count(), docCount);

      const idx = createBackgroundVectorIndex('vecIdxReady', 1);
      assertTrue(waitForVectorIndexState(db._collection(cn), idx.name,
                                         VectorIndexTrainingState.kReady));
      assertVectorIndexInDump('ready');
    }
  };
}

jsunity.run(dumpVectorIndexSuite);

return jsunity.done();
