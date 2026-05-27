/*jshint globalstrict:false, strict:false, maxlen: 500 */
/*global assertEqual, assertTrue */

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
// //////////////////////////////////////////////////////////////////////////////

const internal = require("internal");
const jsunity = require("jsunity");
const db = internal.db;
const helper = require("@arangodb/aql-helper");
const assertQueryError = helper.assertQueryError;
const errors = internal.errors;

function splicingSuite () {
  return {
    testArraySplicing : function() {
      let query = `LET a = [1,2,3] RETURN [...a]`;
      let res = db._query(query).toArray();
      assertEqual([[1,2,3]], res);
    },
    testArraySplicingMulti : function() {
      let query = `LET a = [1,2,3] LET b = [4,5,6] RETURN [0,...a,...b,6]`;
      let res = db._query(query).toArray();
      assertEqual([[0,1,2,3,4,5,6,6]], res);
    },
    testArraySplicingEmpty : function() {
      let query = `LET a = [] RETURN [...a]`;
      let res = db._query(query).toArray();
      assertEqual([[]], res);
    },
    testArraySplicingNotArray : function() {
      let query = `LET a = 5 RETURN [...a]`;
      let data = db._query(query).data;
      assertEqual([[]], data.result);
      assertEqual(1, data.extra.warnings.length);
      assertEqual(errors.ERROR_QUERY_ARRAY_EXPECTED.code,
        data.extra.warnings[0].code);
      assertTrue(data.extra.warnings[0].message.includes('array splice'));
    },
    testArraySplicingNull : function() {
      let query = `LET a = null RETURN [...a]`;
      let res = db._query(query).toArray();
      assertEqual([[null]], res);
    },
    testArraySplicingObject : function() {
      let query = `LET a = {} RETURN [...a]`;
      let data = db._query(query).data;
      assertEqual([[]], data.result);
      assertEqual(1, data.extra.warnings.length);
      assertEqual(errors.ERROR_QUERY_ARRAY_EXPECTED.code,
        data.extra.warnings[0].code);
    },
    testArraySplicingNested : function() {
      let query = `LET a = [[1,2,3],[4]] RETURN [...[...a]]`;
      let res = db._query(query).toArray();
      assertEqual([[[1,2,3],[4]]], res);
    },
  };
}

jsunity.run(splicingSuite);
return jsunity.done();
