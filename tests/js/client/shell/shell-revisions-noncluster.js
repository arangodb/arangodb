/*jshint globalstrict:false, strict:false, maxlen: 5000 */
/*global arango, assertEqual, assertTrue, assertFalse, assertMatch */

////////////////////////////////////////////////////////////////////////////////
/// @brief test the revisions
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
const arangodb = require("@arangodb");
const db = arangodb.db;

function RevisionsSuite () {
  'use strict';

  const cn = "UnitTestsCollection";

  let revs = [];
      
  return {
    setUp: function () {
      db._drop(cn);
      let c = db._create(cn);

      revs = [];
      let docs = [];
      for (let i = 0; i < 20000; ++i) {
        docs.push({ _key: "testi" + i, value1: "testi" + i, value2: "testi" + i });
        if (docs.length >= 5000) {
          revs = revs.concat(c.insert(docs).map((doc) => doc._rev));
          docs = [];
        }
      }
    },

    tearDown: function () {
      db._drop(cn);
    },
    
    testCallRevisionDocumentsApiReverse: function () {
      let res = arango.POST("/_api/replication/batch", {});
      let batchId = res.id;

      let n = 1000;
      let i = 0;
      let filteredRevs = revs.filter((rev) => i++ % n === 0);

      assertEqual(20, filteredRevs.length);
      filteredRevs = filteredRevs.reverse();

      try {
        res = arango.PUT("/_api/replication/revisions/documents?collection=" + cn + "&batchId=" + batchId + "&chunkSize=16384", filteredRevs);
        assertTrue(Array.isArray(res));
        assertEqual(20, res.length);
        res.forEach((doc) => {
          assertMatch(/^testi\d+$/, doc._key);
        });
      } finally {
        res = arango.DELETE("/_api/replication/batch/" + batchId);
        assertTrue(res.hasOwnProperty("error"));
        assertFalse(res.error);
      }
    },
    
    testCallRevisionDocumentsApiSome: function () {
      let res = arango.POST("/_api/replication/batch", {});
      let batchId = res.id;

      let n = 1000;
      let i = 0;
      let filteredRevs = revs.filter((rev) => i++ % n === 0);

      assertEqual(20, filteredRevs.length);

      try {
        res = arango.PUT("/_api/replication/revisions/documents?collection=" + cn + "&batchId=" + batchId + "&chunkSize=16384", filteredRevs);
        assertTrue(Array.isArray(res));
        assertEqual(20, res.length);
        res.forEach((doc) => {
          assertMatch(/^testi\d+$/, doc._key);
        });
      } finally {
        res = arango.DELETE("/_api/replication/batch/" + batchId);
        assertTrue(res.hasOwnProperty("error"));
        assertFalse(res.error);
      }
    },

    testCallRevisionDocumentsApiAll: function () {
      let res = arango.POST("/_api/replication/batch", {});
      let batchId = res.id;

      let allRevs = revs;
      try {
        res = arango.PUT("/_api/replication/revisions/documents?collection=" + cn + "&batchId=" + batchId + "&chunkSize=16384", allRevs);
        assertTrue(Array.isArray(res));
        assertTrue(res.length > 100);
        assertTrue(res.length <= 1000);
        res.forEach((doc) => {
          assertMatch(/^testi\d+$/, doc._key);
        });
      } finally {
        res = arango.DELETE("/_api/replication/batch/" + batchId);
        assertTrue(res.hasOwnProperty("error"));
        assertFalse(res.error);
      }
    }

  };
}

jsunity.run(RevisionsSuite);

return jsunity.done();
