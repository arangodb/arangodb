/*jshint globalstrict:false, strict:false */
/*global assertEqual */

////////////////////////////////////////////////////////////////////////////////
/// @brief test the edge index
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

let jsunity = require("jsunity");
let db = require("internal").db;

function EdgeIndexSuite() {
  'use strict';
  const cn = "UnitTestsCollection";

  return {

    setUpAll : function () {
      db._drop(cn);
      let c = db._createEdgeCollection(cn);

      let docs = [];
      for (let i = 0; i < 1000; ++i) {
        docs.push({ _from: "test/v" + i, _to: "test/v" + i });
      }
      c.insert(docs);
    },

    tearDownAll : function () {
      db._drop(cn);
    },

    setUp : function () {
      db[cn].unload(); // drop caches
    },

    tearDown : function () {
      db[cn].unload(); // drop caches
    },
    
    testLookupFrom : function () {
      for (let i = 0; i < 1000; ++i) {
        let result = db._query("FOR doc IN " + cn + " FILTER doc._from == @from RETURN doc", { from: "test/v" + i }).toArray();
        assertEqual(1, result.length);
      }
    },
    
    testLookupTo : function () {
      for (let i = 0; i < 1000; ++i) {
        let result = db._query("FOR doc IN " + cn + " FILTER doc._to == @to RETURN doc", { to: "test/v" + i }).toArray();
        assertEqual(1, result.length);
      }
    },
    
    testLookupFromTo : function () {
      for (let i = 0; i < 1000; ++i) {
        let result = db._query("FOR doc IN " + cn + " FILTER doc._from == @from && doc._to == @to RETURN doc", { from: "test/v" + i, to: "test/v" + i }).toArray();
        assertEqual(1, result.length);
      }
    },
    
    testLookupFromNested : function () {
      let result = db._query("FOR i IN 0..999 FOR doc IN " + cn + " FILTER doc._from == CONCAT('test/v', i) RETURN doc").toArray();
      assertEqual(1000, result.length);
    },
    
    testLookupToNested : function () {
      let result = db._query("FOR i IN 0..999 FOR doc IN " + cn + " FILTER doc._to == CONCAT('test/v', i) RETURN doc").toArray();
      assertEqual(1000, result.length);
    },
    
    testLookupFromToNested : function () {
      let result = db._query("FOR i IN 0..999 FOR doc IN " + cn + " FILTER doc._from == CONCAT('test/v', i) && doc._to == CONCAT('test/v', i) RETURN doc").toArray();
      assertEqual(1000, result.length);
    },

  };
}

jsunity.run(EdgeIndexSuite);

return jsunity.done();
