/*jshint globalstrict:false, strict:false, maxlen: 500 */
/*global fail, assertEqual, AQL_EXPLAIN */

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

var internal = require("internal");
var db = internal.db;
var jsunity = require("jsunity");
var errors = internal.errors;

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////

function ahuacatlSkiplistOverlappingTestSuite () {
  const getIndexNames = function (query) {
    return AQL_EXPLAIN(query, {}, { optimizer: { rules: [ "-all", "+use-indexes" ] } })
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

    setUpAll : function () {
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

    tearDownAll : function () {
      internal.db._drop(cn);
    },

    testFilterNoHint : function () {
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

    testFilterDefaultHint : function () {
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

    testFilterDefaultHintForced : function () {
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

    testFilterNonexistentIndexHint : function () {
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

    testFilterNonexistentIndexHintForced : function () {
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

    testFilterUnusableHint : function () {
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

    testFilterUnusableHintForced : function () {
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
    
    testFilterTypeHint : function () {
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

    testFilterPartialCoverageHint : function () {
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

    testFilterListFirstHint : function () {
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

    testFilterListLastHint : function () {
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

    testFilterNestedMatchedHint : function () {
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

    testFilterNestedUnmatchedHint : function () {
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

    testSortNoHint : function () {
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

    testSortDefaultHint : function () {
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

    testSortDefaultHintForced : function () {
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

    testSortNonexistentIndexHint : function () {
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

    testSortNonexistentIndexHintForced : function () {
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

    testSortUnusableHint : function () {
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

    testSortUnusableHintForced : function () {
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
    
    testSortPartialCoverageHint : function () {
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

    testSortListFirstHint : function () {
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

    testSortListLastHint : function () {
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

  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suite
////////////////////////////////////////////////////////////////////////////////

jsunity.run(ahuacatlSkiplistOverlappingTestSuite);

return jsunity.done();
