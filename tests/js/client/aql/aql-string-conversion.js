/*jshint globalstrict:false, strict:false */
/*global assertEqual, assertTrue, db, arango */

// //////////////////////////////////////////////////////////////////////////////
// / DISCLAIMER
// /
// / Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
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
/// @author Max Neunhöffer
// //////////////////////////////////////////////////////////////////////////////

var jsunity = require("jsunity");
var helper = require("@arangodb/aql-helper");
var getQueryResults = helper.getQueryResults;

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////

function ahuacatlStringConversionTestSuite () {

  let cn = "UnitTestsAhuacatlDocument";

  return {

    setUp : function () {
      db._drop(cn);
      db._create(cn);
    },

    tearDown : function () {
      db._drop(cn);
    },

    testToStringLargeNumbers : function() {
      let actual = getQueryResults("RETURN TO_STRING(1152921504606846976)");
      assertEqual([ "1152921504606846976" ], actual);
      actual = getQueryResults("RETURN TO_STRING(1152921504606846977)");
      assertEqual([ "1152921504606846977" ], actual);
      actual = getQueryResults("RETURN TO_STRING(1152921504606846976.0)");
      assertEqual([ "1152921504606846976.0" ], actual);
      actual = getQueryResults("RETURN TO_STRING(-1152921504606846976)");
      assertEqual([ "-1152921504606846976" ], actual);
      actual = getQueryResults("RETURN TO_STRING(-1152921504606846977)");
      assertEqual([ "-1152921504606846977" ], actual);
      actual = getQueryResults("RETURN TO_STRING(-1152921504606846976.0)");
      assertEqual([ "-1152921504606846976.0" ], actual);
      actual = getQueryResults("RETURN TO_STRING(4503599627370496.0)");
      assertEqual([ "4503599627370496" ], actual);   // 2^52 too small for .0
      actual = getQueryResults("RETURN TO_STRING(9007199254740992.0)");
      assertEqual([ "9007199254740992.0" ], actual);   // 2^53 large enough
      actual = getQueryResults("RETURN TO_STRING(9223372036854775808.0)");
      assertEqual([ "9223372036854775808.0" ], actual);   // 2^63
      actual = getQueryResults("RETURN TO_STRING(18446744073709551616.0)");
      assertEqual([ "18446744073709552000" ], actual);   // 2^64 too large
      // Note that 2^64 cannot be represented in 64 bits, therefore, we 
      // fall back to the fast behaviour which produces a number, which is
      // not numerically correct, but will be parsed back to the same
      // double value by the JSON parser!
    },

    testLargeDoublesInJSON : function() {
      db._query(`INSERT {v: 1152921504606846976, _key: "1"} INTO ${cn}`);
      db._query(`INSERT {v: 1152921504606846977, _key: "2"} INTO ${cn}`);
      db._query(`INSERT {v: 1152921504606846976.0, _key: "3"} INTO ${cn}`);

      let res = db._query(`FOR d IN ${cn} FILTER d.v == 1152921504606846976 RETURN d._key`).toArray();
      assertEqual(2, res.length, res);
      assertTrue(res.indexOf("1") >= 0, res);
      assertTrue(res.indexOf("3") >= 0, res);

      res = db._query(`FOR d IN ${cn} FILTER d.v == 1152921504606846977 RETURN d._key`).toArray();
      assertEqual(1, res.length, res);
      assertTrue(res.indexOf("2") >= 0, res);

      res = db._query(`FOR d IN ${cn} FILTER d.v == 1152921504606846976.0 RETURN d._key`).toArray();
      assertEqual(2, res.length, res);
      assertTrue(res.indexOf("1") >= 0, res);
      assertTrue(res.indexOf("3") >= 0, res);

      // Note that 1152921504606846976.0 is the same as 1152921504606846977.0!
      res = db._query(`FOR d IN ${cn} FILTER d.v == 1152921504606846977.0 RETURN d._key`).toArray();
      assertEqual(2, res.length, res);
      assertTrue(res.indexOf("1") >= 0, res);
      assertTrue(res.indexOf("3") >= 0, res);

      res = arango.GET_RAW(`/_api/document/${cn}/1`, {accept: "application/json"});
      assertTrue(res.body.toString().indexOf('"v":1152921504606846976}') >= 0, res);

      res = arango.GET_RAW(`/_api/document/${cn}/2`, {accept: "application/json"});
      assertTrue(res.body.toString().indexOf('"v":1152921504606846977}') >= 0, res);

      res = arango.GET_RAW(`/_api/document/${cn}/3`, {accept: "application/json"});
      assertTrue(res.body.toString().indexOf('"v":1152921504606846976.0}') >= 0, res);
    },
  };

}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suite
////////////////////////////////////////////////////////////////////////////////

jsunity.run(ahuacatlStringConversionTestSuite);

return jsunity.done();

