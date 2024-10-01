/*jshint globalstrict:false, strict:false, maxlen : 4000 */
/* global arango, assertFalse, assertEqual, assertNull  */

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
/// @author Copyright 2012, triAGENS GmbH, Cologne, Germany
// //////////////////////////////////////////////////////////////////////////////

'use strict';
const jsunity = require('jsunity');

const { getDBServers } = require('@arangodb/test-helper');

function numberOfServersSuite () {
  let numberFound = 3;  // will be overwritten in setUp
  return {
    setUpAll : function() {
      // We need the actual number of DBServers, since when we later
      // set the target number, we want that no cleanOutServer job is
      // started.
      const dbservers = getDBServers();
      numberFound = dbservers.length;
    },

    setUp : function () {
      arango.PUT("/_admin/cluster/numberOfServers", { numberOfCoordinators: null, numberOfDBServers: null });
    },

    tearDown : function () {
      arango.PUT("/_admin/cluster/numberOfServers", { numberOfCoordinators: null, numberOfDBServers: null });
    },

    testGet : function () {
      let result = arango.GET("/_admin/cluster/numberOfServers");
      assertFalse(result.error);
      assertEqual(200, result.code);
      assertNull(result.numberOfDBServers);
      assertNull(result.numberOfCoordinators);
      assertEqual([], result.cleanedServers);
    },
    
    testPutEmptyBody : function () {
      let result = arango.PUT("/_admin/cluster/numberOfServers", {});
      assertFalse(result.error);
      assertEqual(200, result.code);
    }, 
    
    testPutNumberOfDBServers : function () {
      let result = arango.PUT("/_admin/cluster/numberOfServers", { numberOfDBServers: numberFound });
      assertFalse(result.error);
      assertEqual(200, result.code);
      
      result = arango.GET("/_admin/cluster/numberOfServers");
      assertFalse(result.error);
      assertEqual(200, result.code);
      assertEqual(numberFound, result.numberOfDBServers);
      assertNull(result.numberOfCoordinators);
      assertEqual([], result.cleanedServers);
    }, 
    
    testPutNumberOfCoordinators : function () {
      let result = arango.PUT("/_admin/cluster/numberOfServers", { numberOfCoordinators: numberFound });
      assertFalse(result.error);
      assertEqual(200, result.code);
      
      result = arango.GET("/_admin/cluster/numberOfServers");
      assertFalse(result.error);
      assertEqual(200, result.code);
      assertNull(result.numberOfDBServers);
      assertEqual(numberFound, result.numberOfCoordinators);
      assertEqual([], result.cleanedServers);
    },

  };
}

jsunity.run(numberOfServersSuite);
return jsunity.done();
