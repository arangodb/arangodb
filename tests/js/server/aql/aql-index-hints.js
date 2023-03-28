/*jshint globalstrict:false, strict:false, maxlen: 500 */
/*global fail, assertEqual, assertNotEqual, assertTrue, AQL_EXPLAIN */

////////////////////////////////////////////////////////////////////////////////
/// @brief tests for Ahuacatl, skiplist index queries
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2010-2016 ArangoDB GmbH, Cologne, Germany
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
/// @author Michael Hackstein
/// @author Copyright 2016, ArangoDB GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

const internal = require("internal");
const db = internal.db;
const jsunity = require("jsunity");
const errors = internal.errors;
const analyzers = require("@arangodb/analyzers");

function indexHintSuite() {
  const getIndexNames = function (query) {
    return AQL_EXPLAIN(query)
      .plan.nodes.filter(node => (node.type === 'IndexNode' ||
                                  node.type === 'SingleRemoteOperationNode'))
      .map(node => node.indexes.map(index => index.name));
  };

  const cn = 'UnitTestsIndexHints';
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

////////////////////////////////////////////////////////////////////////////////
/// @brief set up
////////////////////////////////////////////////////////////////////////////////

    setUpAll: function () {
      internal.db._drop(cn);
      collection = internal.db._create(cn);

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

      internal.db._drop(cn2);
      collection = internal.db._create(cn2);
      collection.ensureIndex({type: "persistent", fields: ["value1"], name: persistentIdxName});
      collection.ensureIndex({type: "persistent", fields: ["value2"], name: persistentIdxName2});
      collection.ensureIndex({type: "geo", geoJson: true, fields: ["geo"], name: geoIdxName});
      collection.ensureIndex({type: "geo", geoJson: true, fields: ["otherGeo"], name: geoIdxName2});
      analyzers.save("geo_json", "geojson", {type: "point"}, ["frequency", "norm", "position"]);
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
        otherGeo: {type: "Point", coordinates: [60, 61]},
        value1: 1,
        value2: "abc"
      });
      collection.insert({
        geo: {type: "Point", coordinates: [52, 53]},
        otherGeo: {type: "Point", coordinates: [70, 71]},
        value1: 2,
        value2: "123"
      });
    },

    tearDownAll: function () {
      internal.db._drop(cn);
      internal.db._drop(cn2);
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
        let queryExplain = AQL_EXPLAIN(prefix + JSON.stringify(indexHint[0])).plan.nodes;
        queryExplain.filter((node) => node.type === "IndexNode").forEach((node) => {
            assertNotEqual(indexHint[1].indexOf(node.indexes[0].name), -1);
        });
      });
    },

    testForceInvertedIndexWithGeoPresent: function () {
      const query = `FOR d IN ${cn2}  OPTIONS {indexHint: "${invertedIdxName}", forceIndexHint: true, waitForSync: true} 
      FILTER GEO_DISTANCE([70, 170], d.geo) > 50 SORT d.geo.coordinates[0] ASC
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
        assertEqual(result.otherGeo.coordinates[0], 60 + (i * 10));
        assertEqual(result.otherGeo.coordinates[1], 61 + (i * 10));
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
      FILTER GEO_DISTANCE([50, 50], d.geo) > 50 SORT d.geo.coordinates[0] ASC
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
        assertEqual(result.otherGeo.coordinates[0], 60 + (i * 10));
        assertEqual(result.otherGeo.coordinates[1], 61 + (i * 10));
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
      assertEqual(result.otherGeo.coordinates[0], 60);
      assertEqual(result.otherGeo.coordinates[1], 61);
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
      assertEqual(result.otherGeo.coordinates[0], 60);
      assertEqual(result.otherGeo.coordinates[1], 61);
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
      FILTER GEO_DISTANCE([70, 71], d.otherGeo) >= 0 SORT d.otherGeo.coordinates[0] ASC
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
        assertEqual(result.otherGeo.coordinates[0], 60 + (i * 10));
        assertEqual(result.otherGeo.coordinates[1], 61 + (i * 10));
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
      FILTER GEO_DISTANCE([70, 71], d.otherGeo) >= 0 SORT d.otherGeo.coordinates[0] ASC
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
        assertEqual(result.otherGeo.coordinates[0], 60 + (i * 10));
        assertEqual(result.otherGeo.coordinates[1], 61 + (i * 10));
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
      FILTER GEO_DISTANCE([50, 51], d.geo) < 1 && GEO_DISTANCE([60, 61], d.otherGeo) < 1
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
      assertEqual(result.otherGeo.coordinates[0], 60);
      assertEqual(result.otherGeo.coordinates[1], 61);
      assertEqual(result.value1, 1);
      assertEqual(result.value2, "abc");
    },

    testNotForceInvertedIndexWithGeoPresent3: function () {
      const query = `FOR d IN ${cn2}  OPTIONS {indexHint: "${invertedIdxName2}", waitForSync: true} 
      FILTER GEO_DISTANCE([50, 51], d.geo) < 1 && GEO_DISTANCE([60, 61], d.otherGeo) < 1
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
      assertEqual(result.otherGeo.coordinates[0], 60);
      assertEqual(result.otherGeo.coordinates[1], 61);
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
      assertEqual(result.otherGeo.coordinates[0], 60);
      assertEqual(result.otherGeo.coordinates[1], 61);
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
      assertEqual(result.otherGeo.coordinates[0], 60);
      assertEqual(result.otherGeo.coordinates[1], 61);
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
      assertEqual(result.otherGeo.coordinates[0], 60);
      assertEqual(result.otherGeo.coordinates[1], 61);
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
      assertEqual(result.otherGeo.coordinates[0], 60);
      assertEqual(result.otherGeo.coordinates[1], 61);
      assertEqual(result.value1, 1);
      assertEqual(result.value2, "abc");
    },

    testForceIndexWith2GeoPresent: function () {
      const query = `FOR d IN ${cn2}  OPTIONS {indexHint: "${geoIdxName2}", forceIndexHint: true, waitForSync: true} 
      FILTER GEO_DISTANCE([50, 51], d.geo) <= 0 && GEO_DISTANCE([60, 61], d.otherGeo) <= 0
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
      assertEqual(result.otherGeo.coordinates[0], 60);
      assertEqual(result.otherGeo.coordinates[1], 61);
      assertEqual(result.value1, 1);
      assertEqual(result.value2, "abc");
    },

    testNotForceIndexWith2GeoPresent: function () {
      const query = `FOR d IN ${cn2}  OPTIONS {indexHint: "${geoIdxName2}", waitForSync: true} 
      FILTER GEO_DISTANCE([50, 51], d.geo) < 5000 && GEO_DISTANCE([60, 61], d.otherGeo) < 1000
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
      assertEqual(result.otherGeo.coordinates[0], 60);
      assertEqual(result.otherGeo.coordinates[1], 61);
      assertEqual(result.value1, 1);
      assertEqual(result.value2, "abc");
    },
  };
}

function indexHintDisableIndexSuite() {
  const cn = 'UnitTestsIndexHints';

  return {

    setUpAll: function () {
      internal.db._drop(cn);
      let c = internal.db._create(cn);
      c.ensureIndex({type: 'persistent', name: 'value1', fields: ['value1']});
    },

    tearDownAll: function () {
      internal.db._drop(cn);
    },

    testDisableIndexOff : function () {
      [ "", "OPTIONS { disableIndex: false }"].forEach((option) => {
        const queries = [
          [ `FOR doc IN ${cn} ${option} RETURN doc`, null, [] ],
          [ `FOR doc IN ${cn} ${option} RETURN doc._key`, 'primary', ['_key'], true ],
          [ `FOR doc IN ${cn} ${option} FILTER doc._key == '123' RETURN doc`, 'primary', [], false ],
          [ `FOR doc IN ${cn} ${option} FILTER doc._key == '123' RETURN doc._key`, 'primary', ['_key'], true ],
          [ `FOR doc IN ${cn} ${option} FILTER doc.value1 == 123 RETURN doc`, 'value1', [], false ],
          [ `FOR doc IN ${cn} ${option} FILTER doc.value1 == 123 RETURN doc.value1`, 'value1', ['value1'], true ],
          [ `FOR doc IN ${cn} ${option} FILTER doc.value1 == 123 RETURN doc.value2`, 'value1', ['value2'], false ],
          [ `FOR doc IN ${cn} ${option} SORT doc._key RETURN doc._key`, 'primary', ['_key'], true ],
          [ `FOR doc IN ${cn} ${option} SORT doc._key RETURN 1`, 'primary', [], false ],
          [ `FOR doc IN ${cn} ${option} SORT doc._key DESC RETURN doc._key`, 'primary', ['_key'], true ],
          [ `FOR doc IN ${cn} ${option} SORT doc._key DESC RETURN 1`, 'primary', [], false ],
          [ `FOR doc IN ${cn} ${option} SORT doc.value1 RETURN doc.value1`, 'value1', ['value1'], true ],
          [ `FOR doc IN ${cn} ${option} SORT doc.value1 RETURN doc.value2`, 'value1', ['value2'], false ],
        ];

        queries.forEach((query) => {
          let plan = AQL_EXPLAIN(query[0], null, { optimizer: { rules: ["-optimize-cluster-single-document-operations"] } }).plan;
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
          assertEqual(query[2], node.projections);
        });
      });
    },

    testDisableIndexOn : function () {
      const queries = [
        [ `FOR doc IN ${cn} OPTIONS { disableIndex: true } RETURN 1`, [] ],
        [ `FOR doc IN ${cn} OPTIONS { disableIndex: true } RETURN doc`, [] ],
        [ `FOR doc IN ${cn} OPTIONS { disableIndex: true } RETURN doc._key`, ['_key'] ],
        [ `FOR doc IN ${cn} OPTIONS { disableIndex: true } FILTER doc._key == '123' RETURN doc`, [] ],
        [ `FOR doc IN ${cn} OPTIONS { disableIndex: true } FILTER doc._key == '123' RETURN doc._key`, ['_key'] ],
        [ `FOR doc IN ${cn} OPTIONS { disableIndex: true } FILTER doc.value1 == 123 RETURN doc`, [] ],
        [ `FOR doc IN ${cn} OPTIONS { disableIndex: true } FILTER doc.value1 == 123 RETURN doc.value1`, ['value1'] ],
        [ `FOR doc IN ${cn} OPTIONS { disableIndex: true } FILTER doc.value1 == 123 RETURN doc.value2`, ['value1', 'value2'] ],
        [ `FOR doc IN ${cn} OPTIONS { disableIndex: true } SORT doc._key RETURN doc._key`, ['_key'] ],
        [ `FOR doc IN ${cn} OPTIONS { disableIndex: true } SORT doc._key RETURN 1`, ['_key'] ],
        [ `FOR doc IN ${cn} OPTIONS { disableIndex: true } SORT doc._key DESC RETURN doc._key`, ['_key'] ],
        [ `FOR doc IN ${cn} OPTIONS { disableIndex: true } SORT doc._key DESC RETURN 1`, ['_key'] ],
        [ `FOR doc IN ${cn} OPTIONS { disableIndex: true } SORT doc.value1 RETURN doc.value1`, ['value1'] ],
        [ `FOR doc IN ${cn} OPTIONS { disableIndex: true } SORT doc.value1 RETURN doc.value2`, ['value1', 'value2'] ],
      ];

      queries.forEach((query) => {
        let plan = AQL_EXPLAIN(query[0], null, { optimizer: { rules: ["-optimize-cluster-single-document-operations"] } }).plan;
        let nodes = plan.nodes;

        assertEqual(0, nodes.filter((node) => node.type === 'IndexNode').length);
        let ns = nodes.filter((node) => node.type === 'EnumerateCollectionNode');
        assertEqual(1, ns.length, query);
        let node = ns[0];
        assertEqual(query[1], node.projections.sort(), query);
      });
    },

  };
}

jsunity.run(indexHintSuite);
jsunity.run(indexHintDisableIndexSuite);

return jsunity.done();
