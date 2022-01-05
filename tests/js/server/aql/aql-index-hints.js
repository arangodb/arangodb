/*jshint globalstrict:false, strict:false, maxlen: 500 */
/*global fail, assertEqual, assertNotEqual, AQL_EXPLAIN */

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

function indexHintSuite() {
  const getIndexNames = function (query) {
    return AQL_EXPLAIN(query, {}, {optimizer: {rules: ["-all", "+use-indexes"]}})
      .plan.nodes.filter(node => (node.type === 'IndexNode'))
      .map(node => node.indexes.map(index => index.name));
  };

  const cn = 'UnitTestsIndexHints';
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
    },

    tearDownAll: function () {
      internal.db._drop(cn);
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
        `FOR doc IN ${cn} OPTIONS {indexHint: 'primary', forceIndexHint: true} FILTER doc._key == 'test' || doc.testi == 99 RETURN doc`,
        `FOR doc IN ${cn} OPTIONS {indexHint: 'primary', forceIndexHint: true} FILTER doc.testi == 99 || doc._key == 'test' RETURN doc`,
      ].forEach((query) => {
        // no indexes used here
        const usedIndexes = getIndexNames(query);
        assertEqual(usedIndexes.length, 0, query);
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
    }
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
        ];

        queries.forEach((query) => {
          let plan = AQL_EXPLAIN(query[0], null, { optimizer: { rules: ["-optimize-cluster-single-document-operations"] } }).plan;
          let nodes = plan.nodes;

          let node;

          if (query[1] === null) {
            // expect EnumerateCollectionNode
            assertEqual(0, nodes.filter((node) => node.type === 'IndexNode').length);
            let ns = nodes.filter((node) => node.type === 'EnumerateCollectionNode');
            assertEqual(1, ns.length);
            node = ns[0];
          } else {
            // expect IndexNode
            assertEqual(0, nodes.filter((node) => node.type === 'EnumerateCollectionNode').length);
            let ns = nodes.filter((node) => node.type === 'IndexNode');
            assertEqual(1, ns.length);
            node = ns[0];
            assertEqual(1, node.indexes.length);
            assertEqual(query[1], node.indexes[0].name);
            assertEqual(query[3], node.indexCoversProjections);
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
      ];

      queries.forEach((query) => {
        let plan = AQL_EXPLAIN(query[0], null, { optimizer: { rules: ["-optimize-cluster-single-document-operations"] } }).plan;
        let nodes = plan.nodes;

        assertEqual(0, nodes.filter((node) => node.type === 'IndexNode').length);
        let ns = nodes.filter((node) => node.type === 'EnumerateCollectionNode');
        assertEqual(1, ns.length);
        let node = ns[0];
        assertEqual(query[1], node.projections.sort());
      });
    },

  };
}

jsunity.run(indexHintSuite);
jsunity.run(indexHintDisableIndexSuite);

return jsunity.done();
