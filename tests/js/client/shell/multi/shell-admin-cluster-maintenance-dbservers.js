/* jshint globalstrict:false, strict:false, maxlen: 200 */
/* global assertTrue, assertEqual, assertNotEqual, arango */
'use strict';
// //////////////////////////////////////////////////////////////////////////////
// / @brief ArangoDB Enterprise License Tests
// /
// /
// / DISCLAIMER
// /
// / Copyright 2022 ArangoDB Inc, Cologne, Germany
// /
// / Licensed under the Apache License, Version 2.0 (the "License")
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
// / Copyright holder is ArangoDB Inc, Cologne, Germany
// /
// / @author Lars Maier
// //////////////////////////////////////////////////////////////////////////////

const jsunity = require('jsunity');
const internal = require("internal");

const dbservers = (function() {
  const isType = (d) => (d.instanceRole.toLowerCase() === "dbserver");
  const instanceInfo = JSON.parse(internal.env.INSTANCEINFO);
  return instanceInfo.arangods.filter(isType).map((x) => x.id);
})();

function clusterRebalanceSuite() {
  return {
    testAddRemoveServerToMaintenance: function () {
      const server = dbservers[0];
      let result = arango.PUT(`/_admin/cluster/maintenance/${server}`, {mode: "maintenance"});
      assertTrue(!result.error);
      assertEqual(result.code, 200);

      result = arango.GET(`/_admin/cluster/maintenance/${server}`);
      assertTrue(!result.error);
      assertEqual(result.code, 200);
      assertEqual(result.result.Mode, "maintenance");

      result = arango.PUT(`/_admin/cluster/maintenance/${server}`, {mode: "normal"});
      assertTrue(!result.error);
      assertEqual(result.code, 200);

      result = arango.GET(`/_admin/cluster/maintenance/${server}`);
      assertTrue(!result.error);
      assertEqual(result.code, 200);
      assertEqual(result.result, undefined);
    },

    testAddTimeoutServerToMaintenance: function () {
      const server = dbservers[0];
      let result = arango.PUT(`/_admin/cluster/maintenance/${server}`, {mode: "maintenance", timeout: 5});
      assertTrue(!result.error);
      assertEqual(result.code, 200);
      internal.sleep(4); // TODO

      let i = 0;
      for (i = 0; i < 10; i++) {
        result = arango.GET(`/_admin/cluster/maintenance/${server}`);
        if (result.error === false && result.code === 200 && result.result === undefined) {
          break;
        }
        internal.sleep(1);
      }
      assertNotEqual(i, 10);
    },
  };
}

jsunity.run(clusterRebalanceSuite);
return jsunity.done();
