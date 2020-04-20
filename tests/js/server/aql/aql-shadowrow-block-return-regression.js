/*jshint globalstrict:false, strict:false, maxlen: 500 */
/*global assertEqual, AQL_EXECUTE, assertTrue, fail */

////////////////////////////////////////////////////////////////////////////////
/// @brief tests for regression returning blocks to the manager
///
/// @file
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
/// @author Markus Pfeiffer
/// @author Copyright 2020, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

var jsunity = require("jsunity");
var internal = require("internal");
var errors = internal.errors;
var db = require("@arangodb").db, indexId;

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////

function blockReturnRegressionSuite() {
  const activateSplicing = {optimizer: {rules: ["+splice-subqueries"]}};
  const deactivateSplicing = {optimizer: {rules: ["-splice-subqueries"]}};

  const deepAssertElements = (left, right, path) => {
    if (Array.isArray(left)) {
      assertTrue(Array.isArray(right), `On ${path}`);
      assertEqual(left.length, right.length, `On ${path}`);
      for (let i = 0; i < left.length; ++i) {
        deepAssertElements(left[i], right[i], `${path}[${i}]`);
      }
    } else if (left instanceof Object) {
      assertTrue(right instanceof Object, `On ${path}`);
      assertEqual(Object.keys(left).sort(), Object.keys(right).sort(), `On ${path}`);
      for (const [key, val] of Object.entries(left)) {
        deepAssertElements(val, right[key], `${path}.${key}`);
      }
    } else {
      assertEqual(left, right,`On ${path}`);
    }
  };

  return {

////////////////////////////////////////////////////////////////////////////////
/// @brief set up
////////////////////////////////////////////////////////////////////////////////

    setUpAll : function () {
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief tear down
////////////////////////////////////////////////////////////////////////////////

    tearDownAll : function () {
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test reuse of blocks in subquery, they are of identical size each time
////////////////////////////////////////////////////////////////////////////////

    testBlockReuseOkWithSubquery : function () {
      const query = `FOR c in 1..1000 LET su = (FOR d in 1..1000 SORT d RETURN d) RETURN LENGTH(su)`;
      var actual = db._query(query);
      assertEqual(actual.toArray().length , 1000);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief generated subquery, tends to show a difference 
////////////////////////////////////////////////////////////////////////////////

    testHeavyNestedSubquery : function () {
      
      const query = `
      FOR fv0 IN 1..20
        LET sq1 = (FOR fv2 IN 1..20
          LIMIT 10,0
          COLLECT WITH COUNT INTO counter 
          RETURN {counter})
        LET sq3 = (FOR fv4 IN 1..20
          LET sq5 = (FOR fv6 IN 1..20
            LET sq7 = (FOR fv8 IN 1..20
              LET sq9 = (FOR fv10 IN 1..20
                LIMIT 4,7
                COLLECT WITH COUNT INTO counter 
                RETURN {counter})
              LIMIT 11,11
              RETURN {fv8, sq1, sq9})
            LET sq11 = (FOR fv12 IN 1..20
              LIMIT 19,17
              RETURN {fv12, sq1, sq7})
            LIMIT 12,5
            RETURN {fv6, sq1, sq7, sq11})
          LIMIT 11,5
          RETURN {fv4, sq1, sq5})
        LIMIT 10,11
        RETURN {fv0, sq1, sq3}
      `;

      const splicedRes = db._query(query, {}, activateSplicing).toArray();
      const nosplicedRes = db._query(query, {}, deactivateSplicing).toArray();
      deepAssertElements(splicedRes, nosplicedRes, "");
    }


  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suite
////////////////////////////////////////////////////////////////////////////////

jsunity.run(blockReturnRegressionSuite);

return jsunity.done();

