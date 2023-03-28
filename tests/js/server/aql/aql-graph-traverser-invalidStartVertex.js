/* jshint esnext: true */
/* global AQL_EXECUTE, AQL_EXPLAIN, AQL_EXECUTEJSON */

// //////////////////////////////////////////////////////////////////////////////
// / @brief Spec for the AQL FOR x IN GRAPH name statement
// /
// / @file
// /
// / DISCLAIMER
// /
// / Copyright 2014 ArangoDB GmbH, Cologne, Germany
// /
// / Licensed under the Apache License, Version 2.0 (the "License");
// / you may not use this file except in compliance with the License.
// / You may obtain a copy of the License at
// /
// /     http://www.apache.org/licenses/LICENSE-2.0
// /
// / Unless required by applicable law or agreed to in writing, software
// / distributed under the License is distributed on an "AS IS" BASIS,
// / WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// / See the License for the specific language governing permissions and
// / limitations under the License.
// /
// / Copyright holder is ArangoDB GmbH, Cologne, Germany
// /
// / @author Michael Hackstein
// / @author Copyright 2015, ArangoDB GmbH, Cologne, Germany
// //////////////////////////////////////////////////////////////////////////////

'use strict';

const jsunity = require('jsunity');
const {assertEqual, assertTrue, assertFalse, fail} = jsunity.jsUnity.assertions;

const internal = require('internal');
const db = internal.db;
const errors = require('@arangodb').errors;

const vn = 'UnitTestVertexCollection';
const en = 'UnitTestEdgeCollection';



const gh = require('@arangodb/graph/helpers');








function invalidStartVertexSuite() {
  const gn = 'UnitTestGraph';

  return {

    setUpAll: function () {
      db._drop(gn + 'v1');
      db._drop(gn + 'v2');
      db._drop(gn + 'e');

      let c;
      c = db._create(gn + 'v1', {numberOfShards: 1});
      c.insert({_key: "test"});

      c = db._create(gn + 'v2', {numberOfShards: 1});
      c.insert({_key: "test"});

      c = db._createEdgeCollection(gn + 'e', {numberOfShards: 1});
      c.insert({_from: gn + "v2/test", _to: gn + "v1/test"});
    },

    tearDownAll: function () {
      db._drop(gn + 'v1');
      db._drop(gn + 'v2');
      db._drop(gn + 'e');
    },

    testTraversalNullStartVertex: function () {
      let directions = ["INBOUND", "OUTBOUND", "ANY"];
      directions.forEach(function (direction) {
        try {
          let q = `WITH ${gn + 'v1'} ${gn + 'v2'} FOR v, e IN ${direction} null ${gn + 'e'} RETURN {v, e}`;
          AQL_EXECUTE(q);
          fail();
        } catch (err) {
          assertEqual(err.errorNum, errors.ERROR_QUERY_PARSE.code);
        }
      });
    },

    testTraversalNumberStartVertex: function () {
      let directions = ["INBOUND", "OUTBOUND", "ANY"];
      directions.forEach(function (direction) {
        let q = `WITH ${gn + 'v1'} ${gn + 'v2'} FOR v, e IN ${direction} -123 ${gn + 'e'} RETURN {v, e}`;
        try {
          AQL_EXECUTE(q);
          fail();
        } catch (err) {
          assertEqual(err.errorNum, errors.ERROR_QUERY_PARSE.code);
        }
      });
    },

    testTraversalEmptyStartVertex: function () {
      let directions = ["INBOUND", "OUTBOUND", "ANY"];
      directions.forEach(function (direction) {
        let q = `WITH ${gn + 'v1'} ${gn + 'v2'} FOR v, e IN ${direction} '' ${gn + 'e'} RETURN {v, e}`;
        let res = AQL_EXECUTE(q);
        assertEqual([], res.json);
        assertTrue(res.warnings.length > 0);
      });
    },

    testShortestPathNullStartVertex: function () {
      let directions = ["INBOUND", "OUTBOUND", "ANY"];
      directions.forEach(function (direction) {
        let q = `WITH ${gn + 'v1'} ${gn + 'v2'} FOR v, e IN ${direction} SHORTEST_PATH null TO '${vn + 'v1'}/1' ${gn + 'e'} RETURN {v, e}`;
        try {
          AQL_EXECUTE(q);
          fail();
        } catch (err) {
          assertEqual(err.errorNum, errors.ERROR_QUERY_PARSE.code);
        }
      });
    },

    testShortestPathNumberStartVertex: function () {
      let directions = ["INBOUND", "OUTBOUND", "ANY"];
      directions.forEach(function (direction) {
        let q = `WITH ${gn + 'v1'} ${gn + 'v2'} FOR v, e IN ${direction} SHORTEST_PATH -123 TO '${vn + 'v1'}/1' ${gn + 'e'} RETURN {v, e}`;
        try {
          AQL_EXECUTE(q);
          fail();
        } catch (err) {
          assertEqual(err.errorNum, errors.ERROR_QUERY_PARSE.code);
        }
      });
    },

    testShortestPathEmptyStartVertex: function () {
      let directions = ["INBOUND", "OUTBOUND", "ANY"];
      directions.forEach(function (direction) {
        let q = `WITH ${gn + 'v1'} ${gn + 'v2'} FOR v, e IN ${direction} SHORTEST_PATH '' TO '${vn + 'v1'}/1' ${gn + 'e'} RETURN {v, e}`;
        let res = AQL_EXECUTE(q);
        assertEqual([], res.json);
        assertTrue(res.warnings.length > 0);
      });
    },

    testShortestPathEmptyEndVertex: function () {
      let directions = ["INBOUND", "OUTBOUND", "ANY"];
      directions.forEach(function (direction) {
        let q = `WITH ${gn + 'v1'} ${gn + 'v2'} FOR v, e IN ${direction} SHORTEST_PATH '${vn + 'v1'}/1' TO '' ${gn + 'e'} RETURN {v, e}`;
        let res = AQL_EXECUTE(q);
        assertEqual([], res.json);
        assertTrue(res.warnings.length > 0);
      });
    },

    testShortestPathNullEndVertex: function () {
      let directions = ["INBOUND", "OUTBOUND", "ANY"];
      directions.forEach(function (direction) {
        let q = `WITH ${gn + 'v1'} ${gn + 'v2'} FOR v, e IN ${direction} SHORTEST_PATH '${vn + 'v1'}/1' TO null ${gn + 'e'} RETURN {v, e}`;
        try {
          AQL_EXECUTE(q);
          fail();
        } catch (err) {
          assertEqual(err.errorNum, errors.ERROR_QUERY_PARSE.code);
        }
      });
    },

    testShortestPathNumberEndVertex: function () {
      let directions = ["INBOUND", "OUTBOUND", "ANY"];
      directions.forEach(function (direction) {
        let q = `WITH ${gn + 'v1'} ${gn + 'v2'} FOR v, e IN ${direction} SHORTEST_PATH '${vn + 'v1'}/1' TO -123 ${gn + 'e'} RETURN {v, e}`;
        try {
          AQL_EXECUTE(q);
          fail();
        } catch (err) {
          assertEqual(err.errorNum, errors.ERROR_QUERY_PARSE.code);
        }
      });
    },

    testShortestPathBothEmpty: function () {
      let directions = ["INBOUND", "OUTBOUND", "ANY"];
      directions.forEach(function (direction) {
        let q = `WITH ${gn + 'v1'} ${gn + 'v2'} FOR v, e IN ${direction} SHORTEST_PATH '' TO '' ${gn + 'e'} RETURN {v, e}`;
        let res = AQL_EXECUTE(q);
        assertEqual([], res.json);
        assertTrue(res.warnings.length > 0);
      });
    },

    testKShortestPathsNullStartVertex: function () {
      let directions = ["INBOUND", "OUTBOUND", "ANY"];
      directions.forEach(function (direction) {
        let q = `WITH ${gn + 'v1'} ${gn + 'v2'} FOR v IN ${direction} K_SHORTEST_PATHS null TO '${vn + 'v1'}/1' ${gn + 'e'} RETURN v`;
        try {
          AQL_EXECUTE(q);
          fail();
        } catch (err) {
          assertEqual(err.errorNum, errors.ERROR_QUERY_PARSE.code);
        }
      });
    },

    testKShortestPathsNumberStartVertex: function () {
      let directions = ["INBOUND", "OUTBOUND", "ANY"];
      directions.forEach(function (direction) {
        let q = `WITH ${gn + 'v1'} ${gn + 'v2'} FOR v IN ${direction} K_SHORTEST_PATHS -123 TO '${vn + 'v1'}/1' ${gn + 'e'} RETURN v`;
        try {
          AQL_EXECUTE(q);
          fail();
        } catch (err) {
          assertEqual(err.errorNum, errors.ERROR_QUERY_PARSE.code);
        }
      });
    },

    testKShortestPathsEmptyStartVertex: function () {
      let directions = ["INBOUND", "OUTBOUND", "ANY"];
      directions.forEach(function (direction) {
        let q = `WITH ${gn + 'v1'} ${gn + 'v2'} FOR v IN ${direction} K_SHORTEST_PATHS '' TO '${vn + 'v1'}/1' ${gn + 'e'} RETURN v`;
        let res = AQL_EXECUTE(q);
        assertEqual([], res.json);
        assertTrue(res.warnings.length > 0);
      });
    },

    testKShortestPathsEmptyEndVertex: function () {
      let directions = ["INBOUND", "OUTBOUND", "ANY"];
      directions.forEach(function (direction) {
        let q = `WITH ${gn + 'v1'} ${gn + 'v2'} FOR v IN ${direction} K_SHORTEST_PATHS '${vn + 'v1'}/1' TO '' ${gn + 'e'} RETURN v`;
        let res = AQL_EXECUTE(q);
        assertEqual([], res.json);
        assertTrue(res.warnings.length > 0);
      });
    },

    testKShortestPathsNullEndVertex: function () {
      let directions = ["INBOUND", "OUTBOUND", "ANY"];
      directions.forEach(function (direction) {
        let q = `WITH ${gn + 'v1'} ${gn + 'v2'} FOR v IN ${direction} K_SHORTEST_PATHS '${vn + 'v1'}/1' TO null ${gn + 'e'} RETURN v`;
        try {
          AQL_EXECUTE(q);
          fail();
        } catch (err) {
          assertEqual(err.errorNum, errors.ERROR_QUERY_PARSE.code);
        }
      });
    },

    testKShortestPathsNumberEndVertex: function () {
      let directions = ["INBOUND", "OUTBOUND", "ANY"];
      directions.forEach(function (direction) {
        let q = `WITH ${gn + 'v1'} ${gn + 'v2'} FOR v IN ${direction} K_SHORTEST_PATHS '${vn + 'v1'}/1' TO -123 ${gn + 'e'} RETURN v`;
        try {
          AQL_EXECUTE(q);
          fail();
        } catch (err) {
          assertEqual(err.errorNum, errors.ERROR_QUERY_PARSE.code);
        }
      });
    },

    testKShortestPathsBothEmpty: function () {
      let directions = ["INBOUND", "OUTBOUND", "ANY"];
      directions.forEach(function (direction) {
        let q = `WITH ${gn + 'v1'} ${gn + 'v2'} FOR v IN ${direction} K_SHORTEST_PATHS '' TO '' ${gn + 'e'} RETURN v`;
        let res = AQL_EXECUTE(q);
        assertEqual([], res.json);
        assertTrue(res.warnings.length > 0);
      });
    },

    testAllShortestPathsNullStartVertex: function () {
      let directions = ["INBOUND", "OUTBOUND", "ANY"];
      directions.forEach(function (direction) {
        let q = `WITH ${gn + 'v1'} ${gn + 'v2'} FOR v IN ${direction} ALL_SHORTEST_PATHS null TO '${vn + 'v1'}/1' ${gn + 'e'} RETURN v`;
        try {
          AQL_EXECUTE(q);
          fail();
        } catch (err) {
          assertEqual(err.errorNum, errors.ERROR_QUERY_PARSE.code);
        }
      });
    },

    testAllShortestPathsNumberStartVertex: function () {
      let directions = ["INBOUND", "OUTBOUND", "ANY"];
      directions.forEach(function (direction) {
        let q = `WITH ${gn + 'v1'} ${gn + 'v2'} FOR v IN ${direction} ALL_SHORTEST_PATHS -123 TO '${vn + 'v1'}/1' ${gn + 'e'} RETURN v`;
        try {
          AQL_EXECUTE(q);
          fail();
        } catch (err) {
          assertEqual(err.errorNum, errors.ERROR_QUERY_PARSE.code);
        }
      });
    },

    testAllShortestPathsEmptyStartVertex: function () {
      let directions = ["INBOUND", "OUTBOUND", "ANY"];
      directions.forEach(function (direction) {
        let q = `WITH ${gn + 'v1'} ${gn + 'v2'} FOR v IN ${direction} ALL_SHORTEST_PATHS '' TO '${vn + 'v1'}/1' ${gn + 'e'} RETURN v`;
        let res = AQL_EXECUTE(q);
        assertEqual([], res.json);
        assertTrue(res.warnings.length > 0);
      });
    },

    testAllShortestPathsEmptyEndVertex: function () {
      let directions = ["INBOUND", "OUTBOUND", "ANY"];
      directions.forEach(function (direction) {
        let q = `WITH ${gn + 'v1'} ${gn + 'v2'} FOR v IN ${direction} ALL_SHORTEST_PATHS '${vn + 'v1'}/1' TO '' ${gn + 'e'} RETURN v`;
        let res = AQL_EXECUTE(q);
        assertEqual([], res.json);
        assertTrue(res.warnings.length > 0);
      });
    },

    testAllShortestPathsNullEndVertex: function () {
      let directions = ["INBOUND", "OUTBOUND", "ANY"];
      directions.forEach(function (direction) {
        let q = `WITH ${gn + 'v1'} ${gn + 'v2'} FOR v IN ${direction} ALL_SHORTEST_PATHS '${vn + 'v1'}/1' TO null ${gn + 'e'} RETURN v`;
        try {
          AQL_EXECUTE(q);
          fail();
        } catch (err) {
          assertEqual(err.errorNum, errors.ERROR_QUERY_PARSE.code);
        }
      });
    },

    testAllShortestPathsNumberEndVertex: function () {
      let directions = ["INBOUND", "OUTBOUND", "ANY"];
      directions.forEach(function (direction) {
        let q = `WITH ${gn + 'v1'} ${gn + 'v2'} FOR v IN ${direction} ALL_SHORTEST_PATHS '${vn + 'v1'}/1' TO -123 ${gn + 'e'} RETURN v`;
        try {
          AQL_EXECUTE(q);
          fail();
        } catch (err) {
          assertEqual(err.errorNum, errors.ERROR_QUERY_PARSE.code);
        }
      });
    },

    testAllShortestPathsBothEmpty: function () {
      let directions = ["INBOUND", "OUTBOUND", "ANY"];
      directions.forEach(function (direction) {
        let q = `WITH ${gn + 'v1'} ${gn + 'v2'} FOR v IN ${direction} ALL_SHORTEST_PATHS '' TO '' ${gn + 'e'} RETURN v`;
        let res = AQL_EXECUTE(q);
        assertEqual([], res.json);
        assertTrue(res.warnings.length > 0);
      });
    }

  };
}

jsunity.run(invalidStartVertexSuite);
return jsunity.done();
