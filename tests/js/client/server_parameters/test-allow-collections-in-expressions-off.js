/*jshint globalstrict:false, strict:false */
/* global getOptions, assertEqual, assertTrue, fail */

////////////////////////////////////////////////////////////////////////////////
/// @brief test for server parameters
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
/// Copyright holder is ArangoDB Inc, Cologne, Germany
///
/// @author Jan Steemann
/// @author Copyright 2019, ArangoDB Inc, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

if (getOptions === true) {
  return {
    'query.allow-collections-in-expressions': "false",
  };
}
const jsunity = require('jsunity');
const errors = require('@arangodb').errors;
const cn = "UnitTestsCollection";
const en = "UnitTestsEdge";
const db = require('internal').db;

function testSuite() {
  return {
    setUpAll: function() {
      db._drop(cn);
      let c = db._create(cn);
      let docs = [];
      for (let i = 0; i < 500; ++i) {
        docs.push({ _key: "test" + i, value: i, text: "testi" + i, loc: [i * 0.001, -i * 0.001] });
      }
      c.insert(docs);
      c.ensureIndex({ type: "fulltext", fields: ["text"] }); 
      c.ensureIndex({ type: "geo", fields: ["loc"] }); 
      
      db._drop(en);
      db._createEdgeCollection(en);
    },
    
    tearDownAll: function() {
      db._drop(cn);
      db._drop(en);
    },

    testUseInForLoop: function() {
      let result = db._query("FOR doc IN " + cn + " RETURN doc").toArray();
      assertEqual(500, result.length);
      result.forEach((doc) => {
        assertTrue(doc.hasOwnProperty('_key'));
        assertTrue(doc.hasOwnProperty('value'));
      });
    },

    testUseInSubquery: function() {
      let result = db._query("LET sub = (FOR doc IN " + cn + " RETURN doc) RETURN sub").toArray();
      assertEqual(1, result.length);
      assertEqual(500, result[0].length);
      result[0].forEach((doc) => {
        assertTrue(doc.hasOwnProperty('_key'));
        assertTrue(doc.hasOwnProperty('value'));
      });
    },
    
    testUseInInsert: function() {
      let result = db._query("FOR i IN 1..1000 FILTER i > 10000 INSERT {} INTO " + cn).toArray();
      assertEqual(0, result.length);
    },
    
    testUseInUpdate: function() {
      let result = db._query("FOR doc IN " + cn + " FILTER doc.value == 9999999 UPDATE doc WITH {} IN " + cn).toArray();
      assertEqual(0, result.length);
    },
    
    testUseInReplace: function() {
      let result = db._query("FOR doc IN " + cn + " FILTER doc.value == 9999999 REPLACE doc WITH {} IN " + cn).toArray();
      assertEqual(0, result.length);
    },
    
    testUseInRemove: function() {
      let result = db._query("FOR doc IN " + cn + " FILTER doc.value == 9999999 REMOVE doc IN " + cn).toArray();
      assertEqual(0, result.length);
    },
    
    testUseInTraversal: function() {
      let result = db._query("WITH " + cn + " FOR v, e, p IN 1..3 OUTBOUND '" + cn + "/test0' " + en + " RETURN [v, e, p]").toArray();
      assertEqual(0, result.length);
    },
    
    testUseInShortestPath: function() {
      let result = db._query("WITH " + cn + " FOR s IN OUTBOUND SHORTEST_PATH '" + cn + "/test0' TO '" + cn + "/test1' " + en + " RETURN s").toArray();
      assertEqual(0, result.length);
    },
    
    testUseInCollectionCount: function() {
      let result = db._query("RETURN COLLECTION_COUNT(" + cn + ")").toArray();
      assertEqual(1, result.length);
      assertEqual(db[cn].count(), result[0]);
    },
    
    testUseInCount: function() {
      let result = db._query("RETURN COUNT(" + cn + ")").toArray();
      assertEqual(1, result.length);
      assertEqual(db[cn].count(), result[0]);
    },
    
    testUseInLength: function() {
      let result = db._query("RETURN LENGTH(" + cn + ")").toArray();
      assertEqual(1, result.length);
      assertEqual(db[cn].count(), result[0]);
    },
    
    testUseInFulltext: function() {
      let result = db._query("RETURN FULLTEXT(" + cn + ", 'text', 'prefix:testi')").toArray();
      assertEqual(1, result.length);
      assertEqual(500, result[0].length);
    },
    
    testUseInWithinRectangle: function() {
      let result = db._query("RETURN WITHIN_RECTANGLE(" + cn + ", -1, -1, 1, 1)").toArray();
      assertEqual(1, result.length);
      assertEqual(500, result[0].length);
    },
    
    testUseInExpressions: function() {
      let queries = [
        "RETURN " + cn,
        "RETURN " + cn + "[*].value",
        "FOR value IN " + cn + "[*].value RETURN value",
        "FOR doc IN " + cn + " RETURN " + cn + "[0]",
        "FOR doc IN " + cn + " RETURN " + cn + " + 1",
        "FOR doc IN " + cn + " RETURN NOOPT(" + cn + ")",
        "RETURN APPEND(" + cn + ", 'foo')",
      ];

      queries.forEach((query) => {
        try {
          db._query(query);
          fail();
        } catch (err) {
          assertEqual(errors.ERROR_QUERY_COLLECTION_USED_IN_EXPRESSION.code, err.errorNum);
        }
      });
    },
  };
}

jsunity.run(testSuite);
return jsunity.done();
