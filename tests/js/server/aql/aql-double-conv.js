/*jshint globalstrict:false, strict:false, sub: true, maxlen: 500 */
/*global assertEqual, assertTrue, assertNull */

////////////////////////////////////////////////////////////////////////////////
/// @brief tests for double values
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
/// @author Copyright 2021, ArangoDB GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

let jsunity = require("jsunity");
let db = require("@arangodb").db;

function doubleConvSuite () {
  return {

    testDoubleFromPositiveInf: function () {
      const q = `RETURN 1.234e354`;
      let res = db._query(q).toArray()[0];
      assertNull(res);
    },
    
    testPosNegNullsEqual: function () {
      const q1 = `RETURN +0.0 == -0.0`;
      let res = db._query(q1).toArray()[0];
      assertTrue(res);
      
      const q2 = `RETURN NOOPT(+0.0) == NOOPT(-0.0)`;
      res = db._query(q2).toArray()[0];
      assertTrue(res);
    },
    
    testDoubleFromDiv0: function () {
      const q1 = `RETURN -1.0 / 0.0`;
      let res = db._query(q1).toArray()[0];
      assertNull(res);
      
      const q2 = `RETURN -1.0 / -0.0`;
      res = db._query(q2).toArray()[0];
      assertNull(res);
      
      const q3 = `RETURN NOOPT(-1.0) / NOOPT(0.0)`;
      res = db._query(q3).toArray()[0];
      assertNull(res);
      
      const q4 = `RETURN NOOPT(-1.0) / NOOPT(-0.0)`;
      res = db._query(q4).toArray()[0];
      assertNull(res);
      
      const q5 = `RETURN 0.0 / 0.0`;
      res = db._query(q5).toArray()[0];
      assertNull(res);
      
      const q6 = `RETURN NOOPT(0.0) / NOOPT(0.0)`;
      res = db._query(q6).toArray()[0];
      assertNull(res);
    },
    
    testPosNegNullGrouping: function () {
      const q = `FOR value IN [-1.0, -0.0, +0.0, 1.0] COLLECT v = value WITH COUNT INTO c RETURN { v, c }`;
      let res = db._query(q).toArray();
      assertEqual(3, res.length);
      assertEqual({ v: -1, c: 1 }, res[0]);
      assertEqual({ v: 0, c: 2 }, res[1]);
      assertEqual({ v: 1, c: 1 }, res[2]);
    },
  };
}

jsunity.run(doubleConvSuite);

return jsunity.done();
