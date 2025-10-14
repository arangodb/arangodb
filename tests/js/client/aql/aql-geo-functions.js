/*jshint globalstrict:false, strict:false, maxlen: 500 */
/*global assertEqual, assertTrue, assertFalse, print, fail */

// //////////////////////////////////////////////////////////////////////////////
// / DISCLAIMER
// /
// / Copyright 2014-2025 ArangoDB GmbH, Cologne, Germany
// / Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
// /
// / Licensed under the Business Source License 1.1 (the "License");
// / you may not use this file except in compliance with the License.
// / You may obtain a copy of the License at
// /
// /     https://github.com/arangodb/arangodb/blob/devel/LICENSE
// /
// / Unless required by applicable law or agreed to in writing, software
// / distributed under the License is distributed on an "AS IS" BASIS,
// / WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// / See the License for the specific language governing permissions and
// / limitations under the License.
// /
// / Copyright holder is ArangoDB GmbH, Cologne, Germany
// /
/// @author Jure Bajic
/// @author Copyright 2025, ArangoDB GmbH, Cologne, Germany
// //////////////////////////////////////////////////////////////////////////////

const jsunity = require("jsunity");
const db = require("@arangodb").db;
const helper = require("@arangodb/aql-helper");
const assertQueryWarningAndNull = helper.assertQueryWarningAndNull;
const internal = require("internal");
const errors = internal.errors;

function GeoFunctionsTestSuite() {
  return {
    setUpAll: function () {},

    testPositionDefinitionInGeoFunctions: function() {
      const queries = [
        // GEO_MULTIPOINT
        ["RETURN GEO_MULTIPOINT([[1,2,3], [4,5]])", null],
        ["RETURN GEO_MULTIPOINT([[1], [2,3]])", errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code],
        ["RETURN GEO_MULTIPOINT([[], [1,2]])", errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code],
        ["RETURN GEO_MULTIPOINT([[1,2,3,4], [5,6]])", errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code],
        // GEO_LINESTRING
        ["RETURN GEO_LINESTRING([[1,2,3], [4,5]])", null],
        ["RETURN GEO_LINESTRING([[1], [2,3]])", errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code],
        ["RETURN GEO_LINESTRING([[], [1,2]])", errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code],
        ["RETURN GEO_LINESTRING([[1,2,3,4], [5,6]])", errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code],
        // GEO_MULTILINESTRING
        ["RETURN GEO_MULTILINESTRING([[[1,2,3], [4,5]], [[6,7], [8,9]]])", null],
        ["RETURN GEO_MULTILINESTRING([[[1], [2,3]], [[4,5], [6,7]]])", errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code],
        ["RETURN GEO_MULTILINESTRING([[[], [1,2]], [[3,4], [5,6]]])", errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code],
        ["RETURN GEO_MULTILINESTRING([[[1,2,3,4], [5,6]], [[7,8], [9,0]]])", errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code],
      ];

      for(const queryExecution of queries) {
        const query = queryExecution[0];
        const error = queryExecution[1];

        if (error === null) {
          db._query(query);
        } else{
          assertQueryWarningAndNull(error, query);
        }
      }
    },
  };
}

jsunity.run(GeoFunctionsTestSuite);
return jsunity.done();
