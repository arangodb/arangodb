/*jshint globalstrict:false, strict:false, maxlen: 500 */
/*global assertEqual, fail */

////////////////////////////////////////////////////////////////////////////////
/// @brief tests for traversal optimization
///
/// DISCLAIMER
///
/// Copyright 2010-2014 triagens GmbH, Cologne, Germany
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
/// @author Copyright 2020, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

const jsunity = require("jsunity");
const db = require("@arangodb").db;
const internal = require("internal");
const errors = internal.errors;
  
function PruneExpressionsSuite() {
  const vn = "UnitTestsVertex";
  const en = "UnitTestsEdge";
  const udf = "test::fuchs";

  let cleanup = function () {
    db._drop(en);
    db._drop(vn);

    try {
      require("@arangodb/aql/functions").unregister(udf);
    } catch (err) {}
  };

  return {
    setUpAll : function () {
      cleanup();
    
      require("@arangodb/aql/functions").register(udf, function() { return '0'; });
      
      db._create(vn, { numberOfShards: 1 });
      db._createEdgeCollection(en, { distributeShardsLike: vn, numberOfShards: 1 });

      db[vn].insert({ _key: "test1", value: 1 });
      db[vn].insert({ _key: "test2", value: 2 });
      db[vn].insert({ _key: "test3", value: 3 });

      db[en].insert({ _from: vn + "/test1", _to: vn + "/test2", value: 1 });
      db[en].insert({ _from: vn + "/test2", _to: vn + "/test3", value: 0 });
    },
    
    tearDownAll : function () {
      cleanup();
    },

    testAllowedExpression: function () {
      let q = `WITH ${vn} FOR v, e, p IN 1..3 OUTBOUND '${vn}/test1' ${en} PRUNE e.value == '0' RETURN v`;
      let res = db._query(q).toArray();
      assertEqual(2, res.length);
    },

    testV8Expression: function () {
      let q = `WITH ${vn} FOR v, e, p IN 1..3 OUTBOUND '${vn}/test1' ${en} PRUNE e.value == V8('0') RETURN v`;
      try {
        db._query(q);
        fail();
      } catch (err) {
        assertEqual(err.errorNum, errors.ERROR_QUERY_PARSE.code);
      }
    },
    
    testUDFExpression: function () {
      let q = `WITH ${vn} FOR v, e, p IN 1..3 OUTBOUND '${vn}/test1' ${en} PRUNE e.value == ${udf}() RETURN v`;
      try {
        db._query(q);
        fail();
      } catch (err) {
        assertEqual(err.errorNum, errors.ERROR_QUERY_PARSE.code);
      }
    },
    
    testSubqueryExpression: function () {
      let q = `WITH ${vn} FOR v, e, p IN 1..3 OUTBOUND '${vn}/test1' ${en} PRUNE e.value == (FOR i IN 1..1 RETURN '0') RETURN v`;
      try {
        db._query(q);
        fail();
      } catch (err) {
        assertEqual(err.errorNum, errors.ERROR_QUERY_PARSE.code);
      }
    },
  };
}

jsunity.run(PruneExpressionsSuite);
return jsunity.done();
