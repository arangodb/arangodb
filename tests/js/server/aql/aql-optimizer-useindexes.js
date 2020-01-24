/*jshint globalstrict:false, strict:false, maxlen: 500 */
/*global assertEqual, assertFalse, assertTrue, assertNotEqual, AQL_EXPLAIN, AQL_EXECUTE */

////////////////////////////////////////////////////////////////////////////////
/// @brief tests if indexes are used correctly
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
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Jan Christoph Uhde
/// @author Copyright 2017, ArangoDB GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

const expect = require('chai').expect;
var internal = require("internal");
var jsunity = require("jsunity");
var helper = require("@arangodb/aql-helper");
var findExecutionNodes = helper.findExecutionNodes;
var db = require('internal').db;

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////

function useIndexesTestSuite() {
  var enabled = {
    basics : true,
  };

  var ruleName = "UseIndexes";
  var colName1 = "UnitTestsShellServerAqlSkiplist";
  var colName2 = "UnitTestsShellServerAqlHash";

  var collection1;
  var collection2;
  var documents;

  return {

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief set up
    ////////////////////////////////////////////////////////////////////////////////

    setUpAll : function () {
      internal.db._drop(colName1);
      internal.db._drop(colName2);
      collection1 = internal.db._create(colName1);
      collection2 = internal.db._create(colName2);

      // insert documents batched
      let batchSize = 100;
      let batches = 1;
      documents = batches * batchSize;
      for (var i = 0; i < batches; i++) {
          // prepare batch
          var insertArray = [];
          for (var j = 0; j < batchSize; j++) {
              let value = i * batchSize + j;
              insertArray.push({ "value" : value, _key : String(value) });
          }
          // insert
          collection1.insert(insertArray);
          collection2.insert(insertArray);
      }
      collection1.ensureIndex({'type':'skiplist', 'fields' : ['value']});
      collection2.ensureIndex({'type':'hash', 'fields' : ['value']});
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief tear down
    ////////////////////////////////////////////////////////////////////////////////

    tearDownAll : function () {
      internal.db._drop(colName1);
      internal.db._drop(colName2);
    },

    testRuleBasics : function () {
      if (enabled.basics) {
        let query1 = 'for doc in @@col FILTER 25 < doc.value && doc.value < 75 return doc';
        let query2 = 'for doc in @@col FILTER 25 < doc.value && 75 > doc.value return doc';

        let tests = [
          { test : "s<<", query: query1, collection: colName1, sortNodes: 0, count: documents/2-1 },
          { test : "s<>", query: query2, collection: colName1, sortNodes: 0, count: documents/2-1 },
          { test : "h<<", query: query1, collection: colName2, sortNodes: 0, count: documents/2-1 },
          { test : "h<>", query: query2, collection: colName2, sortNodes: 0, count: documents/2-1 },
        ];

        tests.forEach(t => {
            var assertMessage;
            var bindvars = { '@col' : t.collection };

            let plan = AQL_EXPLAIN(t.query , bindvars);

            let sortNodes = findExecutionNodes(plan, "SortNode");
            assertMessage = "\n##### test: " + t.test + " - sortNodes do not match" + " #####\n";
            assertEqual(sortNodes.length, t.sortNodes, assertMessage);

            // Execute the query and make sure it returns
            // on every iteration the same result.
            var rv;
            var last_rv;

            let loop = 2;
            for (var i=0; i < loop; i++){
              rv = db._query(t.query , bindvars);
              rv = rv.toArray().map(doc => { return doc.value; } );

              // check that the result is the same for each call
              if (last_rv) {
                assertEqual(rv,last_rv);
              }
              last_rv = rv;
            };

            // check number of returned documents
            assertEqual(rv.length, t.count);
        });

      }
    }, // testRuleBasics
  }; // test dictionary (return)
} 

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suite
////////////////////////////////////////////////////////////////////////////////

jsunity.run(useIndexesTestSuite);

return jsunity.done();
