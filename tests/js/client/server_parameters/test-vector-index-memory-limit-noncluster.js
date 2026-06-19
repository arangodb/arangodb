/*jshint globalstrict:false, strict:false */
/* global getOptions, assertTrue */

// //////////////////////////////////////////////////////////////////////////////
// / DISCLAIMER
// /
// / Copyright 2014-2026 ArangoDB GmbH, Cologne, Germany
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
// / @author Jure Bajic
// //////////////////////////////////////////////////////////////////////////////

if (getOptions === true) {
  return {
    'vector-index': true,
    'query.global-memory-limit': '8388608',
    'query.memory-limit': '8388608',
  };
}

const jsunity = require('jsunity');
const internal = require('internal');
const db = internal.db;
const {
  randomNumberGeneratorFloat,
  generateSeed,
} = require('@arangodb/testutils/seededRandom');
const {
  generateDocs,
  assertEnsureIndexResultUnusable,
} = require('@arangodb/testutils/vector-index-common');

const dbName = 'vectorMemoryLimitDb';
const collName = 'vectorMemoryLimitColl';

// Reservoir = min(nLists * numberOfDocsPerCentroid, numDocs) * dimension * 4.
// 16 * 100 * 2048 * 4 = 13 107 200 bytes (~12.5 MiB), comfortably above the
// 8 MiB global limit set in getOptions. numDocs is sized so the reservoir is
// not bounded below the limit by the document count.
const dimension = 2048;
const nLists = 16;
const docsPerCentroid = 100;
const numDocs = nLists * docsPerCentroid + 200;

function vectorIndexMemoryLimitSuite() {
  let collection;
  const seed = generateSeed();

  return {
    setUpAll: function () {
      db._useDatabase('_system');
      try { db._dropDatabase(dbName); } catch (e) {}
      db._createDatabase(dbName);
      db._useDatabase(dbName);
      collection = db._create(collName, {numberOfShards: 1});
      const gen = randomNumberGeneratorFloat(seed);
      const docs = generateDocs(gen, numDocs, dimension);
      // Insert in batches so a single bulk insert does not itself trip the
      // global limit during the test's own setup.
      const batchSize = 100;
      for (let i = 0; i < docs.length; i += batchSize) {
        collection.insert(docs.slice(i, i + batchSize));
      }
    },

    tearDownAll: function () {
      db._useDatabase('_system');
      try { db._dropDatabase(dbName); } catch (e) {}
    },

    testTrainingReservoirHitsGlobalMemoryLimit: function () {
      const result = collection.ensureIndex({
        name: 'vec_l2',
        type: 'vector',
        fields: ['vector'],
        inBackground: false,
        params: {
          metric: 'l2',
          dimension,
          nLists,
          numberOfDocsPerCentroid: docsPerCentroid,
          trainingIterations: 10,
        },
      });
      assertEnsureIndexResultUnusable(result, 'reservoir exceeds memory limit');
      assertTrue(/memory|resource/i.test(result.errorMessage),
        'Expected training error to mention memory/resource limit, got: ' +
        result.errorMessage);
    },
  };
}

jsunity.run(vectorIndexMemoryLimitSuite);
return jsunity.done();
