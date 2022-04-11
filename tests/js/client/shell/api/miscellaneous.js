/* jshint globalstrict:false, strict:false, maxlen: 200 */
/* global db, fail, arango, assertTrue, assertFalse, assertEqual, assertNotUndefined */

// //////////////////////////////////////////////////////////////////////////////
// / @brief 
// /
// /
// / DISCLAIMER
// /
// / Copyright 2018 ArangoDB GmbH, Cologne, Germany
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
// / Copyright holder is ArangoDB GmbH, Cologne, Germany
// /
// / @author 
// //////////////////////////////////////////////////////////////////////////////

'use strict';

const internal = require('internal');
const sleep = internal.sleep;
const forceJson = internal.options().hasOwnProperty('server.force-json') && internal.options()['server.force-json'];
const contentType = forceJson ? "application/json" :  "application/x-velocypack";
const jsunity = require("jsunity");

let prefix = "api-miscellaneous";

function admin_statuSuite () {
  return {
    test_checks_attributes: function() {
      let cmd = "/_admin/status";
      let doc = arango.GET(cmd);
      assertEqual(doc["server"], "arango", doc);
      assertTrue(doc.hasOwnProperty("mode")); // to be removed;
      assertTrue(doc.hasOwnProperty("operationMode"));
      assertEqual(doc["mode"], doc["operationMode"]);
      assertTrue(doc.hasOwnProperty("version"));
      assertTrue(doc.hasOwnProperty("pid"));
      assertTrue(doc.hasOwnProperty("license"));
      assertTrue(doc.hasOwnProperty("host"));
      // "hostname" is optional in the response;
      // assertTrue(doc.hasOwnProperty("hostname"));
      let info = doc["serverInfo"];
      assertTrue(info.hasOwnProperty("maintenance"));
      assertTrue(info.hasOwnProperty("role"));
      assertTrue(info.hasOwnProperty("writeOpsEnabled")); // to be removed;
      assertTrue(info.hasOwnProperty("readOnly"));
      assertEqual(info["writeOpsEnabled"], !info["readOnly"]);
    }
  };
}
jsunity.run(admin_statuSuite);
return jsunity.done();
