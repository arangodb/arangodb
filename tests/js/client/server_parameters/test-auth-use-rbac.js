/*jshint globalstrict:false, strict:false */
/* global getOptions, assertEqual, assertTrue, arango */

// //////////////////////////////////////////////////////////////////////////////
// / DISCLAIMER
// /
// / Copyright 2014-2026 ArangoDB GmbH, Cologne, Germany
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
/// @author Max Neunhoeffer
// //////////////////////////////////////////////////////////////////////////////

if (getOptions === true) {
  return {
    "server.external-rbac-service": "https://non-empty-to-activate:9090",
  };
}

const jsunity = require("jsunity");
const request = require("@arangodb/request");

function testSuite() {
  return {
    testServerExternalRBACServiceOption: function () {
      let res = JSON.parse(request.get("/_admin/options").body);

      assertTrue(
        res.hasOwnProperty("server.external-rbac-service"),
        "server.exernal-rbac-sergice should be present in options",
      );
      assertEqual(
        "https://non-empty-to-activate:9090",
        res["server.external-rbac-service"],
        "server.external-rbac-service should be 'https://on-empty-to-activate:9090'",
      );
    },
  };
}

jsunity.run(testSuite);
return jsunity.done();
