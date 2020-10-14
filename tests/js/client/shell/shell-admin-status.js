/* jshint globalstrict:false, strict:false, maxlen: 200 */
/* global assertTrue, assertEqual, assertNotEqual, arango */

// //////////////////////////////////////////////////////////////////////////////
// / @brief ArangoTransaction sTests
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
// / Copyright holder is triAGENS GmbH, Cologne, Germany
// /
// / @author Jan Steemann
// //////////////////////////////////////////////////////////////////////////////

let jsunity = require('jsunity');

function adminStatusSuite () {
  'use strict';
  
  return {

    testShort: function () {
      let result = arango.GET('/_admin/status');

      assertEqual("arango", result.server);
      
      assertTrue(result.hasOwnProperty("version"));
      
      assertTrue(result.hasOwnProperty("pid"));
      assertEqual("number", typeof result.pid);
      
      assertTrue(result.hasOwnProperty("license"));
      assertNotEqual(-1, ["enterprise", "community"].indexOf(result.license));
      
      assertEqual("server", result.mode);
      assertEqual("server", result.operationMode);

      assertTrue(result.hasOwnProperty("foxxApi"));

      assertTrue(result.hasOwnProperty("host"));
      assertEqual("string", typeof result.host);
      
      assertTrue(result.hasOwnProperty("serverInfo"));
      assertTrue(result.serverInfo.hasOwnProperty("maintenance"));
      assertTrue(result.serverInfo.hasOwnProperty("role"));
      assertNotEqual(-1, ["SINGLE", "COORDINATOR"].indexOf(result.serverInfo.role));
      assertTrue(result.serverInfo.hasOwnProperty("readOnly"));
      assertTrue(result.serverInfo.hasOwnProperty("writeOpsEnabled"));
    },
    
    testOverview: function () {
      let result = arango.GET('/_admin/status?overview=true');
      assertTrue(result.hasOwnProperty("version"));
      assertTrue(result.hasOwnProperty("platform"));
      assertTrue(result.hasOwnProperty("license"));
      assertNotEqual(-1, ["enterprise", "community"].indexOf(result.license));
      assertTrue(result.hasOwnProperty("engine"));
      assertTrue(result.hasOwnProperty("role"));
      assertNotEqual(-1, ["SINGLE", "COORDINATOR"].indexOf(result.role));
      assertTrue(result.hasOwnProperty("hash"));
      assertTrue(result.hash.length > 0);
    },

  };
}

jsunity.run(adminStatusSuite);

return jsunity.done();
