/*jshint globalstrict:false, strict:false, maxlen:1000*/
/*global assertEqual, assertTrue, assertUndefined, fail */

////////////////////////////////////////////////////////////////////////////////
/// @brief test the statement class
///
/// @file
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
/// Copyright holder is triAGENS GmbH, Cologne, Germany
///
/// @author Jan Steemann
/// @author Copyright 2012, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

const jsunity = require("jsunity");
const db = require("internal").db;
const aqlfunctions = require("@arangodb/aql/functions");

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite: stream cursors
////////////////////////////////////////////////////////////////////////////////

function StreamCursorSuite () {
  'use strict';

  const cn = "StreamCursorCollection";
  let c = null;
  const queries = [
    `FOR doc IN ${cn} RETURN doc`,
    `FOR doc IN ${cn} FILTER doc.value1 == 1 UPDATE doc WITH { value1: 1 } INTO ${cn}`,
    `FOR doc IN ${cn} FILTER doc.value1 == 1 REPLACE doc WITH { value1: 2 } INTO ${cn}`,
    `FOR doc IN ${cn} FILTER doc.value1 == 2 INSERT { value2: doc.value1 } INTO ${cn}`,
    `FOR doc IN ${cn} FILTER doc.value1 == 2 INSERT { value2: doc.value1 } INTO ${cn}`,
    `FOR doc IN ${cn} FILTER doc.value3 >= 4 RETURN doc`,
    `FOR doc IN ${cn} COLLECT a = doc.value1 INTO groups = doc RETURN { val: a, doc: groups }`
  ];

  return {

////////////////////////////////////////////////////////////////////////////////
/// @brief set up
////////////////////////////////////////////////////////////////////////////////

    setUp : function () {
      c = db._create(cn);
      c.ensureIndex({ type: 'skiplist', fields: ["value1"]});
      c.ensureIndex({ type: 'skiplist', fields: ["value2"]});

      let docs = [];
      for (let i = 0; i < 5000; i++) {
        docs.push({value1: i % 10, value2: i % 25 , value3: i % 25 });
      }
      c.insert(docs);
 
      try {
        aqlfunctions.unregister("my::test");
      } catch (err) {}

      aqlfunctions.register("my::test", function() { return 42; });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief tear down
////////////////////////////////////////////////////////////////////////////////

    tearDown : function () {
      try {
        aqlfunctions.unregister("my::test");
      } catch (err) {}

      c.drop();
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test cursor
////////////////////////////////////////////////////////////////////////////////

    testQueries : function () {
      queries.forEach(q => {
        let stmt = db._createStatement({ query: q,
          options: { stream: true },
          batchSize: 1000});
        let cursor = stmt.execute();

        try {
          assertEqual(undefined, cursor.count());
          while (cursor.hasNext()) {
            cursor.next();
          }
        } catch (err) {
          fail("testQueries failed");
        } finally {
          cursor.dispose();
        }
      });      
    },
    
    testInfiniteAQL : function() {
      let stmt = db._createStatement({ query: "FOR i IN 1..100000000000 RETURN i",
        options: { stream: true },
        batchSize: 1000});
      let cursor = stmt.execute();
      try {
        assertUndefined(cursor.count());
        let i = 10;
        while (cursor.hasNext() && i-- > 0) {
          cursor.next();
        }
      } catch (err) {
        fail("testInfiniteAQL failed");
      } finally {
        cursor.dispose();
      }
    },

    // Regression test, could fail in cluster
    testDisposeCursorWithCollection : function() {
      let cursor = db._query("FOR doc IN @@cn RETURN doc", { "@cn" : cn}, 
      { stream: true, batchSize: 1000});

      assertUndefined(cursor.count());
      let i = 10;
      while (cursor.hasNext() && i-- > 0) {
        cursor.next();
      }

      cursor.dispose();
    },

    testUserDefinedFunction : function () {
      let stmt = db._createStatement({ 
        query: "FOR i IN 1..10000 RETURN my::test()",
        options: { stream: true },
        batchSize: 1000
      });
      let cursor = stmt.execute();

      try {
        assertEqual(undefined, cursor.count());
        let count = 0;
        while (cursor.hasNext()) {
          assertEqual(42, cursor.next());
          ++count;
        }
        assertEqual(10000, count);
      } catch (err) {
        fail("testUserDefinedFunction failed");
      } finally {
        cursor.dispose();
      }
    },
  };
}

jsunity.run(StreamCursorSuite);
return jsunity.done();
