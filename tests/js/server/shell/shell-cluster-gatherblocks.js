/*jshint globalstrict:false, strict:false, maxlen: 500 */
/*global assertEqual, assertFalse, assertTrue, assertNotEqual, AQL_EXPLAIN, AQL_EXECUTE, print */

////////////////////////////////////////////////////////////////////////////////
/// @brief tests that tests if collecting results from multiple
///        shards works correctly
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
var isEqual = helper.isEqual;
var findExecutionNodes = helper.findExecutionNodes;
var findReferencedNodes = helper.findReferencedNodes;
var getQueryMultiplePlansAndExecutions = helper.getQueryMultiplePlansAndExecutions;
var removeAlwaysOnClusterRules = helper.removeAlwaysOnClusterRules;
var db = require('internal').db;

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////

function gatherBlocksTestSuite() {
  // quickly disable tests here
  var enabled = {
    basics : true,
  };

  var ruleName = "ShardMerge";
  var colName1 = "UnitTestsShellServer" + ruleName.replace(/-/g, "_");
  var colName2 = "UnitTestsShellServer" + ruleName.replace(/-/g, "_") + "2";


  var collection1;
  var collection2;
  var documents;

  return {

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief set up
    ////////////////////////////////////////////////////////////////////////////////

    setUp : function () {
      internal.db._drop(colName1);
      internal.db._drop(colName2);
      collection1 = internal.db._create(colName1, { numberOfShards : 3 }); // 3 shards -- minelement merge
      collection2 = internal.db._create(colName2, { numberOfShards : 9 }); // 9 shards -- heap merge
      assertEqual(collection1.properties()['numberOfShards'], 3);
      assertEqual(collection2.properties()['numberOfShards'], 9);

      // insert documents batched
      let batchSize = 1000;
      let batches = 4;
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
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief tear down
    ////////////////////////////////////////////////////////////////////////////////

    tearDown : function () {
      internal.db._drop(colName1);
      internal.db._drop(colName2);
    },

    testRuleBasics : function () {
      if(enabled.basics){
        let query1 = 'for doc in @@col FILTER doc.value >= @splitpoint return doc';
        let bindvars1 = { 'splitpoint' : documents/2 };
        let query2 = `LET queryOne = ( FOR doc in @@col
                                          FILTER doc.value >= @splitpoint
                                          COLLECT WITH COUNT INTO length return length
                                     )
                      RETURN { ac: queryOne }
                     `;
        let bindvars2 = { 'splitpoint' : documents/2 };
        let query3 = 'for doc in @@col SORT doc.value return doc';
        let bindvars3 = false;

        //q(int) query
        //s(int) shards
        //i(bool) index
        let tests = [
          { test : "q1s3if", query: query1, bindvars: bindvars1, collection: colName1, gathernodes: 1, sortmode: 'unset', count: documents/2 , sorted: false },
          { test : "q1s9if", query: query1, bindvars: bindvars1, collection: colName2, gathernodes: 1, sortmode: 'unset', count: documents/2 , sorted: false },
          { test : "q2s3if", query: query2, bindvars: bindvars2, collection: colName1, gathernodes: 1, sortmode: 'unset', count: 1           , sorted: false },
          { test : "q2s9if", query: query2, bindvars: bindvars2, collection: colName2, gathernodes: 1, sortmode: 'unset', count: 1           , sorted: false },
          { test : "q3s3if", query: query3, bindvars: bindvars3, collection: colName1, gathernodes: 1, sortmode: 'minelement' , count: 4000        , sorted: true },
          { test : "q3s9if", query: query3, bindvars: bindvars3, collection: colName2, gathernodes: 1, sortmode: 'heap'       , count: documents   , sorted: true },
        ];
        let loop = 10;
        tests.forEach(t => {
            var assertMessage;
            var bindvars = { '@col' : t.collection };

            // add extra binvars
            if (t.bindvars){
              bindvars = Object.assign(bindvars, t.bindvars);
            }

            let plan = AQL_EXPLAIN(t.query , bindvars);

            // check that the plan contains the expected amount of gatherNodes
            let gatherNodes = findExecutionNodes(plan, "GatherNode");
            assertMessage = "\n##### test: " + t.test + " - did not find gatherNode" + " #####\n";
            assertEqual(gatherNodes.length,1, assertMessage);
            assertMessage = "\n##### test: " + t.test + " - sort mode does not match" + " #####\n";

            // assert that the sortmode set in the gatherNode is correct
            // with respect to the amount of shards.
            // shards < 5 -> minelement
            // else       -> heap
            assertEqual(gatherNodes[0]["sortmode"], t.sortmode, assertMessage);

            // execute the query and make sure it returns on every iteration 
            // the same result
            var rv;
            var last_rv;
            var time = 0;
            for(var i=0; i < loop; i++){
              let start = Date.now();
              try  {
                rv = db._query(t.query , bindvars);
                rv = rv.toArray().map(doc => { return doc.value; } );
                time += (Date.now() - start);
              }
              catch (ex) {
                print("Failed in " + t.query);
                print(bindvars);
                db._explain(t.query , bindvars);
                print(ex);

                throw ex;
              }
              // check number of returned documents
              assertEqual(rv.length, t.count);

              if (t.sortmode === "unset") {
                rv.sort();
              }

              // check that the result is the same for each call
              if (last_rv) {
                assertEqual(rv, last_rv, "q: " + t.test);
              }

              last_rv = rv;
            };

            // check that the returned result is sorted when requested
            if(t.sorted) {
              var old = -1;
              rv.forEach(d => {
                assertMessage = "\n##### test: " + t.test + " - " + old + " -> " + d + " #####\n";
                assertTrue( d >= old, assertMessage);
                old = d;
              });
            }

            t.result = rv;
            t.result_len = t.result.length;
            t.time = time / loop;
        });
      }

    }, // testRuleBasics


  }; // test dictionary (return)
} // gatherBlocksTestSuite

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suite
////////////////////////////////////////////////////////////////////////////////

jsunity.run(gatherBlocksTestSuite);

return jsunity.done();
