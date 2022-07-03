/*jshint globalstrict:false, strict:false, maxlen: 500 */
/*global fail, assertUndefined, assertEqual, assertNotEqual, assertTrue, assertFalse, assertNull*/

////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2022 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
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
/// @author Valery Mironov
////////////////////////////////////////////////////////////////////////////////

const jsunity = require("jsunity");
const arangodb = require("@arangodb");
const {getRawMetric} = require("@arangodb/test-helper");

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////
function CoordinatorMetricsTestSuite() {
  return {
    setUpAll: function () {
    },

    tearDownAll: function () {
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief CoordinatorMetricsTestSuite tests
    ////////////////////////////////////////////////////////////////////////////////
    testMetricsParameterValidation: function () {
      let primary = arangodb.arango.getEndpoint();
      {
        let res = getRawMetric(primary, '?mode=invalid');
        assertEqual(res.code, 400);
      }
      {
        let res = getRawMetric(primary, '?type=invalid');
        assertEqual(res.code, 400);
      }
      {
        let res = getRawMetric(primary, '?type=invalid&mode=invalid');
        assertEqual(res.code, 400);
      }
      {
        let res = getRawMetric(primary, '?serverId=invalid');
        // Should be 404, see TODO in arangod/RestHandler/RestMetricsHandler.cpp
        assertEqual(res.code, 200);
      }
      {
        let res = getRawMetric(primary, '?serverId=invalid&mode=invalid');
        assertEqual(res.code, 400);
      }
      {
        let res = getRawMetric(primary, '?serverId=invalid&type=invalid');
        assertEqual(res.code, 400);
      }
      {
        let res = getRawMetric(primary, '?serverId=invalid&type=invalid&mode=invalid');
        assertEqual(res.code, 400);
      }
      {
        let res = getRawMetric(primary, '?mode=local');
        assertEqual(res.code, 400);
      }
      let type = ['?e0=e0', '?type=invalid', '?type=db_json', '?type=cd_json', '?type=last'];
      let mode = ['&e1=e1', '&mode=invalid', '&mode=local', '&mode=trigger_global', '&mode=read_global', '&mode=write_global'];
      let serverId = ['&e2=e2', '&serverId=invalid'];
      let f = (a, b) => [].concat(...a.map(a => b.map(b => [].concat(a, b))));
      let cartesian = (a, b, ...c) => b ? cartesian(f(a, b), ...c) : a;
      let params = cartesian(type, mode, serverId);
      for (let k = 0; k < params.length; k++) {
        let param = params[k][0] + params[k][1] + params[k][2];
        // require('internal').print(param);
        let res = getRawMetric(primary, param);
        // require('internal').print(res.errorCode);
      }
    },
  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suite
////////////////////////////////////////////////////////////////////////////////

jsunity.run(CoordinatorMetricsTestSuite);

return jsunity.done();
