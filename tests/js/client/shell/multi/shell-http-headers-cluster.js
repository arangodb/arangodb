/* jshint globalstrict:true, strict:true, maxlen: 5000 */
/* global assertTrue, assertFalse, assertEqual, arango, require*/

////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2018 ArangoDB GmbH, Cologne, Germany
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
/// @author Jan Steemann
/// @author Copyright 2018, ArangoDB GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

'use strict';

const jsunity = require("jsunity");
const _ = require("lodash");
const { getDBServers } = require('@arangodb/test-helper');

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
      if (arango.protocol() !== 'vst') {
        // VST does not send this header, never
        assertTrue(result.headers.hasOwnProperty("server"), "server header not found");
        assertEqual("ArangoDB", result.headers["server"]);
      }
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
      });
    },
    
  };
}

jsunity.run(headersClusterSuite);
return jsunity.done();
