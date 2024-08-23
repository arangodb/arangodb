/*jshint globalstrict:false, strict:false, maxlen: 500 */
/*global fail, assertEqual, assertNotEqual, assertTrue, assertFalse */

// //////////////////////////////////////////////////////////////////////////////
// / DISCLAIMER
// /
// / Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
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
/// @author Michael Hackstein
/// @author Copyright 2016, ArangoDB GmbH, Cologne, Germany
// //////////////////////////////////////////////////////////////////////////////

const internal = require("internal");
const db = require('internal').db;
const jsunity = require("jsunity");
const normalize = require("@arangodb/aql-helper").normalizeProjections;
const {waitForEstimatorSync } = require('@arangodb/test-helper');
const errors = internal.errors;
const analyzers = require("@arangodb/analyzers");
const cn = 'UnitTestsIndexHints';

function indexHintCollectionSuite () {
  const getIndexNames = function (query) {
    const explain = db._createStatement(query).explain();
    const usedIndicesInJoin = explain.plan.nodes.filter(node => (node.type === 'JoinNode'))
      .map(node => node.indexInfos.map(indexInfo => indexInfo.index.name))
      .map(subArray => subArray.map(item => [item]))[0];

    const usedIndices =  explain.plan.nodes.filter(node => (node.type === 'IndexNode' ||
        node.type === 'SingleRemoteOperationNode'))
      .map(node => node.indexes.map(index => index.name));

    return usedIndicesInJoin === undefined ? usedIndices : usedIndices.concat(usedIndicesInJoin);
  };

  const cn2 = 'TestCollectionWithGeoIndex';
  const invertedIdxName = 'inverted1';
  const invertedIdxName2 = 'inverted2';
  const persistentIdxName = 'persistent1';
  const persistentIdxName2 = 'persistent2';
  const geoIdxName = 'geo1';
  const geoIdxName2 = 'geo2';
  let collection;
  let defaultEqualityIndex;
  let alternateEqualityIndex;
  let defaultSortingIndex;
  let alternateSortingIndex;

  return {

    setUpAll: function () {
      db._drop(cn);
      collection = db._create(cn);

      collection.ensureIndex({type: 'hash', name: 'hash_a', fields: ['a']});
      collection.ensureIndex({type: 'hash', name: 'hash_a_b', fields: ['a', 'b']});
      collection.ensureIndex({type: 'hash', name: 'hash_b_a', fields: ['b', 'a']});
      collection.ensureIndex({type: 'skiplist', name: 'skip_a', fields: ['a']});
      collection.ensureIndex({type: 'skiplist', name: 'skip_a_b', fields: ['a', 'b']});
      collection.ensureIndex({type: 'skiplist', name: 'skip_b_a', fields: ['b', 'a']});

      defaultEqualityIndex = 'hash_a';
      alternateEqualityIndex = 'skip_a';
      defaultSortingIndex = 'hash_a';
      alternateSortingIndex = 'skip_a_b';

      db._drop(cn2);
      collection = db._create(cn2);
      collection.ensureIndex({type: "persistent", fields: ["value1"], name: persistentIdxName});
      collection.ensureIndex({type: "persistent", fields: ["value2"], name: persistentIdxName2});
      collection.ensureIndex({type: "geo", geoJson: true, fields: ["geo"], name: geoIdxName});
      collection.ensureIndex({type: "geo", geoJson: true, fields: ["otherGeo"], name: geoIdxName2});
      analyzers.save("geo_json", "geojson", {type: "point"}, []);
      let commonInvertedIndexMeta = {
        type: "inverted", name: invertedIdxName, includeAllFields: true, fields: [
          {"name": "geo", "analyzer": "geo_json"}
        ]
      };
      collection.ensureIndex(commonInvertedIndexMeta);
      commonInvertedIndexMeta = {
        type: "inverted", name: invertedIdxName2, includeAllFields: true, fields: [
          {"name": "otherGeo", "analyzer": "geo_json"}
        ]
      };
      collection.ensureIndex(commonInvertedIndexMeta);

      collection.insert({
        geo: {type: "Point", coordinates: [50, 51]},
        otherGeo: {type: "Point", coordinates: [100, 101]},
        value1: 1,
        value2: "abc"
      });
      collection.insert({
        geo: {type: "Point", coordinates: [52, 53]},
        otherGeo: {type: "Point", coordinates: [150, 151]},
        value1: 2,
        value2: "123"
      });
    },

    tearDownAll: function () {
      db._drop(cn);
      db._drop(cn2);
      analyzers.remove("geo_json", true);
    },

    testFilterNoHint: function () {
      const query = `
        FOR doc IN ${cn} OPTIONS {}
          FILTER doc.a == 1
          RETURN doc
      `;
      const usedIndexes = getIndexNames(query);
      assertEqual(usedIndexes.length, 1);
      assertEqual(usedIndexes[0].length, 1);
      assertEqual(usedIndexes[0][0], defaultEqualityIndex);
    },

    testPrimary: function () {
      [
        `FOR doc IN ${cn} OPTIONS {indexHint: 'primary', forceIndexHint: false} FILTER doc._key == 'test' RETURN doc`,
        `FOR doc IN ${cn} OPTIONS {indexHint: 'primary', forceIndexHint: true} FILTER doc._key == 'test' RETURN doc`,
        `FOR doc IN ${cn} OPTIONS {indexHint: 'primary', forceIndexHint: false} FILTER doc._key == 'test' && doc.testi == 99 RETURN doc`,
        `FOR doc IN ${cn} OPTIONS {indexHint: 'primary', forceIndexHint: true} FILTER doc._key == 'test' && doc.testi == 99 RETURN doc`,
        `FOR doc IN ${cn} OPTIONS {indexHint: 'primary', forceIndexHint: false} FILTER doc.testi == 99 && doc._key == 'test' RETURN doc`,
        `FOR doc IN ${cn} OPTIONS {indexHint: 'primary', forceIndexHint: true} FILTER doc.testi == 99 && doc._key == 'test' RETURN doc`,
      ].forEach((query) => {
        const usedIndexes = getIndexNames(query);
        assertEqual(usedIndexes.length, 1, query);
        assertEqual(usedIndexes[0].length, 1, query);
        assertEqual(usedIndexes[0][0], 'primary', query);
      });
    },

    testMultipleOrConditionsPrimary: function () {
      [
        `FOR doc IN ${cn} OPTIONS {indexHint: 'primary', forceIndexHint: false} FILTER doc._key == 'test' || doc.a == 'testi' RETURN doc`,
        `FOR doc IN ${cn} OPTIONS {indexHint: 'primary', forceIndexHint: true} FILTER doc._key == 'test' || doc.a == 'testi' RETURN doc`,
        `FOR doc IN ${cn} OPTIONS {indexHint: 'primary', forceIndexHint: false} FILTER doc.a == 'testi' || doc._key == 'test' RETURN doc`,
        `FOR doc IN ${cn} OPTIONS {indexHint: 'primary', forceIndexHint: true} FILTER doc.a == 'testi' || doc._key == 'test' RETURN doc`,
      ].forEach((query) => {
        const usedIndexes = getIndexNames(query);
        assertEqual(usedIndexes.length, 1, query);
        assertEqual(usedIndexes[0].length, 2, query);
        assertEqual(["hash_a", "primary"], usedIndexes[0].sort(), query);
      });
    },

    testMultipleOrConditionsOther: function () {
      [
        `FOR doc IN ${cn} OPTIONS {indexHint: 'primary', forceIndexHint: false} FILTER doc.a == 'test' || doc.b == 'testi' RETURN doc`,
        `FOR doc IN ${cn} OPTIONS {indexHint: 'primary', forceIndexHint: true} FILTER doc.a == 'test' || doc.b == 'testi' RETURN doc`,
        `FOR doc IN ${cn} OPTIONS {indexHint: 'primary', forceIndexHint: false} FILTER doc.b == 'testi' || doc.a == 'test' RETURN doc`,
        `FOR doc IN ${cn} OPTIONS {indexHint: 'primary', forceIndexHint: true} FILTER doc.b == 'testi' || doc.a == 'test' RETURN doc`,
      ].forEach((query) => {
        const usedIndexes = getIndexNames(query);
        assertEqual(usedIndexes.length, 1, query);
        assertEqual(usedIndexes[0].length, 2, query);
        assertEqual(["hash_a", "hash_b_a"], usedIndexes[0].sort(), query);
      });
    },

    testMultipleOrConditionsSomeOfThemOnNonIndexedAttributes: function () {
      [
        `FOR doc IN ${cn} OPTIONS {indexHint: 'primary', forceIndexHint: true} FILTER doc._key == 'test' || doc.a == 99 RETURN doc`,
        `FOR doc IN ${cn} OPTIONS {indexHint: 'primary', forceIndexHint: true} FILTER doc.a == 99 || doc._key == 'test' RETURN doc`,
      ].forEach((query) => {
        // 2 indexes used here
        const usedIndexes = getIndexNames(query);
        assertEqual(usedIndexes[0].length, 2, query);
      });
    },

    testFilterDefaultHint: function () {
      const query = `
        FOR doc IN ${cn} OPTIONS {indexHint: '${defaultEqualityIndex}'}
          FILTER doc.a == 1
          RETURN doc
      `;
      const usedIndexes = getIndexNames(query);
      assertEqual(usedIndexes.length, 1);
      assertEqual(usedIndexes[0].length, 1);
      assertEqual(usedIndexes[0][0], defaultEqualityIndex);
    },

    testFilterDefaultHintForced: function () {
      const query = `
        FOR doc IN ${cn} OPTIONS {indexHint: '${defaultEqualityIndex}', forceIndexHint: true}
          FILTER doc.a == 1
          RETURN doc
      `;
      const usedIndexes = getIndexNames(query);
      assertEqual(usedIndexes.length, 1);
      assertEqual(usedIndexes[0].length, 1);
      assertEqual(usedIndexes[0][0], defaultEqualityIndex);
    },

    testFilterNonexistentIndexHint: function () {
      const query = `
        FOR doc IN ${cn} OPTIONS {indexHint: 'foo'}
          FILTER doc.a == 1
          RETURN doc
      `;
      const usedIndexes = getIndexNames(query);
      assertEqual(usedIndexes.length, 1);
      assertEqual(usedIndexes[0].length, 1);
      assertEqual(usedIndexes[0][0], defaultEqualityIndex);
    },

    testFilterNonexistentIndexHintForced: function () {
      const query = `
        FOR doc IN ${cn} OPTIONS {indexHint: 'foo', forceIndexHint: true}
          FILTER doc.a == 1
          RETURN doc
      `;
      try {
        const usedIndexes = getIndexNames(query);
        fail();
      } catch (err) {
        assertEqual(errors.ERROR_QUERY_FORCED_INDEX_HINT_UNUSABLE.code, err.errorNum);
      }
    },

    testFilterUnusableHint: function () {
      const query = `
        FOR doc IN ${cn} OPTIONS {indexHint: 'hash_b_a'}
          FILTER doc.a == 1
          RETURN doc
      `;
      const usedIndexes = getIndexNames(query);
      assertEqual(usedIndexes.length, 1);
      assertEqual(usedIndexes[0].length, 1);
      assertEqual(usedIndexes[0][0], defaultEqualityIndex);
    },

    testFilterUnusableHintForced: function () {
      const query = `
        FOR doc IN ${cn} OPTIONS {indexHint: 'hash_b_a', forceIndexHint: true}
          FILTER doc.a == 1
          RETURN doc
      `;
      try {
        const usedIndexes = getIndexNames(query);
        fail();
      } catch (err) {
        assertEqual(errors.ERROR_QUERY_FORCED_INDEX_HINT_UNUSABLE.code, err.errorNum);
      }
    },

    testFilterTypeHint: function () {
      const query = `
        FOR doc IN ${cn} OPTIONS {indexHint: '${alternateEqualityIndex}'}
          FILTER doc.a == 1
          RETURN doc
      `;
      const usedIndexes = getIndexNames(query);
      assertEqual(usedIndexes.length, 1);
      assertEqual(usedIndexes[0].length, 1);
      assertEqual(usedIndexes[0][0], alternateEqualityIndex);
    },

    testFilterPartialCoverageHint: function () {
      const query = `
        FOR doc IN ${cn} OPTIONS {indexHint: 'skip_a_b'}
          FILTER doc.a == 1
          RETURN doc
      `;
      const usedIndexes = getIndexNames(query);
      assertEqual(usedIndexes.length, 1);
      assertEqual(usedIndexes[0].length, 1);
      assertEqual(usedIndexes[0][0], 'skip_a_b');
    },

    testFilterListFirstHint: function () {
      const query = `
        FOR doc IN ${cn} OPTIONS {indexHint: ['skip_a_b', '${alternateEqualityIndex}']}
          FILTER doc.a == 1
          RETURN doc
      `;
      const usedIndexes = getIndexNames(query);
      assertEqual(usedIndexes.length, 1);
      assertEqual(usedIndexes[0].length, 1);
      assertEqual(usedIndexes[0][0], `skip_a_b`);
    },

    testFilterListLastHint: function () {
      const query = `
        FOR doc IN ${cn} OPTIONS {indexHint: ['skip_b_a', '${alternateEqualityIndex}']}
          FILTER doc.a == 1
          RETURN doc
      `;
      const usedIndexes = getIndexNames(query);
      assertEqual(usedIndexes.length, 1);
      assertEqual(usedIndexes[0].length, 1);
      assertEqual(usedIndexes[0][0], alternateEqualityIndex);
    },

    testFilterNestedMatchedHint: function () {
      const query = `
        FOR doc IN ${cn} OPTIONS {indexHint: '${alternateEqualityIndex}'}
          FILTER doc.a == 1
          FOR sub IN ${cn} OPTIONS {indexHint: '${alternateEqualityIndex}'}
            FILTER sub.a == 2
            RETURN [doc, sub]
      `;
      const usedIndexes = getIndexNames(query);
      assertEqual(usedIndexes.length, 2);
      assertEqual(usedIndexes[0].length, 1);
      assertEqual(usedIndexes[0][0], alternateEqualityIndex);
      assertEqual(usedIndexes[1].length, 1);
      assertEqual(usedIndexes[1][0], alternateEqualityIndex);
    },

    testFilterNestedUnmatchedHint: function () {
      const query = `
        FOR doc IN ${cn} OPTIONS {indexHint: '${alternateEqualityIndex}'}
          FILTER doc.a == 1
          FOR sub IN ${cn} OPTIONS {indexHint: 'skip_a_b'}
            FILTER sub.a == 2
            RETURN [doc, sub]
      `;
      const usedIndexes = getIndexNames(query);
      assertEqual(usedIndexes.length, 2);
      assertEqual(usedIndexes[0].length, 1);
      assertEqual(usedIndexes[0][0], alternateEqualityIndex);
      assertEqual(usedIndexes[1].length, 1);
      assertEqual(usedIndexes[1][0], 'skip_a_b');
    },

    testSortNoHint: function () {
      const query = `
        FOR doc IN ${cn} OPTIONS {}
          SORT doc.a
          RETURN doc
      `;
      const usedIndexes = getIndexNames(query);
      assertEqual(usedIndexes.length, 1);
      assertEqual(usedIndexes[0].length, 1);
      assertEqual(usedIndexes[0][0], defaultSortingIndex);
    },

    testSortDefaultHint: function () {
      const query = `
        FOR doc IN ${cn} OPTIONS {indexHint: '${defaultSortingIndex}'}
          SORT doc.a
          RETURN doc
      `;
      const usedIndexes = getIndexNames(query);
      assertEqual(usedIndexes.length, 1);
      assertEqual(usedIndexes[0].length, 1);
      assertEqual(usedIndexes[0][0], defaultSortingIndex);
    },

    testSortDefaultHintForced: function () {
      const query = `
        FOR doc IN ${cn} OPTIONS {indexHint: '${defaultSortingIndex}', forceIndexHint: true}
          SORT doc.a
          RETURN doc
      `;
      const usedIndexes = getIndexNames(query);
      assertEqual(usedIndexes.length, 1);
      assertEqual(usedIndexes[0].length, 1);
      assertEqual(usedIndexes[0][0], defaultSortingIndex);
    },

    testSortNonexistentIndexHint: function () {
      const query = `
        FOR doc IN ${cn} OPTIONS {indexHint: 'foo'}
          SORT doc.a
          RETURN doc
      `;
      const usedIndexes = getIndexNames(query);
      assertEqual(usedIndexes.length, 1);
      assertEqual(usedIndexes[0].length, 1);
      assertEqual(usedIndexes[0][0], defaultSortingIndex);
    },

    testSortNonexistentIndexHintForced: function () {
      const query = `
        FOR doc IN ${cn} OPTIONS {indexHint: 'foo', forceIndexHint: true}
          SORT doc.a
          RETURN doc
      `;
      try {
        const usedIndexes = getIndexNames(query);
        fail();
      } catch (err) {
        assertEqual(errors.ERROR_QUERY_FORCED_INDEX_HINT_UNUSABLE.code, err.errorNum);
      }
    },

    testSortUnusableHint: function () {
      const query = `
        FOR doc IN ${cn} OPTIONS {indexHint: 'skip_b_a'}
          SORT doc.a
          RETURN doc
      `;
      const usedIndexes = getIndexNames(query);
      assertEqual(usedIndexes.length, 1);
      assertEqual(usedIndexes[0].length, 1);
      assertEqual(usedIndexes[0][0], defaultSortingIndex);
    },

    testSortUnusableHintForced: function () {
      const query = `
        FOR doc IN ${cn} OPTIONS {indexHint: 'skip_b_a', forceIndexHint: true}
          SORT doc.a
          RETURN doc
      `;
      try {
        const usedIndexes = getIndexNames(query);
        fail();
      } catch (err) {
        assertEqual(errors.ERROR_QUERY_FORCED_INDEX_HINT_UNUSABLE.code, err.errorNum);
      }
    },

    testSortPartialCoverageHint: function () {
      const query = `
        FOR doc IN ${cn} OPTIONS {indexHint: '${alternateSortingIndex}'}
          SORT doc.a
          RETURN doc
      `;
      const usedIndexes = getIndexNames(query);
      assertEqual(usedIndexes.length, 1);
      assertEqual(usedIndexes[0].length, 1);
      assertEqual(usedIndexes[0][0], alternateSortingIndex);
    },

    testSortListFirstHint: function () {
      const query = `
        FOR doc IN ${cn} OPTIONS {indexHint: ['${alternateSortingIndex}', '${defaultSortingIndex}']}
          SORT doc.a
          RETURN doc
      `;
      const usedIndexes = getIndexNames(query);
      assertEqual(usedIndexes.length, 1);
      assertEqual(usedIndexes[0].length, 1);
      assertEqual(usedIndexes[0][0], alternateSortingIndex);
    },

    testSortListLastHint: function () {
      const query = `
        FOR doc IN ${cn} OPTIONS {indexHint: ['skip_b_a', '${alternateSortingIndex}']}
          SORT doc.a
          RETURN doc
      `;
      const usedIndexes = getIndexNames(query);
      assertEqual(usedIndexes.length, 1);
      assertEqual(usedIndexes[0].length, 1);
      assertEqual(usedIndexes[0][0], alternateSortingIndex);
    },

    testUpsertWithIndexHint: function () {
      const prefix = " UPSERT { a: 1234 } INSERT { a: 1234 } UPDATE {} IN " + cn + " OPTIONS ";
      const indexHints = [
        [{}, ["hash_a", "hash_a_b", "skip_a", "skip_a_b"]],
        [{indexHint: 'foo'}, ["hash_a", "hash_a_b", "skip_a", "skip_a_b"]],
        [{indexHint: ['hash_a'], forceIndexHint: true}, ['hash_a']],
        [{indexHint: ['hash_a']}, ['hash_a']],
        [{indexHint: ['hash_a_b'], forceIndexHint: true}, ['hash_a_b']],
        [{indexHint: ['hash_a_b']}, ['hash_a_b']],
        [{indexHint: ['skip_a'], forceIndexHint: true}, ['skip_a']],
        [{indexHint: ['skip_a']}, ['skip_a']],
        [{indexHint: ['skip_a_b'], forceIndexHint: true}, ['skip_a_b']],
        [{indexHint: ['skip_a_b']}, ['skip_a_b']],
        [{indexHint: ['foo', 'bar', 'hash_a'], forceIndexHint: true}, ['hash_a']],
        [{indexHint: 'hash_a', forceIndexHint: true}, ['hash_a']],
        [{indexHint: 'hash_a'}, ['hash_a']],
        [{indexHint: 'hash_a_b', forceIndexHint: true}, ['hash_a_b']],
        [{indexHint: 'hash_a_b'}, ['hash_a_b']],
        [{indexHint: 'skip_a', forceIndexHint: true}, ['skip_a']],
        [{indexHint: 'skip_a'}, ['skip_a']],
        [{indexHint: 'skip_a_b', forceIndexHint: true}, ['skip_a_b']],
        [{indexHint: 'skip_a_b'}, ['skip_a_b']],
      ];
      indexHints.forEach((indexHint, index) => {
        let queryExplain = db._createStatement(prefix + JSON.stringify(indexHint[0])).explain().plan.nodes;
        queryExplain.filter((node) => node.type === "IndexNode").forEach((node) => {
          assertNotEqual(indexHint[1].indexOf(node.indexes[0].name), -1);
        });
      });
    },

    testForceInvertedIndexWithGeoPresent: function () {
      const query = `FOR d IN ${cn2}  OPTIONS {indexHint: "${invertedIdxName}", forceIndexHint: true, waitForSync: true} 
      FILTER GEO_DISTANCE([150, 160], d.geo) > 50 SORT d.geo.coordinates[0] ASC
      RETURN d`;
      const usedIndexes = getIndexNames(query);
      assertEqual(usedIndexes.length, 1);
      assertEqual(usedIndexes[0].length, 1);
      assertEqual(usedIndexes[0][0], invertedIdxName);
      const res = db._query(query).toArray();
      assertEqual(res.length, 2);
      for (let i = 0; i < res.length; ++i) {
        const result = res[i];
        assertTrue(result.hasOwnProperty("geo"));
        assertTrue(result.hasOwnProperty("otherGeo"));
        assertTrue(result.hasOwnProperty("value1"));
        assertTrue(result.hasOwnProperty("value2"));
        assertEqual(result.geo.type, "Point");
        assertEqual(result.geo.coordinates.length, 2);
        assertEqual(result.geo.coordinates[0], 50 + (i * 2));
        assertEqual(result.geo.coordinates[1], 51 + (i * 2));
        assertEqual(result.otherGeo.type, "Point");
        assertEqual(result.otherGeo.coordinates.length, 2);
        assertEqual(result.otherGeo.coordinates[0], 100 + (i * 50));
        assertEqual(result.otherGeo.coordinates[1], 101 + (i * 50));
        assertEqual(result.value1, 1 + i);
        if (i === 0) {
          assertEqual(result.value2, "abc");
        } else {
          assertEqual(result.value2, "123");
        }
      }
    },

    testNotForceInvertedIndexWithGeoPresent: function () {
      const query = `FOR d IN ${cn2}  OPTIONS {indexHint: "${invertedIdxName}", waitForSync: true} 
      FILTER GEO_DISTANCE([150, 160], d.geo) > 50 SORT d.geo.coordinates[0] ASC
      RETURN d`;
      const usedIndexes = getIndexNames(query);
      assertEqual(usedIndexes.length, 1);
      assertEqual(usedIndexes[0].length, 1);
      assertEqual(usedIndexes[0][0], geoIdxName);
      const res = db._query(query).toArray();
      assertEqual(res.length, 2);
      for (let i = 0; i < res.length; ++i) {
        const result = res[i];
        assertTrue(result.hasOwnProperty("geo"));
        assertTrue(result.hasOwnProperty("otherGeo"));
        assertTrue(result.hasOwnProperty("value1"));
        assertTrue(result.hasOwnProperty("value2"));
        assertEqual(result.geo.type, "Point");
        assertEqual(result.geo.coordinates.length, 2);
        assertEqual(result.geo.coordinates[0], 50 + (i * 2));
        assertEqual(result.geo.coordinates[1], 51 + (i * 2));
        assertEqual(result.otherGeo.type, "Point");
        assertEqual(result.otherGeo.coordinates.length, 2);
        assertEqual(result.otherGeo.coordinates[0], 100 + (i * 50));
        assertEqual(result.otherGeo.coordinates[1], 101 + (i * 50));
        assertEqual(result.value1, 1 + i);
        if (i === 0) {
          assertEqual(result.value2, "abc");
        } else {
          assertEqual(result.value2, "123");
        }
      }
    },

    testForceInvertedIndexWithGeoPresentJustOneDoc: function () {
      const query = `FOR d IN ${cn2}  OPTIONS {indexHint: "${invertedIdxName}", forceIndexHint: true, waitForSync: true} 
      FILTER GEO_DISTANCE([50, 51], d.geo) < 1
      RETURN d`;
      const usedIndexes = getIndexNames(query);
      assertEqual(usedIndexes.length, 1);
      assertEqual(usedIndexes[0].length, 1);
      assertEqual(usedIndexes[0][0], invertedIdxName);
      const res = db._query(query).toArray();
      assertEqual(res.length, 1);
      const result = res[0];
      assertTrue(result.hasOwnProperty("geo"));
      assertTrue(result.hasOwnProperty("otherGeo"));
      assertTrue(result.hasOwnProperty("value1"));
      assertTrue(result.hasOwnProperty("value2"));
      assertEqual(result.geo.type, "Point");
      assertEqual(result.geo.coordinates.length, 2);
      assertEqual(result.geo.coordinates[0], 50);
      assertEqual(result.geo.coordinates[1], 51);
      assertEqual(result.otherGeo.type, "Point");
      assertEqual(result.otherGeo.coordinates.length, 2);
      assertEqual(result.otherGeo.coordinates[0], 100);
      assertEqual(result.otherGeo.coordinates[1], 101);
      assertEqual(result.value1, 1);
      assertEqual(result.value2, "abc");
    },

    testNotForceInvertedIndexWithGeoPresentJustOneDoc: function () {
      const query = `FOR d IN ${cn2}  OPTIONS {indexHint: "${invertedIdxName}", waitForSync: true} 
      FILTER GEO_DISTANCE([50, 51], d.geo) < 1
      RETURN d`;
      const usedIndexes = getIndexNames(query);
      assertEqual(usedIndexes.length, 1);
      assertEqual(usedIndexes[0].length, 1);
      assertEqual(usedIndexes[0][0], geoIdxName);
      const res = db._query(query).toArray();
      assertEqual(res.length, 1);
      const result = res[0];
      assertTrue(result.hasOwnProperty("geo"));
      assertTrue(result.hasOwnProperty("otherGeo"));
      assertTrue(result.hasOwnProperty("value1"));
      assertTrue(result.hasOwnProperty("value2"));
      assertEqual(result.geo.type, "Point");
      assertEqual(result.geo.coordinates.length, 2);
      assertEqual(result.geo.coordinates[0], 50);
      assertEqual(result.geo.coordinates[1], 51);
      assertEqual(result.otherGeo.type, "Point");
      assertEqual(result.otherGeo.coordinates.length, 2);
      assertEqual(result.otherGeo.coordinates[0], 100);
      assertEqual(result.otherGeo.coordinates[1], 101);
      assertEqual(result.value1, 1);
      assertEqual(result.value2, "abc");
    },

    testForceWrongInvertedIndexWithGeoPresent: function () {
      const query = `FOR d IN ${cn2}  OPTIONS {indexHint: "${invertedIdxName2}", forceIndexHint: true, waitForSync: true} 
      FILTER GEO_DISTANCE([50, 50], d.geo) < 5000 
      RETURN d`;
      try {
        getIndexNames(query);
        fail();
      } catch (err) {
        assertEqual(errors.ERROR_QUERY_FORCED_INDEX_HINT_UNUSABLE.code, err.errorNum);
        assertTrue(err.errorMessage.includes("could not use index hint to serve query"));
      }
    },

    testForceInvertedIndexWithGeoPresent2: function () {
      const query = `FOR d IN ${cn2}  OPTIONS {indexHint: "${invertedIdxName2}", forceIndexHint: true, waitForSync: true}
     FILTER GEO_DISTANCE([150, 151], d.otherGeo) >= 0 SORT d.otherGeo.coordinates[0] ASC
      RETURN d`;
      const usedIndexes = getIndexNames(query);
      assertEqual(usedIndexes.length, 1);
      assertEqual(usedIndexes[0].length, 1);
      assertEqual(usedIndexes[0][0], invertedIdxName2);
      const res = db._query(query).toArray();
      assertEqual(res.length, 2);
      for (let i = 0; i < res.length; ++i) {
        const result = res[i];
        assertTrue(result.hasOwnProperty("geo"));
        assertTrue(result.hasOwnProperty("otherGeo"));
        assertTrue(result.hasOwnProperty("value1"));
        assertTrue(result.hasOwnProperty("value2"));
        assertEqual(result.geo.type, "Point");
        assertEqual(result.geo.coordinates.length, 2);
        assertEqual(result.geo.coordinates[0], 50 + (i * 2));
        assertEqual(result.geo.coordinates[1], 51 + (i * 2));
        assertEqual(result.otherGeo.type, "Point");
        assertEqual(result.otherGeo.coordinates.length, 2);
        assertEqual(result.otherGeo.coordinates[0], 100 + (i * 50));
        assertEqual(result.otherGeo.coordinates[1], 101 + (i * 50));
        assertEqual(result.value1, 1 + i);
        if (i === 0) {
          assertEqual(result.value2, "abc");
        } else {
          assertEqual(result.value2, "123");
        }
      }
    },

    testNotForceInvertedIndexWithGeoPresent2: function () {
      const query = `FOR d IN ${cn2}  OPTIONS {indexHint: "${invertedIdxName2}", waitForSync: true}
     FILTER GEO_DISTANCE([150, 151], d.otherGeo) >= 0 SORT d.otherGeo.coordinates[0] ASC
      RETURN d`;
      const usedIndexes = getIndexNames(query);
      assertEqual(usedIndexes.length, 1);
      assertEqual(usedIndexes[0].length, 1);
      assertEqual(usedIndexes[0][0], geoIdxName2);
      const res = db._query(query).toArray();
      assertEqual(res.length, 2);
      for (let i = 0; i < res.length; ++i) {
        const result = res[i];
        assertTrue(result.hasOwnProperty("geo"));
        assertTrue(result.hasOwnProperty("otherGeo"));
        assertTrue(result.hasOwnProperty("value1"));
        assertTrue(result.hasOwnProperty("value2"));
        assertEqual(result.geo.type, "Point");
        assertEqual(result.geo.coordinates.length, 2);
        assertEqual(result.geo.coordinates[0], 50 + (i * 2));
        assertEqual(result.geo.coordinates[1], 51 + (i * 2));
        assertEqual(result.otherGeo.type, "Point");
        assertEqual(result.otherGeo.coordinates.length, 2);
        assertEqual(result.otherGeo.coordinates[0], 100 + (i * 50));
        assertEqual(result.otherGeo.coordinates[1], 101 + (i * 50));
        assertEqual(result.value1, 1 + i);
        if (i === 0) {
          assertEqual(result.value2, "abc");
        } else {
          assertEqual(result.value2, "123");
        }
      }
    },

    testForceWrongGeoIndex: function () {
      const query = `FOR d IN ${cn2}  OPTIONS {indexHint: "${geoIdxName2}", forceIndexHint: true, waitForSync: true} 
      FILTER GEO_DISTANCE([50, 50], d.geo) < 50000
      RETURN d`;
      try {
        getIndexNames(query);
        fail();
      } catch (err) {
        assertEqual(errors.ERROR_QUERY_FORCED_INDEX_HINT_UNUSABLE.code, err.errorNum);
        assertTrue(err.errorMessage.includes("could not use index hint to serve query"));
      }
    },

    testForceWrongGeoIndex2: function () {
      const query = `FOR d IN ${cn2}  OPTIONS {indexHint: "${geoIdxName}", forceIndexHint: true, waitForSync: true} 
      FILTER GEO_DISTANCE([50, 50], d.otherGeo) < 50000
      RETURN d`;
      try {
        getIndexNames(query);
        fail();
      } catch (err) {
        assertEqual(errors.ERROR_QUERY_FORCED_INDEX_HINT_UNUSABLE.code, err.errorNum);
        assertTrue(err.errorMessage.includes("could not use index hint to serve query"));
      }
    },

    testForceInvertedIndexWithGeoPresent3: function () {
      const query = `FOR d IN ${cn2}  OPTIONS {indexHint: "${invertedIdxName2}", forceIndexHint: true, waitForSync: true} 
      FILTER GEO_DISTANCE([50, 51], d.geo) < 1 && GEO_DISTANCE([100, 101], d.otherGeo) < 1
      RETURN d`;
      const usedIndexes = getIndexNames(query);
      assertEqual(usedIndexes.length, 1);
      assertEqual(usedIndexes[0].length, 1);
      assertEqual(usedIndexes[0][0], invertedIdxName2);
      const res = db._query(query).toArray();
      assertEqual(res.length, 1);
      const result = res[0];
      assertTrue(result.hasOwnProperty("geo"));
      assertTrue(result.hasOwnProperty("otherGeo"));
      assertTrue(result.hasOwnProperty("value1"));
      assertTrue(result.hasOwnProperty("value2"));
      assertEqual(result.geo.type, "Point");
      assertEqual(result.geo.coordinates.length, 2);
      assertEqual(result.geo.coordinates[0], 50);
      assertEqual(result.geo.coordinates[1], 51);
      assertEqual(result.otherGeo.type, "Point");
      assertEqual(result.otherGeo.coordinates.length, 2);
      assertEqual(result.otherGeo.coordinates[0], 100);
      assertEqual(result.otherGeo.coordinates[1], 101);
      assertEqual(result.value1, 1);
      assertEqual(result.value2, "abc");
    },

    testNotForceInvertedIndexWithGeoPresent3: function () {
      const query = `FOR d IN ${cn2}  OPTIONS {indexHint: "${invertedIdxName2}", waitForSync: true} 
      FILTER GEO_DISTANCE([50, 51], d.geo) < 1 && GEO_DISTANCE([100, 101], d.otherGeo) < 1
      RETURN d`;
      const usedIndexes = getIndexNames(query);
      assertEqual(usedIndexes.length, 1);
      assertEqual(usedIndexes[0].length, 1);
      assertTrue(usedIndexes[0][0] === geoIdxName || usedIndexes[0][0] === geoIdxName2);
      const res = db._query(query).toArray();
      assertEqual(res.length, 1);
      const result = res[0];
      assertTrue(result.hasOwnProperty("geo"));
      assertTrue(result.hasOwnProperty("otherGeo"));
      assertTrue(result.hasOwnProperty("value1"));
      assertTrue(result.hasOwnProperty("value2"));
      assertEqual(result.geo.type, "Point");
      assertEqual(result.geo.coordinates.length, 2);
      assertEqual(result.geo.coordinates[0], 50);
      assertEqual(result.geo.coordinates[1], 51);
      assertEqual(result.otherGeo.type, "Point");
      assertEqual(result.otherGeo.coordinates.length, 2);
      assertEqual(result.otherGeo.coordinates[0], 100);
      assertEqual(result.otherGeo.coordinates[1], 101);
      assertEqual(result.value1, 1);
      assertEqual(result.value2, "abc");
    },

    testForcePersistentIndexWithGeoPresent: function () {
      const query = `FOR d IN ${cn2}  OPTIONS {indexHint: "${persistentIdxName}", forceIndexHint: true, waitForSync: true} 
      FILTER GEO_DISTANCE([50, 51], d.geo) < 5000 && d.value1 == 1
      RETURN d`;
      const usedIndexes = getIndexNames(query);
      assertEqual(usedIndexes.length, 1);
      assertEqual(usedIndexes[0].length, 1);
      assertEqual(usedIndexes[0][0], persistentIdxName);

      const res = db._query(query).toArray();
      assertEqual(res.length, 1);
      const result = res[0];
      assertTrue(result.hasOwnProperty("geo"));
      assertTrue(result.hasOwnProperty("otherGeo"));
      assertTrue(result.hasOwnProperty("value1"));
      assertTrue(result.hasOwnProperty("value2"));
      assertEqual(result.geo.type, "Point");
      assertEqual(result.geo.coordinates.length, 2);
      assertEqual(result.geo.coordinates[0], 50);
      assertEqual(result.geo.coordinates[1], 51);
      assertEqual(result.otherGeo.type, "Point");
      assertEqual(result.otherGeo.coordinates.length, 2);
      assertEqual(result.otherGeo.coordinates[0], 100);
      assertEqual(result.otherGeo.coordinates[1], 101);
      assertEqual(result.value1, 1);
      assertEqual(result.value2, "abc");
    },

    testNotForcePersistentIndexWithGeoPresent: function () {
      const query = `FOR d IN ${cn2}  OPTIONS {indexHint: "${persistentIdxName}", waitForSync: true} 
      FILTER GEO_DISTANCE([50, 51], d.geo) < 5000 && d.value1 == 1
      RETURN d`;
      const usedIndexes = getIndexNames(query);
      assertEqual(usedIndexes.length, 1);
      assertEqual(usedIndexes[0].length, 1);
      assertEqual(usedIndexes[0][0], geoIdxName);

      const res = db._query(query).toArray();
      assertEqual(res.length, 1);
      const result = res[0];
      assertTrue(result.hasOwnProperty("geo"));
      assertTrue(result.hasOwnProperty("otherGeo"));
      assertTrue(result.hasOwnProperty("value1"));
      assertTrue(result.hasOwnProperty("value2"));
      assertEqual(result.geo.type, "Point");
      assertEqual(result.geo.coordinates.length, 2);
      assertEqual(result.geo.coordinates[0], 50);
      assertEqual(result.geo.coordinates[1], 51);
      assertEqual(result.otherGeo.type, "Point");
      assertEqual(result.otherGeo.coordinates.length, 2);
      assertEqual(result.otherGeo.coordinates[0], 100);
      assertEqual(result.otherGeo.coordinates[1], 101);
      assertEqual(result.value1, 1);
      assertEqual(result.value2, "abc");
    },

    testForcePersistentIndexWithGeoPresent2: function () {
      const query = `FOR d IN ${cn2}  OPTIONS {indexHint: "${persistentIdxName2}", forceIndexHint: true, waitForSync: true} 
      FILTER GEO_DISTANCE([50, 51], d.geo) < 5000 && d.value1 == 1 && d.value2 == "abc"
      RETURN d`;
      const usedIndexes = getIndexNames(query);
      assertEqual(usedIndexes.length, 1);
      assertEqual(usedIndexes[0].length, 1);
      assertEqual(usedIndexes[0][0], persistentIdxName2);

      const res = db._query(query).toArray();
      assertEqual(res.length, 1);
      const result = res[0];
      assertTrue(result.hasOwnProperty("geo"));
      assertTrue(result.hasOwnProperty("otherGeo"));
      assertTrue(result.hasOwnProperty("value1"));
      assertTrue(result.hasOwnProperty("value2"));
      assertEqual(result.geo.type, "Point");
      assertEqual(result.geo.coordinates.length, 2);
      assertEqual(result.geo.coordinates[0], 50);
      assertEqual(result.geo.coordinates[1], 51);
      assertEqual(result.otherGeo.type, "Point");
      assertEqual(result.otherGeo.coordinates.length, 2);
      assertEqual(result.otherGeo.coordinates[0], 100);
      assertEqual(result.otherGeo.coordinates[1], 101);
      assertEqual(result.value1, 1);
      assertEqual(result.value2, "abc");
    },

    testNotForcePersistentIndexWithGeoPresent2: function () {
      const query = `FOR d IN ${cn2}  OPTIONS {indexHint: "${persistentIdxName2}", waitForSync: true} 
      FILTER GEO_DISTANCE([50, 51], d.geo) < 5000 && d.value1 == 1 && d.value2 == "abc"
      RETURN d`;
      const usedIndexes = getIndexNames(query);
      assertEqual(usedIndexes.length, 1);
      assertEqual(usedIndexes[0].length, 1);
      assertEqual(usedIndexes[0][0], geoIdxName);

      const res = db._query(query).toArray();
      assertEqual(res.length, 1);
      const result = res[0];
      assertTrue(result.hasOwnProperty("geo"));
      assertTrue(result.hasOwnProperty("otherGeo"));
      assertTrue(result.hasOwnProperty("value1"));
      assertTrue(result.hasOwnProperty("value2"));
      assertEqual(result.geo.type, "Point");
      assertEqual(result.geo.coordinates.length, 2);
      assertEqual(result.geo.coordinates[0], 50);
      assertEqual(result.geo.coordinates[1], 51);
      assertEqual(result.otherGeo.type, "Point");
      assertEqual(result.otherGeo.coordinates.length, 2);
      assertEqual(result.otherGeo.coordinates[0], 100);
      assertEqual(result.otherGeo.coordinates[1], 101);
      assertEqual(result.value1, 1);
      assertEqual(result.value2, "abc");
    },

    testForceIndexWith2GeoPresent: function () {
      const query = `FOR d IN ${cn2}  OPTIONS {indexHint: "${geoIdxName2}", forceIndexHint: true, waitForSync: true} 
      FILTER GEO_DISTANCE([50, 51], d.geo) <= 0 && GEO_DISTANCE([100, 101], d.otherGeo) <= 0
      RETURN d`;
      const usedIndexes = getIndexNames(query);
      assertEqual(usedIndexes.length, 1);
      assertEqual(usedIndexes[0].length, 1);
      assertEqual(usedIndexes[0][0], geoIdxName2);

      const res = db._query(query).toArray();
      assertEqual(res.length, 1);
      const result = res[0];
      assertTrue(result.hasOwnProperty("geo"));
      assertTrue(result.hasOwnProperty("otherGeo"));
      assertTrue(result.hasOwnProperty("value1"));
      assertTrue(result.hasOwnProperty("value2"));
      assertEqual(result.geo.type, "Point");
      assertEqual(result.geo.coordinates.length, 2);
      assertEqual(result.geo.coordinates[0], 50);
      assertEqual(result.geo.coordinates[1], 51);
      assertEqual(result.otherGeo.type, "Point");
      assertEqual(result.otherGeo.coordinates.length, 2);
      assertEqual(result.otherGeo.coordinates[0], 100);
      assertEqual(result.otherGeo.coordinates[1], 101);
      assertEqual(result.value1, 1);
      assertEqual(result.value2, "abc");
    },

    testNotForceIndexWith2GeoPresent: function () {
      const query = `FOR d IN ${cn2}  OPTIONS {indexHint: "${geoIdxName2}", waitForSync: true} 
      FILTER GEO_DISTANCE([50, 51], d.geo) < 5000 && GEO_DISTANCE([100, 101], d.otherGeo) < 1000
      RETURN d`;
      const usedIndexes = getIndexNames(query);
      assertEqual(usedIndexes.length, 1);
      assertEqual(usedIndexes[0].length, 1);
      assertTrue(usedIndexes[0][0] === geoIdxName || usedIndexes[0][0] === geoIdxName2);

      const res = db._query(query).toArray();
      assertEqual(res.length, 1);
      const result = res[0];
      assertTrue(result.hasOwnProperty("geo"));
      assertTrue(result.hasOwnProperty("otherGeo"));
      assertTrue(result.hasOwnProperty("value1"));
      assertTrue(result.hasOwnProperty("value2"));
      assertEqual(result.geo.type, "Point");
      assertEqual(result.geo.coordinates.length, 2);
      assertEqual(result.geo.coordinates[0], 50);
      assertEqual(result.geo.coordinates[1], 51);
      assertEqual(result.otherGeo.type, "Point");
      assertEqual(result.otherGeo.coordinates.length, 2);
      assertEqual(result.otherGeo.coordinates[0], 100);
      assertEqual(result.otherGeo.coordinates[1], 101);
      assertEqual(result.value1, 1);
      assertEqual(result.value2, "abc");
    },
  };
}

function indexHintTraversalSuite () {
  return {

    setUpAll: function () {
      let c = db._create(cn + "Vertex");
      let docs = [];
      for (let i = 0; i < 1000; ++i) {
        docs.push({ _key: "test" + i });
      }
      c.insert(docs);
      
      c = db._createEdgeCollection(cn + "Edge");
      // note: we want that every index gets a different selectivity estimate, so that we can be sure which
      // index the optimizer will use by default
      c.ensureIndex({type: 'persistent', name: 'from', fields: ['_from']});
      c.ensureIndex({type: 'persistent', name: 'from_value1', fields: ['_from', 'value1']});
      c.ensureIndex({type: 'persistent', name: 'from_value1_value2', fields: ['_from', 'value1', 'value2']});
      c.ensureIndex({type: 'persistent', name: 'to', fields: ['_to']});
      c.ensureIndex({type: 'persistent', name: 'to_value1', fields: ['_to', 'value1']});
      c.ensureIndex({type: 'persistent', name: 'to_value1_value2', fields: ['_to', 'value1', 'value2']});

      docs = [];
      for (let i = 0; i < 10000; ++i) {
        docs.push({ _from: cn + "Vertex/test" + (i % 100), _to: cn + "Vertex/test" + (i % 500), value1: (i % 3), value2: (i % 61) });
        if (docs.length === 1000) {
          c.insert(docs);
          docs = [];
        }
      }
      waitForEstimatorSync();
    },

    tearDownAll: function () {
      db._drop(cn + "Vertex");
      db._drop(cn + "Edge");
    },

    testTraversalUsesEdgeIndexWithoutHints : function () {
      ["inbound", "outbound"].forEach((dir) => {
        const query = `
FOR v, e, p IN 1..4 ${dir} '${cn}Vertex/test0' ${cn}Edge 
  RETURN v._key`;

        let plan = db._createStatement({ query }).explain().plan;
        let nodes = plan.nodes;
        let ns = nodes.filter((node) => node.type === 'TraversalNode');
        assertEqual(1, ns.length, query);
        let node = ns[0];

        let indexes = node.indexes;
        assertTrue(indexes.hasOwnProperty("base"), indexes);

        let baseIndexes = indexes.base;
        assertEqual(1, baseIndexes.length, baseIndexes);
        assertEqual("edge", baseIndexes[0].type);
        assertEqual("edge", baseIndexes[0].name);
        assertEqual([ (dir === "inbound" ? "_to" : "_from") ], baseIndexes[0].fields);
        assertTrue(indexes.hasOwnProperty("levels"), indexes);
        assertEqual({}, indexes.levels);

        assertTrue(node.hasOwnProperty("indexHint"), node);
        assertFalse(node.indexHint.forced, node.indexHint);
        assertEqual("none", node.indexHint.type, node.indexHint);
      });
    },
    
    testAnyTraversalUsesEdgeIndexWithoutHints : function () {
      const query = `
FOR v, e, p IN 1..4 ANY '${cn}Vertex/test0' ${cn}Edge 
  RETURN v._key`;

      let plan = db._createStatement({ query }).explain().plan;
      let nodes = plan.nodes;
      let ns = nodes.filter((node) => node.type === 'TraversalNode');
      assertEqual(1, ns.length, query);
      let node = ns[0];

      let indexes = node.indexes;
      assertTrue(indexes.hasOwnProperty("base"), indexes);

      let baseIndexes = indexes.base;
      assertEqual(2, baseIndexes.length, baseIndexes);
      assertEqual("edge", baseIndexes[0].type);
      assertEqual("edge", baseIndexes[1].type);
      assertEqual("edge", baseIndexes[0].name);
      assertEqual("edge", baseIndexes[1].name);
      assertEqual([ "_from" ], baseIndexes[0].fields);
      assertEqual([ "_to" ], baseIndexes[1].fields);

      assertTrue(indexes.hasOwnProperty("levels"), indexes);
      assertEqual({}, indexes.levels);

      assertTrue(node.hasOwnProperty("indexHint"), node);
      assertFalse(node.indexHint.forced, node.indexHint);
      assertEqual("none", node.indexHint.type, node.indexHint);
    },
    
    testTraversalUsesIndexHintForBase : function () {
      [
        [ "outbound", [
          ["edge", ["_from"]], 
          ["from", ["_from"]], 
          ["from_value1", ["_from", "value1"]], 
          ["from_value1_value2", ["_from", "value1", "value2"]],
        ]],
        [ "inbound", [
          ["edge", ["_to"]], 
          ["to", ["_to"]], 
          ["to_value1", ["_to", "value1"]], 
          ["to_value1_value2", ["_to", "value1", "value2"]],
        ]],
      ].forEach((dirAndIndexes) => {
        let [dir, indexes] = dirAndIndexes;

        indexes.forEach((idxData) => {
          let [idxName, idxFields] = idxData;
          const query = `
FOR v, e, p IN 1..4 ${dir} '${cn}Vertex/test0' ${cn}Edge OPTIONS { indexHint: { ${cn}Edge: { ${dir}: { base: "${idxName}" } } } } 
  RETURN v._key`;

          let plan = db._createStatement({ query }).explain().plan;
          let nodes = plan.nodes;
          let ns = nodes.filter((node) => node.type === 'TraversalNode');
          assertEqual(1, ns.length, query);
          let node = ns[0];

          let indexes = node.indexes;
          assertTrue(indexes.hasOwnProperty("base"), indexes);

          let baseIndexes = indexes.base;
          assertEqual(1, baseIndexes.length, baseIndexes);
          if (idxName === "edge") {
            assertEqual("edge", baseIndexes[0].type);
          } else {
            assertEqual("persistent", baseIndexes[0].type);
          }
          assertEqual(idxName, baseIndexes[0].name);
          assertEqual(idxFields, baseIndexes[0].fields);
          
          assertTrue(indexes.hasOwnProperty("levels"), indexes);
          assertEqual({}, indexes.levels);

          assertTrue(node.hasOwnProperty("indexHint"), node);
          assertFalse(node.indexHint.forced, node.indexHint);
          assertEqual("nested", node.indexHint.type, node.indexHint);
        });
      });
    },
    
    testTraversalUsesIndexHintsOnLevels : function () {
      [
        [ "outbound", [
          ["edge", ["_from"]], 
          ["from", ["_from"]], 
          ["from_value1", ["_from", "value1"]], 
          ["from_value1_value2", ["_from", "value1", "value2"]],
        ]],
        [ "inbound", [
          ["edge", ["_to"]], 
          ["to", ["_to"]], 
          ["to_value1", ["_to", "value1"]], 
          ["to_value1_value2", ["_to", "value1", "value2"]],
        ]],
      ].forEach((dirAndIndexes) => {
        let [dir, indexes] = dirAndIndexes;

        indexes.forEach((idxData) => {
          let [idxName, idxFields] = idxData;
          const query = `
FOR v, e, p IN 1..5 ${dir} '${cn}Vertex/test0' ${cn}Edge OPTIONS { indexHint: { ${cn}Edge: { ${dir}: { base: "edge", "1": "${idxName}", "3": "edge" } } } }
  FILTER p.edges[1].${dir === 'inbound' ? '_to' : '_from'} == '${cn}Vertex/test0'
  FILTER p.edges[3].${dir === 'inbound' ? '_to' : '_from'} == '${cn}Vertex/test0'
  FILTER p.edges[4].${dir === 'inbound' ? '_to' : '_from'} == '${cn}Vertex/test0'
  RETURN v._key`;

          let plan = db._createStatement({ query }).explain().plan;
          let nodes = plan.nodes;
          let ns = nodes.filter((node) => node.type === 'TraversalNode');
          assertEqual(1, ns.length, query);
          let node = ns[0];

          let indexes = node.indexes;
          assertTrue(indexes.hasOwnProperty("base"), indexes);

          let baseIndexes = indexes.base;
          assertEqual(1, baseIndexes.length, baseIndexes);
          assertEqual("edge", baseIndexes[0].type);
          assertEqual("edge", baseIndexes[0].name);
          assertEqual([ dir === 'inbound' ? '_to' : '_from' ], baseIndexes[0].fields);

          assertTrue(indexes.hasOwnProperty("levels"), indexes);
          assertTrue(indexes.levels.hasOwnProperty("1"), indexes);
          assertTrue(indexes.levels.hasOwnProperty("3"), indexes);
          assertFalse(indexes.levels.hasOwnProperty("0"), indexes);
          assertFalse(indexes.levels.hasOwnProperty("2"), indexes);
          assertFalse(indexes.levels.hasOwnProperty("5"), indexes);

          let level1 = indexes.levels["1"];
          assertEqual(1, level1.length, level1);
          if (idxName === "edge") {
            assertEqual("edge", level1[0].type);
          } else {
            assertEqual("persistent", level1[0].type);
          }
          assertEqual(idxName, level1[0].name);
          assertEqual(idxFields, level1[0].fields);
          
          let level3 = indexes.levels["3"];
          assertEqual(1, level3.length, level3);
          assertEqual("edge", level3[0].type);
          assertEqual("edge", level3[0].name);
          assertEqual([ (dir === "inbound" ? "_to" : "_from") ], level3[0].fields);
          
          // no index hint was given to level 4, but it will implicitly use the
          // index hint for base then
          let level4 = indexes.levels["4"];
          assertEqual(1, level4.length, level4);
          assertEqual("edge", level4[0].type);
          assertEqual("edge", level4[0].name);
          assertEqual([ (dir === "inbound" ? "_to" : "_from") ], level4[0].fields);

          assertTrue(node.hasOwnProperty("indexHint"), node);
          assertFalse(node.indexHint.forced, node.indexHint);
          assertEqual("nested", node.indexHint.type, node.indexHint);
        });
      });
    },
    
    testAnyTraversalUsesIndexHintsOnLevels : function () {
      const query = `
FOR v, e, p IN 1..5 ANY '${cn}Vertex/test0' ${cn}Edge OPTIONS { indexHint: { ${cn}Edge: { outbound: { base: "edge", "1": "from_value1", "3": "from_value1_value2" }, inbound: { base: "to_value1", "1": "edge", "4": "to_value1_value2" } } } }
  FILTER p.edges[1]._to == '${cn}Vertex/test0'
  FILTER p.edges[1]._from == '${cn}Vertex/test0'
  FILTER p.edges[3]._from == '${cn}Vertex/test0'
  FILTER p.edges[4]._to == '${cn}Vertex/test0'
  RETURN v._key`;

      let plan = db._createStatement({ query }).explain().plan;
      let nodes = plan.nodes;
      let ns = nodes.filter((node) => node.type === 'TraversalNode');
      assertEqual(1, ns.length, query);
      let node = ns[0];

      let indexes = node.indexes;
      assertTrue(indexes.hasOwnProperty("base"), indexes);

      let baseIndexes = indexes.base;
      assertEqual(2, baseIndexes.length, baseIndexes);

      assertEqual("edge", baseIndexes[0].type);
      assertEqual("edge", baseIndexes[0].name);
      assertEqual(["_from"], baseIndexes[0].fields);
      assertEqual("persistent", baseIndexes[1].type);
      assertEqual("to_value1", baseIndexes[1].name);
      assertEqual(["_to", "value1"], baseIndexes[1].fields);

      assertTrue(indexes.hasOwnProperty("levels"), indexes);
      let levelIndexes = indexes.levels;
      
      assertTrue(indexes.levels.hasOwnProperty("1"), indexes);
      assertTrue(indexes.levels.hasOwnProperty("3"), indexes);
      assertTrue(indexes.levels.hasOwnProperty("4"), indexes);
      assertFalse(indexes.levels.hasOwnProperty("0"), indexes);
      assertFalse(indexes.levels.hasOwnProperty("2"), indexes);
      assertFalse(indexes.levels.hasOwnProperty("5"), indexes);

      let level1 = indexes.levels["1"];
      assertEqual(2, level1.length);

      assertEqual("persistent", level1[0].type);
      assertEqual("from_value1", level1[0].name);
      assertEqual(["_from", "value1"], level1[0].fields);
      
      assertEqual("edge", level1[1].type);
      assertEqual("edge", level1[1].name);
      assertEqual(["_from"], level1[1].fields);

      let level3 = indexes.levels["3"];
      assertEqual(2, level3.length);

      assertEqual("persistent", level3[0].type);
      assertEqual("from_value1_value2", level3[0].name);
      assertEqual(["_from", "value1", "value2"], level3[0].fields);
      
      assertEqual("persistent", level3[1].type);
      assertEqual("to_value1", level3[1].name);
      assertEqual(["_to", "value1"], level3[1].fields);
      
      let level4 = indexes.levels["4"];
      assertEqual(2, level4.length);

      assertEqual("edge", level4[0].type);
      assertEqual("edge", level4[0].name);
      assertEqual(["_from"], level4[0].fields);
      
      assertEqual("persistent", level4[1].type);
      assertEqual("to_value1_value2", level4[1].name);
      assertEqual(["_to", "value1", "value2"], level4[1].fields);
    },
    
    testTraversalMultipleIndexHints : function () {
      const query = `
FOR v, e, p IN 1..2 OUTBOUND '${cn}Vertex/test0' ${cn}Edge OPTIONS { indexHint: { ${cn}Edge: { outbound: { base: ["from_value1", "from_value1_value2"] } } } }
  FILTER p.edges[0]._from == '${cn}Vertex/test0'
  RETURN v._key`;

      let plan = db._createStatement({ query }).explain().plan;
      let nodes = plan.nodes;
      let ns = nodes.filter((node) => node.type === 'TraversalNode');
      assertEqual(1, ns.length, query);
      let node = ns[0];

      let indexes = node.indexes;
      assertTrue(indexes.hasOwnProperty("base"), indexes);

      let baseIndexes = indexes.base;
      assertEqual(1, baseIndexes.length, baseIndexes);

      assertEqual("persistent", baseIndexes[0].type);
      assertEqual("from_value1", baseIndexes[0].name);
      assertEqual(["_from", "value1"], baseIndexes[0].fields);

      assertTrue(indexes.hasOwnProperty("levels"), indexes);
      let levelIndexes = indexes.levels;
      
      assertTrue(indexes.levels.hasOwnProperty("0"), indexes);
      assertFalse(indexes.levels.hasOwnProperty("1"), indexes);

      let level0 = indexes.levels["0"];
      assertEqual(1, level0.length);

      assertEqual("persistent", level0[0].type);
      assertEqual("from_value1", level0[0].name);
      assertEqual(["_from", "value1"], level0[0].fields);
    },
    
    testTraversalMultipleIndexHints2 : function () {
      const query = `
FOR v, e, p IN 1..2 OUTBOUND '${cn}Vertex/test0' ${cn}Edge OPTIONS { indexHint: { ${cn}Edge: { outbound: { base: ["from_value1", "from_value1_value2"], "0": ["from_value1_value2"] } } } } 
  FILTER p.edges[0]._from == '${cn}Vertex/test0'
  RETURN v._key`;

      let plan = db._createStatement({ query }).explain().plan;
      let nodes = plan.nodes;
      let ns = nodes.filter((node) => node.type === 'TraversalNode');
      assertEqual(1, ns.length, query);
      let node = ns[0];

      let indexes = node.indexes;
      assertTrue(indexes.hasOwnProperty("base"), indexes);

      let baseIndexes = indexes.base;
      assertEqual(1, baseIndexes.length, baseIndexes);

      assertEqual("persistent", baseIndexes[0].type);
      assertEqual("from_value1", baseIndexes[0].name);
      assertEqual(["_from", "value1"], baseIndexes[0].fields);

      assertTrue(indexes.hasOwnProperty("levels"), indexes);
      let levelIndexes = indexes.levels;
      
      assertTrue(indexes.levels.hasOwnProperty("0"), indexes);
      assertFalse(indexes.levels.hasOwnProperty("1"), indexes);

      let level0 = indexes.levels["0"];
      assertEqual(1, level0.length);

      assertEqual("persistent", level0[0].type);
      assertEqual("from_value1_value2", level0[0].name);
      assertEqual(["_from", "value1", "value2"], level0[0].fields);
    },
    
    testTraversalWithWrongIndexHints : function () {
      [
        [ "outbound", [ "to", "to_value1", "peter" ] ],
        [ "inbound", [ "from", "from_value1", "peter" ] ],
      ].forEach((dirAndIndexes) => {
        let [dir, indexes] = dirAndIndexes;

        indexes.forEach((idxName) => {
          const query = `
FOR v, e, p IN 1..4 ${dir} '${cn}Vertex/test0' ${cn}Edge OPTIONS { indexHint: { ${cn}Edge: { ${dir}: { base: "${idxName}", "1": "${idxName}", "3": "edge" } } } }
  FILTER p.edges[1].${dir === 'inbound' ? '_to' : '_from'} == '${cn}Vertex/test0'
  FILTER p.edges[3].${dir === 'inbound' ? '_to' : '_from'} == '${cn}Vertex/test0'
  RETURN v._key`;

          let plan = db._createStatement({ query }).explain().plan;
          let nodes = plan.nodes;
          let ns = nodes.filter((node) => node.type === 'TraversalNode');
          assertEqual(1, ns.length, query);
          let node = ns[0];

          let indexes = node.indexes;
          assertTrue(indexes.hasOwnProperty("base"), indexes);

          let baseIndexes = indexes.base;
          assertEqual(1, baseIndexes.length, baseIndexes);
          assertEqual("edge", baseIndexes[0].type);
          assertEqual("edge", baseIndexes[0].name);
          assertEqual([ dir === 'inbound' ? '_to' : '_from' ], baseIndexes[0].fields);

          assertTrue(indexes.hasOwnProperty("levels"), indexes);
          assertTrue(indexes.levels.hasOwnProperty("1"), indexes);
          assertTrue(indexes.levels.hasOwnProperty("3"), indexes);
          assertFalse(indexes.levels.hasOwnProperty("0"), indexes);
          assertFalse(indexes.levels.hasOwnProperty("2"), indexes);
          assertFalse(indexes.levels.hasOwnProperty("4"), indexes);

          // the edge index will be used on the sub-levels as well
          let level1 = indexes.levels["1"];
          assertEqual(1, level1.length, level1);
          assertEqual("edge", level1[0].type);
          assertEqual("edge", level1[0].name);
          assertEqual([ (dir === "inbound" ? "_to" : "_from") ], level1[0].fields);
          
          let level3 = indexes.levels["3"];
          assertEqual(1, level3.length, level3);
          assertEqual("edge", level3[0].type);
          assertEqual("edge", level3[0].name);
          assertEqual([ (dir === "inbound" ? "_to" : "_from") ], level3[0].fields);

          assertTrue(node.hasOwnProperty("indexHint"), node);
          assertFalse(node.indexHint.forced, node.indexHint);
          assertEqual("nested", node.indexHint.type, node.indexHint);
        });
      });
    },

  };

}

function indexHintDisableIndexSuite () {
  return {

    setUpAll: function () {
      db._drop(cn);
      let c = db._create(cn);
      c.ensureIndex({type: 'persistent', name: 'value1', fields: ['value1']});
    },

    tearDownAll: function () {
      db._drop(cn);
    },

    testDisableIndexOff: function () {
      ["", "OPTIONS { disableIndex: false }"].forEach((option) => {
        const queries = [
          [`FOR doc IN ${cn} ${option} RETURN doc`, null, []],
          [`FOR doc IN ${cn} ${option} RETURN doc._key`, 'primary', ['_key'], true],
          [`FOR doc IN ${cn} ${option} FILTER doc._key == '123' RETURN doc`, 'primary', [], false],
          [`FOR doc IN ${cn} ${option} FILTER doc._key == '123' RETURN doc._key`, 'primary', ['_key'], true],
          [`FOR doc IN ${cn} ${option} FILTER doc.value1 == 123 RETURN doc`, 'value1', [], false],
          [`FOR doc IN ${cn} ${option} FILTER doc.value1 == 123 RETURN doc.value1`, 'value1', ['value1'], true],
          [`FOR doc IN ${cn} ${option} FILTER doc.value1 == 123 RETURN doc.value2`, 'value1', ['value2'], false],
          [`FOR doc IN ${cn} ${option} SORT doc._key RETURN doc._key`, 'primary', ['_key'], true],
          [`FOR doc IN ${cn} ${option} SORT doc._key RETURN 1`, 'primary', [], false],
          [`FOR doc IN ${cn} ${option} SORT doc._key DESC RETURN doc._key`, 'primary', ['_key'], true],
          [`FOR doc IN ${cn} ${option} SORT doc._key DESC RETURN 1`, 'primary', [], false],
          [`FOR doc IN ${cn} ${option} SORT doc.value1 RETURN doc.value1`, 'value1', ['value1'], true],
          [`FOR doc IN ${cn} ${option} SORT doc.value1 RETURN doc.value2`, 'value1', ['value2'], false],
        ];

        queries.forEach((query) => {
          let plan = db._createStatement({ query: query[0], bindVars: null, options: {optimizer: {rules: ["-optimize-cluster-single-document-operations"]}}}).explain().plan;
          let nodes = plan.nodes;

          let node;

          if (query[1] === null) {
            // expect EnumerateCollectionNode
            assertEqual(0, nodes.filter((node) => node.type === 'IndexNode').length);
            let ns = nodes.filter((node) => node.type === 'EnumerateCollectionNode');
            assertEqual(1, ns.length, query);
            node = ns[0];
          } else {
            // expect IndexNode
            assertEqual(0, nodes.filter((node) => node.type === 'EnumerateCollectionNode').length);
            let ns = nodes.filter((node) => node.type === 'IndexNode');
            assertEqual(1, ns.length, query);
            node = ns[0];
            assertEqual(1, node.indexes.length, query);
            assertEqual(query[1], node.indexes[0].name, query);
            assertEqual(query[3], node.indexCoversProjections, query);
          }
          assertEqual(normalize(query[2]), normalize(node.projections));
        });
      });
    },

    testDisableIndexOn: function () {
      const queries = [
        [`FOR doc IN ${cn} OPTIONS { disableIndex: true } RETURN 1`, []],
        [`FOR doc IN ${cn} OPTIONS { disableIndex: true } RETURN doc`, []],
        [`FOR doc IN ${cn} OPTIONS { disableIndex: true } RETURN doc._key`, ['_key']],
        [`FOR doc IN ${cn} OPTIONS { disableIndex: true } FILTER doc._key == '123' RETURN doc`, []],
        [`FOR doc IN ${cn} OPTIONS { disableIndex: true } FILTER doc._key == '123' RETURN doc._key`, ['_key']],
        [`FOR doc IN ${cn} OPTIONS { disableIndex: true } FILTER doc.value1 == 123 RETURN doc`, []],
        [`FOR doc IN ${cn} OPTIONS { disableIndex: true } FILTER doc.value1 == 123 RETURN doc.value1`, ['value1']],
        [`FOR doc IN ${cn} OPTIONS { disableIndex: true } FILTER doc.value1 == 123 RETURN doc.value2`, ['value2']],
        [`FOR doc IN ${cn} OPTIONS { disableIndex: true } SORT doc._key RETURN doc._key`, ['_key']],
        [`FOR doc IN ${cn} OPTIONS { disableIndex: true } SORT doc._key RETURN 1`, ['_key']],
        [`FOR doc IN ${cn} OPTIONS { disableIndex: true } SORT doc._key DESC RETURN doc._key`, ['_key']],
        [`FOR doc IN ${cn} OPTIONS { disableIndex: true } SORT doc._key DESC RETURN 1`, ['_key']],
        [`FOR doc IN ${cn} OPTIONS { disableIndex: true } SORT doc.value1 RETURN doc.value1`, ['value1']],
        [`FOR doc IN ${cn} OPTIONS { disableIndex: true } SORT doc.value1 RETURN doc.value2`, ['value1', 'value2']],
      ];

      queries.forEach((query) => {
        let plan = db._createStatement(query[0], null, {optimizer: {rules: ["-optimize-cluster-single-document-operations"]}}).explain().plan;
        let nodes = plan.nodes;

        assertEqual(0, nodes.filter((node) => node.type === 'IndexNode').length);
        let ns = nodes.filter((node) => node.type === 'EnumerateCollectionNode');
        assertEqual(1, ns.length, query);
        let node = ns[0];
        assertEqual(normalize(query[1]), normalize(node.projections), query);
      });
    },

  };
}

jsunity.run(indexHintCollectionSuite);
jsunity.run(indexHintTraversalSuite);
jsunity.run(indexHintDisableIndexSuite);

return jsunity.done();
