/*jshint globalstrict:false, strict:false */
/*global assertEqual, getOptions, fail */

////////////////////////////////////////////////////////////////////////////////
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
/// @author Copyright 2023, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

if (getOptions === true) {
  return {
    'javascript.user-defined-functions': 'false',
  };
}

const jsunity = require("jsunity");
const arangodb = require("@arangodb");
const udf = require("@arangodb/aql/functions");
const db = arangodb.db;
const ERRORS = arangodb.errors;

function UserDefinedFunctionsTestSuite () {
  return {

    setUpAll : function () {
      udf.register("some::func", function() { return 1; });
    },

    tearDownAll : function () {
      udf.unregister("some::func");
    },

    testUseUDF : function () {
      try {
        // invoking a UDF should fail with a parse error
        db._query("RETURN some::func()");
        fail();
      } catch (err) {
        assertEqual(err.errorNum, ERRORS.ERROR_QUERY_PARSE.code);
      }
    },

    testNormalAqlFunction : function () {
      // invoking normal AQL functions should just work
      let res = db._query("RETURN MAX([1, 2, 3])");
      assertEqual([ 3 ], res.toArray());
    },
  };
}

jsunity.run(UserDefinedFunctionsTestSuite);

return jsunity.done();
