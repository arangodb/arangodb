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
  let defaultIndex;
  let alternateIndex;

  return {

////////////////////////////////////////////////////////////////////////////////
/// @brief set up
////////////////////////////////////////////////////////////////////////////////

    setUp : function () {
      internal.db._drop(cn);
      collection = internal.db._create(cn);
      
      collection.ensureIndex({type: 'hash', name: 'hash_a', fields: ['a']});
      collection.ensureIndex({type: 'hash', name: 'hash_a_b', fields: ['a', 'b']});
      collection.ensureIndex({type: 'hash', name: 'hash_b_a', fields: ['b', 'a']});
      collection.ensureIndex({type: 'skiplist', name: 'skip_a', fields: ['a']});
      collection.ensureIndex({type: 'skiplist', name: 'skip_a_b', fields: ['a', 'b']});
      collection.ensureIndex({type: 'skiplist', name: 'skip_b_a', fields: ['b', 'a']});
      
      const isMMFiles = db._engine().name === "mmfiles";
      defaultIndex = isMMFiles ? 'skip_a' : 'hash_a';
      alternateIndex = isMMFiles ? 'hash_a' : 'skip_a';
    },

    tearDown : function () {
      internal.db._drop(cn);
    },

    testNoHint : function () {
      const query = `
        FOR doc IN ${cn} OPTIONS {}
          FILTER doc.a == 1
          RETURN doc
      `;
      const usedIndexes = getIndexNames(query);
      assertEqual(usedIndexes.length, 1);
      assertEqual(usedIndexes[0].length, 1);
      assertEqual(usedIndexes[0][0], defaultIndex);
    },

    testDefaultHint : function () {
      const query = `
        FOR doc IN ${cn} OPTIONS {indexHint: '${defaultIndex}'}
          FILTER doc.a == 1
          RETURN doc
      `;
      const usedIndexes = getIndexNames(query);
      assertEqual(usedIndexes.length, 1);
      assertEqual(usedIndexes[0].length, 1);
      assertEqual(usedIndexes[0][0], defaultIndex);
    },

    testDefaultHintForced : function () {
      const query = `
        FOR doc IN ${cn} OPTIONS {indexHint: '${defaultIndex}', forceIndexHint: true}
          FILTER doc.a == 1
          RETURN doc
      `;
      const usedIndexes = getIndexNames(query);
      assertEqual(usedIndexes.length, 1);
      assertEqual(usedIndexes[0].length, 1);
      assertEqual(usedIndexes[0][0], defaultIndex);
    },

    testNonexistentIndexHint : function () {
      const query = `
        FOR doc IN ${cn} OPTIONS {indexHint: 'foo'}
          FILTER doc.a == 1
          RETURN doc
      `;
      const usedIndexes = getIndexNames(query);
      assertEqual(usedIndexes.length, 1);
      assertEqual(usedIndexes[0].length, 1);
      assertEqual(usedIndexes[0][0], defaultIndex);
    },

    testNonexistentIndexHintForced : function () {
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

    testUnusableHint : function () {
      const query = `
        FOR doc IN ${cn} OPTIONS {indexHint: 'hash_b_a'}
          FILTER doc.a == 1
          RETURN doc
      `;
      const usedIndexes = getIndexNames(query);
      assertEqual(usedIndexes.length, 1);
      assertEqual(usedIndexes[0].length, 1);
      assertEqual(usedIndexes[0][0], defaultIndex);
    },

    testUnusableHintForced : function () {
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
    
    testTypeHint : function () {
      const query = `
        FOR doc IN ${cn} OPTIONS {indexHint: '${alternateIndex}'}
          FILTER doc.a == 1
          RETURN doc
      `;
      const usedIndexes = getIndexNames(query);
      assertEqual(usedIndexes.length, 1);
      assertEqual(usedIndexes[0].length, 1);
      assertEqual(usedIndexes[0][0], alternateIndex);
    },

    testPartialCoverageHint : function () {
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

    testListFirstHint : function () {
      const query = `
        FOR doc IN ${cn} OPTIONS {indexHint: ['skip_a_b', '${alternateIndex}']}
          FILTER doc.a == 1
          RETURN doc
      `;
      const usedIndexes = getIndexNames(query);
      assertEqual(usedIndexes.length, 1);
      assertEqual(usedIndexes[0].length, 1);
      assertEqual(usedIndexes[0][0], `skip_a_b`);
    },

    testListLastHint : function () {
      const query = `
        FOR doc IN ${cn} OPTIONS {indexHint: ['skip_b_a', '${alternateIndex}']}
          FILTER doc.a == 1
          RETURN doc
      `;
      const usedIndexes = getIndexNames(query);
      assertEqual(usedIndexes.length, 1);
      assertEqual(usedIndexes[0].length, 1);
      assertEqual(usedIndexes[0][0], alternateIndex);
    },

    testNestedMatchedHint : function () {
      const query = `
        FOR doc IN ${cn} OPTIONS {indexHint: '${alternateIndex}'}
          FILTER doc.a == 1
          FOR sub IN ${cn} OPTIONS {indexHint: '${alternateIndex}'}
            FILTER sub.a == 2
            RETURN [doc, sub]
      `;
      const usedIndexes = getIndexNames(query);
      assertEqual(usedIndexes.length, 2);
      assertEqual(usedIndexes[0].length, 1);
      assertEqual(usedIndexes[0][0], alternateIndex);
      assertEqual(usedIndexes[1].length, 1);
      assertEqual(usedIndexes[1][0], alternateIndex);
    },

    testNestedUnmatchedHint : function () {
      const query = `
        FOR doc IN ${cn} OPTIONS {indexHint: '${alternateIndex}'}
          FILTER doc.a == 1
          FOR sub IN ${cn} OPTIONS {indexHint: 'skip_a_b'}
            FILTER sub.a == 2
            RETURN [doc, sub]
      `;
      const usedIndexes = getIndexNames(query);
      assertEqual(usedIndexes.length, 2);
      assertEqual(usedIndexes[0].length, 1);
      assertEqual(usedIndexes[0][0], alternateIndex);
      assertEqual(usedIndexes[1].length, 1);
      assertEqual(usedIndexes[1][0], 'skip_a_b');
    },

  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suite
////////////////////////////////////////////////////////////////////////////////

jsunity.run(ahuacatlSkiplistOverlappingTestSuite);

return jsunity.done();
