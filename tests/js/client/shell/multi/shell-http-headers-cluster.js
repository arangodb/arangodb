/* jshint globalstrict:true, strict:true, maxlen: 5000 */
/* global assertTrue, assertFalse, assertEqual, arango */

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
// /
/// @author Jan Steemann
/// @author Copyright 2018, ArangoDB GmbH, Cologne, Germany
// //////////////////////////////////////////////////////////////////////////////

'use strict';

const jsunity = require("jsunity");
const _ = require("lodash");
const { getDBServers, getAgents } = require('@arangodb/test-helper');

function headersClusterSuite () {
  'use strict';

  const originalEndpoint = arango.getEndpoint();
  
  return {
    setUpAll: function() {
      arango.reconnect(originalEndpoint, "_system", arango.connectedUser(), "");
    },

    tearDownAll: function() {
      arango.reconnect(originalEndpoint, "_system", arango.connectedUser(), "");
    },

    setUp: function() {
      arango.reconnect(originalEndpoint, "_system", arango.connectedUser(), "");
    },

    tearDown: function() {
      arango.reconnect(originalEndpoint, "_system", arango.connectedUser(), "");
    },
    
    // test executing request on the coordinator
    testCoordinator: function() {
      let result = arango.GET_RAW("/_api/version");
      assertTrue(result.hasOwnProperty("headers"), "no headers found");
        
      assertTrue(result.headers.hasOwnProperty("server"), "server header not found");
      assertEqual("ArangoDB", result.headers["server"]);
    
      ["cache-control", "content-security-policy", "expires", "pragma", "strict-transport-security", "x-content-type-options"].forEach((h) => {
        assertTrue(result.headers.hasOwnProperty(h), {h, result});
      });
    
      assertTrue(result.headers.hasOwnProperty("x-arango-queue-time-seconds"), result);
    },

    // test executing requests on DB-Servers
    testDBServer: function() {
      const dbservers = getDBServers();
      assertTrue(dbservers.length > 0, "no dbservers found");
       
      dbservers.forEach(function(dbserver, i) {
        let id = dbserver.id;
        require("console").warn("connecting to dbserver", dbserver.endpoint, id);
        arango.reconnect(dbserver.endpoint, "_system", arango.connectedUser(), "");

        let result = arango.GET_RAW("/_api/version");
        assertTrue(result.hasOwnProperty("headers"), "no headers found");
        assertFalse(result.headers.hasOwnProperty("server"), "server header found");
      
        ["cache-control", "content-security-policy", "expires", "pragma", "strict-transport-security", "x-content-type-options"].forEach((h) => {
          assertFalse(result.headers.hasOwnProperty(h), {h, result});
        });

        assertFalse(result.headers.hasOwnProperty("x-arango-queue-time-seconds"), result);
      });
    },
    
    // test executing requests on agent
    testAgent: function() {
      const agents = getAgents();
      assertTrue(agents.length > 0, "no agents found");
       
      agents.forEach(function(agent, i) {
        let id = agent.id;
        require("console").warn("connecting to agent", agent.endpoint, id);
        arango.reconnect(agent.endpoint, "_system", arango.connectedUser(), "");

        let result = arango.GET_RAW("/_api/version");
        assertTrue(result.hasOwnProperty("headers"), "no headers found");
        assertFalse(result.headers.hasOwnProperty("server"), "server header found");
      
        ["cache-control", "content-security-policy", "expires", "pragma", "strict-transport-security", "x-content-type-options"].forEach((h) => {
          assertFalse(result.headers.hasOwnProperty(h), {h, result});
        });

        assertFalse(result.headers.hasOwnProperty("x-arango-queue-time-seconds"), result);
      });
    },
    
  };
}

jsunity.run(headersClusterSuite);
return jsunity.done();
