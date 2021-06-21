/*jshint globalstrict:false, strict:false, maxlen: 500 */
/*global assertEqual, AQL_EXPLAIN */

////////////////////////////////////////////////////////////////////////////////
/// @brief tests for query language, sort optimizations
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2010-2021 ArangoDB GmbH, Cologne, Germany
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
/// @author Heiko Kernbach
/// @author Copyright 2021, ArangoDB GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

let jsunity = require("jsunity");
let internal = require("internal");
let helper = require("@arangodb/aql-helper");
let getQueryResults = helper.getQueryResults;

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////

function ahuacatlQueryOptimizerCollectTestSuite() {
  let collection = null;
  let cn = "UnitTestsAhuacatlOptimizerCountCollect";

  return {

////////////////////////////////////////////////////////////////////////////////
/// @brief set up
////////////////////////////////////////////////////////////////////////////////

    setUp: function () {
      internal.db._drop(cn);
      collection = internal.db._create(cn);

      // create an index with two attributes (first: boolean, second: array)
      collection.ensureIndex({type: "persistent", fields: ["someBoolean", "someArray[*]"]});

      // create 100 (50+50) documents in total
      let docsToInsert = [];
      for (let outer = 0; outer < 50; outer++) {
        let myArray = [];
        // create 100 entries for our inner document array
        for (let inner = 0; inner < 10; inner++) {
          // write numeric strings
          myArray.push(JSON.stringify(inner));
        }
        docsToInsert.push({"someBoolean": true, "someArray": myArray});
        docsToInsert.push({"someBoolean": false, "someArray": myArray});
      }
      collection.save(docsToInsert);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief tear down
////////////////////////////////////////////////////////////////////////////////

    tearDown: function () {
      internal.db._drop(cn);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief check collect with count with index (on array index attribute)
////////////////////////////////////////////////////////////////////////////////

    testCountWithMultipleAttributesIncludingIndexArray: function () {
      let countWithLengthQuery = "RETURN LENGTH(" + cn + ")";
      let countWithCollectQuery = `
        FOR d IN ${cn} FILTER (d.someBoolean == true) OR (d.someBoolean == false)
          COLLECT WITH COUNT INTO c
          OPTIONS {indexHint: ['someBoolean', 'someArray'], forceIndexHint: true}
        RETURN c`;

      let resCountWithLength = getQueryResults(countWithLengthQuery);
      let resCountWithCollect = getQueryResults(countWithCollectQuery);

      assertEqual(1, resCountWithLength.length);
      assertEqual(1, resCountWithCollect.length);
      assertEqual(resCountWithLength[0], 100);
      assertEqual(resCountWithCollect[0], 100);

      assertEqual(resCountWithCollect[0], resCountWithLength[0]);
    }
  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suite
////////////////////////////////////////////////////////////////////////////////

jsunity.run(ahuacatlQueryOptimizerCollectTestSuite);

return jsunity.done();